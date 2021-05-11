#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <serverapi.h>
#include "client.h"
#include "testing.h"

#define MAX_NUM_OPTIONS 128

int main(int argc, char **argv) {
    set_log_level(LOG_LEVEL_INFO);

    TestMode_t test_mode = TEST_NONE;
    CmdLineOpt_t options[MAX_NUM_OPTIONS];

    // Check for testing argument
    if (argc > 2 && strcmp("-T", argv[1]) == 0) {
        if (handle_test_argument_option(&test_mode, argv[2]) == RES_OK) {
            // TODO
        }
        argc -= 2;
        argv += 2;
    }

    // Parse and handle arguments
    int opt, optIndex = 0;
    while((opt = getopt(argc, argv, "hf:w:W:D:r:R::d:t:l:u:c:p")) != -1) {
        handle_param_option(&options[optIndex], opt, optarg);
    }

    // Cleanup and log (if needed)

    return EXIT_SUCCESS;
}