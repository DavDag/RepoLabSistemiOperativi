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
void writeToBuffer(char** buf, void* data, size_t size);

/**
 * Read 'size' bytes from buffer into data.
 * Increments buf by size.
 */
void readFromBuffer(char** buf, void* data, size_t size);

// Converts content of 'data' from integer to char*
void convertOffsetToPtr(char* begin, MsgPtr_t* data);

// Converts content of 'data' from char* to integer
void convertPtrToOffset(char* begin, MsgPtr_t* data);

// Estimate raw content size
int calcMsgRawContentOffset(SockMessage_t* msg);

// ======================================= DEFINITIONS: net.h functions =============================================

int readMessage(long socketfd, char* bbegin, size_t bufferSize, SockMessage_t* msg) {
    errno = 0;
    int res = -1;
    char* buffer = bbegin;

    // 1. Read size from socket
    int msgSize = 0;
    if ((res = readN(socketfd, (char*) &msgSize, sizeof(int))) != 1)
        return res;
    if (msgSize > bufferSize) {
        errno = ENOMEM;
        return -1;
    }

    // 2. Read from socket (into temp buffer)
    memset(buffer, 0, msgSize * sizeof(char));
    if ((res = readN(socketfd, buffer, sizeof(char) * msgSize)) != 1)
        return res;
    
    // 3. Read message (from temp buffer)
    
    // UID
    readFromBuffer(&buffer, &msg->uid, sizeof(UUID_t));

    // Type
    readFromBuffer(&buffer, &msg->type, sizeof(SockMessageType_t));
    
    // Body
    switch (msg->type)
    {
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        {
            break;
        }

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
            // Flags
            readFromBuffer(&buffer, &msg->request.flags, sizeof(int));

            // File
            MsgFile_t file;
            readFromBuffer(&buffer, &file.filename.len  , sizeof(int));
            readFromBuffer(&buffer, &file.filename.abs.i, sizeof(int));
            readFromBuffer(&buffer, &file.filename.rel.i, sizeof(int));
            readFromBuffer(&buffer, &file.contentLen    , sizeof(int));
            readFromBuffer(&buffer, &file.content.i     , sizeof(int));
            convertOffsetToPtr(bbegin, &file.filename.abs);
            convertOffsetToPtr(bbegin, &file.filename.rel);
            convertOffsetToPtr(bbegin, &file.content);
            msg->request.file = file;
            break;
        }

        case MSG_RESP_SIMPLE:
        {
            // Status
            readFromBuffer(&buffer, &msg->response.status, sizeof(RespStatus_t));
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            // Status
            readFromBuffer(&buffer, &msg->response.status, sizeof(RespStatus_t));

            // Num files
            readFromBuffer(&buffer, &msg->response.numFiles, sizeof(int));

            // Files
            const int numFiles = msg->response.numFiles;
            MsgFile_t* files = NULL;
            if (numFiles > 0) {
                files = (MsgFile_t*) mem_calloc(numFiles, sizeof(MsgFile_t));
                for (int i = 0; i < numFiles; ++i) {
                    readFromBuffer(&buffer, &files[i].filename.len  , sizeof(int));
                    readFromBuffer(&buffer, &files[i].filename.abs.i, sizeof(int));
                    readFromBuffer(&buffer, &files[i].filename.rel.i, sizeof(int));
                    readFromBuffer(&buffer, &files[i].contentLen    , sizeof(int));
                    readFromBuffer(&buffer, &files[i].content.i     , sizeof(int));
                    convertOffsetToPtr(bbegin, &files[i].filename.abs);
                    convertOffsetToPtr(bbegin, &files[i].filename.rel);
                    convertOffsetToPtr(bbegin, &files[i].content);
                }
            }
            msg->response.files = files;
            break;
        }

        default:
            break;
    }

    // Raw content
    int rawBytes = msgSize - calcMsgRawContentOffset(msg);
    msg->raw_content = NULL;
    if (rawBytes) {
        // Read raw content
        msg->raw_content = (char*) mem_calloc(rawBytes, sizeof(char));
        readFromBuffer(&buffer, msg->raw_content, rawBytes * sizeof(char));
    }

    // Returns success
    return 1;
}

