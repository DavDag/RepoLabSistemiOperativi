#include "file_system.h"

#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <pthread.h>

#include <common.h>

#define FS_SUCCESS            0
#define FS_FILE_ALREADY_EXIST 1
#define FS_FILE_NOT_EXIST     2

#define EMPTY_OWNER -1

typedef struct FSCacheEntry_t {
    pthread_rwlock_t rwmutex;         // RW_lock mutex // TODO: use mutex
    ClientID owner;                   // owner of the lock (if locked, otherwise -1)
    FSFile_t* file;                   // Ptr to data
    struct FSCacheEntry_t* pre, *nex; // Ptr to previous and next entry in list
} FSCacheEntry_t;

typedef struct {
    int slotUsed, slotMax;   // Total slots managment
    int bytesUsed, bytesMax; // Total bytes managment
    FSCacheEntry_t* head;    // Ptr to newest entry used
    FSCacheEntry_t* tail;    // Ptr to oldest entry used (LRU)
} FSCache_t;

typedef FSCacheEntry_t* FSHashMapEntry_t;

// =============================================================================================

static FSConfig_t gConfigs;
static FSHashMapEntry_t* gHashmap = NULL;
static pthread_mutex_t gFSMutex   = PTHREAD_MUTEX_INITIALIZER;
static FSCache_t gCache;

// =============================================================================================

int fs_insert(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount);
int fs_remove(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount);
int fs_obtain(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount);
int fs_modify(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount);

// =============================================================================================

int initializeFileSystem(FSConfig_t configs) {
    LOG_VERB("[#FS] Initializing file system ...");
    gConfigs = configs;

    // Cache
    gCache.bytesMax  = gConfigs.maxFileCapacityMB * 1024 * 1024;
    gCache.slotMax   = gConfigs.maxFileCapacitySlot;
    gCache.bytesUsed = 0;
    gCache.slotUsed  = 0;
    gCache.head      = NULL;
    gCache.tail      = NULL;

    // Hashmap
    gHashmap = (FSHashMapEntry_t*) mem_calloc(gConfigs.tableSize, sizeof(FSHashMapEntry_t));

    // Returns success
    return FS_SUCCESS;
}

int terminateFileSystem() {
    // Cache
    lock_mutex(&gFSMutex);
    FSCacheEntry_t* item = gCache.head;
    while (item != NULL) {
        FSCacheEntry_t* tmp = item->nex;
        free(item);
        item = tmp;
    }
    unlock_mutex(&gFSMutex);

    // Hashmap
    free(gHashmap);

    // Returns success
    return FS_SUCCESS;
}

int openFile(ClientID client, FSFile_t file, int mode) {

    return FS_SUCCESS;
}

int closeFile(ClientID client, FSFile_t file) {

    return FS_SUCCESS;
}

int readFile(ClientID client, FSFile_t* file) {

    return FS_SUCCESS;
}

int readNFiles(ClientID client, FSFile_t* files, int fileCount) {

    return FS_SUCCESS;
}

int writeFile(ClientID client, FSFile_t file) {

    return FS_SUCCESS;
}

int appendToFile(ClientID client, FSFile_t file) {

    return FS_SUCCESS;
}

int lockFile(ClientID client, FSFile_t file) {

    return FS_SUCCESS;
}

int unlockFile(ClientID client, FSFile_t file) {

    return FS_SUCCESS;
}

int removeFile(ClientID client, FSFile_t file) {

    return FS_SUCCESS;
}

// =============================================================================================

HashValue getKey(FSFile_t* file) {
    return hash_string(file->name, file->nameLen) % gConfigs.tableSize;
}

FSHashMapEntry_t getValueFromKey(HashValue key) {
    return gHashmap[key];
}

void setValueForKey(HashValue key, FSHashMapEntry_t value) {
    gHashmap[key] = value;
}

FSCacheEntry_t* createEmptyCacheEntry(FSFile_t* file) {
    // Create empty entry
    FSCacheEntry_t* entry = (FSCacheEntry_t*) mem_malloc(sizeof(FSCacheEntry_t*));
    entry->file  = file;
    entry->nex   = NULL;
    entry->pre   = NULL;
    entry->owner = EMPTY_OWNER;
    
    // Initialize rw lock
    int res = 0;
    if ((res = pthread_rwlock_init(&entry->rwmutex, NULL)) != 0) {
        errno = res;

        // On error, function fails (some bigger problem occurred)
        LOG_ERRNO("Unable to initialize rw mutex");
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }

    // Returns the entry
    return entry;
}

int fs_insert(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key             = getKey(file);
    FSHashMapEntry_t newEntry = createEmptyCacheEntry(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSHashMapEntry_t oldEntry = getValueFromKey(key);
    if (oldEntry == NULL) {
        // Update hashmap
        setValueForKey(key, newEntry);

        // Update new entry setting next ptr to current cache head
        newEntry->nex = gCache.head;

        // Update cache head (if needed)
        if (gCache.head != NULL) gCache.head->pre = newEntry;
        gCache.head = newEntry;
    } else {
        // Hash collision
        res = FS_FILE_ALREADY_EXIST;
    }

    // Release lock
    unlock_mutex(&gFSMutex);

    // Check if any error occurred
    if (res != 0) free(newEntry);

    // Returns the result
    return res;
}

int fs_remove(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Link pre and nex toghether
        if (entry->pre != NULL) entry->pre->nex = entry->nex;
        if (entry->nex != NULL) entry->nex->pre = entry->pre;

        // Update cache
        if (gCache.head == entry) gCache.head = entry->nex;

        // Pass file
        *file = *entry->file;
    } else {
        res = FS_FILE_NOT_EXIST;
    }

    // Release lock
    lock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_obtain(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Link pre and nex toghether
        if (entry->pre != NULL) entry->pre->nex = entry->nex;
        if (entry->nex != NULL) entry->nex->pre = entry->pre;
        
        // Update entry
        entry->nex = gCache.head;
        entry->pre = NULL;

        // Update cache
        if (gCache.head != NULL) gCache.head->pre = entry;
        gCache.head = entry;

        // Pass file
        *file = *entry->file;
    } else {
        res = FS_FILE_NOT_EXIST;
    }

    // Release lock
    lock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_modify(ClientID client, FSFile_t* file, FSFile_t* outFiles, int outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Link pre and nex toghether
        if (entry->pre != NULL) entry->pre->nex = entry->nex;
        if (entry->nex != NULL) entry->nex->pre = entry->pre;
        
        // Update entry
        entry->nex = gCache.head;
        entry->pre = NULL;

        // Update cache
        if (gCache.head != NULL) gCache.head->pre = entry;
        gCache.head = entry;

        // Update file
        *entry->file = *file;
    } else {
        res = FS_FILE_NOT_EXIST;
    }

    // Release lock
    lock_mutex(&gFSMutex);

    // Returns the result
    return res;
}
