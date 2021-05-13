#pragma once

#include <errno.h>
#include <stdlib.h>

/*
 * Returns -1 for conversion error
 * Returns -2 for range error
 * Otherwise return the value (>=0)
 */
int parse_positive_integer(const char* str);

/*
 * Always returns a valid ptr or throw an error terminating the app
 */
void* mem_malloc(size_t);

/*
 * Always returns a valid ptr or throw an error terminating the app
 */
void* mem_realloc(void*, size_t);