#pragma once

#define DEFAULT_SOCK_FILE "./cs_sock"

// TODO: Make a function to generate 128 bit UUID using Version 4.
// Unique identifier type
typedef unsigned long long UID_t;

/**
 * To be able to send ptr's via socket, they needs to be converted
 * to offsets relative to message's begin.
 */
typedef union { int i; char* ptr; } MsgPtr_t;

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
} SockeMessageType_t;

// Message's response status type
typedef enum {
    RESP_STATUS_NONE  = 0, // Default
    RESP_STATUS_OK    = 1, // Success
    RESP_STATUS_ERROR = 2, // Failure
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
    size_t len;   // Filename's length
    MsgPtr_t abs; // Absolute name str
    MsgPtr_t rel; // Relative name str
} ResourcePath_t;

/**
 * Its the hearth of the communication.
 * It contains an uid to identify uniquely the message.
 * 
 * (Server response example)
 * ex:
 *    A                B    C    D    E    F    G    H    I
 *    +--------------+ ---+ ---+ ---+ ---+ ---+ ---+ ---+ -------------------- -----------+
 *    |      16      |  4 |  4 |  4 |  4 |  4 |  4 |  4 | E                    G          |
 *    +--------------+ ---+ ---+ ---+ ---+ ---+ ---+ ---+ -------------------- -----------+
 * 0x 0000000000000522 0015 0001 0001 000A 002C 0006 0036 70726f76612e74787400 706970706f00
 * 
 * A) UID: 1314
 * B) TYPE: 21 (MSG_RESP_WITH_FILES)
 * C) STATUS: 1 (RESP_STATUS_OK)
 * D) NUM_FILES: 1
 * E) FILENAME_LEN: 10
 * F) FILENAME_PTR: 44 (16 + 7 * (4) + 0)
 * G) CONTENT_LEN: 6
 * H) CONTENT_PTR: 54 (F + E)
 * I) RAW_DATA: 0x 70726f76612e74787400  706970706f00
 *                 ^                     ^
 *                 |                     |
 *                 p r o v a . t x t \0  p i p p o \0
 */
typedef struct {
    UID_t uid;                           // Unique identifier for message
    SockeMessageType_t type;             // Type of message
    union {
        struct {
            RespStatus_t status;         // Status
            int numFiles;                // Attached files count
            struct {
                ResourcePath_t filename; // Filename
                int contentLen;          // Length of content
                MsgPtr_t content;        // Content
            }* files;                    // Files
        } response;                      // Response data
        struct {
            ResourcePath_t filename;     // Filename
            union {
                int flags;               // Flags
                struct {
                    int contentLen;      // Length of content
                    MsgPtr_t content;    // Content
                };
            };
        } request;                       // Request data
    };
    char* raw_content;                   // Raw bytes
} SockMessage_t;
