#pragma once

#ifndef SESSION_H
#define SESSION_H

#include <time.h>

#include <common.h>

#define MAX_CLIENT_OPENED_FILES 16
#define MAX_CLIENT_COUNT 4096

#define SESSION_ALREADY_EXIST       201
#define SESSION_NOT_EXIST           202
#define SESSION_FILE_ALREADY_OPENED 203
#define SESSION_FILE_NEVER_OPENED   204
#define SESSION_OUT_OF_MEMORY       205
#define SESSION_CANNOT_WRITE_FILE   206

typedef struct { const char* name; int len; } SessionFile_t;

typedef time_t timestamp_t;

typedef struct {
    pthread_mutex_t mutex;                          // Ensure only 1 worker at time
    int isValid;                                    // Is session valid ?
    timestamp_t creation_time, last_operation_time; // Timestamp for creation date and last operation
    int numFileOpened;                              // Num files opened
    HashValue filenames[MAX_CLIENT_OPENED_FILES];   // Filename         of file i
    int flags[MAX_CLIENT_OPENED_FILES];             // Flags at opening of file i
} ClientSession_t;

/**
 * Initialize session system
 * 
 * \retval  0
 */
void initSessionSystem();

/**
 * Terminate session system
 * 
 * \retval  0
 */
void terminateSessionSystem();

/**
 * Create new session for client.
 * 
 * \param client: destination client
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_ALREADY_EXIST ]
 */
int createSession(int client);

/**
 * Retrieve current active session for client.
 * 
 * \param client : destination client
 * \param session: ptr where to store result
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_NOT_EXIST ]
 */
int getSession(int client, int* session);

/**
 * To retrieve internal data of the current user session.
 * 
 * \param client : session's owner
 * \param session: where to store the ptr to the internal session
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_NOT_EXIST ]
 */
int getRawSession(int client, ClientSession_t** session);

/**
 * Destroy session.
 * 
 * \param client: destination client
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_NOT_EXIST ]
 */
int destroySession(int client);

/**
 * Check if file is opened inside session.
 * 
 * \param session: session to check
 * \param file   : file to check
 * 
 * \retval SESSION_FILE_ALREADY_OPENED: when file is opened in current session
 * \retval SESSION_FILE_NEVER_OPENED  : when file is not opened in current session
 * \retval SESSION_NOT_EXIST          : on error
 */
int hasOpenedFile(int session, SessionFile_t file);

/**
 * Add file inside opened list for session.
 * 
 * \param session: destination session
 * \param file   : file to add
 * \param flags  : flags (CREATE, LOCK, etc...)
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_NOT_EXIST, SESSION_FILE_ALREADY_OPENED, SESSION_OUT_OF_MEMORY ]
 */
int addFileOpened(int session, SessionFile_t file, int flags);

/**
 * Remove file from opened list for session.
 * 
 * \param session: destination session
 * \param file   : file to remove
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_NOT_EXIST, SESSION_FILE_NEVER_OPENED ]
 */
int remFileOpened(int session, SessionFile_t file);

/**
 * Test if can write into file.
 * 
 * \param session: session to check
 * \param file   : file to check
 * 
 * \retval  0: on success
 * \retval >0: on error. possible values: [ SESSION_NOT_EXIST, SESSION_FILE_NEVER_OPENED, SESSION_CANNOT_WRITE_FILE ]
 */
int canWriteIntoFile(int session, SessionFile_t file);

#endif // SESSION_H
