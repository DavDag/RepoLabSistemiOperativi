#include "client.h"
#include <string.h>

// const char *const TEST_ARG_PARS_T = "arg_pars";

Result_t handle_param_option(CmdLineOpt_t *option, int opt, const char* value) {
    // Fill memory with zeros
    memset(option, 0, sizeof(CmdLineOpt_t));

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
            const char *pos = strchr(value, ',');
            if (pos != NULL) {
                // Check for ',n='
                if (strlen(pos) < 4 || pos[1] != 'n' || pos[2] != '=') {
                    LOG_ERRO("Error while parsing number. Correct syntax is ',n=' (ex.',n=12').");
                    return RES_ERROR;
                }

                // Parse integer
                val = parse_positive_integer(pos + 3);
                if (val < 0) {
                    LOG_ERRO("Error while parsing number.");
                    return RES_ERROR;
                }
            } else {
                LOG_VERB("Default value used (0) for n while parsing -w option.");
            }

            // Copy value str
            int dirnameLen = (pos == NULL) ? strlen(value) : (pos - value);
            char *dir = (char *) malloc(sizeof(char) * (dirnameLen + 1));
            if (!dir) {
                LOG_CRIT("Malloc error !");
                exit(EXIT_FAILURE);
            }
            dir = strncpy(dir, value, dirnameLen);
            dir[dirnameLen] = '\0';

            // Set values
            option->max_files = val;
            option->dirname = dir;
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
            int valueLen = strlen(value), numFiles = 0;
            char *buffer = (char *) malloc(sizeof(char) * valueLen);
            if (!buffer) {
                LOG_CRIT("Malloc error !");
                exit(EXIT_FAILURE);
            }
            for (int i = 0; i < valueLen; ++i)
                if (value[i] == ',')
                    ++numFiles;
            const char **files = (const char **) malloc(sizeof(char *) * numFiles);
            if (!files) {
                LOG_CRIT("Malloc error !");
                exit(EXIT_FAILURE);
            }
            for (int i = 0, j = 0; i < valueLen; ++i)
                if (value[i] == ',') {
                    buffer[i] = '\0';
                    files[j++] = &buffer[i];
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
                    LOG_ERRO("Error while parsing number. Correct syntax is 'n=' (ex.'n=12').");
                    return RES_ERROR;
                }
                
                // Parse integer
                val = parse_positive_integer(value + 2);
                if (val < 0) {
                    LOG_ERRO("Error while parsing number.");
                    return RES_ERROR;
                }
            } else {
                LOG_VERB("Default value used (0) for n while parsing -R option.");
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
                LOG_ERRO("Error while parsing number.");
                return RES_ERROR;
            }

            // Set values
            option->val = val;
            break;
        }
        
        default:
            return RES_ERROR;
    }
    return RES_OK;
}
