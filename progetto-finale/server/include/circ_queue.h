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
static CircQueue_t* createQueue(int size) {
    // Create queue
    CircQueue_t* queue = (CircQueue_t*) mem_malloc(sizeof(CircQueue_t));

    // Initialize mutex and cond var
    int res = 0;
    if ((res = pthread_mutex_init(&queue->mutex, NULL)) != 0) {
        errno = res;
        
        // On error, function fails (some bigger problem occurred)
        // source:
        //   https://linux.die.net/man/3/pthread_mutex_init
        LOG_ERRNO("Server process crashed initializing mutex for queue");
        LOG_CRIT("Terminating server...");
        exit(EXIT_FAILURE);
    }

    // Initialize other fields
    queue->data     = mem_calloc(size, sizeof(CircQueueItemPtr_t));
    queue->head     = 0;
    queue->tail     = 0;
    queue->size     = 0;
    queue->capacity = size;

    // Returns newly allocated queue
    return queue;
}

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
static int tryPop(CircQueue_t* queue, CircQueueItemPtr_t* item) {
    // vars
    int res = 0;

    lock_mutex(&queue->mutex);
    // Is it not empty ?
    if (queue->size > 0) {
        *item = queue->data[queue->tail];
        queue->tail = (queue->tail + 1) % queue->capacity;
        queue->size--;
        res = 1;
    }
    unlock_mutex(&queue->mutex);

    // Returns result
    return res;
}

/**
 * Try pushing item into queue.
 * 
 * \param queue: the queue itself
 * \param  item: the item to push
 * 
 * \retval  0: if queue was full
 * \retval  1: if item was pushed correctly 
 */
static int tryPush(CircQueue_t* queue, CircQueueItemPtr_t item) {
    // vars
    int res = 0;

    lock_mutex(&queue->mutex);
    // Is it not full ?
    if (queue->size < queue->capacity) {
        queue->data[queue->head] = item;
        queue->head = (queue->head + 1) % queue->capacity;
        queue->size++;
        res = 1;
    }
    unlock_mutex(&queue->mutex);

    // Returns result
    return res;
}

#endif // CIRCULAR_QUEUE_H
