#include "server.h"

#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>

#include <common.h>

#define LOG_TIMESTAMP
#include <logger.h>

#include "circ_queue.h"
#include "file_system.h"
#include "session.h"

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

#define RING_BUFFER_SIZE 64
#define LOCK_QUEUE_SIZE 256

#define EXIT_REQUESTED 1000 // Main => Select   : request to stop reading
#define NEW_CONNECTION 1001 // Main => Select   : client connected
#define SET_CONNECTION 1002 // Worker => Select : readd client
#define REM_CONNECTION 1003 // Worker => Select : client disconnected

#define MAX_FILE_SIZE 32

#define LOCK_THREAD_ID 1001
#define OPT_OPEN_EMPTY  (50 | FLAG_EMPTY)
#define OPT_OPEN_CREATE (50 | FLAG_CREATE)
#define OPT_OPEN_LOCK   (50 | FLAG_LOCK)
#define OPT_OPEN_WRITE  (50 | FLAG_CREATE | FLAG_LOCK)

#define ELAPSED_MICR_S(e,b) (((e.tv_sec - b.tv_sec) * 1e6 + (e.tv_nsec - b.tv_nsec) * 1e-3))

// ======================================== DECLARATIONS: Types =====================================================

// Syntactic sugar
typedef struct sigaction SigAction_t;
typedef struct sockaddr_un SocketAddress_t;

typedef struct { int id; } WorkerThreadArgs_t;

// ======================================== DECLARATIONS: Inner functions ===========================================

void signalHandlerCallback();
void setupSignals();
int setupSocket();
void cleanup();
int spawnWorkers();
int spawnSideThreads();
void* workerThreadFun(void*);
void* selectThreadFun(void*);
void* lockThreadFun(void*);
SockMessage_t handleWork(int, int, SockMessage_t);
void handleError(int, SockMessage_t*);
FSFile_t copyRequestIntoFile(SockMessage_t);
FSFile_t deepCopyRequestIntoFile(SockMessage_t);
void log_into_file(SockMessage_t*, RespStatus_t, long long, long long, long long, int, int);

// ======================================= DEFINITIONS: Global vars =================================================

volatile sig_atomic_t gSigIntReceived  = 0;
volatile sig_atomic_t gSigQuitReceived = 0;
volatile sig_atomic_t gSigHupReceived  = 0;
static int gShouldStopWorking          = 0;

static pthread_mutex_t gNumClientMutex = PTHREAD_MUTEX_INITIALIZER;
static int gNumClientConnected         = 0;

static WorkerThreadArgs_t* gWorkerArgs = NULL;
static pthread_t* gWorkerThreads       = NULL;
static pthread_t gSelectThread;
static pthread_t gLockThread;
static ServerConfig_t gConfigs;
static fd_set gFdSet;
static int gSocketFd = -1;
static int gMaxFd    = 0;

static FILE* gLogFile = NULL;
static struct timespec gServerStartTime;

// Multiples write using the same pipe are guaranteed to be atomic under certain sizes (PIPE_BUF)
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html#tag_16_685
static int mainToSelectPipe[2]         = { -1, -1 };

static CircQueue_t* gWorkQueue         = NULL;
static pthread_mutex_t gWorkMutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gWorkCond        = PTHREAD_COND_INITIALIZER;
static CircQueue_t* gLockQueue         = NULL;
static pthread_mutex_t gLockMutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gLockCond        = PTHREAD_COND_INITIALIZER;

// ======================================= DEFINITIONS: client.h functions ==========================================

int initializeServer(ServerConfig_t configs) {
    LOG_VERB("[#MN] Initializing server ...");
    gConfigs = configs;

    // Registering function to call on exit
    atexit(cleanup);

    // Open log file
    if ((gLogFile = fopen(configs.logFilename, "wb")) == NULL) {
        LOG_ERRNO("[#MN] Error opening log file");
        return RES_ERROR;
    }

    // Signals
    gSigIntReceived  = 0;
    gSigQuitReceived = 0;
    gSigHupReceived  = 0;
    setupSignals();

    // Socket
    if (setupSocket() != RES_OK) return RES_ERROR;

    // Msg queue & Lock queue
    gWorkQueue = createQueue(RING_BUFFER_SIZE);
    gLockQueue = createQueue(LOCK_QUEUE_SIZE);

    // Initialize Sessions & FS
    initSessionSystem();
    initializeFileSystem(gConfigs.fsConfigs, gLockQueue, &gLockCond, &gLockMutex);

    // Pipe MainThread -> SelectThread
    if (pipe(mainToSelectPipe) < 0) {
        LOG_ERRNO("[#MN] Error creating pipe");
        return RES_ERROR;
    }

    // File descriptor set
    FD_ZERO(&gFdSet);
    FD_SET(mainToSelectPipe[PIPE_READ_END], &gFdSet);
    gMaxFd = mainToSelectPipe[PIPE_READ_END];

    // Threads
    LOG_VERB("[#MN] Spawning threads...");
    if (spawnWorkers() != RES_OK)     return RES_ERROR;
    if (spawnSideThreads() != RES_OK) return RES_ERROR;

    // Return success
    return RES_OK;
}

