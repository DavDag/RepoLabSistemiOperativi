#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#ifdef LOG_TIMESTAMP
#include <time.h>
#endif

#define LOG_LEVEL_CRITICAL 0
#define LOG_LEVEL_PERROR   1
#define LOG_LEVEL_ERROR    2
#define LOG_LEVEL_WARNING  3
#define LOG_LEVEL_INFO     4
#define LOG_LEVEL_VERBOSE  5

#define LOG_CRIT_INTO_STREAM(stream, ...) custom_formatted_log(stream, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERRO_INTO_STREAM(stream, ...) custom_formatted_log(stream,    LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN_INTO_STREAM(stream, ...) custom_formatted_log(stream,  LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO_INTO_STREAM(stream, ...) custom_formatted_log(stream,     LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_VERB_INTO_STREAM(stream, ...) custom_formatted_log(stream,  LOG_LEVEL_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_PERR_INTO_STREAM(stream, ...) custom_formatted_log(stream,   LOG_LEVEL_PERROR, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_EMPTY_INTO_STREAM(stream, ...) fprintf(stream, __VA_ARGS__)

#define LOG_CRIT(...) LOG_CRIT_INTO_STREAM(stderr, __VA_ARGS__)
#define LOG_ERRO(...) LOG_ERRO_INTO_STREAM(stderr, __VA_ARGS__)
#define LOG_WARN(...) LOG_WARN_INTO_STREAM(stdout, __VA_ARGS__)
#define LOG_INFO(...) LOG_INFO_INTO_STREAM(stdout, __VA_ARGS__)
#define LOG_VERB(...) LOG_VERB_INTO_STREAM(stdout, __VA_ARGS__)

#define LOG_ERRNO(...) LOG_PERR_INTO_STREAM(stderr, __VA_ARGS__)
#define LOG_EMPTY(...) LOG_EMPTY_INTO_STREAM(stdout, __VA_ARGS__)

void set_log_level(int new_level);
int get_log_level();

__attribute__((unused))
static void custom_formatted_log(FILE* stream, int loglevel, const char* file, const int line, const char* fmt, ...) {
    // Constants
    static const char* const DESCR[] = { "C", "!", "E", "W", "I", "V" };

#ifndef LOG_WITHOUT_COLORS
    static const char* const COLOR[] = { "\033[91m", "\033[35m", "\033[31m", "\033[33m", "\033[0m", "\033[2m", "\033[0m" };
#else
    static const char* const COLOR[] = { "", "", "", "", "", "", "" };
#endif

    // Save copy of errno
    int errno_s = errno;
    
    // Filter based on loglevel
    if (get_log_level() < loglevel || (loglevel < LOG_LEVEL_CRITICAL || loglevel > LOG_LEVEL_VERBOSE)) return;

    // Add logging infos
    fprintf(stream, "%s[%s] ", COLOR[loglevel], DESCR[loglevel]);

#ifdef LOG_TIMESTAMP
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    fprintf(stream, "(%02d/%02d/%d-%02d:%02d:%02d) ", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif

#ifdef LOG_DEBUG
    fprintf(stream, "%s:%d > ", file, line);
#endif

    // Log data
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    // Log errno if needed
    if (loglevel == LOG_LEVEL_PERROR) fprintf(stream, ": %s", strerror(errno_s));

    // Add newline, reset color
    fprintf(stream, "%s\n", COLOR[6]);
}
