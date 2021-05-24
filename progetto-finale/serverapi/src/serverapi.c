#include "serverapi.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int gSocketFd   = -1;
static char* gBuffer   = NULL;
static int gBufferSize = MAX_FILE_SIZE;

static size_t bytesRead    = 0;
static size_t bytesWritten = 0;

int waitServerResponse();
int handleServerStatus(RespStatus_t);

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    // 0. Create socket
    if ((gSocketFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return SERVER_API_FAILURE;

    // 1. Allocate buffer
    if (!gBuffer) {
        gBufferSize = MAX_FILE_SIZE;
        gBuffer     = (char*) mem_malloc(MAX_FILE_SIZE * sizeof(char));
    }
    
    // 2. Connect to server
    struct sockaddr_un serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, sockname);
    if (connect(gSocketFd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0)
        return SERVER_API_FAILURE;

    // 3. Send 'MSG_REQ_OPEN_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_OPEN_SESSION
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int closeConnection(const char* sockname) {
    // 1. Send 'MSG_REQ_CLOSE_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_CLOSE_SESSION
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    if (waitServerResponse() != SERVER_API_SUCCESS)
        return SERVER_API_FAILURE;

    // 3. Close socket
    if (close(gSocketFd) < 0)
        return SERVER_API_FAILURE;
    
    // 4. Deallocate buffer
    if (gBuffer) {
        free(gBuffer);
        gBuffer     = NULL; 
        gBufferSize = 0;
    }

    // 5. Returns SUCCESS
    return SERVER_API_SUCCESS;
}

int openFile(const char* pathname, int flags) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Send 'MSG_REQ_OPEN_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_OPEN_FILE,
        .request = {
            .flags = flags,
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
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int readFile(const char* pathname, void** buf, size_t* size) {
    int filenameLen = strlen(pathname) + 1;
    
    // 1. Send 'MSG_REQ_READ_FILE' message
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
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int readNFiles(int N, const char* dirname) {
    // 1. Send 'MSG_REQ_READ_N_FILES' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_READ_N_FILES,
        .request = {
            .flags = N
        }
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int writeFile(const char* pathname, const char* dirname) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Read file content
    char* content  = NULL;
    int contentLen = 0;
    {
        // Read entire file
        FILE *f = fopen(pathname, "rb");
        if (f == NULL) return SERVER_API_FAILURE;
        fseek(f, 0, SEEK_END);
        contentLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        content = malloc(contentLen + 1);
        fread(content, 1, contentLen, f);
        fclose(f);
        content[contentLen] = '\0';
    }
    
    // 2. Send 'MSG_REQ_WRITE_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_WRITE_FILE,
        .request = {
            .flags = FLAG_EMPTY,
            .file = {
                .filename = {
                    .len = filenameLen,
                    .abs = { .ptr = pathname }
                },
                .contentLen = contentLen + 1,
                .content = { .ptr = content }
            }
        }
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 3. Wait for server response
    free(content);
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int lockFile(const char* pathname) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Send 'MSG_REQ_LOCK_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_LOCK_FILE,
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
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int unlockFile(const char* pathname) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Send 'MSG_REQ_UNLOCK_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_UNLOCK_FILE,
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
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int closeFile(const char* pathname) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Send 'MSG_REQ_CLOSE_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_CLOSE_FILE,
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
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int removeFile(const char* pathname) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Send 'MSG_REQ_REMOVE_FILE' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_REMOVE_FILE,
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
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0)
        return SERVER_API_FAILURE;
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse();
}

int waitServerResponse() {
    // Wait message from server
    SockMessage_t msg;
    int bytes = 0;
    if ((bytes = readMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        errno = ECANCELED;
        return SERVER_API_FAILURE;
    }
    bytesRead += bytes;

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
            if (handleServerStatus(msg.response.status) == SERVER_API_FAILURE)
                return SERVER_API_FAILURE;
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            LOG_INFO("Resp status: %d", msg.response.status);
            LOG_INFO("Resp num files: %d", msg.response.numFiles);
            for (int i = 0; i < msg.response.numFiles; ++i) {
                const MsgFile_t file = msg.response.files[i];
                LOG_INFO("Resp file [#%.3d]: %s | %s >> %dB", i, file.filename.abs.ptr, file.filename.rel.ptr, file.contentLen);
            }
            // TODO: Passare la dirname per il salvataggio (se necessario)
            break;
        }
    
        default:
            break;
    }

    freeMessageContent(&msg, 0);
    return SERVER_API_SUCCESS;
}

int handleServerStatus(RespStatus_t status) {
    switch (status)
    {
        case RESP_STATUS_GENERIC_ERROR:
            errno = ECANCELED;
            return SERVER_API_FAILURE;
        
        case RESP_STATUS_NOT_PERMITTED:
            errno = EPERM;
            return SERVER_API_FAILURE;
        
        case RESP_STATUS_INVALID_ARG:  
            errno = EINVAL;
            return SERVER_API_FAILURE;
        
        case RESP_STATUS_NOT_FOUND:
            errno = ENOENT;
            return SERVER_API_FAILURE;

        case RESP_STATUS_NONE:
            LOG_WARN("Server response status empty...");
        case RESP_STATUS_OK:
        default:
            break;
    }
    return SERVER_API_SUCCESS;
}

ApiBytesInfo_t getBytesData() {
    ApiBytesInfo_t result = {
        .bytesR = bytesRead,
        .bytesW = bytesWritten
    };
    bytesRead    = 0;
    bytesWritten = 0;
    return result;
}
