#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#define MAX_OPTIONS_COUNT 512

/**
 * Initialize global state.
 * 
 * MUST be called before any other function.
 */
int initializeClient();

/**
 * Start parsing command line arguments.
 * Update internal state by saving parsed options and handles
 * commands that have priority (like -h or -p).
 * 
 * If the options buffer max capacity is reached, it stops
 * parsing and returns RES_OK.
 * 
 * Returns RES_ERROR if argv format is incorrect.
 */
int parseArguments(int argc, char** argv);

/**
 * Handles options synchronously.
 * 
 * Error handling one option causes the function to return
 * RES_ERROR.
 */
int handleOptions();

/**
 * Release resources.
 * 
 * MUST be called after any other function.
 */
int terminateClient();

#endif // CLIENT_H
