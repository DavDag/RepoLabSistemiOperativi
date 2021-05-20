#include "net.h"

#include "utils.h"
#include "logger.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

// ======================================= DECLARATIONS: Inner functions ============================================

/**
 * Write 'size' bytes from data into buffer.
 * Increments buf by size.
 */
void writeToBuffer(void* data, char** buf, size_t size);

/**
 * Read 'size' bytes from buffer into data.
 * Increments buf by size.
 */
void readFromBuffer(void* data, char** buf, size_t size);

// Converts content of 'data' from integer to char*
void convertOffsetToPtr(char* begin, MsgPtr_t* data);

// Converts content of 'data' from char* to integer
void convertPtrToOffset(char* begin, MsgPtr_t* data);

// ======================================= DEFINITIONS: net.h functions =============================================

int readMessage(long socketfd, char* bbegin, size_t bufferSize, SockMessage_t* msg) {
    errno = 0;
    int res = -1;
    char* buffer = bbegin;

    // 1. Read size
    int msgSize = 0;
    if ((res = readN(socketfd, (char*) &msgSize, sizeof(int))) != 1)
        return res;
    if (msgSize > bufferSize) {
        errno = ENOMEM;
        return -1;
    }

    // 2. Read socket (into temp buffer)
    memset(buffer, 0, msgSize * sizeof(char));
    if ((res = readN(socketfd, buffer, sizeof(char) * msgSize)) != 1)
        return res;
    
    // 3. Read message (from temp buffer)
    
    // UID
    readFromBuffer(&msg->uid, &buffer, sizeof(UUID_t));

    // Type
    readFromBuffer(&msg->type, &buffer, sizeof(SockeMessageType_t));
    
    // Body
    switch (msg->type)
    {
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_READ_N_FILES:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            // TODO:
            break;
        }

        case MSG_RESP_SIMPLE:
        {
            // Status
            readFromBuffer(&msg->response.status, &buffer, sizeof(RespStatus_t));
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            // Status
            readFromBuffer(&msg->response.status, &buffer, sizeof(RespStatus_t));

            // Num files attached
            readFromBuffer(&msg->response.numFiles, &buffer, sizeof(int));

            /* TODO:
            // ============ Filename ============
            ResourcePath_t* filename = &msg->response.filename;

            // Length
            readFromBuffer(&filename->len, &buffer, sizeof(int));

            // Absolute path name 'ptr'
            readFromBuffer(&filename->abs.i, &buffer, sizeof(int));
            convertOffsetToPtr(bbegin, &filename->abs);

            // Relative path name 'ptr'
            readFromBuffer(&filename->rel.i, &buffer, sizeof(int));
            convertOffsetToPtr(bbegin, &filename->rel);

            // ============ Content ============
            // Length
            readFromBuffer(&msg->response.contentLen, &buffer, sizeof(int));

            // Data 'ptr'
            readFromBuffer(&msg->response.content, &buffer, sizeof(int));
            convertOffsetToPtr(bbegin, &msg->response.content);
            */

            break;
        }

        default:
            break;
    }

    return 1;
}

int writeMessage(long socketfd, char* bbegin, size_t bufferSize, SockMessage_t* msg) {
    errno = 0;
    int res = -1;
    char* buffer = bbegin;

    // 1. Process message
    switch (msg->type)
    {
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_READ_N_FILES:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            // TODO:
            break;
        }

        case MSG_RESP_SIMPLE:
        {
            // TODO:
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            // TODO:
            break;
        }

        default:
            break;
    }

    // 2. Write message (into tmp buffer)
    writeToBuffer(&msg->uid, &buffer, sizeof(UUID_t));
    writeToBuffer(&msg->type, &buffer, sizeof(SockeMessageType_t));

    // 3. Write buffer (into socket)
    int msgSize = buffer - bbegin;
    if ((res = writeN(socketfd, (char*) &msgSize, sizeof(int))) != 1)
        return res;
    if ((res = writeN(socketfd, bbegin, msgSize * sizeof(char))) != 1)
        return res;

    return 1;
}

// ======================================= DEFINITIONS: Inner functions =============================================

// https://linux.die.net/man/2/read
// https://linux.die.net/man/2/write
// http://didawiki.cli.di.unipi.it/doku.php/informatica/sol/laboratorio21/esercitazionib/readnwriten

int readN(int fd, char* buf, size_t size) {
    ssize_t r = 0;
    
    // Read 'size' bytes
    while(size > 0) {
        
        // Call 'read' and save returns values
        if ((r = read(fd, buf, size)) == -1) {
            /**
             * The call was interrupted by a signal before any data was read
             * 
             * source: https://linux.die.net/man/2/read
             */
            if (errno == EINTR) continue;
            else                return -1;
        }
        
        // Check for 'EOF' (or connection closed from the other end)
        if (r == 0) return 0;

        // Update size and buffer
        size -= r;
        buf  += r;
    }
    return 1;
}

int writeN(int fd, char* buf, size_t size) {
    ssize_t w = 0;
    
    // Write 'size' bytes
    while(size > 0) {
        
        // Call 'write' and save returns values
        if ((w = write(fd, buf, size)) == -1) {
            /**
             * The call was interrupted by a signal before any data was written
             * 
             * source: https://linux.die.net/man/2/write
             */
            if (errno == EINTR) continue;
            else                return -1;
        }
        
        // Check for 0
        if (w == 0) return 0;

        // Update size and buffer
        size -= w;
        buf  += w;
    }
    return 1;
}

void writeToBuffer(void* data, char** buf, size_t size) {
    // buffer <= data
    memcpy(*buf, data, size);
    *buf += size;
}

void readFromBuffer(void* data, char** buf, size_t size) {
    // buffer => data
    memcpy(data, *buf, size);
    *buf += size;
}

void convertOffsetToPtr(char* begin, MsgPtr_t* data) {
    data->ptr = begin + data->i;
}

void convertPtrToOffset(char* begin, MsgPtr_t* data) {
    data->i = data->ptr - begin; 
}
