#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/un.h>
#include <sys/socket.h>

#define SOCKNAME "./d_sock"
#define SOCKNAME_LEN 10

void cleanup() {
    unlink(SOCKNAME);
}

void common_init() {
    cleanup();
    atexit(cleanup);
}

long create_socket() {
    // Create socket descriptor
    long sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("ERROR opening socket");
        exit(1);
    }

    // Create address struct
    struct sockaddr_un serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKNAME, SOCKNAME_LEN);

    // Assign address to socket
    int res = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (res < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    // Return socket descriptor
    return sockfd;
}

void listen_on_socket(long sock, int max_clients) {
    // Convert unconnected socket into passive socket that can accepts connections 
    int res = listen(sock, max_clients);
    if (res < 0) {
        perror("ERROR on listen");
        exit(1);
    }
}

long accept_on_socket(long sock) {
    // Retrieve next connection from connection queue
    long conn = accept(sock, NULL, NULL);
    if (conn < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    // Return client descriptor
    return conn;
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
    return 1;
}
