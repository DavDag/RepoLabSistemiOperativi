#include "net.h"

#define LOG_DEBUG
#include "logger.h"
#include "utils.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

// #define DEBUG_MESSAGES

// ======================================= DECLARATIONS: Inner functions ============================================

/**
 * Write 'size' bytes from data into buffer.
 * Increments buf by size.
 */
void writeToBuffer(char** buf, const void* data, size_t size);

/**
 * Read 'size' bytes from buffer into data.
 * Increments buf by size.
 */
void readFromBuffer(char** buf, void* data, size_t size);

// Converts content of 'data' from integer to char*
void convertOffsetToPtr(char* begin, MsgPtr_t* data, int isNotNull);

// Converts content of 'data' from char* to integer
void convertPtrToOffset(char* begin, MsgPtr_t* data);

// ======================================= DEFINITIONS: net.h functions =============================================

#ifdef DEBUG_MESSAGES
static thread_local int roff = 0;
#endif

int readMessage(long socketfd, char* bbegin, size_t bufferSize, SockMessage_t* msg) {
#ifdef DEBUG_MESSAGES
    roff = 0;
    lock_mutex(&gLogMutex);
    LOG_EMPTY("\033[31mReading message...\n");
    unlock_mutex(&gLogMutex);
#endif

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

        case MSG_REQ_READ_N_FILES:
        {
            // Flags
            readFromBuffer(&buffer, &msg->request.flags, sizeof(int));
            break;
        }

        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            // Flags
            readFromBuffer(&buffer, &msg->request.flags, sizeof(int));

            // File
            MsgFile_t file;
            readFromBuffer(&buffer, &file.filename.len  , sizeof(int));
            readFromBuffer(&buffer, &file.filename.abs.i, sizeof(int));
            readFromBuffer(&buffer, &file.contentLen    , sizeof(int));
            readFromBuffer(&buffer, &file.content.i     , sizeof(int));
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
                    readFromBuffer(&buffer, &files[i].contentLen    , sizeof(int));
                    readFromBuffer(&buffer, &files[i].content.i     , sizeof(int));
                }
            }
            msg->response.files = files;
            break;
        }

        default:
            break;
    }

#ifdef DEBUG_MESSAGES
    lock_mutex(&gLogMutex);
    roff = 0;
    LOG_EMPTY("\nread raw content:\n");
    unlock_mutex(&gLogMutex);
#endif

    // Raw content
    int rawBytes = msgSize - (buffer - bbegin);
    msg->raw_content = NULL;
    if (rawBytes) {
        // Read raw content
        msg->raw_content = (char*) mem_calloc(rawBytes, sizeof(char));
        readFromBuffer(&buffer, msg->raw_content, rawBytes * sizeof(char));
    }

    // Update ptrs
    switch (msg->type)
    {
        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            // TODO: Update rel
            convertOffsetToPtr(msg->raw_content, &msg->request.file.filename.abs, msg->request.file.filename.len);
            convertOffsetToPtr(msg->raw_content, &msg->request.file.content, msg->request.file.contentLen);
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            for (int i = 0; i < msg->response.numFiles; ++i) {
                // TODO: Update rel
                convertOffsetToPtr(msg->raw_content, &msg->response.files[i].filename.abs, msg->response.files[i].filename.len);
                convertOffsetToPtr(msg->raw_content, &msg->response.files[i].content, msg->response.files[i].contentLen);
            }
            break;
        }
    
        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        case MSG_REQ_READ_N_FILES:
        case MSG_RESP_SIMPLE:
        default:
            break;
    }

#ifdef DEBUG_MESSAGES
    lock_mutex(&gLogMutex);
    LOG_EMPTY("\nMsg size: %d bytes, type %d, uuid: %s\n\033[0m", msgSize, msg->type, UUID_to_String(msg->uid));
    unlock_mutex(&gLogMutex);
#endif

    // Returns success
    return msgSize;
}

#ifdef DEBUG_MESSAGES
static thread_local int woff = 0;
#endif

int writeMessage(long socketfd, char* bbegin, size_t bufferSize, SockMessage_t* msg) {
#ifdef DEBUG_MESSAGES
    woff = 0;
    lock_mutex(&gLogMutex);
    LOG_EMPTY("\033[32mSending message...\n");
    unlock_mutex(&gLogMutex);
#endif

    errno = 0;
    int res = -1;
    char* buffer = bbegin;
    int rawBufferIndex = 0;

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

        case MSG_REQ_READ_N_FILES:
        {
            // Flags
            writeToBuffer(&buffer, &msg->request.flags, sizeof(int));
            break;
        }

        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            // Flags
            writeToBuffer(&buffer, &msg->request.flags, sizeof(int));

            // File
            MsgFile_t file = msg->request.file;
            writeToBuffer(&buffer, &file.filename.len, sizeof(int)); // Filename length
            writeToBuffer(&buffer, &rawBufferIndex   , sizeof(int)); // Filename ptr offset of abs path
            rawBufferIndex += file.filename.len;
            writeToBuffer(&buffer, &file.contentLen  , sizeof(int)); // Content length
            writeToBuffer(&buffer, &rawBufferIndex   , sizeof(int)); // Content ptr offset
            rawBufferIndex += file.contentLen;
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
                writeToBuffer(&buffer, &files[i].filename.len, sizeof(int)); // Filename length
                writeToBuffer(&buffer, &rawBufferIndex       , sizeof(int)); // Filename ptr offset of abs path
                rawBufferIndex += files[i].filename.len;
                writeToBuffer(&buffer, &files[i].contentLen  , sizeof(int)); // Content length
                writeToBuffer(&buffer, &rawBufferIndex       , sizeof(int)); // Content ptr offset
                rawBufferIndex += files[i].contentLen;
            }
            break;
        }

        default:
            break;
    }

