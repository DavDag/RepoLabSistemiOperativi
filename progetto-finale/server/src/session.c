#include "session.h"

#include <logger.h>
#include <common.h>

#define NOW() (time(NULL))

// ===============================================================================================================

static ClientSession_t gSessionTable[MAX_CLIENT_COUNT];

// ===============================================================================================================

void _intUpdateSession(ClientSession_t* session);

// ===============================================================================================================

void initSessionSystem() {
    for (int i = 0; i < MAX_CLIENT_COUNT; ++i) {
        if (pthread_mutex_init(&gSessionTable[i].mutex, NULL) < 0) {
            LOG_ERRNO("Error creating mutex");
        }
    }
}

void terminateSessionSystem() {
    for (int i = 0; i < MAX_CLIENT_COUNT; ++i) {
        if (pthread_mutex_destroy(&gSessionTable[i].mutex) < 0) {
            LOG_ERRNO("Error destroying mutex");
        }
    }
}

int createSession(int client) {
    int res = 0;

    // Check if client already has a session
    ClientSession_t* _inn_session = &gSessionTable[client];
    lock_mutex(&_inn_session->mutex);
    if (_inn_session->isValid)
        res = SESSION_ALREADY_EXIST;
    
    if (res != SESSION_ALREADY_EXIST) {
        // Create session
        _inn_session->isValid             = 1;
        _inn_session->numFileOpened       = 0;
        _inn_session->creation_time       = NOW();
        _inn_session->last_operation_time = NOW();
        for (int i = 0; i < MAX_CLIENT_OPENED_FILES; ++i) {
            _inn_session->filenames[i] = 0;
            _inn_session->flags[i]     = 0;
        }
    }

    // Returns success
    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

int getSession(int client, int* session) {
    int res = 0;

    // Check if client already has a session
    ClientSession_t* _inn_session = &gSessionTable[client];
    lock_mutex(&_inn_session->mutex);
    if (!_inn_session->isValid)
        res = SESSION_NOT_EXIST;

    if (res != SESSION_NOT_EXIST) {
        // Pass value
        *session = client;
    }
    
    // Returns success
    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

int getRawSession(int client, ClientSession_t** session) {
    int res = 0;

    // Check if client already has a session
    ClientSession_t* _inn_session = &gSessionTable[client];
    lock_mutex(&_inn_session->mutex);
    if (!_inn_session->isValid)
        res = SESSION_NOT_EXIST;
    
    if (res != SESSION_NOT_EXIST) {
        // Pass value
        *session = _inn_session;
    }

    // Returns success
    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

int destroySession(int client) {
    int res = 0;

    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[client];
    lock_mutex(&_inn_session->mutex);
    if (!_inn_session->isValid)
        res = SESSION_NOT_EXIST;

    if (res != SESSION_NOT_EXIST) {
        // Update session
        _inn_session->isValid             = 0;
        _inn_session->numFileOpened       = 0;
        _inn_session->creation_time       = 0;
        _inn_session->last_operation_time = 0;
    }

    unlock_mutex(&_inn_session->mutex);

    // Returns success
    return res;
}

int _intHasOpenedFile(ClientSession_t* session, HashValue file) {
    if (!session->isValid)
        return SESSION_NOT_EXIST;

    // Search for file
    for (int i = 0; i < session->numFileOpened; ++i)
        if (session->filenames[i] == file)
            return SESSION_FILE_ALREADY_OPENED;

    // Otherwise
    return SESSION_FILE_NEVER_OPENED;
}

int hasOpenedFile(int session, SessionFile_t file) {
    int res = 0;

    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    lock_mutex(&_inn_session->mutex);
    
    HashValue key = hash_string(file.name, file.len);
    res = _intHasOpenedFile(_inn_session, key);

    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

int addFileOpened(int session, SessionFile_t file, int flags) {
    int res = 0;

    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    lock_mutex(&_inn_session->mutex);
    if (!_inn_session->isValid)
        res = SESSION_NOT_EXIST;

    if (res != SESSION_NOT_EXIST) {
        // Ensure file was not opened before
        HashValue key = hash_string(file.name, file.len);
        res = _intHasOpenedFile(_inn_session, key);
        if (res == SESSION_FILE_NEVER_OPENED)
            res = 0;
            
        // Check for space
        if (res != SESSION_FILE_ALREADY_OPENED && _inn_session->numFileOpened >= MAX_CLIENT_OPENED_FILES)
            res = SESSION_OUT_OF_MEMORY;

        if (res != SESSION_FILE_ALREADY_OPENED && res != SESSION_OUT_OF_MEMORY) {
            // Add file
            HashValue key   = hash_string(file.name, file.len);
            const int index = _inn_session->numFileOpened;
            _inn_session->filenames[index] = key;
            _inn_session->flags[index]     = flags;
            _inn_session->numFileOpened++;
        }
    }

    // Returns success
    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

int remFileOpened(int session, SessionFile_t file) {
    int res = 0;

    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    lock_mutex(&_inn_session->mutex);
    if (!_inn_session->isValid)
        res = SESSION_NOT_EXIST;
    
    if (res != SESSION_NOT_EXIST) {
        // Rem file
        HashValue key = hash_string(file.name, file.len);
        res = SESSION_FILE_NEVER_OPENED;
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
                res = 0;
                break;
            }
        }
    }

    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

int canWriteIntoFile(int session, SessionFile_t file) {
    int res = 0;

    // Retrieve session
    ClientSession_t* _inn_session = &gSessionTable[session];
    lock_mutex(&_inn_session->mutex);
    if (!_inn_session->isValid)
        res = SESSION_NOT_EXIST;
    
    if (res != SESSION_NOT_EXIST) {
        // Ensure file was opened RIGHT before
        HashValue key = hash_string(file.name, file.len);
        res = SESSION_FILE_NEVER_OPENED;
        for (int i = 0; i < _inn_session->numFileOpened; ++i)
            if (_inn_session->filenames[i] == key)
                res = (_inn_session->flags[i] == (FLAG_CREATE | FLAG_LOCK)) ? 0 : SESSION_CANNOT_WRITE_FILE;
    }

    _intUpdateSession(_inn_session);
    unlock_mutex(&_inn_session->mutex);
    return res;
}

// ===============================================================================================================

void _intUpdateSession(ClientSession_t* session) {
    session->last_operation_time = NOW();
}
