#pragma once

#ifndef SERVER_H
#define SERVER_H

#include "file_system.h"

#define DEFAULT_LOG_FILE    "../log.txt"
#define DEFAULT_CONFIG_FILE "../configs/default.txt"

typedef struct {
    const char* socketFilename;
    const char* logFilename;
    int numWorkers;
    int maxClients;
    int maxFileSizeMB;
    FSConfig_t fsConfigs;
} ServerConfig_t;

/**
 * 
 */
int initializeServer(ServerConfig_t configs);

/**
 * 
 */
int runServer();

/**
 * 
 */
int terminateSever();

#endif // SERVER_H
