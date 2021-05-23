#pragma once

#ifndef SERVER_H
#define SERVER_H

#include "file_system.h"

typedef struct {
    const char* socketFilame;
    int numWorkers;
    int maxMemory_MB;
    int maxSlot;
    int maxClients;
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
