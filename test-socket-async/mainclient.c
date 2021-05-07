#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include "common.h"

int main(int argc, char *argv[]) {
    // 1. Creo il socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // 2. Connetto il socket
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME)+1);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    // 4. Main Client
    int n;
    int buffer_len = 0;
    char buffer[256];

    while (1) {
        // 4.1 Wait for user to enter the message
        memset(buffer, 0, 256);
        printf("Please enter the message: ");
        char* res = fgets(buffer, 255, stdin);
        if (res == NULL) {
            perror("ERROR reading from stdin");
            exit(1);
        }
        buffer_len = strlen(buffer) + 1;

        // 4.2 Send message to server
        n = writen(sockfd, &buffer_len, sizeof(int));
        if (n < 0) {
            perror("ERROR writing 1 to socket");
            exit(1);
        }
        n = writen(sockfd, buffer, buffer_len * sizeof(char));
        if (n < 0) {
            perror("ERROR writing 2 to socket");
            exit(1);
        }

        // 4.3 Read message from server
        n = readn(sockfd, &buffer_len, sizeof(int));
        if (n == 0) break;
        if (n < 0) {
            perror("ERROR reading 1 from socket");
            exit(1);
        }
        n = readn(sockfd, buffer, buffer_len * sizeof(char));
        if (n < 0) {
            perror("ERROR reading 2 from socket");
            exit(1);
        }
        buffer[buffer_len] = '\0';
        printf("%s\n", buffer);
    }

    printf("%s\n",buffer);
    return 0;
}