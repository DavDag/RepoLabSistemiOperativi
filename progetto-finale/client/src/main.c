#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <serverapi.h>
#include "client.h"

int main(int argc, char **argv) {
    set_log_level(LOG_LEVEL_INFO);

    // Parse and (possibly) handle arguments
    int opt = -1, optIndex = 0, optionsSize = (argc / 2) + 2; // MAX options are 1(filename) + 1(-h) + 1(-p) + 1/2(argc)
    CmdLineOpt_t options[optionsSize];
    while((opt = getopt(argc, argv, "hf:w:W:D:r:R::d:t:l:u:c:p" "L:")) != -1) {
        // Try parsing argument
        if (parse_param_option(&options[optIndex], opt, optarg) == RES_ERROR) {
            LOG_CRIT("Client process terminated while parsing arguments.");
            exit(EXIT_FAILURE);
        }

        // Handle requests that have priority
        can_handle_option_before_start(options[optIndex].type) ? handle_option(options[optIndex]) : optIndex++;
    }

    // Handle requests
    for (int i = 0; i < optIndex; ++i) {
        // Try handling option
        if (handle_option(options[i]) == RES_ERROR) {
            LOG_CRIT("Client process terminated while handling arguments.");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}