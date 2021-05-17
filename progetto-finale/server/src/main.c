#include <common.h>
#include "server.h"

int main(int argc, char** argv) {
    set_log_level(LOG_LEVEL_VERBOSE);

    // Do-While just for better error handling
    do {
        // TODO: read from file
        ServerConfig_t configs = {
            .socketFilame = DEFAULT_SOCK_FILE,
            .maxClients = 4,
            .maxMemory_MB = 32,
            .maxSlot = 10,
            .numWorkers = 2,
        };

        // Initialize server and its internal state. Must be called before anything else
        if (initializeServer(configs) != RES_OK) break;

        // Run server
        if (runServer() != RES_OK) break;

    } while(0);

    // Terminate server and its internal state releasing resources
    terminateSever();
    return EXIT_SUCCESS;
}