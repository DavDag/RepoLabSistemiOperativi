#include "utils.h"
#include "logger.h"

int parse_positive_integer(const char* str) {
    char* end = NULL;
    errno = 0;
    int val = strtol(str, &end, 10);

    // Check if conversion can be done
    if (str == end || (end != NULL && *end != '\0')) return -1;

    // Check range error
    if (errno == ERANGE) return -2;

    // Otherwise return parsed value
    return val;
}

int read_entire_file(const char* file, char** buffer, int* len) {
    // vars
    int res        = 0;
    int contentLen = 0;
    char* content  = NULL;

    // Open file
    FILE *f = fopen(file, "rb");
    if (f == NULL) return -1;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    if ((contentLen = ftell(f)) < 0) return -1;
    if ((res = fseek(f, 0, SEEK_SET)) != 0) return -1;
    content = (char*) mem_malloc(contentLen);
    fread(content, sizeof(char), contentLen, f);
    fclose(f);

    // Pass values
    *buffer = content;
    *len   = contentLen;

    // Returns success
    return 0;
}

void* mem_malloc(size_t size) {
    void* ptr = malloc(size);

    // Check for error
    if (ptr == NULL) {
        // On error, function fails (some bigger problem occurred)
        LOG_CRIT("Malloc failed trying to allocate %ul bytes", size);
        exit(EXIT_FAILURE);
    }

    // Otherwise returns ptr
    return ptr;
}

void* mem_calloc(size_t num, size_t size) {
    void* ptr = calloc(num, size);

    // Check for error
    if (ptr == NULL) {
        // On error, function fails (some bigger problem occurred)
        LOG_CRIT("Calloc failed trying to allocate %ul bytes", num * size);
        exit(EXIT_FAILURE);
    }

    // Otherwise returns ptr
    return ptr;
}

void* mem_realloc(void* ptr, size_t size) {
    void* newptr = realloc(ptr, size);
    
    // Check for error
    if (newptr == NULL) {
        free(ptr); // Original ptr still valid

        // On error, function fails (some bigger problem occurred)
        LOG_CRIT("Realloc failed trying to allocate %ul bytes", size);
        exit(EXIT_FAILURE);
    }

    // Otherwise returns ptr
    return newptr;
}

void lock_mutex(pthread_mutex_t* mutex) {
    int res = 0;
    if ((res = pthread_mutex_lock(mutex)) != 0) {
        errno = res;
        LOG_ERRNO("Server process crashed locking mutex");

        // On error, function fails (some bigger problem occurred)
        // source:
        //   https://linux.die.net/man/3/pthread_mutex_lock
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }
}

void unlock_mutex(pthread_mutex_t* mutex) {
    int res = 0;
    if ((res = pthread_mutex_unlock(mutex)) != 0) {
        errno = res;
        LOG_ERRNO("Server process crashed unlocking mutex");

        // On error, function fails (some bigger problem occurred)
        // source:
        //   https://linux.die.net/man/3/pthread_mutex_unlock
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }
}

void lock_rw_mutex_read(pthread_rwlock_t* mutex) {
    int res = 0;
    if ((res = pthread_rwlock_rdlock(mutex)) != 0) {
        errno = res;
        LOG_ERRNO("Server process crashed locking rw mutex for read");

        // On error, function fails (some bigger problem occurred)
        // source:
        //   https://linux.die.net/man/3/pthread_rwlock_rdlock
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }
}

void lock_rw_mutex_write(pthread_rwlock_t* mutex) {
    int res = 0;
    if ((res = pthread_rwlock_wrlock(mutex)) != 0) {
        errno = res;
        LOG_ERRNO("Server process crashed locking rw mutex for write");

        // On error, function fails (some bigger problem occurred)
        // source:
        //   https://linux.die.net/man/3/pthread_rwlock_wrlock
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }
}

void unlock_rw_mutex(pthread_rwlock_t* mutex) {
    int res = 0;
    if ((res = pthread_rwlock_unlock(mutex)) != 0) {
        errno = res;
        LOG_ERRNO("Server process crashed unlocking rw mutex");

        // On error, function fails (some bigger problem occurred)
        // source:
        //   https://linux.die.net/man/3/pthread_mutex_unlock
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }
}

void notify_one(pthread_cond_t* cond) {
    int res = 0;
    if ((res = pthread_cond_signal(cond)) != 0) {
        errno = res;
        LOG_ERRNO("Error sending signal");
    }
}

void notify_all(pthread_cond_t* cond) {
    int res = 0;
    if ((res = pthread_cond_broadcast(cond)) != 0) {
        errno = res;
        LOG_ERRNO("Error sending signal");
    }
}