int runServer() {
    LOG_INFO("[#MN] Server running ...");
    clock_gettime(CLOCK_MONOTONIC, &gServerStartTime);

    // Run until signal is received
    int newConnFd = -1;
    while (!gSigIntReceived && !gSigQuitReceived && !gSigHupReceived) {
        // Accept new client
        if ((newConnFd = accept(gSocketFd, NULL, 0)) < 0) {
            if (errno != EINTR) {
                LOG_ERRNO("[#MN] Unable to accept client");
            }
            continue;
        }

        // Send message to Select thread
        int messageToSend[2] = { NEW_CONNECTION, newConnFd };
        if (writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, 2 * sizeof(int)) < 0) {
            LOG_ERRNO("[#MN] Unable to send message to select thread");
            LOG_WARN("[#MN] Client cannot be accepted.");
            if (close(newConnFd) < 0)
                LOG_ERRNO("[#MN] Error closing client connection");
            continue;
        }
    }

    // Returns success
    return RES_OK;
}

int terminateSever() {
    LOG_VERB("[#MN] Terminating server ...");

    // Send signal
    lock_mutex(&gWorkMutex);
    notify_all(&gWorkCond);
    unlock_mutex(&gWorkMutex);
    
    // Wait worker threads
    LOG_VERB("[#MN] Waiting worker threads...");
    for (int i = 0; i < gConfigs.numWorkers; ++i)
        if (pthread_join(gWorkerThreads[i], NULL) < 0)
            LOG_ERRNO("[#MN] Error joining worker thread #%d", i + 1);
    free(gWorkerThreads);
    free(gWorkerArgs);

    // Now lock thread can stop
    gShouldStopWorking = 1;
    
    // Signal lock thread
    lock_mutex(&gLockMutex);
    notify_one(&gLockCond);
    unlock_mutex(&gLockMutex);
    
    // Wait lock thread
    LOG_VERB("[#MN] Waiting lock thread...");
    if (pthread_join(gLockThread, NULL) < 0)
        LOG_ERRNO("[#MN] Error joining lock thread");
    
    // Send request to exit from select thread
    int messageToSend = EXIT_REQUESTED;
    if (writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, sizeof(int)) < 0) {
        LOG_ERRNO("[#MN] Unable to send message to select thread");
        LOG_WARN("[#MN] Stopping thread using pthread_cancel...");

        // Select is a cancellation point.
        // source:
        //    https://man7.org/linux/man-pages/man7/pthreads.7.html
        if (pthread_cancel(gSelectThread) < 0) {
            LOG_ERRNO("[#MN] Unable to cancel execution of select thread");
        }
    } else {
        LOG_VERB("[#MN] Message 'EXIT_REQUESTED' sent to select thread");
    }
    
    // Wait select thread
    LOG_VERB("[#MN] Waiting select thread...");
    if (pthread_join(gSelectThread, NULL) < 0)
        LOG_ERRNO("[#MN] Error joining select thread");

    // Free work queue
    free(gWorkQueue->data);
    free(gWorkQueue);

    // Free lock queue
    free(gLockQueue->data);
    free(gLockQueue);

    // Close Sessions & FS (Log execution summary)
    terminateSessionSystem();
    terminateFileSystem();

    // Close log file
    if (fflush(gLogFile) != 0)
        LOG_ERRNO("[#MN] Error flushing log file");
    if (fclose(gLogFile) != 0)
        LOG_ERRNO("[#MN] Error closing log file");
    gLogFile = NULL;

    // Returns success
    return RES_OK;
}

// ======================================= DEFINITIONS: Inner functions =============================================

void signalHandlerCallback(int signum) {
    gSigIntReceived  |= (signum == SIGINT);
    gSigQuitReceived |= (signum == SIGQUIT);
    gSigHupReceived  |= (signum == SIGHUP);
}

void setupSignals() {
    LOG_VERB("[#MN] Registering signals...");

    // Signal list
    static int signals[] = { SIGINT, SIGQUIT, SIGHUP };

    // On signal received action
    SigAction_t s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = signalHandlerCallback;
    for (int i = 0; i < 3; ++i) {
        // Try setting signal action
        if (sigaction(signals[i], &s, NULL) < 0) {
            LOG_ERRNO("[#MN] Unable to register handler for signal %d", signals[i]);
            LOG_WARN("[#MN] Server is using the default handler");
        }
    }
}

