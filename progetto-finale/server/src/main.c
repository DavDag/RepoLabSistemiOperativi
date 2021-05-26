#include <common.h>
#include "server.h"
#include "loader.h"

int main(int argc, char** argv) {
    srand(time(NULL));
    set_log_level(LOG_LEVEL_INFO);
    
    // Configs file must be the first parameter
    const char* configsFile = (argc > 1) ? argv[1] : DEFAULT_CONFIG_FILE;

    // Do-While just for better error handling
    do {
        // Load configs file
        ServerConfig_t configs = readConfigs(configsFile);

        // Initialize server and its internal state. Must be called before anything else
        if (initializeServer(configs) != RES_OK) break;

        // Run server
        if (runServer() != RES_OK) break;

    } while(0);

    // Terminate server and its internal state releasing resources
    terminateSever();
    return EXIT_SUCCESS;
}