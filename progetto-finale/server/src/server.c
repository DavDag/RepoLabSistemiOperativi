#define _POSIX_C_SOURCE 200809L
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

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

#define EXIT_REQUESTED 1000 // Message sent from main thread to select thread to request to stop reading

// ======================================== DECLARATIONS: Types =====================================================

// Syntactic sugar
typedef struct sigaction SigAction_t;
typedef struct sockaddr_un SocketAddress_t;

// ======================================== DECLARATIONS: Inner functions ===========================================

void signalHandlerCallback();
void setupSignals();
int setupSocket();
void cleanup();
int spawnWorkers();
void* workerThreadFun();
int spawnThreadForSelect();
void* selectThreadFun();

// ======================================= DEFINITIONS: Global vars =================================================

volatile sig_atomic_t gSigIntReceived  = 0;
volatile sig_atomic_t gSigQuitReceived = 0;
volatile sig_atomic_t gSigHupReceived  = 0;

static pthread_t* gWorkerThreads = NULL, gSelectThread;
static size_t gWorkerThreadsSize = 0;
static ServerConfig_t gConfigs;
static fd_set gFdSet;
static int gMaxFd = -1, gSocketFd = -1, mainToSelectPipe[2];

// ======================================= DEFINITIONS: client.h functions ==========================================

int initializeServer(ServerConfig_t configs) {
    LOG_VERB("Initializing server ...");

    // Registering function to call on exit
    atexit(cleanup);

    // Signals
    gSigIntReceived  = 0;
    gSigQuitReceived = 0;
    gSigHupReceived  = 0;
    setupSignals();

    // Socket and global state
    gConfigs = configs;
    if (setupSocket() != RES_OK) return RES_ERROR;

    // Pipe MainThread -> SelectThread
    if (pipe(mainToSelectPipe) < 0) {
        LOG_ERRNO("Error creating pipe");
        return RES_ERROR;
    }

    // File descriptor set
    FD_ZERO(&gFdSet);
    FD_SET(mainToSelectPipe[PIPE_READ_END], &gFdSet);
    gMaxFd = mainToSelectPipe[PIPE_READ_END];

    // Threads
    LOG_VERB("Spawning threads...");
    if (spawnWorkers() != RES_OK) return RES_ERROR;
    if (spawnThreadForSelect() != RES_OK) return RES_ERROR;

    // Return success
    return RES_OK;
}

int runServer() {
    LOG_INFO("Server running ...");

    // Run until signal is received
    int newConnFd = -1;
    while (!gSigIntReceived && !gSigQuitReceived && !gSigHupReceived) {
        // Accept new client
        if ((newConnFd = accept(gSocketFd, NULL, 0)) < 0) {
            LOG_ERRNO("Unable to accept client");
            continue;
        }
        LOG_VERB("Client accepted");

        // Update set & max 
        FD_SET(newConnFd, &gFdSet);
        gMaxFd = MAX(gMaxFd, newConnFd);
    }

    // Send request to exit from select thread
    int messageToSend = EXIT_REQUESTED;
    if (writeN(mainToSelectPipe[PIPE_WRITE_END], (char*) &messageToSend, sizeof(int)) < 0) {
        LOG_ERRNO("Unable to send message to select thread");
        LOG_WARN("Stopping thread using pthread_cancel...");

        // Select is a cancellation point.
        // source:
        //    https://man7.org/linux/man-pages/man7/pthreads.7.html
        if (pthread_cancel(gSelectThread) < 0) {
            LOG_ERRNO("Unable to cancel execution of select thread");
        }
    } else {
        LOG_VERB("Message 'EXIT_REQUESTED' sent to select thread");
    }

    // Returns success
    return RES_OK;
}

int terminateSever() {
    LOG_VERB("Terminating server ...");
    
    // Wait all threads
    
    // Select thread
    LOG_VERB("Waiting select thread...");
    if (pthread_join(gSelectThread, NULL) < 0)
        LOG_ERRNO("Error joining select thread");
    
    // Worker threads
    LOG_VERB("Waiting worker threads...");
    for (int i = 0; i < gWorkerThreadsSize; ++i)
        if (pthread_join(gWorkerThreads[i], NULL) < 0)
        LOG_ERRNO("Error joining worker thread #%d", i + 1);

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
    LOG_VERB("Registering signals...");

    // Signal list
    static int signals[] = { SIGINT, SIGQUIT, SIGHUP };

    // On signal received action
    SigAction_t s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = signalHandlerCallback;
    for (int i = 0; i < 3; ++i) {
        // Try setting signal action
        if (sigaction(signals[i], &s, NULL) < 0) {
            LOG_ERRNO("Unable to register handler for signal %d", signals[i]);
            LOG_WARN("Server is using the default handler");
        }
    }
}

