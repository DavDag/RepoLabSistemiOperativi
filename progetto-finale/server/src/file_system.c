#include "file_system.h"

#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <pthread.h>

#include <common.h>

#define EMPTY_OWNER -1

#define DEBUG_LOG

typedef struct FSCacheEntry_t {
    ClientID owner;                   // owner of the lock (if locked, otherwise -1)
    FSFile_t file;                    // Data
    struct FSCacheEntry_t* pre, *nex; // Ptr to previous and next entry in list
} FSCacheEntry_t;

typedef struct {
    int slotUsed, slotMax;   // Total slots managment
    int bytesUsed, bytesMax; // Total bytes managment
    FSCacheEntry_t* head;    // Ptr to newest entry used
    FSCacheEntry_t* tail;    // Ptr to oldest entry used (LRU)
} FSCache_t;

// =============================================================================================

static FSConfig_t gConfigs;
static FSCacheEntry_t** gHashmap = NULL;
static pthread_mutex_t gFSMutex  = PTHREAD_MUTEX_INITIALIZER;
static FSCache_t gCache;

// =============================================================================================

HashValue getKey(FSFile_t);
FSCacheEntry_t* getValueFromKey(HashValue);
void setValueForKey(HashValue, FSCacheEntry_t*);
FSCacheEntry_t* createEmptyCacheEntry(FSFile_t);
void moveToTop(FSCacheEntry_t*);

void log_cache_entirely();

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
    gHashmap = (FSCacheEntry_t**) mem_calloc(gConfigs.tableSize, sizeof(FSCacheEntry_t*));

    // Returns success
    return 0;
}

int terminateFileSystem() {
    LOG_VERB("[#FS] Terminating file system ...");

    // Cache
    lock_mutex(&gFSMutex);
    int depth = 0;
    FSCacheEntry_t* item = gCache.head;
    while (item != NULL && depth < 1024) {
        FSCacheEntry_t* tmp = item->pre;
        if (item->file.content) free((char*) item->file.content);
        if (item->file.name)    free((char*) item->file.name);
        free(item);
        item = tmp;
        depth++;
    }
    unlock_mutex(&gFSMutex);

    // Hashmap
    free(gHashmap);

    // Returns success
    return 0;
}

