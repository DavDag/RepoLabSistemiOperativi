#include "file_system.h"

#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <pthread.h>

// #define LOG_TIMESTAMP
// #define LOG_DEBUG
#include <logger.h>
#include <common.h>

#define EMPTY_OWNER -1

#define DEBUG_LOG

// To ensure the server break at certain point when using while
#define DEPTH_LIMIT 1024*1024
#define MAX_EJECTED_FILES_AT_SAME_TIME 1024

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

// SUMMARY Data
static int gMaxSlotUsed  = 0;
static int gMaxBytesUsed = 0;
static int gCacheMisses  = 0;

// =============================================================================================

HashValue getKey(FSFile_t);
FSCacheEntry_t* getValueFromKey(HashValue);
void setValueForKey(HashValue, FSCacheEntry_t*);
FSCacheEntry_t* createEmptyCacheEntry(FSFile_t);
void moveToTop(FSCacheEntry_t*);
void updateCacheSize(FSFile_t*, FSFile_t*, FSFile_t**, int*);
void deepCopyFile(FSFile_t file, FSFile_t* outFile);

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
    LOG_INFO("[#FS] =========== SUMMARY ============");

    // 
    LOG_INFO("[#FS] Max slot used: %6d / %6d", gMaxSlotUsed, gConfigs.maxFileCapacitySlot); // SUMMARY: Max slot used
    LOG_INFO("[#FS] Max MB used  : %4.2fMB / %4.2fMB", (gMaxBytesUsed / 1024.0f / 1024.0f), ((float) gConfigs.maxFileCapacityMB)); // SUMMARY: Max MB used
    LOG_INFO("[#FS] Cache misses : %6d", gCacheMisses); // SUMMARY: Cache misses count
    // [#FS] 

    // Cache
    lock_mutex(&gFSMutex);
    int depth = 0;
    FSCacheEntry_t* item = gCache.head;
    LOG_INFO("[#FS] ============ FILES =============");
    while (item != NULL && depth < DEPTH_LIMIT) {
        FSCacheEntry_t* tmp = item->pre;
        //
        LOG_INFO("[#FS] %-32s", item->file.name); // SUMMARY: Files
        // 
        if (item->file.content) free((char*) item->file.content);
        if (item->file.name)    free((char*) item->file.name);
        free(item);
        item = tmp;
        depth++;
    }
    LOG_INFO("[#FS] ================================");
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
        // Update hashmap
        setValueForKey(key, newEntry);

        // Update cache
        moveToTop(newEntry);
        updateCacheSize(&file, NULL, outFiles, outFilesCount);
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
        
            updateCacheSize(NULL, &file, NULL, NULL);

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
        // Deep copy file out
        deepCopyFile(entry->file, outFile);

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

int __inn_sort_int_func(const void * a, const void * b) { return (*(int*)a - *(int*)b); }

