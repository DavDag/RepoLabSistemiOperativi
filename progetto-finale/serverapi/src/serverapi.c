#include "serverapi.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int sockfd = -1;
static char buffer[4096];

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    errno = 0; // Reset errno
    int res = -1;

    // 1. Create socket
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return SERVER_API_FAILURE;
    
    // 2. Connect to server
    struct sockaddr_un serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, sockname);
    if ((res = connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr))) < 0)
        return SERVER_API_FAILURE;

    // 3. Send 'CREATE_SESSION' message
    SockMessage_t msg = {
        .uid = UUID_new(),
        .type = MSG_REQ_OPEN_SESSION
    };
    writeMessage(sockfd, buffer, 4096, &msg);

    // 4. Returns SUCCESS
    return SERVER_API_SUCCESS;
}

int closeConnection(const char* sockname) {
    errno = 0;
    int res = -1;

    // 1. Send 'CLOSE_SESSION' message
    // TODO:

    // 2. Close socket
    if ((res = close(sockfd)) < 0)
        return SERVER_API_FAILURE;

    // 3. Returns SUCCESS
    return SERVER_API_SUCCESS;
}

int openFile(const char* pathname, int flags) {
    return SERVER_API_SUCCESS;
}

int readFile(const char* pathname, void** buf, size_t* size) {
    return SERVER_API_SUCCESS;
}

int readNFiles(int N, const char* dirname) {
    return SERVER_API_SUCCESS;
}

int writeFile(const char* pathname, const char* dirname) {
    return SERVER_API_SUCCESS;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    return SERVER_API_SUCCESS;
}

int lockFile(const char* pathname) {
    return SERVER_API_SUCCESS;
}

int unlockFile(const char* pathname) {
    return SERVER_API_SUCCESS;
}

int closeFile(const char* pathname) {
    return SERVER_API_SUCCESS;
}

int removeFile(const char* pathname) {
    return SERVER_API_SUCCESS;
}
