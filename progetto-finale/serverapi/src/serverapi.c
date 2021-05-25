#include "serverapi.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 32 * 1024 * 1024 // ~> 1MB

static int gSocketFd   = -1;
static char* gBuffer   = NULL;
static int gBufferSize = MAX_BUFFER_SIZE;

static size_t bytesRead    = 0;
static size_t bytesWritten = 0;

int waitServerResponse(const char*);
int handleServerStatus(RespStatus_t);

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    // 0. Create socket
    if ((gSocketFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return SERVER_API_FAILURE;

    // 1. Allocate buffer
    if (!gBuffer) {
        gBufferSize = MAX_BUFFER_SIZE;
        gBuffer     = (char*) mem_malloc(MAX_BUFFER_SIZE * sizeof(char));
    }
    
    // 2. Connect to server
    struct sockaddr_un serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, sockname);
    
    struct timespec waitTime = { .tv_nsec = (msec % 1000) * 1000000, .tv_sec = msec / 1000 };
    int status = 0;
    long timeout = (abstime.tv_sec * 1000 + (abstime.tv_nsec / 1000000));
    while (((status = connect(gSocketFd, (struct sockaddr*) &serveraddr, sizeof(serveraddr))) < 0) && timeout > 0) {
        timeout -= msec;
        if (nanosleep(&waitTime, NULL) < 0)
            LOG_ERRNO("Error sleeping");
        LOG_VERB("Retrying to connect to server...");
    }
    if (status < 0)
        return SERVER_API_FAILURE;

    // 3. Send 'MSG_REQ_OPEN_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_OPEN_SESSION
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    // bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
}

int closeConnection(const char* sockname) {
    // 1. Send 'MSG_REQ_CLOSE_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_CLOSE_SESSION
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    // bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    if (waitServerResponse(NULL) != SERVER_API_SUCCESS)
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(dirname);
}

int writeFile(const char* pathname, const char* dirname) {
    int filenameLen = strlen(pathname) + 1;

    // 1. Read file content
    char* content  = NULL;
    int contentLen = 0;
    {
        // Read entire file
        int res = 0;
        FILE *f = fopen(pathname, "rb");                   // Open the file
        if (f == NULL) return SERVER_API_FAILURE;          // Ensure correct opening
        do {                                               // Simplify error managment
            if ((res = fseek(f, 0, SEEK_END)) != 0) break; // Set 'cursor' to EOF
            if ((contentLen = ftell(f)) < 0) {             // Retrieve 'cursor' position
                res = contentLen;                          // 
                contentLen = 0;                            // 
                break;                                     // 
            }                                              // 
            if ((res = fseek(f, 0, SEEK_SET)) != 0) break; // Set 'cursor' to begin of file
            content = (char*) mem_malloc(contentLen);      // Allocate memory to read entire file
            fread(content, sizeof(char), contentLen, f);   // Read entire file
        } while(0);                                        // 
        fclose(f);                                         // Close file
        if (res) {                                         // On error
            if (content) free(content);                    //    release content (if allocated)
            return SERVER_API_FAILURE;                     //    return error
        }                                                  // 
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
                .contentLen = contentLen,
                .content = { .ptr = content }
            }
        }
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        free(content);
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 3. Wait for server response
    free(content);
    freeMessageContent(&msg, 0);
    return waitServerResponse(dirname);
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    // 1. Send '' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_NONE
    };
    int bytes = 0;
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(dirname);
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
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
    if ((bytes = writeMessage(gSocketFd, gBuffer, gBufferSize, &msg)) <= 0) {
        freeMessageContent(&msg, 0);
        return SERVER_API_FAILURE;
    }
    bytesWritten += bytes;
    
    // 2. Wait for server response
    freeMessageContent(&msg, 0);
    return waitServerResponse(NULL);
}

int waitServerResponse(const char* dirname) {
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
            if (handleServerStatus(msg.response.status) == SERVER_API_FAILURE)
                return SERVER_API_FAILURE;
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            LOG_VERB("Resp status: %d", msg.response.status);
            LOG_VERB("Resp num files: %d", msg.response.numFiles);
            for (int i = 0; i < msg.response.numFiles; ++i) {
                const MsgFile_t file = msg.response.files[i];
                LOG_VERB("Resp file [#%.3d]: %s >> %dB", i, file.filename.abs.ptr, file.contentLen);
            }
            if (dirname) {
                for (int i = 0; i < msg.response.numFiles; ++i) {
                    int contentLen        = msg.response.files[i].contentLen;
                    const char* content   = msg.response.files[i].content.ptr;
                    const char* pathname  = msg.response.files[i].filename.abs.ptr;

                    LOG_VERB("Saving file %s into directory %s ...", pathname, dirname);

                    // Calc path
                    static char mkdirCmdPrefix[] = "mkdir -p ";
                    static char mkdirCmd[4096];
                    memset(mkdirCmd, 0, 4096 * sizeof(char));
                    strcpy(mkdirCmd, mkdirCmdPrefix);
                    strcat(mkdirCmd, dirname);
                    strcat(mkdirCmd, "/");
                    strcat(mkdirCmd, pathname);
                    char* path = &mkdirCmd[9];
                    int pathLen = strlen(path);

                    // Temporary shrink string
                    int slashIndex = 0;
                    for (int c = pathLen - 1; c > 0; --c) {
                        if (path[c] == '/') {
                            path[c] = '\0';
                            slashIndex = c;
                            break;
                        }
                    }

                    // Create directory
                    system(mkdirCmd);
                    path[slashIndex] = '/';

                    // Open file
                    FILE* file = NULL;
                    if ((file = fopen(path, "wb")) == NULL) {
                        LOG_ERRNO("Error opening file %s", path);
                        continue;
                    }
                    
                    // Write file
                    if (fwrite(content, sizeof(char), contentLen, file) != contentLen)
                        LOG_ERRNO("Error writing file %s", path);

                    // Close file
                    fclose(file);
                }
            }
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
