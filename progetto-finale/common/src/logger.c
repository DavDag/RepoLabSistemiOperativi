#include "logger.h"

static int g_log_level = LOG_LEVEL_VERBOSE;

void set_log_level(int new_level) {
    g_log_level = new_level;
}

int get_log_level() {
    return g_log_level;
}
