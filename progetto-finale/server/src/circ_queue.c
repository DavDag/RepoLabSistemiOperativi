#include "circ_queue.h"

#include <logger.h>

CircQueue_t* createQueue(int size) {
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

int tryPop(CircQueue_t* queue, CircQueueItemPtr_t* item) {
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

int tryPush(CircQueue_t* queue, CircQueueItemPtr_t item) {
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
