#include "client.h"
#include <string.h>

static const char* socket_file = DEFAULT_SOCK_FILE;
static int is_extended_log_enabled = 0;

static const char* const CLIENT_USAGE =
"Client usage\n"
"-h                 stampa la lista di tutte le opzioni accettate dal client e termina immediatamente.\n"
"-f filename        specifica il nome del socket AF_UNIX a cui connettersi.\n"
"-w dirname[,n=0]   invia al server i file nella cartella 'dirname' ricorsivamente (al massimo n file).\n"
"-W file1[,file2    lista di nomi di file da scrivere nel server separati da ','.\n"
"-D dirname         cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove a seguito di capacity misses.\n"
"-r file1[,file2]   lista di nomi di file da leggere dal server separati da ','.\n"
"-R [n=0]           tale opzione permette di leggere 'n' file qualsiasi attualmente memorizzati nel server; se n=0 vengono letti tutti i file nel server.\n"
"-d dirname         cartella in memoria secondaria dove scrivere i file letti dal server con l'opzione '-r' o '-R'.\n"
"-t time            tempo in millisecondi che intercorre tra l'invio di due richieste successive al server.\n"
"-l file1[,file2]   lista di nomi di file su cui acquisire la mutua esclusione.\n"
"-u file1[,file2]   lista di nomi di file su cui rilasciare la mutua esclusione.\n"
"-c file1[,file2]   lista di file da rimuovere dal server se presenti.\n"
"-p                 abilita le stampe sullo standard output per ogni operazione.\n"
;

int parse_param_option(CmdLineOpt_t* option, int opt, char* value) {
    // Fill memory with zeros
    memset(option, 0, sizeof(CmdLineOpt_t));

    LOG_VERB("%s", value);

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
            int val = 0;
            const char* pos = strchr(value, ',');
            if (pos != NULL) {
                // Check for ',n='
                if (strlen(pos) < 4 || pos[1] != 'n' || pos[2] != '=') {
                    LOG_ERRO("Error while parsing number. Correct syntax is ',n=' (ex.',n=12')");
                    return RES_ERROR;
                }

                // Parse integer
                val = parse_positive_integer(pos + 3);
                if (val < 0) {
                    LOG_ERRO("Error while parsing number");
                    return RES_ERROR;
                }

                // Replace ',' with '\0' to obtain a zero-terminated-string out of the same buffer
                value[(pos - value)] = '\0';
            } else {
                LOG_VERB("Default value used (0) for n while parsing -w option");
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
            for (int i = 0; i < valueLen; ++i) if (value[i] == ',') ++numFiles;       // Count files (1 + num of ',')
            const char** files = (const char**) mem_malloc(sizeof(char*) * numFiles); // Create ptr array
            for (int i = 0, j = 0; i < valueLen && j < numFiles; ++i)
                if (value[i] == ',') {
                    value[i] = '\0';                    // Replace ',' with '\0' to obtain 0-terminated-strings out of the same buffers
                    files[j++] = &value[lastFileIndex]; // Insert ptr to the j-th string. 
                    lastFileIndex = i + 1;
                }
            
            // Set values
            option->file_count = numFiles;
            option->files = files;
            break;
        }
        
        case 'R':
        {
            option->type = OPT_READ_RAND_REQ;

            // Check for n argument
            int val = 0;
            if (value != NULL) {
                // Check for 'n='
                if (strlen(value) < 3 || value[0] != 'n' || value[1] != '=') {
                    LOG_ERRO("Error while parsing number. Correct syntax is 'n=' (ex.'n=12')");
                    return RES_ERROR;
                }
                
                // Parse integer
                val = parse_positive_integer(value + 2);
                if (val < 0) {
                    LOG_ERRO("Error while parsing number");
                    return RES_ERROR;
                }
            } else {
                LOG_VERB("Default value used (0) for n while parsing -R option");
            }

            // Set values
            option->val = val;
            break;
        }

        case 't':
        {
            option->type = OPT_WAIT;

            // Parse param
            int val = parse_positive_integer(value);
            if (val < 0) {
                LOG_ERRO("Error while parsing number");
                return RES_ERROR;
            }

            // Set values
            option->val = val;
            break;
        }

        case 'L':
        {
            option->type = OPT_CHANGE_LOG_LEVEL;

            // Parse integer
            int val = parse_positive_integer(value);
            if (val < 0) {
                LOG_ERRO("Error while parsing number");
                return RES_ERROR;
            }

            // Set values
            option->val = val;
            break;
        }
        
        default:
            LOG_ERRO("Unable to parse option %c", opt);
            return RES_ERROR;
    }
    return RES_OK;
}