#ifdef DEBUG_MESSAGES
    lock_mutex(&gLogMutex);
    woff = 0;
    LOG_EMPTY("\nwrite raw content:\n");
    unlock_mutex(&gLogMutex);
#endif

    char* rawbuffer = buffer;

    // Raw content
    switch (msg->type)
    {
        case MSG_REQ_OPEN_FILE:
        case MSG_REQ_CLOSE_FILE:
        case MSG_REQ_READ_FILE:
        case MSG_REQ_LOCK_FILE:
        case MSG_REQ_UNLOCK_FILE:
        case MSG_REQ_REMOVE_FILE:
        case MSG_REQ_WRITE_FILE:
        case MSG_REQ_APPEND_TO_FILE:
        {
            MsgFile_t file = msg->request.file;
            writeToBuffer(&rawbuffer, file.filename.abs.ptr, file.filename.len * sizeof(char)); // Filename path
            writeToBuffer(&rawbuffer, file.content.ptr     , file.contentLen * sizeof(char));   // Content
            break;
        }

        case MSG_RESP_WITH_FILES:
        {
            MsgFile_t* files = msg->response.files;
            for (int i = 0; i < msg->response.numFiles; ++i) {
                writeToBuffer(&rawbuffer, files[i].filename.abs.ptr, files[i].filename.len * sizeof(char)); // Filename path
                writeToBuffer(&rawbuffer, files[i].content.ptr     , files[i].contentLen * sizeof(char));   // Content
            }
            break;
        }

        case MSG_REQ_OPEN_SESSION:
        case MSG_REQ_CLOSE_SESSION:
        case MSG_REQ_READ_N_FILES:
        case MSG_RESP_SIMPLE:
        default:
            break;
    }

    // 2. Write buffer (into socket)
    int msgSize = rawbuffer - bbegin;
    if ((res = writeN(socketfd, (char*) &msgSize, sizeof(int))) != 1)
        return res;
    if ((res = writeN(socketfd, bbegin, msgSize * sizeof(char))) != 1)
        return res;

#ifdef DEBUG_MESSAGES
    lock_mutex(&gLogMutex);
    LOG_EMPTY("\nMsg size: %d bytes, type %d, uuid: %s\n\033[0m", msgSize, msg->type, UUID_to_String(msg->uid));
    unlock_mutex(&gLogMutex);
#endif

    // Returns success
    return msgSize;
}

void freeMessageContent(SockMessage_t* msg, int deep) {
    // Release memory for allocated array
    if (msg->type == MSG_RESP_WITH_FILES) {
        if (deep) {
            for (int i = 0; i < msg->response.numFiles; ++i) {
                MsgFile_t file = msg->response.files[i];
                if (file.filename.abs.ptr) free((char*) file.filename.abs.ptr);
                if (file.content.ptr)      free((char*) file.content.ptr);
            }
        }        
        free(msg->response.files);
    }
    
    // Release memory for raw content
    if (msg->raw_content != NULL)
        free(msg->raw_content);
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

void writeToBuffer(char** buf, const void* data, size_t size) {
    // buffer <= data
#ifdef DEBUG_MESSAGES
    lock_mutex(&gLogMutex);
    for (int i = 0; i < size; ++i) {
        char c = (((char*) data)[i]) & 0x000000FF;
        LOG_EMPTY("%.2X(%c) ", c & 0x000000FF, (c < 32 || c > 126) ? '?' : c);
        woff = (woff + 1) % 8;
        if (woff == 4) LOG_EMPTY(" ");
        if (woff == 0) LOG_EMPTY("\n");
    }
    unlock_mutex(&gLogMutex);
#endif
    memcpy(*buf, data, size);
    *buf += size;
}

void readFromBuffer(char** buf, void* data, size_t size) {
    // buffer => data
#ifdef DEBUG_MESSAGES
    lock_mutex(&gLogMutex);
    for (int i = 0; i < size; ++i) {
        char c = ((*buf)[i]) & 0x000000FF;
        LOG_EMPTY("%.2X(%c) ", c & 0x000000FF, (c < 32 || c > 126) ? '?' : c);
        roff = (roff + 1) % 8;
        if (roff == 4) LOG_EMPTY(" ");
        if (roff == 0) LOG_EMPTY("\n");
    }
    unlock_mutex(&gLogMutex);
#endif
    memcpy(data, *buf, size);
    *buf += size;
}

void convertOffsetToPtr(char* begin, MsgPtr_t* data, int isNotNull) {
    data->ptr = (!isNotNull) ? NULL : (begin + data->i);
}

void convertPtrToOffset(char* begin, MsgPtr_t* data) {
    data->i = data->ptr - begin; 
}