int setupSocket() {
    LOG_VERB("[#MN] Setupping socket...");

    // Create socket
    if ((gSocketFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        LOG_ERRNO("[#MN] Error creating socket");
        return RES_ERROR;
    }
    
    // Binding socket
    SocketAddress_t addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, gConfigs.socketFilename);
    unlink(gConfigs.socketFilename); // Make sure file does not exist
    if (bind(gSocketFd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        LOG_ERRNO("[#MN] Error binding socket");
        return RES_ERROR;
    }

    // Start listening
    if (listen(gSocketFd, gConfigs.maxClients) < 0) {
        LOG_ERRNO("[#MN] Error starting listen");
        return RES_ERROR;
    }

    // Returns success
    return RES_OK;
}

void cleanup() {
    LOG_VERB("[#MN] Starting cleanup process...");
    
    // Check for valid socket
    if (gSocketFd > 0) {
        if (close(gSocketFd) < 0)
            LOG_ERRNO("[#MN] Error closing socket");
    }

    // Close pipe
    if (mainToSelectPipe[PIPE_READ_END] > 0)
        if (close(mainToSelectPipe[PIPE_READ_END]) < 0)
            LOG_ERRNO("[#MN] Error closing pipe 0");
    if (mainToSelectPipe[PIPE_WRITE_END] > 0)
        if (close(mainToSelectPipe[PIPE_WRITE_END]) < 0)
            LOG_ERRNO("[#MN] Error closing pipe 1");

    // Delete socket fd
    if (unlink(gConfigs.socketFilename) < 0)
        LOG_ERRNO("[#MN] Error deleting socket file");

    // Close log file
    if (gLogFile) {
        fflush(gLogFile);
        fclose(gLogFile);
        gLogFile = NULL;
    }

    LOG_VERB("[#MN] Cleanup terminated");
}

int numClientConnected() {
    int res = 0;
    lock_mutex(&gNumClientMutex);
    res = gNumClientConnected;
    unlock_mutex(&gNumClientMutex);
    return res;
}

void* workerThreadFun(void* args) {
    // Mask signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    // vars
    char* _inn_buffer   = NULL;
    size_t innerBufferSize = 0;
    const int threadID  = ((WorkerThreadArgs_t*) args)->id;
    int res = 0;

    // Stop working when SIGINT / SIGQUIT received and on SIGHUP when there're no more clients connected
    while (!gSigIntReceived && !gSigQuitReceived && !(gSigHupReceived && (numClientConnected() == 0))) {
        // Vars
        CircQueueItemPtr_t item = NULL;
        long long bytesRead = 0, bytesWrote = 0;

        // Lock mutex and wait on cond (if queue is empty)
        lock_mutex(&gWorkMutex);
        while (!gSigIntReceived && !gSigQuitReceived && !(gSigHupReceived && (numClientConnected() == 0)) && (tryPop(gWorkQueue, &item) == 0)) {
            LOG_VERB("[#%.2d] waiting for work...", threadID);
            // Wait for cond
            if ((res = pthread_cond_wait(&gWorkCond, &gWorkMutex)) != 0) {
                errno = res;
                LOG_ERRNO("[#%.2d] Error waiting on cond var inside worker func", threadID);
            }
        }
        unlock_mutex(&gWorkMutex);

        // Check if exited for signal
        if (item == NULL) {
            LOG_VERB("[#%.2d] Queue empty %d %d %d %d", threadID, numClientConnected(), gSigIntReceived, gSigQuitReceived, gSigHupReceived);
            continue;
        }
        struct timespec begin, end;
        clock_gettime(CLOCK_MONOTONIC, &begin);

        // Get FD
        int client = (intptr_t) item;

        // Read message from client
        SockMessage_t requestMsg;
        if ((bytesRead = readMessage(client, &_inn_buffer, &innerBufferSize, &requestMsg)) < 0) {
            LOG_ERRNO("[#SE] Error reading message");
            // Send message to select pipe to notify client disconnected
            int messageToSend[2] = { REM_CONNECTION, client };
            writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, 2 * sizeof(int)); 
            continue;
        }
        if (bytesRead == 0) {
            // Handle disconnection
            ClientSession_t* session = NULL;
            if (getRawSession(client, &session) == 0) {
                // Clean FS using its session
                fs_clean(client, session);
                // Clean session
                destroySession(client);
            }
            LOG_VERB("[#SE] Client on FD#%02d disconnected !", client);
            // Send message to select pipe to notify client disconnected
            int messageToSend[2] = { REM_CONNECTION, client };
            writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, 2 * sizeof(int)); 
            continue;
        }
        // Message received !
        LOG_VERB("[#SE] Message received from FD#%0d", client);

        // Handle message
        SockMessage_t responseMsg = handleWork(threadID, client, requestMsg);
        if ((requestMsg.type == MSG_REQ_LOCK_FILE || (requestMsg.type == MSG_REQ_OPEN_FILE && (requestMsg.request.flags == FLAG_LOCK))) && responseMsg.type == MSG_NONE) {
            // Unable to lock.
            // Already pushed into side queue for future handling
        } else {
            LOG_VERB("[#%.2d] work completed. Sending response...", threadID);
            // Write response to client
            if ((bytesWrote = writeMessage(client, &_inn_buffer, &innerBufferSize, &responseMsg)) == -1)
                LOG_ERRNO("Error sending response");
            // Readd descriptor to set
            int messageToSend[2] = { SET_CONNECTION, client };
            writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, 2 * sizeof(int));
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        // Log request
        log_into_file(&requestMsg, responseMsg.response.status, ELAPSED_MICR_S(end, begin), bytesRead, bytesWrote, threadID, client);
       
        // Release resources
        freeMessageContent(&requestMsg , 1);
        freeMessageContent(&responseMsg, 1);
    }

    free(_inn_buffer);
    return NULL;
}

