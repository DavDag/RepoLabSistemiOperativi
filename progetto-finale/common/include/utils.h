#pragma once

#include <errno.h>
#include <stdlib.h>

/*
 * Returns -1 for conversion error
 * Returns -2 for range error
 * Otherwise return the value (>=0)
 */
static inline int parse_positive_integer(const char *str) {
    char *end = NULL;
    errno = 0;
    int val = strtol(str, &end, 10);

    // Check if conversion can be done
    if (str == end || (end != NULL && *end != '\0')) return -1;

    // Check range error
    if (errno == ERANGE) return -2;

    // Otherwise return parsed value
    return val;
}