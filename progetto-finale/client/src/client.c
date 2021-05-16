#include "client.h"

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

// #define LOG_TIMESTAMP
// #define LOG_WITHOUT_COLORS
#define LOG_DEBUG
#include <common.h>
#include <serverapi.h>

// ======================================== DECLARATIONS: Types ================================================

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

    // GetOpt
    OPT_OPTIONAL_ARG       = 15, // "\1" Sended by getopt when parsing optional parameter

    // Custom
    OPT_CHANGE_LOG_LEVEL   = 20, // "-L" Change log level from here on

} CmdLineOptType_t;

typedef struct {
    CmdLineOptType_t type;
    union {
        // void;            // -h, -p
        char* filename;     // -f
        char* save_dirname; // -D, -d
        struct {            // -w
            int max_files;
            char* dirname;
        };
        struct {            // -W, -r, -l, -u, -c
            int file_count;
            char** files;
        };
        int val;            // -R, -t, -L
        char* content;      // optional arguments (sent by getopt with opt '\1')
    };
} CmdLineOpt_t;

// ======================================== DECLARATIONS: Inner functions ============================================

int parseParam(int, int, char*);
int hasPriority(CmdLineOptType_t);
int handleOption(int);
int freeOption(int);

// ======================================= DEFINITIONS: Global vars ================================================

static const char* socket_file = NULL;
static int is_extended_log_enabled = 0;
static CmdLineOpt_t options[MAX_OPTIONS_COUNT];
static int optionsSize = 0;
static const char* const CLIENT_USAGE =
"Client usage\n"
"  -h                 stampa la lista di tutte le opzioni accettate dal client e \n"
"                     termina immediatamente.\n\n"
"  -p                 abilita le stampe sullo standard output per ogni operazione.\n"
"  -f filename        specifica il nome del socket AF_UNIX a cui connettersi.\n"
"  -t time            tempo in millisecondi che intercorre tra l'invio di due richieste\n"
"                     successive al server.\n\n"
"  -w dirname[,n=0]   invia al server i file nella cartella 'dirname' ricorsivamente\n"
"                     (al massimo n file).\n"
"  -W file1[,file2    lista di nomi di file da scrivere nel server separati da ','.\n"
"  -D dirname         cartella in memoria secondaria dove vengono scritti (lato client)\n"
"                     i file che il server restituisce con l'opzione '-w' o '-W'.\n"
"                     Il server ritorna i file quando, in seguito ad una richiesta di\n"
"                     scrittura, viene rimpiazzato un file (capacity misses).\n\n"
"  -r file1[,file2]   lista di nomi di file da leggere dal server separati da ','.\n"
"  -R [n=0]           tale opzione permette di leggere 'n' file qualsiasi attualmente\n"
"                     memorizzati nel server; se n=0 vengono letti tutti i file nel\n"
"                     server.\n"
"  -d dirname         cartella in memoria secondaria dove scrivere i file letti dal\n"
"                     server con l'opzione '-r' o '-R'.\n\n"
"  -l file1[,file2]   lista di nomi di file su cui acquisire la mutua esclusione.\n"
"  -u file1[,file2]   lista di nomi di file su cui rilasciare la mutua esclusione.\n"
"  -c file1[,file2]   lista di file da rimuovere dal server se presenti.\n\n"
;

// ======================================= DEFINITIONS: client.h functions ================================================

int initializeClient() {
    // Initialize global state
    LOG_VERB("Initializing client state...");
    socket_file = DEFAULT_SOCK_FILE;
    is_extended_log_enabled = 0;
    optionsSize = 0;
    return RES_OK;
}

int parseArguments(int argc, char** argv) {
    // Start parsing arguments
    LOG_VERB("Parsing arguments...");
    int opt = -1;
    while((opt = getopt(argc, argv, "-hpf:w:W:D:r:R::d:t:l:u:c:" "L:")) != -1) {
        // Check for max options count
        if (optionsSize == MAX_OPTIONS_COUNT) {
            // In case it reaches maximum options, it returns RES_OK to continue execution
            LOG_WARN("Max options count too low. Maximum reached: %d", optionsSize);
            return RES_OK;
        }

        // Try parsing argument
        if (parseParam(optionsSize, opt, optarg) != RES_OK) {
            LOG_CRIT("Client process terminated while parsing arguments");
            return RES_ERROR;
        }

        // Handle requests that have priority
        (hasPriority(options[optionsSize].type)) ? handleOption(optionsSize) : optionsSize++;
    }
    return RES_OK;
}