int can_handle_option_before_start(CmdLineOptType_t type) {
    return (type == OPT_HELP_ENABLED || type ==  OPT_LOG_ENABLED || type ==  OPT_SOCKET_FILE);
}

int handle_option(CmdLineOpt_t option, CmdLineOpt_t* nextOpt) {
    switch (option.type)
    {
        case OPT_HELP_ENABLED:
        {
            printf("%s", CLIENT_USAGE);
            LOG_VERB("Client process terminated. Usage printed");
            exit(EXIT_SUCCESS);
            break;
        }

        case OPT_SOCKET_FILE:
        {
            socket_file = option.filename;
            LOG_VERB("Socket filename updated into: %s", option.filename);
            break;
        }

        case OPT_WRITE_DIR_REQ:
        {
            LOG_VERB("Directory sent to server", option.dirname);
            break;
        }

        case OPT_WRITE_FILE_REQ:
        {

            LOG_VERB("Files sent to server");
            break;
        }

        case OPT_READ_FILE_REQ:
        {
            LOG_VERB("Files read from server");
            break;
        }

        case OPT_READ_RAND_REQ:
        {
            LOG_VERB("Files read from server");
            break;
        }

        case OPT_WAIT:
        {
            LOG_VERB("Waited %d ms", option.val);
            break;
        }

        case OPT_LOCK_FILE_REQ:
        {
            LOG_VERB("Files locked");
            break;
        }

        case OPT_UNLOCK_FILE_REQ:
        {
            LOG_VERB("Files unlocked");
            break;
        }

        case OPT_REMOVE_FILE_REQ:
        {
            LOG_VERB("Files removed from server");
            break;
        }

        case OPT_LOG_ENABLED:
        {
            is_extended_log_enabled = 1;
            LOG_VERB("Enabled per-operation logging");
            break;
        }

        case OPT_READ_SAVE:
        case OPT_WRITE_SAVE:
            break;

        case OPT_CHANGE_LOG_LEVEL:
        {
            if (option.val < 0 || option.val > 4) {
                LOG_ERRO("Log level value error. %d is not between 0 and 4", option.val);
                return RES_ERROR;
            }
            set_log_level(option.val);
            LOG_VERB("Logging level updated into #%d", option.val);
            break;
        }

        default:
            LOG_ERRO("Unable to handle option %d", option.type);
            return RES_ERROR;
            break;
    }
    return RES_OK;
}

void free_options(CmdLineOpt_t* options, size_t size) {
    for (int i = 0; i < size; ++i) {
        switch (options[i].type)
        {
            case OPT_HELP_ENABLED:
            case OPT_LOG_ENABLED:
            case OPT_SOCKET_FILE:
            case OPT_WRITE_DIR_REQ:
            case OPT_WRITE_SAVE:
            case OPT_READ_RAND_REQ:
            case OPT_READ_SAVE:
            case OPT_WAIT:
            case OPT_CHANGE_LOG_LEVEL:
                break;
            
            case OPT_WRITE_FILE_REQ:
            case OPT_READ_FILE_REQ:
            case OPT_LOCK_FILE_REQ:
            case OPT_UNLOCK_FILE_REQ:
            case OPT_REMOVE_FILE_REQ:
                free(options[i].files);
                break;
            
            default:
                LOG_ERRO("Unable to free option %d", options[i].type);
                break;
        }
    }
}
