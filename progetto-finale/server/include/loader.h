#pragma once

#ifndef LOADER_H
#define LOADER_H

#include <stdio.h>
#include <stdlib.h>

#include "server.h"
#include <common.h>
#include <logger.h>

// #define DEBUG_LOG

#define MAX_FILE_PATH_LEN 256

/**
 * Read configs from file.
 * Always returns a valid configuration for the server.
 * Use default values for undefined or wrong defined ones.
 * 
 * \param filename: config file path
 * 
 * \retval struct: server config struct filled with values from file
 */
static ServerConfig_t readConfigs(const char* filename) {
    // Static values (not added globally to hide them)
    // Option names
    static const char* const OPT_SOCKFILE     = "socketFile";
    static const char* const OPT_LOGFILE      = "logFile";
    static const char* const OPT_NUMWORKERS   = "numWorkers";
    static const char* const OPT_MAXCLIENTS   = "maxClients";
    static const char* const OPT_MAXSIZEMB    = "maxSizeMB";
    static const char* const OPT_MAXSLOTCOUNT = "maxSizeSlot";
    static const char* const OPT_TABLESIZE    = "tableSize";

    // Table size ratio
    static const int tableRatio[]       = { 0, 2, 3, 4, 6, 8 };
    static const char* tableSizeValue[] = { "SMALLEST", "SMALL", "MEDIUM", "BIG", "BIGGEST" };

    // 0. vars
    static char socketFilenameBuf[MAX_FILE_PATH_LEN];
    static char logFilenameBuf[MAX_FILE_PATH_LEN];
    FILE* stream   = NULL;
    char* line     = NULL;
    size_t lineLen = 0;
    ssize_t bytes  = 0;

    // 1. Open file
    if ((stream = fopen(filename, "r")) == NULL) {
        LOG_ERRNO("Unable to open configs file.");
        exit(EXIT_FAILURE);
    }

    // 2. Clear configs
    ServerConfig_t configs;
    memset(&configs, 0, sizeof(configs));

#ifdef DEBUG_LOG
    // Check values
    LOG_INFO("KEY              VALUE    ");
#endif

    // 3. Read file line by line
    while ((bytes = getline(&line, &lineLen, stream)) != -1) {
        // Skip empty lines
        if (bytes == 1) continue;

        // Skip comments
        if (line[0] == '#') continue;

        // Remove endline
        line[bytes - 1] = '\0';

        // Find '=' to "split" line in two pieces
        char* equals = NULL;
        if ((equals = strchr(line, '=')) == NULL || equals == line) {
            LOG_ERRO("Error parsing configs file. Pattern should be key=value.");
            exit(EXIT_FAILURE);
        }

        // Split line in 'key' + 'value' pair
        *equals = '\0';
        const char* key   = line;
        const char* value = equals + 1;

        // Check for correct config
        // Socket filename
        if (strcmp(key, OPT_SOCKFILE) == 0) {
            strncpy(socketFilenameBuf, value, MAX_FILE_PATH_LEN);
            configs.socketFilename = socketFilenameBuf;
        }
        // Log filename
        else if (strcmp(key, OPT_LOGFILE) == 0) {
            strncpy(logFilenameBuf, value, MAX_FILE_PATH_LEN);
            configs.logFilename = logFilenameBuf;
        }
        // Num workers
        else if (strcmp(key, OPT_NUMWORKERS) == 0) {
            int num = parse_positive_integer(value);
            if (num > 0) {
                configs.numWorkers = num;
            } else {
                LOG_ERRO("Invalid value (%s) for option: %s. Must be an integer > 0", value, OPT_NUMWORKERS);
                continue;
            }
        }
        // Max clients
        else if (strcmp(key, OPT_MAXCLIENTS) == 0) {
            int num = parse_positive_integer(value);
            if (num > 0) {
                configs.maxClients = num;
            } else {
                LOG_ERRO("Invalid value (%s) for option: %s. Must be an integer > 0", value, OPT_MAXCLIENTS);
                continue;
            }
        }
        // Max size MB
        else if (strcmp(key, OPT_MAXSIZEMB) == 0) {
            int num = parse_positive_integer(value);
            if (num > 0) {
                configs.fsConfigs.maxFileCapacityMB = num;
            } else {
                LOG_ERRO("Invalid value (%s) for option: %s. Must be an integer > 0", value, OPT_MAXSIZEMB);
                continue;
            }
        }
        // Max size Slot
        else if (strcmp(key, OPT_MAXSLOTCOUNT) == 0) {
            int num = parse_positive_integer(value);
            if (num > 0) {
                configs.fsConfigs.maxFileCapacitySlot = num;
            } else {
                LOG_ERRO("Invalid value (%s) for option: %s. Must be an integer > 0", value, OPT_MAXSLOTCOUNT);
                continue;
            }
        }
        // Table size
        else if (strcmp(key, OPT_TABLESIZE) == 0) {
            for (int i = 0; i < 5; ++i) {
                if (strcmp(value, tableSizeValue[i]) == 0) {
                    configs.fsConfigs.tableSize = i + 1; // Save index temporary
                    break;
                }
            }
            if (configs.fsConfigs.tableSize == 0) {
                LOG_ERRO("Invalid value (%s) for option: %s. Must be one of [ SMALLEST, SMALL, MEDIUM, BIG, BIGGEST ]", value, OPT_TABLESIZE);
                continue;
            }
        }
        // Error
        else {
            LOG_ERRO("Unsupported config named: %s", key);
            continue;
        }

#ifdef DEBUG_LOG
        // Log values
        LOG_VERB("%-16s %-32s", key, value);
#endif
    }

    // 4. Release resources
    free(line);
    fclose(stream);

    // 5. Validate configs
    {
        // Socket filename
        if (configs.socketFilename == NULL) {
            LOG_WARN("Using default name (%s) for socket", DEFAULT_SOCK_FILE);
            configs.socketFilename = DEFAULT_SOCK_FILE;
        }

        // Log filename
        if (configs.logFilename == NULL) {
            LOG_WARN("Using default name (%s) for file logging", DEFAULT_LOG_FILE);
            configs.logFilename = DEFAULT_LOG_FILE;
        }

        // Max client
        if (configs.maxClients == 0) {
            LOG_WARN("Using default value (8) for max client count");
            configs.maxClients = 8;
        }

        // Num workers
        if (configs.numWorkers == 0) {
            LOG_WARN("Using default value (2) for thread workers count");
            configs.numWorkers = 2;
        }

        // Max capacity (MB)
        if (configs.fsConfigs.maxFileCapacityMB == 0) {
            LOG_WARN("Using default value (16) for filesystem max capacity (MB)");
            configs.fsConfigs.maxFileCapacityMB = 16;
        }

        // Hashtable size
        if (configs.fsConfigs.tableSize == 0) {
            LOG_WARN("Using default value (MEDIUM) for filesystem hashtable size");
            configs.fsConfigs.tableSize = 3;
        }

        // Max capacity (Slot)
        if (configs.fsConfigs.maxFileCapacitySlot == 0) {
            LOG_WARN("Using default value (32) for filesystem max capacity (Slot)");
            configs.fsConfigs.maxFileCapacitySlot = 32;
        }
    }
    
    // Table size must be processed after all config file is read
    // size = numSlot * ratio + 4096.
    configs.fsConfigs.tableSize = 4096 + (configs.fsConfigs.maxFileCapacitySlot * tableRatio[configs.fsConfigs.tableSize - 1]);

    // Returns configs
    return configs;
}

#endif // LOADER_H
