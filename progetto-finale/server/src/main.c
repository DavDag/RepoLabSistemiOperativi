#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/un.h>

#define LOG_TIMESTAMP
// #define LOG_WITHOUT_COLORS
#define LOG_DEBUG
#include <logger.h>
#include <common.h>

void cleanup() {
    unlink(DEFAULT_SOCK_FILE);
}

int main(int argc, char** argv) {
    atexit(cleanup);
    LOG_VERB("Server started !");

    // 1. Creo il socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    // 2. Assegno indirizzo al socket
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, DEFAULT_SOCK_FILE, strlen(DEFAULT_SOCK_FILE) + 1);
    
    unlink(DEFAULT_SOCK_FILE);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }

    // 3. Mod Passiva, Max Clients: 3
    listen(sockfd, 10);

    // 4. Main Server
    int connfd, n;
    char buffer[4096];
    while(1) {
        LOG_INFO("Waiting client...");
        
        // 4.1 Accetto un client
        if ((connfd = accept(sockfd, (struct sockaddr *)NULL, NULL)) < 0) {
            LOG_ERRNO("ERROR on accept");
            exit(EXIT_FAILURE);
        }
        LOG_INFO("Client connected.");

        LOG_INFO("Waiting for client request...");
        SockMessage_t msg;
        if ((n = readMessage(connfd, buffer, 4096, &msg)) < 0) {
            LOG_ERRNO("ERROR reading msg");
            exit(EXIT_FAILURE);
        }
        if (n != 0) LOG_INFO("Msg arrived: uid=%s,type=%d", UUID_to_String(msg.uid), msg.type);
        else        LOG_INFO("Connection dropped from client.");

        LOG_INFO("Terminating client connection...");
        if (close(connfd) < 0) {
            LOG_ERRNO("ERROR on closing socket");
            exit(EXIT_FAILURE);
        }
        LOG_INFO("Client connection terminated.");
    }

    if ((n = close(sockfd)) < 0) {
        LOG_ERRNO("ERROR closing socket");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}