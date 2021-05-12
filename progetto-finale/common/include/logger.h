#pragma once

#include <stdio.h>
#include <stdarg.h>

#ifdef LOG_TIMESTAMP
#include <time.h>
#endif

#define LOG_LEVEL_CRITICAL 0
#define LOG_LEVEL_ERROR    1
#define LOG_LEVEL_WARNING  2
#define LOG_LEVEL_INFO     3
#define LOG_LEVEL_VERBOSE  4

#define LOG_CRIT_INTO_STREAM(stream, ...) custom_log(stream, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERRO_INTO_STREAM(stream, ...) custom_log(stream,    LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN_INTO_STREAM(stream, ...) custom_log(stream,  LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO_INTO_STREAM(stream, ...) custom_log(stream,     LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_VERB_INTO_STREAM(stream, ...) custom_log(stream,  LOG_LEVEL_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_CRIT(...) LOG_CRIT_INTO_STREAM(stderr, __VA_ARGS__)
#define LOG_ERRO(...) LOG_ERRO_INTO_STREAM(stderr, __VA_ARGS__)
#define LOG_WARN(...) LOG_WARN_INTO_STREAM(stdout, __VA_ARGS__)
#define LOG_INFO(...) LOG_INFO_INTO_STREAM(stdout, __VA_ARGS__)
#define LOG_VERB(...) LOG_VERB_INTO_STREAM(stdout, __VA_ARGS__)

__attribute__((unused))
void set_log_level(int new_level);

__attribute__((unused))
int get_log_level();

__attribute__((unused))
static void custom_log(FILE *stream, int loglevel, const char* file, const int line, const char *fmt, ...) {
    // Constants
    static const char * const DESCR[] = { "C", "E", "W", "I", "V" };

#ifndef LOG_WITHOUT_COLORS
    static const char * const COLOR[] = { "\033[91m", "\033[31m", "\033[33m", "\033[0m", "\033[2m", "\033[0m" };
#else
    static const char * const COLOR[] = { "", "", "", "", "", "" };
#endif
    
    // Filter based on loglevel
    if (get_log_level() < loglevel || (loglevel < 0 || loglevel > 4)) return;

    // Add logging infos
#ifdef LOG_TIMESTAMP
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(stream, "%s[%s] (%02d/%02d/%d-%02d:%02d:%02d) %s:%d > ", 
        COLOR[loglevel], DESCR[loglevel],
        tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec,
        file, line);
#else
    fprintf(stream, "%s[%s] %s:%d > ", COLOR[loglevel], DESCR[loglevel], file, line);
#endif

    // Log data
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    // Add newline, reset color
    fprintf(stream, "%s\n", COLOR[5]);
}
