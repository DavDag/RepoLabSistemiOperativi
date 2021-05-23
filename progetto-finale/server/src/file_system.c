#include "file_system.h"

#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <pthread.h>

#include <common.h>

#define EMPTY_OWNER -1

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

typedef FSCacheEntry_t* FSHashMapEntry_t;

// =============================================================================================

static FSConfig_t gConfigs;
static FSHashMapEntry_t* gHashmap = NULL;
static pthread_mutex_t gFSMutex   = PTHREAD_MUTEX_INITIALIZER;
static FSCache_t gCache;

// =============================================================================================

HashValue getKey(FSFile_t);
FSHashMapEntry_t getValueFromKey(HashValue);
void setValueForKey(HashValue, FSHashMapEntry_t);
FSCacheEntry_t* createEmptyCacheEntry(FSFile_t);

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
    return 0;
}

int terminateFileSystem() {
    LOG_VERB("[#FS] Terminating file system ...");

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
    return 0;
}

int fs_insert(ClientID client, FSFile_t file, int aquireLock, FSFile_t** outFiles, int* outFilesCount) {
    // vars
    int res = 0;

    // Get key
    HashValue key             = getKey(file);
    FSHashMapEntry_t newEntry = createEmptyCacheEntry(file);
    newEntry->owner           = (aquireLock) ? client : EMPTY_OWNER;

    // Acquire lock
    lock_mutex(&gFSMutex);

    // Check if file exist
    FSHashMapEntry_t oldEntry = getValueFromKey(key);
    if (oldEntry == NULL) {
        // TODO: Add cache misses managment

        // Update hashmap
        setValueForKey(key, newEntry);

        // Update new entry setting next ptr to current cache head
        newEntry->nex = gCache.head;

        // Update cache head (if needed)
        if (gCache.head != NULL) gCache.head->pre = newEntry;
        gCache.head = newEntry;
    } else {
        // Hash collision
        res = FS_FILE_ALREADY_EXISTS;
    }

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
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if its owned by someone else
        if (entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // Link pre and nex toghether
            if (entry->pre != NULL) entry->pre->nex = entry->nex;
            if (entry->nex != NULL) entry->nex->pre = entry->pre;

            // Update cache
            if (gCache.head == entry) gCache.head = entry->nex;

            // Release memory
            free(entry->file.content);
            free(entry->file.name);
            free(entry);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

    // Release lock
    lock_mutex(&gFSMutex);

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

        // Pass copy of the file
        outFile->nameLen    = entry->file.nameLen;
        outFile->contentLen = entry->file.contentLen;
        outFile->name       = (char*) mem_malloc(entry->file.nameLen    * sizeof(char));
        outFile->content    = (char*) mem_malloc(entry->file.contentLen * sizeof(char));
        memcpy(outFile->name   , entry->file.name   , entry->file.nameLen);
        memcpy(outFile->content, entry->file.content, entry->file.contentLen);
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

    // Release lock
    lock_mutex(&gFSMutex);

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
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone else
        if (entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // TODO: Add cache misses managment

            // Link pre and nex toghether
            if (entry->pre != NULL) entry->pre->nex = entry->nex;
            if (entry->nex != NULL) entry->nex->pre = entry->pre;
            
            // Update entry
            entry->nex = gCache.head;
            entry->pre = NULL;

            // Update cache
            if (gCache.head != NULL) gCache.head->pre = entry;
            gCache.head = entry;

            // Release memory
            free(entry->file.content);
            free(entry->file.name);

            // Update file
            entry->file = file;
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

    // Release lock
    lock_mutex(&gFSMutex);

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
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone
        if (entry->owner != EMPTY_OWNER && entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // 'Lock' file
            entry->owner = client;

            // Link pre and nex toghether
            if (entry->pre != NULL) entry->pre->nex = entry->nex;
            if (entry->nex != NULL) entry->nex->pre = entry->pre;

            // Update cache
            if (gCache.head == entry) gCache.head = entry->nex;
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

    // Release lock
    lock_mutex(&gFSMutex);

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
    FSHashMapEntry_t entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone
        if (entry->owner != client) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else {
            // 'Lock' file
            entry->owner = EMPTY_OWNER;

            // Link pre and nex toghether
            if (entry->pre != NULL) entry->pre->nex = entry->nex;
            if (entry->nex != NULL) entry->nex->pre = entry->pre;

            // Update cache
            if (gCache.head == entry) gCache.head = entry->nex;
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

    // Release lock
    lock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

// =============================================================================================

HashValue getKey(FSFile_t file) {
    return hash_string(file.name, file.nameLen) % gConfigs.tableSize;
}

FSHashMapEntry_t getValueFromKey(HashValue key) {
    return gHashmap[key];
}

void setValueForKey(HashValue key, FSHashMapEntry_t value) {
    gHashmap[key] = value;
}

FSCacheEntry_t* createEmptyCacheEntry(FSFile_t file) {
    // Create empty entry
    FSCacheEntry_t* entry = (FSCacheEntry_t*) mem_malloc(sizeof(FSCacheEntry_t*));
    entry->file  = file;
    entry->nex   = NULL;
    entry->pre   = NULL;
    entry->owner = EMPTY_OWNER;

    // Returns the entry
    return entry;
}
