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

#define SOCKNAME "./cs_sock"

void cleanup() {
    unlink(SOCKNAME);
}

int main(int argc, char *argv[]) {
    cleanup();
    atexit(cleanup);

    // 1. Creo il socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // 2. Assegno indirizzo al socket
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME)+1);
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    // 3. Mod Passiva, Max Clients: 3
    listen(sockfd, 1);

    // 4. Main Server
    int connfd, n;
    char buffer[256];
    while(1) {
        
        // 4.1 Accetto un client
        connfd = accept(sockfd, (struct sockaddr *)NULL, NULL);
        if (connfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        // 4.2 Read from client
        memset(buffer, 0, 256);
        n = read(connfd, buffer, 255 );
        if (n < 0) {
            perror("ERROR reading from socket");
            exit(1);
        }
   
        printf("Here is the message: %s", buffer);

        // 4.3 Write to client
        n = write(connfd, "I got your message", 18);
        if (n < 0) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }

   return 0;
}