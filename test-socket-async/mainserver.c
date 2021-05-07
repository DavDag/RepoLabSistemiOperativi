#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_CLIENTS 64

#include "common.h"

static int next_thread_id = 1;

typedef struct thread_args {
    int id;
    long conn;
} thread_args_t;

typedef struct socket_msg {
    int len;
    char *content;
} socket_msg_t;

void *thread_func(void *_args) {
    if (_args == NULL)
        pthread_exit((void *) EXIT_FAILURE);

    // Retrieve arguments
    thread_args_t args = *((thread_args_t *) _args);

    printf("[#%d] Thread Opened.\n", args.id);

    int n = 0, tmpLen = 19;
    socket_msg_t msg;
    msg.len = 0;
    msg.content = NULL;
    while (1) {
        // Correct behaviour

        // 1. Read msg
        // 2. switch on msg.type
        // ... do things ...
        // 3. eventually write back to client
        // back to 1

        // Just for testing purposes
        n = readn(args.conn, &msg.len, sizeof(int));
        if (n == 0) break;
        msg.content = realloc(msg.content, (msg.len + 1) * sizeof(char));
        n = readn(args.conn, msg.content, msg.len);
        msg.content[msg.len] = '\0';
        printf("[#%d] Here is the message: %s", args.id, msg.content);
        n = writen(args.conn, &tmpLen, sizeof(int));
        n = writen(args.conn, "I got your message", tmpLen * sizeof(char));

        // Check for "quit" message
        if (strcmp("quit\n", msg.content) == 0) {
            printf("[#%d] Quit request received. Terminating connection...\n", args.id);
            free(msg.content);
            break;
        }
    }

    printf("[#%d] Thread Closed.\n", args.id);

    // Release resources
    close(args.conn);
    free(_args);
    return NULL;
}

void spawn_thread(long conn) {
    pthread_attr_t attr;
    pthread_t id;

    // Initialize thread's attributes
    if (pthread_attr_init(&attr) != 0) {
		fprintf(stderr, "ERROR on pthread_attr_init\n");
		close(conn);
		return;
    }

    // Set DETACHED mode
    if (pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) != 0) {
		fprintf(stderr, "ERROR on pthread_attr_setdetachstate\n");
		pthread_attr_destroy(&attr);
		close(conn);
		return;
    }

    // Thread's arguments
    thread_args_t *tArgs = (thread_args_t *) malloc(sizeof(thread_args_t));
    tArgs->conn = conn;
    tArgs->id = next_thread_id++;

    // Spawn thread
    if (pthread_create(&id, &attr, thread_func, (void *) tArgs) != 0) {
		fprintf(stderr, "ERROR on pthread_create");
		pthread_attr_destroy(&attr);
		close(conn);
        free(tArgs);
		return;
    }
}

int main(int argc, char *argv[]) {
    common_init();
    long sock = create_socket();
    listen_on_socket(sock, MAX_CLIENTS);
    while (1) {
        printf("[#M] Accepting... !\n");
        long conn = accept_on_socket(sock);
        printf("[#%d] Accepted !\n", next_thread_id);
        spawn_thread(conn);
    }
    return 0;
}