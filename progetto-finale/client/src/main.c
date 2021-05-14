#include <common.h>
#include "client.h"

int main(int argc, char** argv) {
    // Set global log level to INFO
    set_log_level(LOG_LEVEL_VERBOSE);

    // Initialize client and its internal state. Must be called before anything else
    if (initializeClient() != RES_OK) {
        LOG_CRIT("Process terminated initializing client.");
        return EXIT_FAILURE;
    }

    // Start parsing command line arguments
    if (parseArguments(argc, argv)) {
        LOG_CRIT("Process terminated parsing arguments.");
        return EXIT_FAILURE;
    }
    
    // Start handling options (communicating with server if needed)
    if (handleOptions()) {
        LOG_CRIT("Process terminated handling options.");
        return EXIT_FAILURE;
    }

    // Terminate client and its internal state releasing resources
    if (terminateClient()) {
        LOG_CRIT("Process terminated terminating client.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}