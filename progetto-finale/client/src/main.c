#include <common.h>
#include "client.h"

int main(int argc, char** argv) {
    set_log_level(LOG_LEVEL_INFO);

    // Do-While just for better error handling
    do {
        // Initialize client and its internal state. Must be called before anything else
        if (initializeClient() != RES_OK) break;
        
        // Start parsing command line arguments
        if (parseArguments(argc, argv) != RES_OK) break;
        
        // Start handling options (communicating with server if needed)
        if (handleOptions() != RES_OK) break;

    } while(0);

    // Terminate client and its internal state releasing resources
    terminateClient();

    return EXIT_SUCCESS;
}