void* selectThreadFun(void* args) {
    // Mask signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    // Run until signal is received
    fd_set fds_cpy;
    int res = -1;
    while (!gSigIntReceived && !gSigQuitReceived) {
        // Copy set of file descriptor
        fds_cpy = gFdSet;

        // Select
        if ((res = select(gMaxFd + 1, &fds_cpy, NULL, NULL, NULL)) < 0) {
            LOG_ERRNO("[#SE] Error monitoring descriptors with pselect");
            continue; // Retry
        }

        // Should never happend
        if (res == 0) {
            // Timeout (?)
            LOG_WARN("[#SE] Select timeout");
        }

        // HAS PRIORITY: Check if message comes from MainThread
        if (FD_ISSET(mainToSelectPipe[PIPE_READ_END], &fds_cpy)) {
            // Read message type
            int message;
            if ((res = readN(mainToSelectPipe[PIPE_READ_END], (char*) &message, sizeof(int))) < 0) {
                LOG_ERRNO("[#SE] Error reading from pipe");
            }
            else {
                // Check for request to exit
                if (message == EXIT_REQUESTED) {
                    break;
                }

                // Otherwise
                int value;
                readN(mainToSelectPipe[PIPE_READ_END], (char*) &value, sizeof(int));
                
                // Check message type
                switch (message)
                {
                    case NEW_CONNECTION:
                        // Add descriptor to set
                        FD_SET(value, &gFdSet);
                        gMaxFd = MAX(gMaxFd, value);
                        LOG_VERB("[#SE] Client connected on FD#%02d !", value);
                        // Increment counter
                        lock_mutex(&gNumClientMutex);
                        ++gNumClientConnected;
                        unlock_mutex(&gNumClientMutex);
                        break;
                    
                    case REM_CONNECTION:
                        // Client disconnected !
                        if (close(value) < 0)
                            LOG_ERRNO("[#SE] Error closing socket #%02d", value);
                        // Decrement counter
                        lock_mutex(&gNumClientMutex);
                        --gNumClientConnected;
                        unlock_mutex(&gNumClientMutex);
                        // Send signal to another worker
                        lock_mutex(&gWorkMutex);
                        notify_all(&gWorkCond);
                        unlock_mutex(&gWorkMutex);
                        break;
                    
                    case SET_CONNECTION:
                        // Readd descriptor to set
                        FD_SET(value, &gFdSet);
                        gMaxFd = MAX(gMaxFd, value);
                        break;
                
                    default:
                        break;
                }
            }
        }

        // Check for active descriptors
        int newMaxFD = 0;
        for(int fd = 0; fd <= gMaxFd; fd++) {
            // Check file descriptor
            if (FD_ISSET(fd, &fds_cpy)) {
                // Already checked
                if (fd == mainToSelectPipe[PIPE_READ_END]) continue;

                // Remove fd from set
                FD_CLR(fd, &gFdSet);
                
                // Try push message into queue
                if (tryPush(gWorkQueue, (void*) (intptr_t) fd) == 0) {
                    LOG_WARN("[#SE] Msg queue is full. Consider upgrading its capacity. (curr = %d)", gWorkQueue->capacity);
                    // Disconnect client !
                    if (close(fd) < 0)
                        LOG_ERRNO("[#SE] Error closing socket #%02d", fd);
                }

                // Signal item added to queue
                lock_mutex(&gWorkMutex);
                notify_one(&gWorkCond);
                unlock_mutex(&gWorkMutex);

                // Jump last line
                continue;
            }
            newMaxFD = fd;
        }
        gMaxFd = newMaxFD;
    }

    return NULL;
}

