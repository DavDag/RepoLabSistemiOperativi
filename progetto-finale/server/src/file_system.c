#include "file_system.h"

#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <pthread.h>

#include <logger.h>
#include <common.h>

#define EMPTY_OWNER -1

#define DEBUG_LOG

// To ensure the server break at certain point when using while
#define DEPTH_LIMIT 1024*1024
#define MAX_EJECTED_FILES_AT_SAME_TIME 1024*1024
#define MAX_CLIENT_WAITING_ON_LOCK 32

// Just for summary uses
#define TIMES(n, x) for (int i = 0; i < n; ++i) { x; }

typedef struct FSCacheEntry_t {
    int owner;                   // owner of the lock (if locked, otherwise -1)
    FSFile_t file;                    // Data
    CircQueue_t* waitingLockQueue;    // Queue of clients waiting on lock
    struct FSCacheEntry_t* pre, *nex; // Ptr to previous and next entry in list
} FSCacheEntry_t;

typedef struct {
    int slotUsed, slotMax;         // Total slots managment
    long long bytesUsed, bytesMax; // Total bytes managment
    FSCacheEntry_t* head;          // Ptr to newest entry used
    FSCacheEntry_t* tail;          // Ptr to oldest entry used (LRU)
} FSCache_t;

// =============================================================================================

static FSConfig_t gConfigs;
static FSCacheEntry_t** gHashmap   = NULL;
static pthread_mutex_t gFSMutex    = PTHREAD_MUTEX_INITIALIZER;
static FSCache_t gCache;
static CircQueue_t* gLockQueue     = NULL;
static pthread_cond_t* gLockCond   = NULL;
static pthread_mutex_t* gLockMutex = NULL;

// SUMMARY Data
static int gMaxSlotUsed  = 0;
static long long gMaxBytesUsed = 0;
static int gCapacityMisses  = 0;
static int gQueryCount   = 0;

#define INCREASE_QUERY_COUNT (++gQueryCount)

// =============================================================================================

HashValue getKey(FSFile_t);
FSCacheEntry_t* getValueFromKey(HashValue);
void setValueForKey(HashValue, FSCacheEntry_t*);
FSCacheEntry_t* createEmptyCacheEntry(FSFile_t);
void freeCacheEntry(FSCacheEntry_t*);
void moveToTop(FSCacheEntry_t*);
void updateCacheSize(FSFile_t*, FSFile_t*, FSFile_t**, int*);
void deepCopyFile(FSFile_t, FSFile_t*);

void log_cache_entirely();
void summary();

// =============================================================================================

FSInfo_t fs_get_infos() {
    // Atomic read
    lock_mutex(&gFSMutex);
    FSInfo_t result = {
        .bytesUsedCount = gCache.bytesUsed,
        .slotsUsedCount = gCache.slotUsed,
        .capacityMissCount = gCapacityMisses
    };
    unlock_mutex(&gFSMutex);
    return result;
}

int initializeFileSystem(FSConfig_t configs, CircQueue_t* lockQueue, pthread_cond_t* lockCond, pthread_mutex_t* lockMutex) {
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

    // Lock queue & cond
    gLockCond  = lockCond;
    gLockQueue = lockQueue;
    gLockMutex = lockMutex;

    // Returns success
    return 0;
}

int terminateFileSystem() {
    LOG_VERB("[#FS] Terminating file system ...");

    summary();

    // Cache
    lock_mutex(&gFSMutex);
    int depth = 0;
    FSCacheEntry_t* item = gCache.head;
    while (item != NULL && depth < DEPTH_LIMIT) {
        FSCacheEntry_t* tmp = item->pre;
        // Release memory
        freeCacheEntry(item);
        item = tmp;
        depth++;
    }
    unlock_mutex(&gFSMutex);

    // Hashmap
    free(gHashmap);

    // Returns success
    return 0;
}

int fs_insert(int client, FSFile_t file, int aquireLock, FSFile_t** outFiles, int* outFilesCount) {
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
        LOG_WARN("[#FS] File exists or Hash collision");
        res = FS_FILE_ALREADY_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("insert");
#endif

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Check if any error occurred
    if (res != 0) freeCacheEntry(newEntry);
    
    // Returns the result
    return res;
}

