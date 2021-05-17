#include <common.h>
#include "client.h"

int main(int argc, char** argv) {
    // Set global log level to INFO
    // set_log_level(LOG_LEVEL_VERBOSE);
    set_log_level(LOG_LEVEL_INFO);

    int status = RES_OK;

    // Initialize client and its internal state. Must be called before anything else
    if (status == RES_OK) status = initializeClient();
    
    // Start parsing command line arguments
    if (status == RES_OK) status = parseArguments(argc, argv);
    
    // Start handling options (communicating with server if needed)
    if (status == RES_OK) status = handleOptions();
    
    // Terminate client and its internal state releasing resources
    status = terminateClient();

    return EXIT_SUCCESS;
}