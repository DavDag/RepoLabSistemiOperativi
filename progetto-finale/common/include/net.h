#pragma once

#ifndef NET_H
#define NET_H

#include "uuid.h"
#include <stdlib.h>

#define DEFAULT_SOCK_FILE "../cs_sock"

/**
 * To be able to send ptr's via socket, they needs to be converted
 * to offsets relative to message's begin.
 */
typedef union { int i; const char* ptr; } MsgPtr_t;

// Message's type
typedef enum {
    MSG_NONE               =  0, // Default message type
    MSG_REQ_OPEN_SESSION   =  1, // Create client's session
    MSG_REQ_CLOSE_SESSION  =  2, // Clean client's session
    MSG_REQ_OPEN_FILE      =  3, // Request to open file
    MSG_REQ_CLOSE_FILE     =  4, // Request to close file
    MSG_REQ_READ_FILE      =  5, // Request to read file
    MSG_REQ_LOCK_FILE      =  6, // Request to lock file
    MSG_REQ_UNLOCK_FILE    =  7, // Request to unlock file
    MSG_REQ_REMOVE_FILE    =  8, // Request to remove file
    MSG_REQ_READ_N_FILES   =  9, // Request to read n files
    MSG_REQ_WRITE_FILE     = 10, // Request to write file
    MSG_REQ_APPEND_TO_FILE = 11, // Request to append to file
    MSG_RESP_SIMPLE        = 20, // Basic response
    MSG_RESP_WITH_FILES    = 21, // Response with files attached
} SockMessageType_t;

// Message's response status type
typedef enum {
    RESP_STATUS_NONE          = 0, // Default
    RESP_STATUS_OK            = 1, // Success
    RESP_STATUS_GENERIC_ERROR = 2, // Error
    RESP_STATUS_NOT_PERMITTED = 3, // Failure: EPERM
    RESP_STATUS_INVALID_ARG   = 4, // Failure: EINVAL
    RESP_STATUS_NOT_FOUND     = 5, // Failure: ENOENT
} RespStatus_t;

/**
 * It represents the resource uniquely.
 * (Like an absolute path inside an OS or an URI on the web).
 * 
 * ex:
 *   '/root/directory_1/directory_2/testfile.ext'
 *    ^                             ^
 *    |                             |
 *   ABS                           REL
 */
typedef struct {
    int len;      // Filename's length
    MsgPtr_t abs; // Absolute name str
    MsgPtr_t rel; // Relative name str
} ResourcePath_t;

typedef struct {
    ResourcePath_t filename; // Filename
    int contentLen;          // Length of content
    MsgPtr_t content;        // Content
} MsgFile_t;

/**
 * Its the hearth of the communication.
 * It contains an uid to identify uniquely the message.
 * 
 * (Server response example)
 * ex:
 *    0    A                B    C    D    E    F    G    H    I    L
 *    +--- +--------------+ ---+ ---+ ---+ ---+ ---+ ---+ ---+ ---+ -------------------- -----------+
 *    | 4  |      16      |  4 |  4 |  4 |  4 |  4 |  4 |  4 |  4 | E                    G          |
 *    +--- +--------------+ ---+ ---+ ---+ ---+ ---+ ---+ ---+ ---+ -------------------- -----------+
 * 0x 0050 0000000000000522 0015 0001 0001 000A 002C 0006 0036 0030 70726f76612e74787400 706970706f00
 * 
 * 0) MSG_SIZE: 80 (bytes)
 * A) UID: 1314
 * B) TYPE: 21 (MSG_RESP_WITH_FILES)
 * C) STATUS: 1 (RESP_STATUS_OK)
 * D) NUM_FILES: 1
 * E) FILENAME_LEN: 10
 * F) FILENAME_PTR: 0 (RELATIVE TO RAW_DATA_PTR)
 * G) CONTENT_LEN: 6
 * H) CONTENT_PTR: 10 (RELATIVE TO RAW_DATA_PTR)
 * I) RAW_DATA_PTR: 48 (16 + 4 * 8)
 * L) RAW_DATA: 0x 70726f76612e74787400  706970706f00
 *                 ^                     ^
 *                 |                     |
 *                 p r o v a . t x t \0  p i p p o \0
 */
typedef struct {
    UUID_t uid;                  // Unique identifier for message
    SockMessageType_t type;     // Type of message
    union {
        struct {
            RespStatus_t status; // Status
            int numFiles;        // Attached files count
            MsgFile_t *files;    // Files
        } response;              // Response data
        struct {
            int flags;           // Flags
            MsgFile_t file;      // File
        } request;               // Request data
    };
    char* raw_content;           // Raw bytes
} SockMessage_t;

/**
 * Read message from socket. No memory allocation done, message struct contains 
 * pointers to buffer data. Do not free buffer without freeing message.
 * 
 * \param socketfd   : file descriptor of the socket
 * \param buffer     : buffer for storing data
 * \param bufferSize : buffer size
 * \param msg        : ptr to the destination message
 * 
 * \retval -1 : on error (errno set)
 * \retval  0 : on EOF (connection closed)
 * \retval  1 : on success
 */
int readMessage(long socketfd, char* buffer, size_t bufferSize, SockMessage_t* msg);

/**
 * Write message into socket. No memory allocation done, message struct contains 
 * pointers to buffer data. Do not free buffer without freeing message.
 * 
 * \param socketfd   : file descriptor of the socket
 * \param buffer     : buffer for storing data
 * \param bufferSize : buffer size
 * \param msg        : ptr to the source message
 * 
 * \retval -1 : on error (errno set)
 * \retval  0 : on EOF (connection closed)
 * \retval  1 : on success
 */
int writeMessage(long socketfd, char* buffer, size_t bufferSize, SockMessage_t* msg);

/*
 * Correctly handle messages content deallocation.
 */
void freeMessageContent(SockMessage_t* msg);

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

#endif // NET_H