int writeMessage(long socketfd, char* bbegin, size_t bufferSize, SockMessage_t* msg) {
    errno = 0;
    int res = -1;
    char* buffer = bbegin;
    char* brawbuffer = bbegin + calcMsgRawContentOffset(msg);
    char* rawbuffer = brawbuffer;

    // 1. Write message into buffer

    // UUID
    writeToBuffer(&buffer, &msg->uid, sizeof(UUID_t));

    // Type
    writeToBuffer(&buffer, &msg->type, sizeof(SockMessageType_t));

    // Body
    switch (msg->type)
    {
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        {
            break;
        }

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
            // Flags
            writeToBuffer(&buffer, &msg->request.flags, sizeof(int));

            // File
            MsgFile_t file = msg->request.file;
            convertOffsetToPtr(brawbuffer, &file.filename.abs);
            convertOffsetToPtr(brawbuffer, &file.filename.rel);
            convertOffsetToPtr(brawbuffer, &file.content);
            writeToBuffer(&buffer, &file.filename.len  , sizeof(int)); // Filename length
            writeToBuffer(&buffer, &file.filename.abs.i, sizeof(int)); // Filename ptr offset of abs path
            writeToBuffer(&buffer, &file.filename.rel.i, sizeof(int)); // Filename ptr offset of rel path
            writeToBuffer(&buffer, &file.contentLen    , sizeof(int)); // Content length
            writeToBuffer(&buffer, &file.content.i     , sizeof(int)); // Content ptr offset
            writeToBuffer(&rawbuffer, &file.filename.abs, file.filename.len * sizeof(char)); // Filename path
            writeToBuffer(&rawbuffer, &file.content, file.contentLen * sizeof(char));        // Content
            break;
        }

        case MSG_RESP_SIMPLE:
        {
            // Status
            writeToBuffer(&buffer, &msg->response.status, sizeof(RespStatus_t));
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            // Status
            writeToBuffer(&buffer, &msg->response.status, sizeof(RespStatus_t));

            // Num files
            writeToBuffer(&buffer, &msg->response.numFiles, sizeof(int));

            // Files
            const int numFiles = msg->response.numFiles;
            MsgFile_t* files = msg->response.files;
            for (int i = 0; i < numFiles; ++i) {
                convertOffsetToPtr(brawbuffer, &files[i].filename.abs);
                convertOffsetToPtr(brawbuffer, &files[i].filename.rel);
                convertOffsetToPtr(brawbuffer, &files[i].content);
                writeToBuffer(&buffer, &files[i].filename.len  , sizeof(int)); // Filename length
                writeToBuffer(&buffer, &files[i].filename.abs.i, sizeof(int)); // Filename ptr offset of abs path
                writeToBuffer(&buffer, &files[i].filename.rel.i, sizeof(int)); // Filename ptr offset of rel path
                writeToBuffer(&buffer, &files[i].contentLen    , sizeof(int)); // Content length
                writeToBuffer(&buffer, &files[i].content.i     , sizeof(int)); // Content ptr offset
                writeToBuffer(&rawbuffer, &files[i].filename.abs, files[i].filename.len * sizeof(char)); // Filename path
                writeToBuffer(&rawbuffer, &files[i].content, files[i].contentLen * sizeof(char));        // Content
            }
            break;
        }

        default:
            break;
    }

    // 2. DOUBLE CHECK for msg malformation
    if (buffer > brawbuffer) {
        errno = EINVAL;
        return -1;
    }
    if (buffer > (bbegin + bufferSize) || rawbuffer > (bbegin + bufferSize)) {
        errno = ENOMEM;
        return -1;
    }

    // 2. Write buffer (into socket)
    int msgSize = buffer - bbegin;
    if ((res = writeN(socketfd, (char*) &msgSize, sizeof(int))) != 1)
        return res;
    if ((res = writeN(socketfd, bbegin, msgSize * sizeof(char))) != 1)
        return res;

    // Returns success
    return 1;
}

void freeMessageContent(SockMessage_t* msg) {
    // TODO:
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

void writeToBuffer(char** buf, void* data, size_t size) {
    // buffer <= data
    memcpy(*buf, data, size);
    *buf += size;
}

void readFromBuffer(char** buf, void* data, size_t size) {
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

int calcMsgRawContentOffset(SockMessage_t* msg) {
    int offset = sizeof(UUID_t) + sizeof(SockMessageType_t);
    switch (msg->type)
    {
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        {
            break;
        }

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
            // Flags
            offset += sizeof(int);

            // File
            offset += 5 * sizeof(int);
            break;
        }

        case MSG_RESP_SIMPLE:
        {
            // Status
            offset += sizeof(RespStatus_t);
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            // Status
            offset += sizeof(RespStatus_t);

            // Num files
            offset += sizeof(int);

            // Files
            offset += 5 * msg->response.numFiles * sizeof(int);
            break;
        }

        default:
            break;
    }
    return offset;
}