int fs_remove(int client, FSFile_t file) {
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

            // Notify all clients waiting for lock
            CircQueueItemPtr_t item;
            while (tryPop(entry->waitingLockQueue, &item) == 1) {
                // Create notification to send
                FSLockNotification_t* notification = (FSLockNotification_t*) mem_malloc(sizeof(FSLockNotification_t));
                notification->fd     = (intptr_t) item;
                notification->status = FS_FILE_NOT_EXISTS; // File has been removed
                // Push into lock queue
                while (tryPush(gLockQueue, notification) != 1) {
                    // Try again fastest possible.
                    // It blocks the entire server and needs to be done fast
                }
                LOG_VERB("[#FS] Removed %d from lock req queue", notification->fd);
                // Signal queue
                lock_mutex(gLockMutex);
                notify_one(gLockCond);
                unlock_mutex(gLockMutex);
            }

            // Update cache
            updateCacheSize(NULL, &entry->file, NULL, NULL);

            // Release memory
            freeCacheEntry(entry);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("remove");
#endif

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_obtain(int client, FSFile_t file, FSFile_t* outFile) {
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

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int __inn_sort_int_func(const void * a, const void * b) { return (*(int*)a - *(int*)b); }

int fs_obtain_n(int client, int n, FSFile_t** outFiles, int* outFilesCount) {
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

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns success
    return 0;
}

int fs_modify(int client, FSFile_t file, FSFile_t** outFiles, int* outFilesCount) {
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
        } else if (file.contentLen + file.nameLen > gCache.bytesMax) {
            res = FS_FILE_TOO_BIG;
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

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_append(int client, FSFile_t file, FSFile_t** outFiles, int* outFilesCount) {
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
        if (entry->owner != client && entry->owner != EMPTY_OWNER) {
            res = FS_CLIENT_NOT_ALLOWED;
        } else if (file.contentLen + file.nameLen + entry->file.contentLen > gCache.bytesMax) {
            res = FS_FILE_TOO_BIG;
        } else {
            // Resize file content buffer to be large enough
            int newContentLen      = file.contentLen + entry->file.contentLen;
            entry->file.content    = mem_realloc((void*) entry->file.content, newContentLen * sizeof(char));

            // Append content
            memcpy((void*) &entry->file.content[entry->file.contentLen], file.content, file.contentLen);
            entry->file.contentLen = newContentLen;

            // Make dummy file to update size correctly
            FSFile_t tmpFile = entry->file;
            tmpFile.nameLen  = 0;

            // Update cache
            moveToTop(entry);
            updateCacheSize(&file, &tmpFile, outFiles, outFilesCount);
        }
    } else {
        res = FS_FILE_NOT_EXISTS;
    }

#ifdef DEBUG_LOG
    log_cache_entirely("append");
#endif

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_trylock(int client, FSFile_t file) {
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
            // Try push into relative queue
            if (tryPush(entry->waitingLockQueue, (void*) (intptr_t) client) == 1) {
                // Successfully pushed into queue
                res = FS_CLIENT_WAITING_ON_LOCK;
                LOG_VERB("[#FS] Added %d to lock req queue", client);
            }
            else {
                // Queue may be full
                res = FS_CLIENT_NOT_ALLOWED;
            }
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

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_exists(int client, FSFile_t file) {
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

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

// Inner implementation of the lock to use inside clean too
int _inner_unlock(int client, HashValue key) {
    // Check if file exist
    FSCacheEntry_t* entry = getValueFromKey(key);
    if (entry != NULL) {
        // Check if file is owned by someone
        if (entry->owner != client) {
            return FS_CLIENT_NOT_ALLOWED;
        } else {
            // 'Unlock' file
            entry->owner = EMPTY_OWNER;

            // Get first client waiting on lock (if any)
            CircQueueItemPtr_t item;
            if (tryPop(entry->waitingLockQueue, &item) == 1) {
                // Create notification to send
                FSLockNotification_t* notification = (FSLockNotification_t*) mem_malloc(sizeof(FSLockNotification_t));
                notification->fd     = (intptr_t) item;
                notification->status = 0; // OK
                // 'lock' file
                entry->owner = notification->fd;
                // Push into lock queue
                while (tryPush(gLockQueue, notification) != 1) {
                    // Try again fastest possible.
                    // It blocks the entire server and needs to be done fast
                }
                LOG_VERB("[#FS] Removed %d from lock req queue", notification->fd);
                // Signal queue
                lock_mutex(gLockMutex);
                notify_one(gLockCond);
                unlock_mutex(gLockMutex);
            } else {
                LOG_VERB("[#FS] No client found in lock req queue");
            }

            // Update cache
            moveToTop(entry);
        }
    } else {
        return FS_FILE_NOT_EXISTS;
    }
    return 0;
}

int fs_unlock(int client, FSFile_t file) {
    // vars
    int res = 0;

    // Get key
    HashValue key = getKey(file);

    // Acquire lock
    lock_mutex(&gFSMutex);

    res = _inner_unlock(client, key);

#ifdef DEBUG_LOG
    log_cache_entirely("unlock");
#endif

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);

    // Returns the result
    return res;
}

int fs_clean(int client, ClientSession_t* session) {
    // Acquire lock
    lock_mutex(&gFSMutex);

    // Cycle through the files left opened by the client
    for (int i = 0; i < session->numFileOpened; ++i) {
        HashValue key = session->filenames[i] % gConfigs.tableSize;
        _inner_unlock(client, key);
    }

#ifdef DEBUG_LOG
    log_cache_entirely("clean");
#endif

    INCREASE_QUERY_COUNT;

    // Release lock
    unlock_mutex(&gFSMutex);
    return 0;
}

void log_cache_entirely(const char* const after) {
    LOG_VERB("========= FS after: %7s =========", after);
    LOG_VERB("Cache slot in use: %d, slot max: %d, B in use: %lld, B max: %lld", gCache.slotUsed, gCache.slotMax, gCache.bytesUsed, gCache.bytesMax);
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
    FSCacheEntry_t* entry   = (FSCacheEntry_t*) mem_malloc(sizeof(FSCacheEntry_t));
    entry->file             = file;
    entry->nex              = NULL;
    entry->pre              = NULL;
    entry->owner            = EMPTY_OWNER;
    entry->waitingLockQueue = createQueue(MAX_CLIENT_WAITING_ON_LOCK);

#ifdef DEBUG_LOG
    LOG_VERB("NEW ENTRY %p FOR CACHE: %d %s", entry, entry->file.nameLen, entry->file.name);
#endif

    // Returns the entry
    return entry;
}

void freeCacheEntry(FSCacheEntry_t* entry) {
    if (entry->file.content) free((char*) entry->file.content);
    free((char*) entry->file.name);
    free(entry->waitingLockQueue->data);
    free(entry->waitingLockQueue);
    free(entry);
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
    if (newFile != NULL) gCache.bytesUsed += +(newFile->contentLen + newFile->nameLen);
    if (oldFile != NULL) gCache.bytesUsed += -(oldFile->contentLen + oldFile->nameLen);

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

    // Check for size limits (Slot)
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

        // Increment capacity misses
        gCapacityMisses += 1;
    }

    if (ejectedFilesBufIndex) {
        // Increment capacity misses
        gCapacityMisses += ejectedFilesBufIndex;

        // Save values and release entries memory
        FSFile_t* files = (FSFile_t*) mem_malloc(ejectedFilesBufIndex * sizeof(FSFile_t));
        for (int i = 0; i < ejectedFilesBufIndex; ++i) {
            FSCacheEntry_t* entry = ejectedFilesBuf[i];
            files[i] = entry->file;

            // Notify all clients waiting for lock
            CircQueueItemPtr_t item;
            while (tryPop(entry->waitingLockQueue, &item) == 1) {
                // Create notification to send
                FSLockNotification_t* notification = (FSLockNotification_t*) mem_malloc(sizeof(FSLockNotification_t));
                notification->fd     = (intptr_t) item;
                notification->status = FS_FILE_NOT_EXISTS; // File has been removed
                // Push into lock queue
                while (tryPush(gLockQueue, notification) != 1) {
                    // Try again fastest possible.
                    // It blocks the entire server and needs to be done fast
                }
                LOG_VERB("[#FS] Removed %d from lock req queue", notification->fd);
                // Signal queue
                lock_mutex(gLockMutex);
                notify_one(gLockCond);
                unlock_mutex(gLockMutex);
            }

            // Release memory
            free(entry->waitingLockQueue->data);
            free(entry->waitingLockQueue);
            free(entry);
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

void summary() {
    // Calc stats
    int slotU = gCache.slotUsed , slotM = gCache.slotMax , slotP = gMaxSlotUsed;
    long long bytesU = gCache.bytesUsed, bytesM = gCache.bytesMax, bytesP = gMaxBytesUsed;

    // To write more readable code
    static char CC = '+';
    static char CV = '|';
    static char CH = '-';

    int tTSize = 100;
    int fCSize = 32;
    int sCSize = (100-fCSize-5)/3;

    // Header
    LOG_EMPTY("  %c", CC); TIMES(fCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); 
    LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c\n", CC);

    // Separator
    LOG_EMPTY("  %c%*s%c%*s%c%*s%c%*s%c\n", CV, fCSize, "", CV, sCSize, "    used    ", CV, sCSize, "    max    ", CV, sCSize, "    peak    ", CV);

    // Separator
    LOG_EMPTY("  %c", CC); TIMES(fCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); 
    LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c\n", CC);

    // Cache size (Slot)
    LOG_EMPTY("  %c%-*s%c %*d %c %*d %c %*d %c\n", CV, fCSize, "size (slot)", CV, sCSize-2, slotU, CV, sCSize-2, slotM, CV, sCSize-2, slotP, CV);

    // Cache size (MB)
    LOG_EMPTY("  %c%-*s%c %*.2f %s %c %*.2f %s %c %*.2f %s %c\n", CV, fCSize, "size (MB)", CV, sCSize-5, BYTES(bytesU), CV, sCSize-5, BYTES(bytesM), CV, sCSize-5, BYTES(bytesP), CV);

    // Separator
    LOG_EMPTY("  %c", CC); TIMES(fCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); 
    LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c\n", CC);

    // Files left
    if (gCache.slotUsed > 0) {
        int depth = 0;
        FSCacheEntry_t* item = gCache.head;
        while (item != NULL && depth < DEPTH_LIMIT) {
            // File data
            int bytes = item->file.contentLen;
            // Log
            LOG_EMPTY("  %c%-*s%c %*.2f %s %c\n", CV, tTSize-sCSize-3, item->file.name, CV, sCSize-5, BYTES(bytes), CV);
            // Next
            item = item->pre;
            depth++;
        }
    } else {
        LOG_EMPTY("  %c                                     No file left in the server                                   %c\n", CV, CV);
    }
    LOG_EMPTY("  %c", CC); TIMES(tTSize-sCSize-3, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c", CC); TIMES(sCSize, LOG_EMPTY("%c", CH)); LOG_EMPTY("%c\n", CC);
}