int handleOptions() {
    // Open connection
    const struct timespec abstime = { .tv_sec = 1, .tv_nsec = 0 };
    if (openConnection(socket_file, 250, abstime) != RES_OK) {
        LOG_ERRNO("Error opening connection");
        return RES_ERROR;
    }
    // Start handling options
    LOG_VERB("Handling requests...");
    for (int i = 0; i < optionsSize; ++i) {
        // Try handling option
        if (handleOption(i) != RES_OK) {
            LOG_CRIT("Client process terminated while handling arguments");
            return RES_ERROR;
        }
    }
    // Close connection
    if (closeConnection(socket_file) != RES_OK) {
        LOG_ERRNO("Error closing connection");
        return RES_ERROR;
    }
    return RES_OK;
}

int terminateClient() {
    // Free options content
    LOG_VERB("Terminating client state...");
    for (int i = 0; i < optionsSize; ++i) {
        // Try freeing option
        if (freeOption(i) != RES_OK) {
            LOG_ERRO("Error freeing option. Termination resumed ...");
        }
    }
    return RES_OK;
}

// ======================================= DEFINITIONS: Inner functions ================================================

int parseParam(int index, int opt, char* value) {
    CmdLineOpt_t* option = &options[index];

    // Fill memory with zeros
    memset(option, 0, sizeof(CmdLineOpt_t));

    LOG_VERB("Option %c with args %s", opt, value);

    // Check option
    switch (opt)
    {
        case 'h':
        case 'p':
        {
            option->type = (opt == 'h') ? OPT_HELP_ENABLED : option->type;
            option->type = (opt == 'p') ?  OPT_LOG_ENABLED : option->type;
            break;
        }
    
        case 'f':
        {
            option->type = OPT_SOCKET_FILE;
            option->filename = value;
            break;
        }
    
        case 'D':
        case 'd':
        {
            option->type = (opt == 'D') ? OPT_WRITE_SAVE : option->type;
            option->type = (opt == 'd') ?  OPT_READ_SAVE : option->type;
            option->save_dirname = value;
            break;
        }
    
        case 'w':
        {
            option->type = OPT_WRITE_DIR_REQ;

            // Check for n argument
            int val = INT_MAX;
            const char* pos = strchr(value, ',');
            if (pos != NULL) {
                // Check for ',n='
                if (strlen(pos) < 4 || pos[1] != 'n' || pos[2] != '=') {
                    LOG_CRIT("Error while parsing number. Correct syntax is ',n=' (ex.',n=12')");
                    return RES_ERROR;
                }

                // Parse integer
                if ((val = parse_positive_integer(pos + 3)) < 0) {
                    LOG_CRIT("Error while parsing number");
                    return RES_ERROR;
                }

                // Replace ',' with '\0' to obtain a zero-terminated-string out of the same buffer
                value[(pos - value)] = '\0';
            }

            // Set values
            option->max_files = val;
            option->dirname = value;
            break;
        }
        
        case 'W':
        case 'r':
        case 'l':
        case 'u':
        case 'c':
        {
            option->type = (opt == 'W') ?  OPT_WRITE_FILE_REQ : option->type;
            option->type = (opt == 'r') ?   OPT_READ_FILE_REQ : option->type;
            option->type = (opt == 'l') ?   OPT_LOCK_FILE_REQ : option->type;
            option->type = (opt == 'u') ? OPT_UNLOCK_FILE_REQ : option->type;
            option->type = (opt == 'c') ? OPT_REMOVE_FILE_REQ : option->type;

            // Parse filenames file1[,file2,file3]
            int valueLen = strlen(value), numFiles = 1, lastFileIndex = 0;
            for (int i = 0; i < valueLen; ++i) if (value[i] == ',') ++numFiles; // Count files (1 + num of ',')
            char** files = mem_malloc(sizeof(char*) * numFiles);                // Create ptr array
            for (int i = 0, j = 0; i < valueLen && j < numFiles; ++i)
                if (value[i] == ',') {
                    value[i] = '\0';                    // Replace ',' with '\0' to obtain 0-terminated-strings out of the same buffers
                    files[j++] = &value[lastFileIndex]; // Insert ptr to the j-th string. 
                    lastFileIndex = i + 1;
                }
            files[numFiles - 1] = &value[lastFileIndex];
            
            // Set values
            option->file_count = numFiles;
            option->files = files;
            break;
        }

        case 'L':
        case 't':
        {
            option->type = (opt == 'L') ? OPT_CHANGE_LOG_LEVEL : option->type;
            option->type = (opt == 't') ?             OPT_WAIT : option->type;

            // Parse param
            int val = -1;
            if ((val = parse_positive_integer(value)) < 0) {
                LOG_CRIT("Error while parsing number");
                return RES_ERROR;
            }

            // Set values
            option->val = val;
            break;
        }

        case 'R':
        {
            option->type = OPT_READ_RAND_REQ;
            option->val = 0; // Default value for -R without 'n' argument
            break;
        }

        case '\1':
        {
            option->type = OPT_OPTIONAL_ARG;
            option->content = value;
            break;
        }

        default:
            LOG_CRIT("Unable to parse option %c", opt);
            return RES_ERROR;
    }
    return RES_OK;
}

