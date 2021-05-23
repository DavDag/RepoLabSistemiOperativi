#include "session.h"

#include <common.h>

#define NOW() (time(NULL))

// ===============================================================================================================

typedef time_t timestamp_t;

typedef struct {
    int isValid;                                    // Is session valid ?
    timestamp_t creation_time, last_operation_time; // Timestamp for creation date and last operation
    int numFileOpened;                              // Num files opened
    HashValue filenames[MAX_CLIENT_OPENED_FILES];   // Filename         of file i
    int flags[MAX_CLIENT_OPENED_FILES];             // Flags at opening of file i
} ClientSession_t;

// ===============================================================================================================

static ClientSession_t gSessionTable[MAX_CLIENT_COUNT];

// ===============================================================================================================

int __int_ret_updating_ses(ClientSession_t* session, int returnValue);

// ===============================================================================================================

int createSession(ClientID client) {
    // Check if client already has a session
    ClientSession_t* _inn_session = &gSessionTable[client];
    if (_inn_session->isValid)
        return SESSION_ALREADY_EXIST;
    
    // Create session
    _inn_session->isValid             = 1;
    _inn_session->numFileOpened       = 0;
    _inn_session->creation_time       = NOW();
    _inn_session->last_operation_time = NOW();
    for (int i = 0; i < MAX_CLIENT_OPENED_FILES; ++i) {
        _inn_session->filenames[i] = 0;
        _inn_session->flags[i]     = 0;
    }

    // Returns success
    return __int_ret_updating_ses(_inn_session, 0);
}

int getSession(ClientID client, SessionClientID* session) {
    // Check if client already has a session
    ClientSession_t* _inn_session = &gSessionTable[client];
    if (!_inn_session->isValid)
        return SESSION_NOT_EXIST;

    // Pass value
    // TODO:
    *session = client;
    
    // Returns success
    return __int_ret_updating_ses(_inn_session, 0);
}

int destroySession(ClientID client) {
    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[client];
    if (!_inn_session->isValid)
        return SESSION_NOT_EXIST;

    // Update session
    _inn_session->isValid = 0;

    // Returns success
    return __int_ret_updating_ses(_inn_session, 0);
}

int hasOpenedFile(SessionClientID session, SessionFile_t file) {
    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    if (!_inn_session->isValid)
        return SESSION_NOT_EXIST;

    // Search for file
    HashValue key = hash_string(file.name, file.len);
    for (int i = 0; i < _inn_session->numFileOpened; ++i)
        if (_inn_session->filenames[i] == key)
            return __int_ret_updating_ses(_inn_session, SESSION_FILE_ALREADY_OPENED);
    
    // Otherwise file is not opened inside current session
    return __int_ret_updating_ses(_inn_session, SESSION_FILE_NEVER_OPENED);
}

int addFileOpened(SessionClientID session, SessionFile_t file, int flags) {
    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    if (!_inn_session->isValid)
        return SESSION_NOT_EXIST;

    // Ensure file was not opened before
    int status = hasOpenedFile(session, file);
    if (status == 0)
        return SESSION_FILE_ALREADY_OPENED;

    // Check for space
    if (_inn_session->numFileOpened >= MAX_CLIENT_OPENED_FILES)
        return __int_ret_updating_ses(_inn_session, SESSION_OUT_OF_MEMORY);
    
    // Add file
    HashValue key   = hash_string(file.name, file.len);
    const int index = _inn_session->numFileOpened;
    _inn_session->filenames[index] = key;
    _inn_session->flags[index]     = flags;
    _inn_session->numFileOpened++;

    // Returns success
    return __int_ret_updating_ses(_inn_session, 0);
}

int remFileOpened(SessionClientID session, SessionFile_t file) {
    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    if (!_inn_session->isValid)
        return SESSION_NOT_EXIST;
    
    // Rem file
    HashValue key = hash_string(file.name, file.len);
    for (int i = 0; i < _inn_session->numFileOpened; ++i) {
        // Search for file
        if (_inn_session->filenames[i] == key) {
            // Shift entire array 1 position to the left
            for (int j = i; j < _inn_session->numFileOpened - 1; ++j) {
                _inn_session->filenames[j] = _inn_session->filenames[j + 1];
                _inn_session->flags[j]     = _inn_session->flags[j + 1];
            }
            // Reset top element
            _inn_session->filenames[_inn_session->numFileOpened - 1] = 0;
            _inn_session->flags[_inn_session->numFileOpened - 1]     = 0;
            // Decrement total size
            _inn_session->numFileOpened--;
            return __int_ret_updating_ses(_inn_session, 0);
        }
    }

    // Otherwise returns file not opened
    return __int_ret_updating_ses(_inn_session, SESSION_FILE_NEVER_OPENED);
}

int canWriteIntoFile(SessionClientID session, SessionFile_t file) {
    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    if (!_inn_session->isValid)
        return SESSION_NOT_EXIST;
    
    // Ensure file was opened RIGHT before
    HashValue key = hash_string(file.name, file.len);
    for (int i = 0; i < _inn_session->numFileOpened; ++i)
        if (_inn_session->filenames[i] == key)
            return __int_ret_updating_ses(_inn_session, 
                (_inn_session->flags[i] == (FLAG_CREATE | FLAG_LOCK)) ? 0 : SESSION_CANNOT_WRITE_FILE
            );

    // Otherwise returns file not opened
    return __int_ret_updating_ses(_inn_session, SESSION_FILE_NEVER_OPENED);
}

// ===============================================================================================================

int __int_ret_updating_ses(ClientSession_t* session, int returnValue) {
    assert(session);
    session->last_operation_time = NOW();
    return returnValue;
}
