#include "serverapi.h"

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    return SERVER_API_SUCCESS;
}

int closeConnection(const char* sockname) {
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
