#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/un.h>

#define LOG_TIMESTAMP
// #define LOG_WITHOUT_COLORS
#define LOG_DEBUG
#include <logger.h>
#include <common.h>

#include "server.h"
#include "circ_queue.h"
#include "file_system.h"
#include "session.h"

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

#define RING_BUFFER_SIZE 16

#define EXIT_REQUESTED 1000 // Message sent from main thread to select thread to request to stop reading
#define NEW_CONNECTION 1001 // Message sent from main thread to select thread to notify client connection

// ======================================== DECLARATIONS: Types =====================================================

// Syntactic sugar
typedef struct sigaction SigAction_t;
typedef struct sockaddr_un SocketAddress_t;

typedef struct { int id; } WorkerThreadArgs_t;
typedef struct { int fd; SockMessage_t msg; } WorkEntry_t;

// ======================================== DECLARATIONS: Inner functions ===========================================

void signalHandlerCallback();
void setupSignals();
int setupSocket();
void cleanup();
int spawnWorkers();
void* workerThreadFun();
int spawnThreadForSelect();
void* selectThreadFun();
SockMessage_t handleWork(int, ClientID, SockMessage_t);
void handleError(int, SockMessage_t*);
FSFile_t copySessionFileIntoFSFile(SessionFile_t file);
FSFile_t deepCopySessionFileIntoFSFile(SessionFile_t file);

// ======================================= DEFINITIONS: Global vars =================================================

volatile sig_atomic_t gSigIntReceived  = 0;
volatile sig_atomic_t gSigQuitReceived = 0;
volatile sig_atomic_t gSigHupReceived  = 0;

static WorkerThreadArgs_t* gWorkerArgs = NULL;
static pthread_t* gWorkerThreads       = NULL;
static pthread_t gSelectThread;
static ServerConfig_t gConfigs;
static fd_set gFdSet;
static int gMaxFd                      = -1;
static int gSocketFd                   = -1;
static int mainToSelectPipe[2]         = { -1, -1 };
static CircQueue_t* gMsgQueue          = NULL;
static pthread_mutex_t gCondMutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gQueueCond       = PTHREAD_COND_INITIALIZER;

// ======================================= DEFINITIONS: client.h functions ==========================================

int initializeServer(ServerConfig_t configs) {
    LOG_VERB("[#MN] Initializing server ...");
    gConfigs = configs;

    // Registering function to call on exit
    atexit(cleanup);

    // Signals
    gSigIntReceived  = 0;
    gSigQuitReceived = 0;
    gSigHupReceived  = 0;
    setupSignals();

    // Socket
    if (setupSocket() != RES_OK) return RES_ERROR;

    // Msg queue
    gMsgQueue = createQueue(RING_BUFFER_SIZE);

    // Initialize FS
    initializeFileSystem(gConfigs.fsConfigs);

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
    if (spawnWorkers() != RES_OK) return RES_ERROR;
    if (spawnThreadForSelect() != RES_OK) return RES_ERROR;

    // Return success
    return RES_OK;
}

