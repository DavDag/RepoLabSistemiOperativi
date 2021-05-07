#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#define THREAD_COUNT 5

typedef struct threadArgs {
    int id;
    pthread_mutex_t* consoleMutex;
} threadArgs_t;

void* ThreadFun(void* funArgs) {
    char buffer[256];
    memset(buffer, 0, 256);

    // Retrieve arguments
    threadArgs_t *args = (threadArgs_t*) funArgs;

    // Lock on console mutex
    if (pthread_mutex_lock(args->consoleMutex) != 0) {
        fprintf(stderr, "ERRORE FATALE lock\n");
        pthread_exit((void*) EXIT_FAILURE);
    }

    // Read user input and print something
    printf("Thread #%d <<< ", args->id);
    char* res = fgets(buffer, 255, stdin);
    if (res == NULL) {
        fprintf(stderr, "ERROR reading from stdin");
        pthread_exit((void*) EXIT_FAILURE);
    }
    printf("Thread #%d >>> Received: {\n\t%s}\n", args->id, buffer);

    // Unlock console mutex
    if (pthread_mutex_unlock(args->consoleMutex) != 0) {
        fprintf(stderr, "ERRORE FATALE unlock\n");
        pthread_exit((void*) EXIT_FAILURE);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // 1. Faccio spazio per i thread e i loro parametri
    pthread_t *threads = malloc((THREAD_COUNT) * sizeof(pthread_t));
    threadArgs_t *tArgs = malloc((THREAD_COUNT) * sizeof(threadArgs_t));
    if (!threads || !tArgs) {
        fprintf(stderr, "malloc fallita\n");
        exit(EXIT_FAILURE);
    }

    // 2. Creo mutex per accesso condiviso alla console
    pthread_mutex_t consoleMutex;
    if (pthread_mutex_init(&consoleMutex, NULL) != 0) {
		fprintf(stderr, "pthread_mutex_init fallita\n");
		exit(EXIT_FAILURE);
    }

    // 3. Preparo i parametri
    for (int i = 0; i < THREAD_COUNT; ++i) {
        tArgs[i].id = i + 1;
        tArgs[i].consoleMutex = &consoleMutex;
    }

    // 4. Creo e lancio i thread
    for (int i = 0; i < THREAD_COUNT; ++i) {
        if (pthread_create(&threads[i], NULL, ThreadFun, &tArgs[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
			exit(EXIT_FAILURE);
        }
    }

    // 5. Aspetto la terminazione dei vari thread
    for (int i = 0; i < THREAD_COUNT; ++i) {
        if (pthread_join(threads[i], NULL) == -1) {
			fprintf(stderr, "pthread_join failed\n");
			exit(EXIT_FAILURE);
        }
    }

    free(threads);
    free(tArgs);
    return EXIT_SUCCESS;
}