int hasPriority(CmdLineOptType_t type) {
    return (type == OPT_HELP_ENABLED || type ==  OPT_LOG_ENABLED || type ==  OPT_SOCKET_FILE || type == OPT_OPTIONAL_ARG);
}

int handleOptArgument(int index) {
    const CmdLineOpt_t option = options[index];

    // Can access position without checking because its guaranteed from getopt that \ 1 always follows another option
    CmdLineOpt_t lastOption = options[index - 1];

    // Check type of lastOption
    int val = 0;
    switch (lastOption.type)
    {
        case OPT_READ_RAND_REQ:
        {
            // Check for n argument
            if (option.content != NULL) {
                // Check for 'n='
                if (strlen(option.content) < 3 || option.content[0] != 'n' || option.content[1] != '=') {
                    LOG_CRIT("Error while parsing number. Correct syntax is 'n=' (ex.'n=12')");
                    return RES_ERROR;
                }
                
                // Parse integer
                if ((val = parse_positive_integer(option.content + 2)) < 0) {
                    LOG_CRIT("Error while parsing number");
                    return RES_ERROR;
                }
                
                // Set values
                lastOption.val = val;
            } else {
                // Should never happen
                LOG_CRIT("Empty optional argument");
                return RES_ERROR;
            }
            break;
        }
        
        default:
            // Should never happen
            LOG_CRIT("Optional argument found without option before that handles it");
            return RES_ERROR;
    }

    LOG_VERB("Handled optional argument %s", option.content);
    return RES_OK;
}