int fs_obtain_n(ClientID client, int n, FSFile_t** outFiles, int* outFilesCount) {
    // Acquire lock
    lock_mutex(&gFSMutex);

    // Cap n
    if (n != 0) n = MIN(MIN(n, gCache.slotUsed), MAX_EJECTED_FILES_AT_SAME_TIME);

    // Allocate files array
    FSFile_t* files = NULL;
    int filesIndex  = 0;

    if (n == 0) {
        // Compute min between cache size and define for max files
        n     = MIN(gCache.slotUsed, MAX_EJECTED_FILES_AT_SAME_TIME);
        files = (FSFile_t*) mem_malloc(n * sizeof(FSFile_t));

        // Take all
        FSCacheEntry_t* item = gCache.head;
        while (item != NULL && filesIndex < n) {
            deepCopyFile(item->file, &files[filesIndex++]);
            item = item->pre;
        }
    } else {
        files = (FSFile_t*) mem_malloc(n * sizeof(FSFile_t));

        // Calculate n random index between 0 and cache size
        static int indexes[MAX_EJECTED_FILES_AT_SAME_TIME];
        int indexesIndex = 0;

        // RESERVOIR METHOD
        // th. source:
        //    https://en.wikipedia.org/wiki/Reservoir_sampling#:~:text=Reservoir%20sampling%20is%20a%20family,to%20fit%20into%20main%20memory.
        for (int i = 0; i < n; ++i) indexes[i] = i;
        for (int i = n; i < gCache.slotUsed && i < MAX_EJECTED_FILES_AT_SAME_TIME; ++i) {
            int m = rand() % (i + 1);
            if (m < n) indexes[m] = i;
        }

        // Sort (just for easy of use)
        qsort(indexes, n, sizeof(int), __inn_sort_int_func);

        // Take only indexes from the array
        int cacheIndex = 0;
        FSCacheEntry_t* item = gCache.tail;
        while (item != NULL && filesIndex < n) {
            if (cacheIndex == indexes[indexesIndex]) {
                deepCopyFile(item->file, &files[filesIndex++]);
                ++indexesIndex;
            }
            item = item->nex;
            ++cacheIndex;
        }
    }

    // Pass values
    *outFiles      = files;
    *outFilesCount = filesIndex;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns success
    return 0;
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
            // Release memory
            if (entry->file.content) free((char*) entry->file.content);
            if (entry->file.name)    free((char*) entry->file.name);

            // Update file
            FSFile_t oldFile = entry->file;
            entry->file      = file;

            // Update cache
            moveToTop(entry);
            updateCacheSize(&file, &oldFile, outFiles, outFilesCount);
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
    while (item != NULL && depth < DEPTH_LIMIT) {
        LOG_VERB("Ptr: %p. Owner: %+.3d Name: '%16s'. Content Size: %dB. N: %p. P: %p", item, item->owner, item->file.name, item->file.contentLen, item->nex, item->pre);
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

void updateCacheSize(FSFile_t* newFile, FSFile_t* oldFile, FSFile_t** ejectedFiles, int* ejectedFilesCount) {
    // Update current size (MB)
    if (newFile != NULL) gCache.bytesUsed += newFile->contentLen + newFile->nameLen;
    if (oldFile != NULL) gCache.bytesUsed -= oldFile->contentLen + oldFile->nameLen;

    // Update current size (Slot)
    if (newFile == NULL) gCache.slotUsed--;
    if (oldFile == NULL) gCache.slotUsed++;

    gMaxSlotUsed  = MAX(gMaxSlotUsed,  gCache.slotUsed);
    gMaxBytesUsed = MAX(gMaxBytesUsed, gCache.bytesUsed);

    // Tmp array
    static FSCacheEntry_t* ejectedFilesBuf[MAX_EJECTED_FILES_AT_SAME_TIME];
    int ejectedFilesBufIndex = 0;

    // Check for size limits (MB)
    if (gCache.bytesUsed > gCache.bytesMax) {
        // Eject file until reaching a correct size
        int depth = 0;
        FSCacheEntry_t* item = gCache.tail;
        while (gCache.bytesUsed > gCache.bytesMax && item != NULL && depth < DEPTH_LIMIT) {
            FSCacheEntry_t* tmp = item->nex;
            // Remove item from cache
            {
                // Update hashmap
                HashValue key = getKey(item->file);
                setValueForKey(key, NULL);

                // Update cache
                if (item->nex) item->nex->pre = NULL;
                if (gCache.head == item) gCache.head = NULL;
                
                // Add to ejected
                ejectedFilesBuf[ejectedFilesBufIndex++] = item;

                // Remove from cache sizes
                gCache.bytesUsed -= (item->file.nameLen + item->file.contentLen);
                --gCache.slotUsed;
            }
            // 
            item = tmp;
            depth++;
        }
        // Save new tail
        gCache.tail = item;
    }

    // Check for slot limits (MB)
    if (gCache.slotUsed > gCache.slotMax) {
        // Eject 1 file
        FSCacheEntry_t* item = gCache.tail;

        // Update hashmap
        HashValue key = getKey(item->file);
        setValueForKey(key, NULL);

        // Update cache
        if (item->nex) item->nex->pre = NULL;
        if (gCache.head == item) gCache.head = NULL;

        // Save new tail
        gCache.tail = item->nex;

        ejectedFilesBufIndex = 1;
        ejectedFilesBuf[0] = item;

        // Remove from cache sizes
        gCache.bytesUsed -= (item->file.nameLen + item->file.contentLen);
        --gCache.slotUsed;

        // Increment cache misses
        gCacheMisses += 1;
    }

    if (ejectedFilesBufIndex) {
        // Increment cache misses
        gCacheMisses += ejectedFilesBufIndex;

        // Save values and release entries memory
        FSFile_t* files = (FSFile_t*) mem_malloc(ejectedFilesBufIndex * sizeof(FSFile_t));
        for (int i = 0; i < ejectedFilesBufIndex; ++i) {
            files[i] = ejectedFilesBuf[i]->file;
            free(ejectedFilesBuf[i]);
        }

        // Pass values
        *ejectedFilesCount = ejectedFilesBufIndex;
        *ejectedFiles      = files;
    }
}

void deepCopyFile(FSFile_t file, FSFile_t* outFile) {
    // Copy name
    char* name = (char*) mem_malloc(file.nameLen * sizeof(char));
    memcpy(name, file.name, file.nameLen);
    outFile->name    = name;
    outFile->nameLen = file.nameLen;

    // Copy content
    char* content = NULL;
    if (file.contentLen) {
        content = (char*) mem_malloc(file.contentLen * sizeof(char));
        memcpy(content, file.content, file.contentLen);
    }
    outFile->content    = content;
    outFile->contentLen = file.contentLen;
}
