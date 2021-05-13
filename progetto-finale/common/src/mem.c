#include "mem.h"
#include "logger.h"

void* mem_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        LOG_CRIT("Malloc failed trying to allocate %ul bytes.", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void* mem_realloc(void *ptr, size_t size) {
    void *newptr = realloc(ptr, size);
    if (newptr == NULL) {
        free(ptr); // Original ptr still valid
        LOG_CRIT("Realloc failed trying to allocate %ul bytes.", size);
        exit(EXIT_FAILURE);
    }
    return newptr;
}