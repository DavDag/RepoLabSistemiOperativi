#pragma once

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

// Syntactic sugar
typedef int ClientID;

// To communicate file's data, in/out from the "file_system"
typedef struct {
    int nameLen;
    const char* name;
    int contentLen;
    const char* content;
} FSFile_t;

// Configs to pass at initialization
typedef struct {
    int tableSize;           // 2^8 (very small) -> 2^30 (very big)
    int maxFileCapacitySlot; // 1 ~> 1'000'000
    int maxFileCapacityMB;   // 1MB ~> 512MB
} FSConfig_t;

/**
 * Initialize the "file_system".
 * MUST BE called ONCE before any other call.
 * 
 * \retval > 0: on error (error code is returned) 
 * \retval   0: on success
 */
int initializeFileSystem(FSConfig_t configs);

/**
 * Terminate the "file_system".
 * MUST BE called ONCE after any other call.
 * 
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int terminateFileSystem();

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int openFile(ClientID client, FSFile_t file, int mode);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int closeFile(ClientID client, FSFile_t file);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int readFile(ClientID client, FSFile_t* file);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int readNFiles(ClientID client, FSFile_t* files, int fileCount);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int writeFile(ClientID client, FSFile_t file);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int appendToFile(ClientID client, FSFile_t file);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int lockFile(ClientID client, FSFile_t file);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int unlockFile(ClientID client, FSFile_t file);

/**
 * \retval > 0: on error (error code is returned)
 * \retval   0: on success
 */
int removeFile(ClientID client, FSFile_t file);

#endif // FILE_SYSTEM_H
