#include <common.h>
#include "client.h"

int main(int argc, char** argv) {
    // Set global log level to INFO
    set_log_level(LOG_LEVEL_VERBOSE);

    // Initialize client and its internal state. Must be called before anything else
    if (initialize_client() != RES_OK) {
        LOG_CRIT("Process terminated initializing client.");
        return EXIT_FAILURE;
    }

    // Start parsing command line arguments
    if (parse_arguments(argc, argv)) {
        LOG_CRIT("Process terminated parsing arguments.");
        return EXIT_FAILURE;
    }
    
    // Start handling options (communicating with server if needed)
    if (handle_options()) {
        LOG_CRIT("Process terminated handling options.");
        return EXIT_FAILURE;
    }

    // Terminate client and its internal state releasing resources
    if (terminate_client()) {
        LOG_CRIT("Process terminated terminating client.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}