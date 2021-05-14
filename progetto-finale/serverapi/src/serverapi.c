#include "serverapi.h"

#include <common.h>

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    LOG_VERB("ServerApi: openConnection(\"%s\", %d, %ld)", sockname, msec, abstime.tv_sec);
    return RES_OK;
}

int closeConnection(const char* sockname) {
    LOG_VERB("ServerApi: closeConnection(\"%s\")", sockname);
    return RES_OK;
}

int openFile(const char* pathname, int flags) {
    LOG_VERB("ServerApi: openFile(\"%s\",%d), pathname, flags");
    return RES_OK;
}

int readFile(const char* pathname, void** buf, size_t* size) {
    LOG_VERB("ServerApi: readFile(\"%s\", %p, %p)", pathname, buf, size);
    return RES_OK;
}

int readNFiles(int N, const char* dirname) {
    LOG_VERB("ServerApi: readNFiles(%d, \"%s\")", N, dirname);
    return RES_OK;
}

int writeFile(const char* pathname, const char* dirname) {
    LOG_VERB("ServerApi: writeFile(\"%s\", \"%s\")", pathname, dirname);
    return RES_OK;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    LOG_VERB("ServerApi: appendToFile(\"%s\", %p, %ul, \"%s\")", pathname, buf, size, dirname);
    return RES_OK;
}

int lockFile(const char* pathname) {
    LOG_VERB("ServerApi: lockFile(\"%s\")", pathname);
    return RES_OK;
}

int unlockFile(const char* pathname) {
    LOG_VERB("ServerApi: unlockFile(\"%s\")", pathname);
    return RES_OK;
}

int closeFile(const char* pathname) {
    LOG_VERB("ServerApi: closeFile(\"%s\")", pathname);
    return RES_OK;
}

int removeFile(const char* pathname) {
    LOG_VERB("ServerApi: removeFile(\"%s\")", pathname);
    return RES_OK;
}
