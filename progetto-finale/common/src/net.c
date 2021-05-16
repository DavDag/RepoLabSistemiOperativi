#include "net.h"

#include "utils.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

// ======================================= DECLARATIONS: Inner functions ============================================

/**
 * Read EXACLTY N bytes from the filedescriptor 'fd'
 * 
 * \retval -1 : on error (errno set)
 * \retval  0 : when one of the inner 'read' call returns EOF
 * \retval  1 : on success
 */
int readN(int fd, char* buf, size_t size);

/**
 * Write EXACLTY N bytes from the filedescriptor 'fd'
 * 
 * \retval -1 : on error (errno set)
 * \retval  0 : when one of the inner 'write' call returns 0
 * \retval  1 : on success
 */
int writeN(int fd, char* buf, size_t size);

/**
 * Write 'size' bytes from data into buffer.
 * Increments buf by size.
 */
void writeToBuffer(char* data, char** buf, size_t size);

/**
 * Read 'size' bytes from buffer into data.
 * Increments buf by size.
 */
void readFromBuffer(char* data, char** buf, size_t size);

void convertOffsetToPtr(const char* begin, MsgPtr_t* data);
void convertPtrToOffset(const char* begin, MsgPtr_t* data);

// ======================================= DEFINITIONS: net.h functions =============================================

int readMsg(long fd, char* buffer, size_t bufferSize, SockMessage_t* msg) {
    errno = 0;
    int res = -1;
    const char* bbegin = buffer;

    // 1. Read size
    int msgSize = 0;
    if ((res = readN(fd, &msgSize, sizeof(msgSize))) != 1)
        return res;
    if (msgSize > bufferSize) {
        errno = ENOMEM;
        return -1;
    }

    // 2. Read message (into temp buffer)
    memset(buffer, 0, msgSize * sizeof(char));
    if ((res = readN(fd, buffer, sizeof(char) * msgSize)) != -1)
        return res;
    
    // 3. Process message (from temp buffer)
    // UID
    readFromBuffer(&msg->uid, &buffer, sizeof(UID_t));
   // Type
    readFromBuffer(&msg->type, &buffer, sizeof(SockeMessageType_t));
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
            break;
        }

        default:
            break;
    }

    return 1;
}

int writeMsg(long fc, char* buffer, size_t bufferSize, SockMessage_t* msg) {
    errno = 0;
    int res = -1;

    // 1. Process message

    // 2. Write size

    // 3. Write message

    return 1;
}

void freeMessage(SockMessage_t* msg) {

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

void writeToBuffer(char* data, char** buf, size_t size) {
    // buffer <= data
    memcpy(*buf, data, size);
    *buf += size;
}

void readFromBuffer(char* data, char** buf, size_t size) {
    // buffer => data
    memcpy(data, *buf, size);
    *buf += size;
}

void convertOffsetToPtr(const char* begin, MsgPtr_t* data) {
    data->ptr = begin + data->i;
}

void convertPtrToOffset(const char* begin, MsgPtr_t* data) {
    data->i = data->ptr - begin; 
}
