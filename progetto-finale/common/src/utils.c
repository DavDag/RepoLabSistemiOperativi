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

void* mem_malloc(size_t size) {
    void* ptr = malloc(size);

    // Check for error
    if (ptr == NULL) {
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
        LOG_CRIT("Realloc failed trying to allocate %ul bytes", size);
        exit(EXIT_FAILURE);
    }

    // Otherwise returns ptr
    return newptr;
}