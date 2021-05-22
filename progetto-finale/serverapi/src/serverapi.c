#include "serverapi.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int gSocketFd = -1;
static char gBuffer[4096];

int waitServerResponse();

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    // 1. Create socket
    if ((gSocketFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return SERVER_API_FAILURE;
    
    // 2. Connect to server
    struct sockaddr_un serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, sockname);
    if (connect(gSocketFd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0)
        return SERVER_API_FAILURE;

    // 3. Send 'CREATE_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_OPEN_SESSION
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int closeConnection(const char* sockname) {
    // 1. Send 'CLOSE_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_CLOSE_SESSION
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    if (waitServerResponse() != SERVER_API_SUCCESS)
        return SERVER_API_FAILURE;

    // 3. Close socket
    if (close(gSocketFd) < 0)
        return SERVER_API_FAILURE;

    // 4. Returns SUCCESS
    return SERVER_API_SUCCESS;
}

int openFile(const char* pathname, int flags) {
    int filenameLen = strlen(pathname);

    // 1. Send 'OPEN_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_OPEN_FILE,
        .request = {
            .flags = FLAG_LOCK,
            .file = {
                .filename = {
                    .len = filenameLen,
                    .abs = { .ptr = pathname }
                },
                .content = { .ptr = NULL },
                .contentLen = 0
            }
        }
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int readFile(const char* pathname, void** buf, size_t* size) {    int filenameLen = strlen(pathname);
    // 1. Send 'READ_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_READ_FILE,
        .request = {
            .flags = FLAG_EMPTY,
            .file = {
                .filename = {
                    .len = filenameLen,
                    .abs = { .ptr = pathname }
                },
                .content = { .ptr = NULL },
                .contentLen = 0
            }
        }
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int readNFiles(int N, const char* dirname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int writeFile(const char* pathname, const char* dirname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int lockFile(const char* pathname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int unlockFile(const char* pathname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int closeFile(const char* pathname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int removeFile(const char* pathname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    if (writeMessage(gSocketFd, gBuffer, 4096, &msg) != 1)
        return SERVER_API_FAILURE;
    
    // 2. Wait for server response
    freeMessageContent(&msg);
    return waitServerResponse();
}

int waitServerResponse() {
    // Wait message from server
    SockMessage_t msg;
    if (readMessage(gSocketFd, gBuffer, 4096, &msg) != 1) {
        errno = ECANCELED;
        return SERVER_API_FAILURE;
    }

    // Handles it
    switch (msg.type)
    {
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_READ_N_FILES:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            LOG_WARN("Invalid message type from server.");
            break;
        }
        
        case MSG_RESP_SIMPLE:
        {
            LOG_INFO("Resp status: %d", msg.response.status);
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            LOG_INFO("Resp status: %d", msg.response.status);
            LOG_INFO("Resp num files: %d", msg.response.numFiles);
            for (int i = 0; i < msg.response.numFiles; ++i) {
                const MsgFile_t file = msg.response.files[i];
                LOG_INFO("Resp file [#3d]: %s | %s >> %s", file.filename.abs, file.filename.rel, file.content);
            }
            break;
        }
    
        default:
            break;
    }

    return SERVER_API_SUCCESS;
}
