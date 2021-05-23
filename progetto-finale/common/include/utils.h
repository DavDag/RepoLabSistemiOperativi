#pragma once

#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <threads.h>

#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)

/*
 * Returns -1 for conversion error
 * Returns -2 for range error
 * Otherwise return the value (>=0)
 */
int parse_positive_integer(const char* str);

/*
 * Always returns a valid ptr or terminate the process
 */
void* mem_malloc(size_t);

/*
 * Always returns a valid ptr or terminate the process
 */
void* mem_calloc(size_t, size_t);

/*
 * Always returns a valid ptr or terminate the process
 */
void* mem_realloc(void*, size_t);

/**
 * Always lock the mutex or terminate the process
 */
void lock_mutex(pthread_mutex_t* mutex);

/**
 * Always unlock the mutex or terminate the process
 */
void unlock_mutex(pthread_mutex_t* mutex);

/**
 * Always lock the mutex in read or terminate the process
 */
void lock_rw_mutex_read(pthread_rwlock_t* mutex);

/**
 * Always lock the mutex in write or terminate the process
 */
void lock_rw_mutex_write(pthread_rwlock_t* mutex);

/**
 * Always unlock the mutex or terminate the process
 */
void unlock_rw_mutex(pthread_rwlock_t* mutex);

/**
 * Notify one thread waiting on 'cond' (using pthread_cond_signal)
 */
void notify_one(pthread_cond_t* cond);

/**
 * Notify all threads waiting on 'cond' (using pthread_cond_broadcast)
 */
void notify_all(pthread_cond_t* cond);

#endif // UTILS_H