int handleOption(int index) {
    const CmdLineOpt_t option = options[index];
    switch (option.type)
    {
        case OPT_HELP_ENABLED:
        {
            LOG_EMPTY("%s", CLIENT_USAGE);
            LOG_VERB("Client process terminated. Usage printed");
            terminateClient();
            exit(EXIT_SUCCESS);
            break;
        }

        case OPT_SOCKET_FILE:
        {
            socket_file = option.filename;

            // Log operation data
            if (is_extended_log_enabled)
                LOG_INFO("{-f} Socket file changed into '%s'", option.filename);

            LOG_VERB("Socket filename updated");
            break;
        }

        case OPT_LOG_ENABLED:
        {
            is_extended_log_enabled = 1;
            LOG_INFO("Enabled per-operation logging");
            break;
        }
        
        case OPT_WAIT:
        {
            usleep(option.val);
            
            // Log operation data
            if (is_extended_log_enabled)
                LOG_INFO("{-t} Waited %dms", option.val);

            LOG_VERB("Process resumed");
            break;
        }
            
        case OPT_OPTIONAL_ARG:
            return handleOptArgument(index);

        case OPT_CHANGE_LOG_LEVEL:
        {
            if (option.val < LOG_LEVEL_CRITICAL || option.val > LOG_LEVEL_VERBOSE) {
                LOG_CRIT("Log level value error. %d is not between 0 and 4", option.val);
                return RES_ERROR;
            }
            set_log_level(option.val);
            LOG_VERB("Logging level updated into #%d", option.val);
            break;
        }

        case OPT_READ_RAND_REQ:
        {
            int status = SERVER_API_SUCCESS;
            char* dirname = NULL;

            // Check if next option is '-d'
            if ((index < optionsSize - 1) && (options[index + 1].type == OPT_READ_SAVE))
                dirname = options[index + 1].save_dirname;
            
            // Try read N files from server
            if ((status = readNFiles(option.val, dirname)) == SERVER_API_FAILURE) {
                LOG_ERRNO("Error reading files from server");
            }

            // Log operation data
            if (is_extended_log_enabled)
                LOG_INFO("{-R} numFiles: %d, dirname: '%s' > %s ! Bytes wrote %llu",
                    option.val, dirname, (status == SERVER_API_SUCCESS) ? "SUCCEDED" : "FAILED", 0L);

            LOG_VERB("Files read from server");
            break;
        }

        /**
         * Explains code for:
         *    OPT_WRITE_FILE_REQ
         *    OPT_READ_FILE_REQ
         *    OPT_LOCK_FILE_REQ
         *    OPT_UNLOCK_FILE_REQ
         *    OPT_REMOVE_FILE_REQ
         *    OPT_WRITE_DIR_REQ
         * 
         * (1)
         * Try opening file (always update status):
         *    -OnError   : Log and go to (4)
         *    -OnSuccess : Continue (2)
         * 
         * (2)
         * Try request (always update status):
         *    -OnError   : Log and continue (3)
         *    -OnSuccess : Continue (3)
         * 
         * (3)
         * Try closing file (update status ONLY on error):
         *    -OnError   : Log, update status and continue (4)
         *    -OnSuccess : Continue (4)
         * 
         * (4)
         * if '-p' is enabled, log requesta infos and status.
         * 
         * Requests can fail at any point (1/2/3) but when a file its open,
         * close request must be done and its response must not override
         * the one from point (2).
         */

        case OPT_WRITE_FILE_REQ:
        {
            char* dirname = NULL;

            // Check if next option is '-D'
            if ((index < optionsSize - 1) && (options[index + 1].type == OPT_WRITE_SAVE))
                dirname = options[index + 1].save_dirname;

            // Start sending files
            for (int i = 0; i < option.file_count; ++i) {
                int status = SERVER_API_SUCCESS;
                char* pathname = option.files[i];

                // [1]
                if ((status = openFile(pathname, FLAG_CREATE | FLAG_LOCK)) == SERVER_API_SUCCESS) {
                    // [2]
                    if ((status = writeFile(pathname, dirname)) == SERVER_API_FAILURE)
                        LOG_ERRNO("Error writing file '%s'", pathname);
                    // [3]
                    if (closeFile(pathname) == SERVER_API_FAILURE) {
                        LOG_ERRNO("Error closing file '%s'", pathname);
                        status = SERVER_API_FAILURE;
                    }
                } else {
                    LOG_ERRNO("Error opening file '%s'", pathname);
                }

                // [4]
                // Log operation data
                if (is_extended_log_enabled)
                    LOG_INFO("{-W} file: '%s', dirname: '%s' > %s ! Bytes wrote %llu",
                        pathname, dirname, (status == SERVER_API_SUCCESS) ? "SUCCEDED" : "FAILED", 0L);
            }
            break;
        }

        case OPT_READ_FILE_REQ:
        {
            char* dirname = NULL;

            // Check if next option is '-d'
            if ((index < optionsSize - 1) && (options[index + 1].type == OPT_READ_SAVE))
                dirname = options[index + 1].save_dirname;

            // Start requesting files
            for (int i = 0; i < option.file_count; ++i) {
                int status = SERVER_API_SUCCESS;
                char* pathname = option.files[i];
                void* buff = NULL;
                size_t buffSize = 0L;

                // [1]
                if ((status = openFile(pathname, FLAG_EMPTY)) == SERVER_API_SUCCESS) {
                    // [2]
                    if ((status = readFile(pathname, &buff, &buffSize)) == SERVER_API_FAILURE)
                        LOG_ERRNO("Error reading file '%s'", pathname);
                    // [3]
                    if (closeFile(pathname) == SERVER_API_FAILURE) {
                        LOG_ERRNO("Error closing file '%s'", pathname);
                        status = SERVER_API_FAILURE;
                    }
                } else {
                    LOG_ERRNO("Error opening file '%s'", pathname);
                }

                // [4]
                // Log operation data
                if (is_extended_log_enabled)
                    LOG_INFO("{-r} file: '%s', dirname: '%s' > %s ! Bytes read %llu",
                        pathname, dirname, (status == SERVER_API_SUCCESS) ? "SUCCEDED" : "FAILED", 0L);
            }
            LOG_VERB("Files read from server");
            break;
        }

        case OPT_LOCK_FILE_REQ:
        {
            // Start locking files
            for (int i = 0; i < option.file_count; ++i) {
                int status = SERVER_API_SUCCESS;
                char* pathname = option.files[i];

                // [1]
                if ((status = openFile(pathname, FLAG_EMPTY)) == SERVER_API_SUCCESS) {
                    // [2]
                    if ((status = lockFile(pathname)) == SERVER_API_FAILURE)
                        LOG_ERRNO("Error locking file '%s'", pathname);
                    // [3]
                    if (closeFile(pathname) == SERVER_API_FAILURE) {
                        LOG_ERRNO("Error closing file '%s'", pathname);
                        status = SERVER_API_FAILURE;
                    }
                } else {
                    LOG_ERRNO("Error opening file '%s'", pathname);
                }

                // [4]
                // Log operation data
                if (is_extended_log_enabled)
                    LOG_INFO("{-l} file: '%s' > %s !", pathname, (status == SERVER_API_SUCCESS) ? "SUCCEDED" : "FAILED");
            }
            LOG_VERB("Files locked");
            break;
        }

        case OPT_UNLOCK_FILE_REQ:
        {
            // Start unlocking files
            for (int i = 0; i < option.file_count; ++i) {
                int status = SERVER_API_SUCCESS;
                char* pathname = option.files[i];

                // [1]
                if ((status = openFile(pathname, FLAG_EMPTY)) == SERVER_API_SUCCESS) {
                    // [2]
                    if ((status = unlockFile(pathname)) == SERVER_API_FAILURE)
                        LOG_ERRNO("Error locking file '%s'", pathname);
                    // [3]
                    if (closeFile(pathname) == SERVER_API_FAILURE) {
                        LOG_ERRNO("Error closing file '%s'", pathname);
                        status = SERVER_API_FAILURE;
                    }
                } else {
                    LOG_ERRNO("Error opening file '%s'", pathname);
                }

                // [4]
                // Log operation data
                if (is_extended_log_enabled)
                    LOG_INFO("{-u} file: '%s' > %s !", pathname, (status == SERVER_API_SUCCESS) ? "SUCCEDED" : "FAILED");
            }
            LOG_VERB("Files unlocked");
            break;
        }

        case OPT_REMOVE_FILE_REQ:
        {
            // Start removing files
            for (int i = 0; i < option.file_count; ++i) {
                int status = SERVER_API_SUCCESS;
                char* pathname = option.files[i];

                // [1]
                if ((status = openFile(pathname, FLAG_LOCK)) == SERVER_API_SUCCESS) {
                    // [2]
                    if ((status = removeFile(pathname)) == SERVER_API_FAILURE)
                        LOG_ERRNO("Error removing file '%s'", pathname);
                    // [3]
                    if (closeFile(pathname) == SERVER_API_FAILURE) {
                        LOG_ERRNO("Error closing file '%s'", pathname);
                        status = SERVER_API_FAILURE;
                    }
                } else {
                    LOG_ERRNO("Error opening file '%s'", pathname);
                }

                // [4]
                // Log operation data
                if (is_extended_log_enabled)
                    LOG_INFO("{-c} file: '%s' > %s !", pathname, (status == SERVER_API_SUCCESS) ? "SUCCEDED" : "FAILED");
            }
            LOG_VERB("Files removed from server");
            break;
        }

        case OPT_WRITE_DIR_REQ:
        {
            // TODO: use ntfw()
            // TODO: Log operation data
            LOG_VERB("Directory sent to server", option.dirname);
            break;
        }

        case OPT_READ_SAVE:
        case OPT_WRITE_SAVE:
            // Nothings to do
            break;

        default:
            LOG_CRIT("Unable to handle option %d", option.type);
            return RES_ERROR;
    }
    return RES_OK;
}

int freeOption(int index) {
    const CmdLineOpt_t option = options[index];
    switch (option.type)
    {
        case OPT_HELP_ENABLED:
        case OPT_LOG_ENABLED:
        case OPT_SOCKET_FILE:
        case OPT_WRITE_DIR_REQ:
        case OPT_WRITE_SAVE:
        case OPT_READ_RAND_REQ:
        case OPT_READ_SAVE:
        case OPT_WAIT:
        case OPT_OPTIONAL_ARG:
        case OPT_CHANGE_LOG_LEVEL:
            break;
        
        case OPT_WRITE_FILE_REQ:
        case OPT_READ_FILE_REQ:
        case OPT_LOCK_FILE_REQ:
        case OPT_UNLOCK_FILE_REQ:
        case OPT_REMOVE_FILE_REQ:
            free(option.files);
            break;
        
        default:
            LOG_ERRO("Unable to free option %d", option.type);
            return RES_ERROR;
    }
    return RES_OK;
}
