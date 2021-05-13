#pragma once

#include <common.h>

typedef enum {
    OPT_NONE               =  0, // Default
    
    // Requested
    OPT_HELP_ENABLED       =  1, // "-h" Print usage (help).
    OPT_SOCKET_FILE        =  2, // "-f" Socket file path.
    OPT_WRITE_DIR_REQ      =  3, // "-w" Send directory to server.
    OPT_WRITE_FILE_REQ     =  4, // "-W" Send file/s to server.
    OPT_WRITE_SAVE         =  5, // "-D" Where to save file/s received from server after write request.
    OPT_READ_FILE_REQ      =  6, // "-r" Read file/s from server.
    OPT_READ_RAND_REQ      =  7, // "-R" Read random file/s from server.
    OPT_READ_SAVE          =  8, // "-d" Where to save file/s received from server after read request.
    OPT_WAIT               =  9, // "-t" Time to wait between requests.
    OPT_LOCK_FILE_REQ      = 10, // "-l" Send file/s lock request to server.
    OPT_UNLOCK_FILE_REQ    = 11, // "-u" Send file/s unlock request to server.
    OPT_REMOVE_FILE_REQ    = 12, // "-c" Remove file/s from server.
    OPT_LOG_ENABLED        = 13, // "-p" Enable debug output.

    // Custom
    OPT_CHANGE_LOG_LEVEL   = 20, // "-L" Change log level from here on

} CmdLineOptType_t;

typedef struct {
    CmdLineOptType_t type;
    union {
        // -h, -p
        // void

        // -f
        const char* filename;

        // -D, -d
        const char* save_dirname;

        // -w
        struct {
            int max_files;
            const char* dirname;
        };

        // -W, -r, -l, -u, -c
        struct {
            int file_count;
            const char** files;
        };

        // -R, -t, -L
        int val;
    };
} CmdLineOpt_t;

/*
 * Set option data based on type param 'opt' and parsing values from value.
 */
int parse_param_option(CmdLineOpt_t* option, int opt, char* value);

/*
 * Check if option can be executed before starting serving requests.
 */
int can_handle_option_before_start(CmdLineOptType_t type);

/*
 * Handle an option.
 * Requires next option to check for -r -d and -w -d.
 */
int handle_option(CmdLineOpt_t option, CmdLineOpt_t* nextOpt);

/*
 * Release memory allocated for options array
 */
void free_options(CmdLineOpt_t* options, size_t size);
