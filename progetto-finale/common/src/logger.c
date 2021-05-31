#include "logger.h"

static int g_log_level = LOG_LEVEL_VERBOSE;
static pthread_mutex_t gLogMutex = PTHREAD_MUTEX_INITIALIZER;

void set_log_level(int new_level) {
    g_log_level = new_level;
}

int get_log_level() {
    return g_log_level;
}