void* lockThreadFun(void* args) {
    // Mask signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    // vars
    size_t innerBufferSize = 1024;
    char* _inn_buff = (char*) mem_malloc(innerBufferSize * sizeof(char)); // Can be fixed because responses are always the same
    int res = 0;
    SockMessage_t msg;

    // Stop working when SIGINT / SIGQUIT received
    while (!gSigIntReceived && !gSigQuitReceived && !gShouldStopWorking) {
        // Reset item
        CircQueueItemPtr_t item = NULL;
        long long bytesWrote = 0;

        // Lock mutex and wait on cond (if queue is empty)
        lock_mutex(&gLockMutex);
        while (!gSigIntReceived && !gSigQuitReceived && !gShouldStopWorking && (tryPop(gLockQueue, &item) == 0)) {
            LOG_VERB("[#LK] waiting for notifications...");
            // Wait for cond
            if ((res = pthread_cond_wait(&gLockCond, &gLockMutex)) != 0) {
                errno = res;
                LOG_ERRNO("[#LK] Error waiting on cond var inside lock thread func");
            }
        }
        unlock_mutex(&gLockMutex);

        // Check if exited for signal
        if (item == NULL) {
            LOG_VERB("[#LK] Queue empty");
            continue;
        }

        // Someone unlocked a file that was locked by this client
        // and the file_system automatically locked it for him, notifying
        // the queue this thread is waiting for to send response to client.
        FSLockNotification_t notification = *((FSLockNotification_t*) item);

        // Empty response
        msg = (SockMessage_t) {
            .uid  = UUID_new(),
            .type = MSG_RESP_SIMPLE,
            .response = {
                .status = RESP_STATUS_OK
            }
        };

        // Handle error (if any)
        handleError(notification.status, &msg);

        // Write response to client
        LOG_VERB("[#LK] Notification arrived. Sending %d to %d...", msg.response.status, notification.fd);
        if ((bytesWrote = writeMessage(notification.fd, &_inn_buff, &innerBufferSize, &msg)) == -1)
            LOG_ERRNO("Error sending response");

        // Readd descriptor to set
        int messageToSend[2] = { SET_CONNECTION, notification.fd };
        writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, 2 * sizeof(int));

        // Release memory
        free(item);
        
        // Log request
        SockMessage_t tmp = { .type = MSG_REQ_LOCK_FILE, .uid = EMPTY_UUID };
        log_into_file(&tmp, msg.response.status, 0LL, 0LL, bytesWrote, LOCK_THREAD_ID, notification.fd);
    }
    free(_inn_buff);
    return NULL;
}

int spawnWorkers() {
    gWorkerArgs    = (WorkerThreadArgs_t*) mem_malloc(gConfigs.numWorkers * sizeof(WorkerThreadArgs_t));
    gWorkerThreads = (pthread_t*) mem_malloc(gConfigs.numWorkers * sizeof(pthread_t));
    for (int i = 0; i < gConfigs.numWorkers; ++i) {
        gWorkerArgs[i].id = i;
        if (pthread_create(&gWorkerThreads[i], NULL, workerThreadFun, &gWorkerArgs[i]) < 0) {
            LOG_ERRNO("[#MN] Error creating worker thread");
            LOG_CRIT("[#MN] Unable to start worker thread #%d", i + 1);
            return RES_ERROR;
        }
    }
    return RES_OK;
}

int spawnSideThreads() {
    if (pthread_create(&gSelectThread, NULL, selectThreadFun, NULL) < 0) {
        LOG_ERRNO("[#MN] Error creating select thread");
        return RES_ERROR;
    }
    if (pthread_create(&gLockThread, NULL, lockThreadFun, NULL) < 0) {
        LOG_ERRNO("[#MN] Error creating lock thread");
        return RES_ERROR;
    }
    return RES_OK;
}

