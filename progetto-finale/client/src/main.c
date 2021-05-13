#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <serverapi.h>
#include "client.h"

#define STATUS_GOOD 1
#define STATUS_BAD  2

volatile __sig_atomic_t status = STATUS_GOOD;

int main(int argc, char** argv) {
    set_log_level(LOG_LEVEL_VERBOSE);

    // Parse and (possibly) handle arguments
    LOG_VERB("Parsing requests...");
    int opt = -1, optIndex = 0, optionsSize = (argc / 2) + 2; // MAX options are 1(filename) + 1(-h) + 1(-p) + 1/2(argc)
    CmdLineOpt_t options[optionsSize];
    while((opt = getopt(argc, argv, "hf:w:W:D:r:R::d:t:l:u:c:p" "L:")) != -1 && status == STATUS_GOOD) {
        // Try parsing argument
        if (parse_param_option(&options[optIndex], opt, optarg) == RES_ERROR) {
            LOG_CRIT("Client process terminated while parsing arguments");
            status = STATUS_BAD;
        }

        // Handle requests that have priority
        (can_handle_option_before_start(options[optIndex].type)) ? handle_option(options[optIndex], NULL) : optIndex++;
    }

    // Handle requests
    LOG_VERB("Handling requests...");
    for (int i = 0; i < optIndex && status == STATUS_GOOD; ++i) {
        // Try handling option
        CmdLineOpt_t* nextOpt = (i < optIndex - 1) ? &options[i + 1] : NULL;
        if (handle_option(options[i], nextOpt) == RES_ERROR) {
            LOG_CRIT("Client process terminated while handling arguments");
            status = STATUS_BAD;
        }
    }

    // Release resources
    LOG_VERB("Releasing resources...");
    free_options(options, optIndex);
    return EXIT_SUCCESS;
}