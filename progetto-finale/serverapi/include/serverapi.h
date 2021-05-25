#pragma once

#ifndef SERVER_API_H
#define SERVER_API_H

#define LOG_DEBUG
#include <common.h>
#include <stdlib.h>
#include <time.h>

#define SERVER_API_SUCCESS  0 // On Success
#define SERVER_API_FAILURE -1 // On Failure

typedef struct { int bytesW, bytesR; } ApiBytesInfo_t;

/**
 * Open connection with an AF_UNIX socket.
 * The connection process repeats every 'msec' for at least 'abstime' (until connection
 * is completed or timeout).
 * 
 * \param sockname: The socket path to use for connection
 * \param msec    : Interval between tries
 * \param abstime : Timeout
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
 * Close connection.
 * 
 * \param sockname:
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int closeConnection(const char* sockname);

/**
 * Send a request for opening a file.
 * When opening a file in 'LOCK' mode, the call is blocking.
 * Cannot open same file multiple times.
 * 
 * \param pathname: file to open
 * \param flags   : opening mode: FLAG_EMPY, FLAG_CREATE, FLAG_LOCK. Can be composed using |
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int openFile(const char* pathname, int flags);

/**
 * Send a request for  reading a file.
 * 
 * \param pathname: file to read
 * \param buf     : destination buffer
 * \param size    : file returned size
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int readFile(const char* pathname, void** buf, size_t* size);

/**
 * Send a request for reading 'N' random files.
 * Passing a dirname will save them locally.
 * 
 * \param N      : numer of files to open. (0) to open all
 * \param dirname: where to save returned files. NULL to reject them
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int readNFiles(int N, const char* dirname);

/**
 * Send a request for writing a file.
 * The file must be opened in FLAG_CREATE | FLAG_LOCK mode and must be the
 * last operation done to the file.
 * Otherwise check appendToFile.
 * 
 * When server is full, it returns files to clear memory.
 * Passing a dirname will store them.
 * 
 * \param pathname: file to write
 * \param dirname : where to save returned files. NULL to reject them
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int writeFile(const char* pathname, const char* dirname);

/**
 * Send a request for appending data to a file.
 * The file must be opened. FLAG_LOCK is required and the operation guaranteed to be atomic
 * (server-side).
 * 
 * When server is full, it returns files to clear memory.
 * Passing a dirname will store them.
 * 
 * \param pathname: file to modify
 * \param buf     : bytes to send
 * \param size    : buffer size
 * \param dirname : where to save returned files. NULL to reject them
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/**
 * Send a request for locking a file.
 * The call is blocking and the client can continues only after aquiring the lock.
 * If the file was already locked, it succed.
 * 
 * \param pathname: file to lock
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int lockFile(const char* pathname);

/**
 * Send a request for unlocking a file.
 * 
 * \param pathname: file to unlock
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int unlockFile(const char* pathname);

/**
 * Send a request for closing a file.
 * Any operation done after closing the file will not work.
 * 
 * \param pathname: file to close
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int closeFile(const char* pathname);

/**
 * Send a request for removing a file.
 * The file must be locked by this client or the request fails.
 * 
 * \param pathname: file to remove
 * 
 * \retval  0: on success
 * \retval -1: on error (errno set)
 */
int removeFile(const char* pathname);

/**
 * Request info about bytes read and written. 
 * 
 * \retval struct: containing bytes data
 */
ApiBytesInfo_t getBytesData();

#endif // SERVER_API_H