int runServer() {
    LOG_INFO("[#MN] Server running ...");

    // Run until signal is received
    int newConnFd = -1;
    while (!gSigIntReceived && !gSigQuitReceived && !gSigHupReceived) {
        // Accept new client
        if ((newConnFd = accept(gSocketFd, NULL, 0)) < 0) {
            if (errno != EINTR) LOG_ERRNO("[#MN] Unable to accept client");
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

    // Returns success
    return RES_OK;
}

int terminateSever() {
    LOG_VERB("[#MN] Terminating server ...");
    
    // Wait select thread
    LOG_VERB("[#MN] Waiting select thread...");
    if (pthread_join(gSelectThread, NULL) < 0)
        LOG_ERRNO("[#MN] Error joining select thread");

    // Broadcast signal
    lock_mutex(&gCondMutex);
    notify_all(&gQueueCond);
    unlock_mutex(&gCondMutex);
    
    // Wait worker threads
    LOG_VERB("[#MN] Waiting worker threads...");
    for (int i = 0; i < gConfigs.numWorkers; ++i)
        if (pthread_join(gWorkerThreads[i], NULL) < 0)
        LOG_ERRNO("[#MN] Error joining worker thread #%d", i + 1);
    free(gWorkerThreads);
    free(gWorkerArgs);

    // Free remaining items inside queue (if any)
    CircQueueItemPtr_t item;
    while (tryPop(gMsgQueue, &item) == 1) {
        freeMessageContent(&((WorkEntry_t*) item)->msg);
    }
    free(gMsgQueue->data);
    free(gMsgQueue);

    // Close FS
    terminateFileSystem();

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
    strcpy(addr.sun_path, gConfigs.socketFilame);
    unlink(gConfigs.socketFilame); // Make sure file does not exist
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
    if (unlink(gConfigs.socketFilame) < 0)
        LOG_ERRNO("[#MN] Error deleting socket file");
    
    LOG_VERB("[#MN] Cleanup terminated");
}

void* workerThreadFun(void* args) {
    // vars
    char _inn_buffer[4096];
    const int threadID = ((WorkerThreadArgs_t*) args)->id;
    int res = 0;

    // Stop working when SIGINT / SIGQUIT received and on SIGHUP when queue is empty
    int hasFoundItem = 0;
    while (!gSigIntReceived && !gSigQuitReceived && !(gSigHupReceived && hasFoundItem)) {
        // Reset item
        CircQueueItemPtr_t item = NULL;

        // Lock mutex and wait on cond (if queue is empty)
        lock_mutex(&gCondMutex);
        while (!gSigIntReceived && !gSigQuitReceived && !gSigHupReceived && (hasFoundItem = tryPop(gMsgQueue, &item)) == 0) {
            LOG_INFO("[#%.2d] waiting for work...", threadID);
            // Wait for cond
            if ((res = pthread_cond_wait(&gQueueCond, &gCondMutex)) != 0) {
                errno = res;
                LOG_ERRNO("[#%.2d] Error waiting on cond var inside worker func", threadID);
            }
        }
        unlock_mutex(&gCondMutex);

        // Has exited from above loop so item should contains a message
        // Double check to ensure correct behaviour
        if (item == NULL) {
            LOG_VERB("[#%.2d] Queue empty", threadID);
            continue;
        }

        // Message read.
        ClientID client       = ((WorkEntry_t*) (item))->fd;
        SockMessage_t request = ((WorkEntry_t*) (item))->msg;
        LOG_INFO("[#%.2d] work received.", threadID);

        // Handle message
        SockMessage_t response = handleWork(threadID, client, request);
        LOG_INFO("[#%.2d] work completed. Sending response...", threadID);

        // Write response to client
        if (writeMessage(client, _inn_buffer, 4096, &response) == -1)
            LOG_ERRNO("Error sending response");

        // Release resources        
        freeMessageContent(&request);
        freeMessageContent(&response);
    }
    
    return NULL;
}

void* selectThreadFun(void* args) {
    // TODO: Update adding a config
    char _inn_buffer[4096];
    SockMessage_t msg;
    int bufferIndex = 0;
    WorkEntry_t works[RING_BUFFER_SIZE];

    // Run until signal is received
    fd_set fds_cpy;
    int res = -1;
    while (!gSigIntReceived && !gSigQuitReceived && !gSigHupReceived) {
        // Copy set of file descriptor
        fds_cpy = gFdSet;
        errno = 0;

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
            int message;

            // Read message type
            if ((res = readN(mainToSelectPipe[PIPE_READ_END], (char*) &message, sizeof(int))) < 0)
                LOG_ERRNO("[#SE] Error reading from pipe");
            else {
                LOG_VERB("[#SE] Message received from main: %d", message);

                // Main thread received a signal, select thread should stop
                if (message == EXIT_REQUESTED) break;

                // Main thread accepted a new request, add it to the set
                else if (message == NEW_CONNECTION) {
                    // Read file descriptor
                    if ((res = readN(mainToSelectPipe[PIPE_READ_END], (char*) &message, sizeof(int))) < 0)
                        LOG_ERRNO("[#SE] Error reading new connection descriptor from pipe");
                    else {
                        FD_SET(message, &gFdSet);      // Add descriptor to set
                        gMaxFd = MAX(gMaxFd, message); // Update max
                        LOG_VERB("[#SE] Client connected on FD#%02d !", message);
                    }
                }
            }
        }

        // Check for active descriptors
        int newMaxFd = 0;
        for(int fd = 0; fd <= gMaxFd; fd++) {
            // Find new max
            newMaxFd = fd;

            // Check file descriptor
            if (FD_ISSET(fd, &fds_cpy)) {
                // Already checked
                if (fd == mainToSelectPipe[PIPE_READ_END]) continue;

                // Read message from client
                if ((res = readMessage(fd, _inn_buffer, 4096, &msg)) < 0) {
                    LOG_ERRNO("[#SE] Error reading message");
                    continue;
                }
                if (res == 0) {
                    // Client disconnected !
                    if (close(fd) < 0)
                        LOG_ERRNO("[#SE] Error closing socket #%02d", fd);
                    // Update fd set
                    FD_CLR(fd, &gFdSet);                 // Remove descriptor from set
                    newMaxFd = (fd == 0) ? 0 : (fd - 1); // Remove this descriptor from max calculation
                    LOG_VERB("[#SE] Client on FD#%02d disconnected !", fd);
                } else {
                    // Message received !
                    LOG_VERB("[#SE] Message received from FD#%0d", fd);
                    // try push message into queue
                    WorkEntry_t* work = &works[bufferIndex];
                    work->fd = fd;
                    work->msg = msg;
                    if (tryPush(gMsgQueue, work) == 0)
                        LOG_WARN("[#SE] Msg queue is full. Consider upgrading its capacity. (curr = %d)", gMsgQueue->capacity);
                    else {
                        bufferIndex = (bufferIndex + 1) % RING_BUFFER_SIZE;                        
                    }
                    // Signal item added to queue
                    lock_mutex(&gCondMutex);
                    notify_one(&gQueueCond);
                    unlock_mutex(&gCondMutex);
                }
            }
        }
        gMaxFd = newMaxFd;
    }

    return NULL;
}

int spawnWorkers() {
    gWorkerArgs    = (WorkerThreadArgs_t*) mem_malloc(gConfigs.numWorkers * sizeof(WorkerThreadArgs_t));
    gWorkerThreads = (pthread_t*) mem_malloc(gConfigs.numWorkers * sizeof(pthread_t));
    for (int i = 0; i < gConfigs.numWorkers; ++i) {
        gWorkerArgs[i].id = i;
        if (pthread_create(&gWorkerThreads[i], NULL, workerThreadFun, &gWorkerArgs[i]) < 0) {
            LOG_ERRNO("[#MN] Creating worker thread");
            LOG_CRIT("[#MN] Unable to start worker thread #%d", i + 1);
            return RES_ERROR;
        }
    }
    return RES_OK;
}

int spawnThreadForSelect() {
    if (pthread_create(&gSelectThread, NULL, selectThreadFun, NULL) < 0) {
        LOG_ERRNO("[#MN] Creating select thread");
        return RES_ERROR;
    }
    return RES_OK;
}

SockMessage_t handleWork(int workingThreadID, ClientID client, SockMessage_t request) {
    // Prepare response
    SockMessage_t response = {
        .uid  = UUID_new(),
        .type = MSG_RESP_SIMPLE,
        .response = {
            .status = RESP_STATUS_OK
        }
    };

    // Handle request
    int res            = 0; // tmp var to handle partial results
    int outFilesCount  = 0; // store emitted files count
    FSFile_t* outFiles = 0; // store emitted files
    switch (request.type)
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
            // Destroy session
            if ((res = destroySession(client)) != 0) {
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
            SessionClientID session;
            if ((res = getSession(client, &session)) != 0) {
                LOG_ERRO("[#%.2d] Error retrieving session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            // Check file was not opened
            const int flags    = request.request.flags;
            SessionFile_t file = {
                .name = request.request.file.filename.abs.ptr,
                .len  = request.request.file.filename.len,
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
                FSFile_t fs_file = deepCopySessionFileIntoFSFile(file);
                if ((res = fs_insert(client, fs_file, isLockRequested, &outFiles, &outFilesCount)) != 0) {
                    LOG_ERRO("[#%.2d] Error creating file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    if (fs_file.content) free((char*) fs_file.content);
                    if (fs_file.name)    free((char*) fs_file.name);
                    break;
                }
            } else if(isLockRequested) {
                // Lock file
                FSFile_t fs_file = copySessionFileIntoFSFile(file);
                if ((res = fs_trylock(client, fs_file)) != 0) {
                    LOG_ERRO("[#%.2d] Error locking file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            } else {
                // Ensure it exists
                FSFile_t fs_file = copySessionFileIntoFSFile(file);
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
            SessionClientID session;
            if ((res = getSession(client, &session)) != 0) {
                LOG_ERRO("[#%.2d] Error retrieving session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            // Check file was opened
            SessionFile_t file = {
                .name = request.request.file.filename.abs.ptr,
                .len  = request.request.file.filename.len,
            };
            if ((res = hasOpenedFile(session, file)) != SESSION_FILE_ALREADY_OPENED) {
                LOG_ERRO("[#%.2d] Error closing file '%s' for client #%.2d", workingThreadID, file.name, client);
                handleError(res, &response);
                break;
            }

            // Close file
            // Try unlock file
            FSFile_t fs_file = copySessionFileIntoFSFile(file);
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
            // TODO:
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
            SessionClientID session;
            if ((res = getSession(client, &session)) != 0) {
                LOG_ERRO("[#%.2d] Error retrieving session for client #%.2d", workingThreadID, client);
                handleError(res, &response);
                break;
            }

            SessionFile_t file = {
                .name = request.request.file.filename.abs.ptr,
                .len  = request.request.file.filename.len,
            };
            if (request.type == MSG_REQ_WRITE_FILE) {
                // Ensure file was opened and can write into it
                if ((res = canWriteIntoFile(session, file)) != 0) {
                    LOG_ERRO("[#%.2d] Error writing file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }                
            } else {
                // Check file was opened
                if ((res = hasOpenedFile(session, file)) != SESSION_FILE_ALREADY_OPENED) {
                    LOG_ERRO("[#%.2d] Error closing file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            }

            // Apply request
            switch (request.type)
            {
                case MSG_REQ_WRITE_FILE:
                {
                    // TODO:
                    break;
                }

                case MSG_REQ_READ_FILE:
                {
                    // Read file
                    FSFile_t outFile;
                    FSFile_t fs_file = copySessionFileIntoFSFile(file);
                    if ((res = fs_obtain(client, fs_file, &outFile)) != 0) {
                        LOG_ERRO("[#%.2d] Error reading file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    // Add file to response
                    // TODO:
                    if (outFile.content) free((char*) outFile.content);
                    if (outFile.name)    free((char*) outFile.name);
                    break;
                }

                case MSG_REQ_LOCK_FILE:
                {
                    // Lock file
                    FSFile_t fs_file = copySessionFileIntoFSFile(file);
                    if ((res = fs_trylock(client, fs_file)) != 0) {
                        // TODO:
                        // 
                        // if (res == FS_CLIENT_NOT_ALLOWED) pushIntoSideQueue()
                        // 
                        LOG_ERRO("[#%.2d] Error locking file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    break;
                }

                case MSG_REQ_UNLOCK_FILE:
                {
                    // Unlock file
                    FSFile_t fs_file = copySessionFileIntoFSFile(file);
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
                    FSFile_t fs_file = copySessionFileIntoFSFile(file);
                    if ((res = fs_remove(client, fs_file)) != 0) {
                        LOG_ERRO("[#%.2d] Error removing file '%s' for client #%.2d", workingThreadID, file.name, client);
                        handleError(res, &response);
                        break;
                    }
                    break;
                }

                case MSG_REQ_APPEND_TO_FILE:
                {
                    // TODO: 
                    break;
                }
    
                default:
                    // Should never happend
                    break;
            }

            if (request.type == MSG_REQ_REMOVE_FILE) {
                // Save file closed
                if ((res = remFileOpened(session, file)) != 0) {
                    LOG_ERRO("[#%.2d] Error closing file '%s' for client #%.2d", workingThreadID, file.name, client);
                    handleError(res, &response);
                    break;
                }
            }

            // LOG
            LOG_VERB("[#%.2d] Generic request (%d) handled from client #%.2d", workingThreadID, request.type, client);
            break;
        }

        case MSG_NONE:
        case MSG_RESP_SIMPLE:
        case MSG_RESP_WITH_FILES:
        default:
        {
            // LOG
            LOG_WARN("[#%.2d] Invalid message type (%d) from client #%.2d", workingThreadID, request.type, client);
            response.response.status = RESP_STATUS_INVALID_ARG;
            break;
        }
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

FSFile_t copySessionFileIntoFSFile(SessionFile_t file) {
    FSFile_t fs_file = {
        .content    = NULL,
        .contentLen = 0,
        .name       = file.name,
        .nameLen    = file.len,
    };
    return fs_file;
}

FSFile_t deepCopySessionFileIntoFSFile(SessionFile_t file) {
    char* name = (char*) mem_malloc(file.len * sizeof(char));
    memcpy(name, file.name, file.len * sizeof(char));
    FSFile_t fs_file = {
        .content    = NULL,
        .contentLen = 0,
        .name       = name,
        .nameLen    = file.len,
    };
    return fs_file;

}