int fs_insert(ClientID client, FSFile_t file, int aquireLock, FSFile_t** outFiles, int* outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key            = getKey(file);
    FSCacheEntry_t* newEntry = createEmptyCacheEntry(file);
    newEntry->owner          = (aquireLock) ? client : EMPTY_OWNER;

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* oldEntry = getValueFromKey(key);
    if (oldEntry == NULL) {
        // TODO: Add cache misses managment

        // Update hashmap
        setValueForKey(key, newEntry);

        // Update cache
        moveToTop(newEntry);
    } else {
        // Hash collision
        res = FS_FILE_ALREADY_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("insert");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Check if any error occurred
    if (res != 0) free(newEntry);

    // Returns the result
    return res;
}

int fs_remove(ClientID client, FSFile_t file) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if its owned by someone else
        if (entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // Update hashmap
            setValueForKey(key, NULL);

            // Link pre and nex toghether
            FSCacheEntry_t* tmp1 = entry->pre;
            FSCacheEntry_t* tmp2 = entry->nex;
            if (tmp1 != NULL) entry->pre->nex = tmp2;
            if (tmp2 != NULL) entry->nex->pre = tmp1;

            // Update cache
            if (gCache.head == entry) gCache.head = entry->pre;
            if (gCache.tail == entry) gCache.tail = entry->nex;

            // Release memory
            if (entry->file.content) free((char*) entry->file.content);
            if (entry->file.name)    free((char*) entry->file.name);
            free(entry);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("remove");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_obtain(ClientID client, FSFile_t file, FSFile_t* outFile) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Copy file and pass values
        char* name    = (char*) mem_malloc(entry->file.nameLen    * sizeof(char));
        char* content = (char*) mem_malloc(entry->file.contentLen * sizeof(char));
        memcpy(name   , entry->file.name   , entry->file.nameLen);
        memcpy(content, entry->file.content, entry->file.contentLen);
        outFile->name       = name;
        outFile->content    = content;
        outFile->nameLen    = entry->file.nameLen;
        outFile->contentLen = entry->file.contentLen;

        // Update cache
        moveToTop(entry);
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("obtain");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_modify(ClientID client, FSFile_t file, FSFile_t** outFiles, int* outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone else
        if (entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // TODO: Add cache misses managment

            // Release memory
            if (entry->file.content) free((char*) entry->file.content);
            if (entry->file.name)    free((char*) entry->file.name);

            // Update file
            entry->file = file;

            // Update cache
            moveToTop(entry);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("modify");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_trylock(ClientID client, FSFile_t file) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone
        if (entry->owner != EMPTY_OWNER && entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // 'Lock' file
            entry->owner = client;

            // Update cache
            moveToTop(entry);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("lock");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_unlock(ClientID client, FSFile_t file) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone
        if (entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // 'Lock' file
            entry->owner = EMPTY_OWNER;

            // Update cache
            moveToTop(entry);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("unlock");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_exists(ClientID client, FSFile_t file) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Update cache
        moveToTop(entry);
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("exists");
#endif

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

void log_cache_entirely(const char* const after) {
    LOG_VERB("========= FS after: %7s =========", after);
    LOG_VERB("Cache slot in use: %d, slot max: %d, B in use: %d, B max: %d", gCache.slotUsed, gCache.slotMax, gCache.bytesUsed, gCache.bytesMax);
    LOG_VERB("Cache head: %p, tail: %p", gCache.head, gCache.tail);
    LOG_VERB("================================");
    int depth = 0;
    FSCacheEntry_t* item = gCache.head;
    while (item != NULL && depth < 1024) {
        LOG_VERB("Ptr: %p. Owner: %+.3d Name: '%16s'. Content: '%16s'. N: %p. P: %p", item, item->owner, item->file.name, item->file.content, item->nex, item->pre);
        item = item->pre;
        depth++;
    }
    LOG_VERB("================================");
}

// =============================================================================================

HashValue getKey(FSFile_t file) {
    return hash_string(file.name, file.nameLen) % gConfigs.tableSize;
}

FSCacheEntry_t* getValueFromKey(HashValue key) {
    return gHashmap[key];
}

void setValueForKey(HashValue key, FSCacheEntry_t* value) {
    gHashmap[key] = value;
}

FSCacheEntry_t* createEmptyCacheEntry(FSFile_t file) {
    // Create empty entry
    FSCacheEntry_t* entry = (FSCacheEntry_t*) mem_malloc(sizeof(FSCacheEntry_t));
    entry->file  = file;
    entry->nex   = NULL;
    entry->pre   = NULL;
    entry->owner = EMPTY_OWNER;

#ifdef DEBUG_LOG
    LOG_VERB("NEW ENTRY %p FOR CACHE: %d %s", entry, entry->file.nameLen, entry->file.name);
#endif

    // Returns the entry
    return entry;
}

void moveToTop(FSCacheEntry_t* entry) {
    if (gCache.head != entry) {
        // Update tail
        if (gCache.tail == NULL)       gCache.tail = entry;
        else if (gCache.tail == entry) gCache.tail = entry->nex;
        
        // Link pre and nex toghether
        FSCacheEntry_t* tmp1 = entry->pre;
        FSCacheEntry_t* tmp2 = entry->nex;
        if (tmp1 != NULL) entry->pre->nex = tmp2;
        if (tmp2 != NULL) entry->nex->pre = tmp1;

        // Update entry
        entry->pre = gCache.head;
        entry->nex = NULL;

        // Update cache head (if needed)
        if (gCache.head != NULL) gCache.head->nex = entry;
        gCache.head = entry;
    }
}