SockMessage_t handleWork(int workingThreadID, int client, SockMessage_t msg) {
    // Empty response
    SockMessage_t response = {
        .uid  = msg.uid,
        .type = MSG_RESP_SIMPLE,
        .response = {
            .status = RESP_STATUS_OK
        }
    };

    // Handle request
    int res            = 0;    // tmp var to handle partial results
    int outFilesCount  = 0;    // store emitted files count
    FSFile_t* outFiles = NULL; // store emitted files
    switch (msg.type)
    {
        case MSG_REQ_OPEN_SESSION:
        {
            // Create session
            if ((res = createSession(client)) != 0) {
                LOG_ERRO("[#%.2d] Error creating session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            // LOG
            LOG_VERB("[#%.2d] Intialized session for client #%.2d", workingThreadID, client);
            break;
        }

        case MSG_REQ_CLOSE_SESSION:
        {
            // Handle disconnection
            ClientSession_t* session = NULL;
            if (getRawSession(client, &session) == 0) {
                // Clean FS using its session
                fs_clean(client, session);
                // Clean session
                destroySession(client);
            } else  {
                LOG_ERRO("[#%.2d] Error destroying session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            // LOG
            LOG_VERB("[#%.2d] Terminated session for client #%.2d", workingThreadID, client);
            break;
        }

        case MSG_REQ_OPEN_FILE:
        {
            // Retrieve session
            int session;
            if ((res = getSession(client, &session)) != 0) {
                LOG_ERRO("[#%.2d] Error retrieving session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            // Check file was not opened
            const int flags    = msg.request.flags;
            SessionFile_t file = {
                .name = msg.request.file.filename.abs.ptr,
                .len  = msg.request.file.filename.len,
            };
            if ((res = hasOpenedFile(session, file)) != SESSION_FILE_NEVER_OPENED) {
                LOG_ERRO("[#%.2d] Error opening file '%s' for client #%.2d", workingThreadID, file.name, client);
                handleError(res, &response);
                break;
            }

            // Open file
            int isLockRequested = flags & FLAG_LOCK;
            if (flags & FLAG_CREATE) {
                // Create file
                FSFile_t fs_file = deepCopyRequestIntoFile(msg);
                if ((res = fs_insert(client, fs_file, isLockRequested, &outFiles, &outFilesCount)) != 0) {
                    LOG_ERRO("[#%.2d] Error creating file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            } else if(isLockRequested) {
                // Lock file
                FSFile_t fs_file = copyRequestIntoFile(msg);
                if ((res = fs_trylock(client, fs_file)) != 0) {
                    // Save response as 'none' or 'empty'
                    if (res == FS_CLIENT_WAITING_ON_LOCK) {
                        response.type = MSG_NONE;
                        break;
                    }
                    LOG_ERRO("[#%.2d] Error locking file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            } else {
                // Ensure it exists
                FSFile_t fs_file = copyRequestIntoFile(msg);
                if ((res = fs_exists(client, fs_file)) != 0) {
                    LOG_ERRO("[#%.2d] Error opening file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            }

            // Save file opened
            if ((res = addFileOpened(session, file, flags)) != 0) {
                LOG_ERRO("[#%.2d] Error opening file '%s' for client #%.2d", workingThreadID, file.name, client);
                handleError(res, &response);
                break;
            }

            // LOG
            LOG_VERB("[#%.2d] File '%s' opened with flags %d for client #%.2d", workingThreadID, file.name, flags, client);
            break;
        }

        case MSG_REQ_CLOSE_FILE:
        {
            // Retrieve session
            int session;
            if ((res = getSession(client, &session)) != 0) {
                LOG_ERRO("[#%.2d] Error retrieving session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            // Check file was opened
            SessionFile_t file = {
                .name = msg.request.file.filename.abs.ptr,
                .len  = msg.request.file.filename.len,
            };
            if ((res = hasOpenedFile(session, file)) != SESSION_FILE_ALREADY_OPENED) {
                LOG_ERRO("[#%.2d] Error closing file '%s' for client #%.2d", workingThreadID, file.name, client);
                handleError(res, &response);
                break;
            }

            // Close file
            // Try unlock file
            FSFile_t fs_file = copyRequestIntoFile(msg);
            // Do not handle errors. Simply unlock the file if owned. Otherwise just do nothing.
            fs_unlock(client, fs_file);

            // Save file closed
            if ((res = remFileOpened(session, file)) != 0) {
                LOG_ERRO("[#%.2d] Error closing file '%s' for client #%.2d", workingThreadID, file.name, client);
                handleError(res, &response);
                break;
            }

            // LOG
            LOG_VERB("[#%.2d] File '%s' closed for client #%.2d", workingThreadID, file.name, client);
            break;
        }

        case MSG_REQ_READ_N_FILES:
        {
            // Read n files
            const int n = msg.request.flags;
            if ((res = fs_obtain_n(client, n, &outFiles, &outFilesCount)) != 0) {
                LOG_ERRO("[#%.2d] Error reading %d files for client #%.2d", workingThreadID, n, client);
                handleError(res, &response);
                break;
            }

            // LOG
            LOG_VERB("[#%.2d] %d file read for client #%.2d", workingThreadID, outFilesCount, client);
            break;
        }

        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            // Retrieve session
            int session;
            if ((res = getSession(client, &session)) != 0) {
                LOG_ERRO("[#%.2d] Error retrieving session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            SessionFile_t file = {
                .name = msg.request.file.filename.abs.ptr,
                .len  = msg.request.file.filename.len,
            };
            if (msg.type == MSG_REQ_WRITE_FILE) {
                // Ensure file was opened and can write into it
                if ((res = canWriteIntoFile(session, file)) != 0) {
                    LOG_ERRO("[#%.2d] Error writing file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }                
            } else {
                // Check file was opened
                if ((res = hasOpenedFile(session, file)) != SESSION_FILE_ALREADY_OPENED) {
                    LOG_ERRO("[#%.2d] Error %d, file '%s' was not open for client #%.2d", workingThreadID, res, file.name, client);
                    handleError(res, &response);
                    break;
                }
            }

            // Apply request
            switch (msg.type)
            {
                case MSG_REQ_WRITE_FILE:
                {
                    // Write file
                    FSFile_t fs_file = deepCopyRequestIntoFile(msg);
                    if ((res = fs_modify(client, fs_file, &outFiles, &outFilesCount)) != 0) {
                        LOG_ERRO("[#%.2d] Error writing file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        if (fs_file.content) free((char*) fs_file.content);
                        free((char*) fs_file.name);
                        break;
                    }
                    break;
                }

                case MSG_REQ_READ_FILE:
                {
                    // Read file
                    FSFile_t outFile;
                    FSFile_t fs_file = copyRequestIntoFile(msg);
                    if ((res = fs_obtain(client, fs_file, &outFile)) != 0) {
                        LOG_ERRO("[#%.2d] Error reading file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    // Add file to outFiles
                    outFiles      = (FSFile_t*) mem_malloc(sizeof(FSFile_t));
                    outFilesCount = 1;
                    outFiles[0]   = outFile;
                    break;
                }

                case MSG_REQ_LOCK_FILE:
                {
                    // Lock file
                    FSFile_t fs_file = copyRequestIntoFile(msg);
                    if ((res = fs_trylock(client, fs_file)) != 0) {
                        // Save response as 'none' or 'empty'
                        if (res == FS_CLIENT_WAITING_ON_LOCK) {
                            response.type = MSG_NONE;
                            break;
                        }
                        LOG_ERRO("[#%.2d] Error locking file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                    }
                    break;
                }

                case MSG_REQ_UNLOCK_FILE:
                {
                    // Unlock file
                    FSFile_t fs_file = copyRequestIntoFile(msg);
                    if ((res = fs_unlock(client, fs_file)) != 0) {
                        LOG_ERRO("[#%.2d] Error unlocking file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    break;
                }

                case MSG_REQ_REMOVE_FILE:
                {
                    // Remove file
                    FSFile_t fs_file = copyRequestIntoFile(msg);
                    if ((res = fs_remove(client, fs_file)) != 0) {
                        LOG_ERRO("[#%.2d] Error removing file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    break;
                }

                case MSG_REQ_APPEND_TO_FILE:
                {
                    // Append data to file
                    FSFile_t fs_file = copyRequestIntoFile(msg);
                    if ((res = fs_append(client, fs_file, &outFiles, &outFilesCount)) != 0) {
                        LOG_ERRO("[#%.2d] Error appending data to file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    break;
                }
    
                default:
                    // Should never happend
                    break;
            }

            if (msg.type == MSG_REQ_REMOVE_FILE) {
                // Save file closed
                if ((res = remFileOpened(session, file)) != 0) {
                    LOG_ERRO("[#%.2d] Error closing file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            }

            // LOG
            LOG_VERB("[#%.2d] Generic request (%d) handled from client #%.2d", workingThreadID, msg.type, client);
            break;
        }

        case MSG_NONE:
        case MSG_RESP_SIMPLE:
        case MSG_RESP_WITH_FILES:
        default:
        {
            // LOG
            LOG_WARN("[#%.2d] Invalid message type (%d) from client #%.2d", workingThreadID, msg.type, client);
            response.response.status = RESP_STATUS_INVALID_ARG;
            break;
        }
    }

    // Add outFiles to response
    if (outFilesCount > 0) {
        // Update response type
        response.type = MSG_RESP_WITH_FILES;
        // Copy data
        MsgFile_t* files = mem_malloc(outFilesCount * sizeof(MsgFile_t));
        for (int i = 0; i < outFilesCount; ++i) {
            files[i].contentLen       = outFiles[i].contentLen; // 
            files[i].content.ptr      = outFiles[i].content;    // Pass ptr
            files[i].filename.len     = outFiles[i].nameLen;    // 
            files[i].filename.abs.ptr = outFiles[i].name;       // Pass ptr
        }
        response.response.files    = files;         // Add files to response
        response.response.numFiles = outFilesCount; // Add files len to response
        // Release memory
        free(outFiles);
    }

    // Return response
    return response;
}

void handleError(int status, SockMessage_t* response) {
    LOG_VERB("Handling status %d...", status);
    switch (status)
    {
        case FS_FILE_ALREADY_EXISTS:
        case FS_FILE_NOT_EXISTS:
        case FS_FILE_TOO_BIG:
        case SESSION_FILE_ALREADY_OPENED:
        {
            response->response.status = RESP_STATUS_INVALID_ARG;
            break;
        }

        case SESSION_FILE_NEVER_OPENED:
        {
            response->response.status = RESP_STATUS_NOT_FOUND;
            break;
        }

        case SESSION_OUT_OF_MEMORY:
        {
            response->response.status = RESP_STATUS_GENERIC_ERROR;
            break;
        }

        case FS_CLIENT_NOT_ALLOWED:
        case SESSION_ALREADY_EXIST:
        case SESSION_NOT_EXIST:
        case SESSION_CANNOT_WRITE_FILE:
        {
            response->response.status = RESP_STATUS_NOT_PERMITTED;
            break;
        }

        case 0:
        default:
            // Should never append
            break;
    }
}

FSFile_t copyRequestIntoFile(SockMessage_t msg) {
    FSFile_t fs_file = {
        .content    = msg.request.file.content.ptr,
        .contentLen = msg.request.file.contentLen,
        .name       = msg.request.file.filename.abs.ptr,
        .nameLen    = msg.request.file.filename.len,
    };
    return fs_file;
}

FSFile_t deepCopyRequestIntoFile(SockMessage_t msg) {
    // Copy name
    int nLen = msg.request.file.filename.len;
    char* filename = (char*) mem_malloc(nLen * sizeof(char));
    memcpy(filename, msg.request.file.filename.abs.ptr, nLen * sizeof(char));
    
    // Copy content (if any)
    int cLen = msg.request.file.contentLen;
    char* content  = (cLen) ? (char*) mem_malloc(cLen * sizeof(char)) : NULL;
    if (cLen) memcpy(content, msg.request.file.content.ptr, cLen * sizeof(char));

    // Copy data
    FSFile_t fs_file = {
        .content    = content,
        .contentLen = cLen,
        .name       = filename,
        .nameLen    = nLen,
    };
    return fs_file;
}

void log_into_file(SockMessage_t* msg, RespStatus_t status, long long msec, long long bytesRead, long long bytesWrote, int workingThreadID, int client) {
    SockMessageType_t type = msg->type;
    if (type == MSG_REQ_OPEN_FILE) {
        type = 50 | msg->request.flags;
    }

    // Table for codes
    static const char* const opTable[] = {
        [MSG_NONE]               = "--",
        [MSG_RESP_SIMPLE]        = "??",
        [MSG_RESP_WITH_FILES]    = "??",
        [MSG_REQ_APPEND_TO_FILE] = "AF",
        [MSG_REQ_CLOSE_FILE]     = "CF",
        [MSG_REQ_CLOSE_SESSION]  = "CS",
        [MSG_REQ_LOCK_FILE]      = "LF",
        [OPT_OPEN_CREATE]        = "OC",
        [OPT_OPEN_EMPTY]         = "OE",
        [MSG_REQ_OPEN_FILE]      = "OF",
        [OPT_OPEN_LOCK]          = "OL",
        [MSG_REQ_OPEN_SESSION]   = "OS",
        [OPT_OPEN_WRITE]         = "OW",
        [MSG_REQ_READ_FILE]      = "RF",
        [MSG_REQ_REMOVE_FILE]    = "RM",
        [MSG_REQ_READ_N_FILES]   = "RN",
        [MSG_REQ_UNLOCK_FILE]    = "UF",
        [MSG_REQ_WRITE_FILE]     = "WF",
    };

    // Calc timestamp
    struct timespec opTime;
    clock_gettime(CLOCK_MONOTONIC, &opTime);
    long long timeInMsFromBegin = ELAPSED_MICR_S(opTime, gServerStartTime);

    // Copy var
    lock_mutex(&gNumClientMutex);
    int connectedClientCount = gNumClientConnected;
    unlock_mutex(&gNumClientMutex);

    // Log data
    lock_mutex(&gLogMutex);
    if (workingThreadID == LOCK_THREAD_ID) {
        FSInfo_t info = fs_get_infos();
        LOG_EMPTY_INTO_STREAM(gLogFile, "T:%12lld | ms: %8lld | [#LK] {%s} | C-ID:%3d | CC:%4d | R:%12lld | W:%12lld | FS-B:%12lld | FS-S:%4d | FS-M:%4d |\n", timeInMsFromBegin, msec,
            opTable[type], client, connectedClientCount, bytesRead, bytesWrote, info.bytesUsedCount, info.slotsUsedCount, info.capacityMissCount);
    } else {
        FSInfo_t info = fs_get_infos();
        LOG_EMPTY_INTO_STREAM(gLogFile, "T:%12lld | ms: %8lld | [#%.2d] {%s} | C-ID:%3d | CC:%4d | R:%12lld | W:%12lld | FS-B:%12lld | FS-S:%4d | FS-M:%4d |\n", timeInMsFromBegin, msec,
            workingThreadID, opTable[type], client, connectedClientCount, bytesRead, bytesWrote, info.bytesUsedCount, info.slotsUsedCount, info.capacityMissCount);
    }
    unlock_mutex(&gLogMutex);
}
