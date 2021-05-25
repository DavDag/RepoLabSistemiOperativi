#pragma once

#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <common.h>

typedef void* CircQueueItemPtr_t;

/**
 * Represents a circular queue (or ring buffer).
 * Its thread-safe if used with proper functions (tryPop / tryPush).
 */
typedef struct {
    pthread_mutex_t mutex;
    CircQueueItemPtr_t* data;
    int head, tail, size, capacity;
} CircQueue_t;

/**
 * Allocate a Circular Queue of size slots.
 * Terminate process on failure.
 * 
 * \param size: number of elements the queue should contains
 * 
 * \retval ptr: the newly allocated queue
 */
CircQueue_t* createQueue(int size);

/**
 * Try popping and element from the queue.
 * Terminate process on failure.
 * 
 * \param queue: the queue itself
 * \param  item: the ptr where to store popped item
 * 
 * \retval  0: if queue was empty
 * \retval  1: if item now contains the element
 */
int tryPop(CircQueue_t* queue, CircQueueItemPtr_t* item);

/**
 * Try pushing item into queue.
 * 
 * \param queue: the queue itself
 * \param  item: the item to push
 * 
 * \retval  0: if queue was full
 * \retval  1: if item was pushed correctly 
 */
int tryPush(CircQueue_t* queue, CircQueueItemPtr_t item);

#endif // CIRCULAR_QUEUE_H