int setupSocket() {
    LOG_VERB("Setupping socket...");

    // Create socket
    if ((gSocketFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        LOG_ERRNO("Error creating socket");
        return RES_ERROR;
    }
    
    // Binding socket
    SocketAddress_t addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, gConfigs.socketFilame);
    unlink(gConfigs.socketFilame); // Make sure file does not exist
    if (bind(gSocketFd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        LOG_ERRNO("Error binding socket");
        return RES_ERROR;
    }

    // Start listening
    if (listen(gSocketFd, gConfigs.maxClients) < 0) {
        LOG_ERRNO("Error starting listen");
        return RES_ERROR;
    }

    // Returns success
    return RES_OK;
}

void cleanup() {
    LOG_VERB("Starting cleanup process...");
    
    // Check for valid socket
    if (gSocketFd > 0) {
        if (close(gSocketFd) < 0)
            LOG_ERRNO("Error closing socket");
    }

    // Close pipe
    if (mainToSelectPipe[PIPE_READ_END] > 0)
        if (close(mainToSelectPipe[PIPE_READ_END]) < 0)
            LOG_ERRNO("Error closing pipe 0");
    if (mainToSelectPipe[PIPE_WRITE_END] > 0)
        if (close(mainToSelectPipe[PIPE_WRITE_END]) < 0)
            LOG_ERRNO("Error closing pipe 1");

    // Delete socket fd
    if (unlink(gConfigs.socketFilame) < 0)
        LOG_ERRNO("Error deleting socket file");
    
    LOG_VERB("Cleanup terminated");
}

void* workerThreadFun(void* args) {
    // TODO:

    return NULL;
}

void* selectThreadFun(void* args) {
    // TODO: Update adding a config
    char _inn_buffer[4096];
    SockMessage_t msg;

    // Run until signal is received
    fd_set fds_cpy;
    int res = -1;
    while (!gSigIntReceived && !gSigQuitReceived && !gSigHupReceived) {
        // Copy set of file descriptor
        fds_cpy = gFdSet;
        errno = 0;

        // Select
        if ((res = select(gMaxFd + 1, &fds_cpy, NULL, NULL, NULL)) < 0) {
            LOG_ERRNO("Error monitoring descriptors with pselect");
            continue; // Retry
        }

        // Should never happend
        if (res == 0) {
            // Timeout (?)
            LOG_WARN("Select timeout");
        }

        // HAS PRIORITY: Check if message comes from MainThread
        if (FD_ISSET(mainToSelectPipe[PIPE_READ_END], &fds_cpy)) {
            int message;
            if ((res = readN(mainToSelectPipe[PIPE_READ_END], (char*) &message, sizeof(int))) < 0)
                LOG_ERRNO("Error reading from pipe");
            else {
                LOG_VERB("Message received from main: %d", message);
                if (message == EXIT_REQUESTED) break;
            }
        }

        // Check for active descriptors
        for(int fd = 0; fd <= gMaxFd; fd++) {
            if (FD_ISSET(fd, &fds_cpy)) {
                // Already checked
                if (fd == mainToSelectPipe[PIPE_READ_END]) continue;

                // Read message from client
                if ((res = readMessage(fd, _inn_buffer, 4096, &msg)) < 0) {
                    LOG_ERRNO("Error reading message");
                    continue;
                }
                if (res == 0) {
                    // TODO: Handle client disconnect
                } else {
                    // TODO: Message received !
                    LOG_INFO("New message: uuid: %s, type: %d", UUID_to_String(msg.uid), msg.type);
                }
            }
        }
    }

    return NULL;
}

int spawnWorkers() {
    gWorkerThreadsSize = gConfigs.maxClients;
    gWorkerThreads = (pthread_t*) mem_malloc(sizeof(pthread_t) * gWorkerThreadsSize);
    for (int i = 0; i < gWorkerThreadsSize; ++i) {
        if (pthread_create(&gWorkerThreads[i], NULL, workerThreadFun, NULL) < 0) {
            LOG_ERRNO("Creating worker thread");
            LOG_CRIT("Unable to start worker thread #%d", i + 1);
            return RES_ERROR;
        }
    }
    return RES_OK;
}

int spawnThreadForSelect() {
    if (pthread_create(&gSelectThread, NULL, selectThreadFun, NULL) < 0) {
        LOG_ERRNO("Creating select thread");
        return RES_ERROR;
    }
    return RES_OK;
}
