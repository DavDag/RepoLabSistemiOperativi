#pragma once

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#define FS_FILE_ALREADY_EXISTS 101
#define FS_FILE_NOT_EXISTS     102
#define FS_CLIENT_NOT_ALLOWED  103

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
 * \param configs: Configs parameters
 * 
 * \retval 0: on success
 */
int initializeFileSystem(FSConfig_t configs);

/**
 * Terminate the "file_system".
 * MUST BE called ONCE after any other call.
 * 
 * \retval 0: on success
 */
int terminateFileSystem();

/**
 * Insert a file inside the filesystem.
 * If aquireLock is set, during the creation it also aquire lock on it.
 * 
 * \param client       : client requesting the action
 * \param file         : destination file
 * \param acquireLock  : should it acquire lock too ?
 * \param outFiles     : ejected files to make room to the inserted file
 * \param outFilesCount: ejected files count
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_FILE_ALREADY_EXISTS ]
 */
int fs_insert(ClientID client, FSFile_t file, int aquireLock, FSFile_t** outFiles, int* outFilesCount);

/**
 * Remove a file from the filesystem.
 * 
 * \param client: client requesting the action
 * \param file  : file to remove
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS ]
 */
int fs_remove(ClientID client, FSFile_t file);

/**
 * Retrieve a file from the filesystem.
 * 
 * \param client : client requesting the action
 * \param file   : file to retrieve
 * \param outFile: retrieved file
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_FILE_NOT_EXISTS ]
 */
int fs_obtain(ClientID client, FSFile_t file, FSFile_t* outFile);

/**
 * Retrieve n random files from the filesystem.
 * 
 * \param client       : client requesting the action
 * \param n            : file count
 * \param outFiles     : ejected files
 * \param outFilesCount: ejected files count
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ ]
 */
int fs_obtain_n(ClientID client, int n, FSFile_t** outFiles, int* outFilesCount);

/**
 * Modify a file from the filesystem.
 * 
 * \param client       : client requesting the action
 * \param file         : file to update (with its new content inside)
 * \param outFiles     : ejected files to make room to the inserted file
 * \param outFilesCount: ejected files count
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS ]
 */
int fs_modify(ClientID client, FSFile_t file, FSFile_t** outFiles, int* outFilesCount);

/**
 * Check if file exist inside the filesystem.
 * 
 * \param client: client requesting the action
 * \param file  : file to check
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_FILE_NOT_EXISTS ]
 */
int fs_exists(ClientID client, FSFile_t file);

/**
 * Try taking ownership of the file.
 * 
 * \param client: client requesting the action
 * \param file  : file to 'lock'
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS ]
 */
int fs_trylock(ClientID client, FSFile_t file);

/**
 * Release ownership of the file.
 * 
 * \param client: client requesting the action
 * \param file  : file to 'unlock'
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS ]
 */
int fs_unlock(ClientID client, FSFile_t file);

#endif // FILE_SYSTEM_H
