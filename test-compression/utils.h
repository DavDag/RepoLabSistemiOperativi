#pragma once

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)


void* mem_malloc(size_t size) {
    void* ptr = malloc(size);

    // Check for error
    if (ptr == NULL) {
        // On error, function fails (some bigger problem occurred)
        // LOG_CRIT("Malloc failed trying to allocate %ul bytes", size);
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
        // LOG_CRIT("Calloc failed trying to allocate %ul bytes", num * size);
        exit(EXIT_FAILURE);
    }

    // Otherwise returns ptr
    return ptr;
}

void* mem_realloc(void* ptr, size_t size) {
    void* newptr = realloc(ptr, size);
    
    // Check for error
    if (newptr == NULL) {
        // On error, function fails (some bigger problem occurred)
        // LOG_CRIT("Realloc failed trying to allocate %ul bytes", size);
        exit(EXIT_FAILURE);
    }

    // Otherwise returns ptr
    return newptr;
}


int save_as_file(const char* dirname, const char* filename, const char* content, int contentSize) {
    // Calc path
    static char mkdirCmdPrefix[] = "mkdir -p ";
    static char mkdirCmd[4096];
    memset(mkdirCmd, 0, 4096 * sizeof(char));
    strcpy(mkdirCmd, mkdirCmdPrefix);
    strcat(mkdirCmd, dirname);
    strcat(mkdirCmd, "/");
    strcat(mkdirCmd, filename);
    char* path = &mkdirCmd[9];
    int pathLen = strlen(path);

    // Temporary shrink string
    int slashIndex = 0;
    for (int c = pathLen - 1; c > 0; --c) {
        if (path[c] == '/') {
            path[c] = '\0';
            slashIndex = c;
            break;
        }
    }

    // Create directory
    if (system(mkdirCmd) < 0)
        printf("Oioi!\n");
    path[slashIndex] = '/';

    // Open file
    FILE* file = NULL;
    if ((file = fopen(path, "wb")) == NULL)
        return -1;
    
    // Write file
    if (fwrite(content, sizeof(char), contentSize, file) != contentSize)
        return -1;

    // Close file
    fclose(file);

    // Returns success
    return 0;
}

