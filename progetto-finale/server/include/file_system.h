#pragma once

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#define FS_FILE_ALREADY_EXISTS    101
#define FS_FILE_NOT_EXISTS        102
#define FS_CLIENT_NOT_ALLOWED     103
#define FS_CLIENT_WAITING_ON_LOCK 104
#define FS_FILE_TOO_BIG           105

#include <pthread.h>

#include "circ_queue.h"
#include "session.h"

// To communicate file's data, in/out from the "file_system"
typedef struct {
    size_t nameLen;
    const char* name;
    size_t contentLen;
    const char* content;
} FSFile_t;

// Configs to pass at initialization
typedef struct {
    size_t tableSize;           // 2^8 (very small) -> 2^30 (very big)
    int maxFileCapacitySlot; // 1 ~> 1'000'000
    int maxFileCapacityMB;   // 1MB ~> 512MB
} FSConfig_t;

// State
typedef struct {
    size_t bytesUsedCount;
    int slotsUsedCount;
    int capacityMissCount;
} FSInfo_t;

typedef struct { int fd; int status; } FSLockNotification_t;

/**
 * Initialize the "file_system".
 * MUST BE called ONCE before any other call.
 * 
 * \param configs  : Configs parameters
 * \param lockQueue: Queue where to insert successfully locked client (only as sideeffect from an unlock)
 * \param lockCond : Where to signal for item inserted in the lock queue
 * \param lockMutex: 
 * 
 * \retval 0: on success
 */
int initializeFileSystem(FSConfig_t configs, CircQueue_t* lockQueue, pthread_cond_t* lockCond, pthread_mutex_t* lockMutex);

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
int fs_insert(int client, FSFile_t file, int aquireLock, FSFile_t** outFiles, int* outFilesCount);

/**
 * Remove a file from the filesystem.
 * 
 * \param client: client requesting the action
 * \param file  : file to remove
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS ]
 */
int fs_remove(int client, FSFile_t file);

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
int fs_obtain(int client, FSFile_t file, FSFile_t* outFile);

/**
 * Retrieve n random files from the filesystem.
 * (Do not update internal cache order to avoid wrong LRU managment).
 * 
 * \param client       : client requesting the action
 * \param n            : file count
 * \param outFiles     : ejected files
 * \param outFilesCount: ejected files count
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ ]
 */
int fs_obtain_n(int client, int n, FSFile_t** outFiles, int* outFilesCount);

/**
 * Modify a file from the filesystem.
 * 
 * \param client       : client requesting the action
 * \param file         : file to update (with its new content inside)
 * \param outFiles     : ejected files to make room to the inserted file
 * \param outFilesCount: ejected files count
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS, FS_FILE_TOO_BIG ]
 */
int fs_modify(int client, FSFile_t file, FSFile_t** outFiles, int* outFilesCount);

/**
 * Append data to a file in the filesystem.
 * 
 * \param client       : client requesting the action
 * \param file         : file to update (with the content to add inside)
 * \param outFiles     : ejected files to make room to the modified file
 * \param outFilesCount: ejected files count
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS, FS_FILE_TOO_BIG ]
 */
int fs_append(int client, FSFile_t file, FSFile_t** outFiles, int* outFilesCount);

/**
 * Check if file exist inside the filesystem.
 * 
 * \param client: client requesting the action
 * \param file  : file to check
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_FILE_NOT_EXISTS ]
 */
int fs_exists(int client, FSFile_t file);

/**
 * Try taking ownership of the file.
 * 
 * \param client: client requesting the action
 * \param file  : file to 'lock'
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS, FS_CLIENT_WAITING_ON_LOCK ]
 */
int fs_trylock(int client, FSFile_t file);

/**
 * Release ownership of the file.
 * 
 * \param client: client requesting the action
 * \param file  : file to 'unlock'
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ FS_CLIENT_NOT_ALLOWED, FS_FILE_NOT_EXISTS ]
 */
int fs_unlock(int client, FSFile_t file);

/**
 * Clean the user data left using its session.
 * 
 * \param client : destination client
 * \param session: session data
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values [ ]
 */
int fs_clean(int client, ClientSession_t* session);

/**
 * Get state of the system.
 * 
 * \retval struct: containing the state of the 'file-system'
 */
FSInfo_t fs_get_infos();

#endif // FILE_SYSTEM_H
