#include <stdio.h>
#include <stdarg.h>

#include "logger.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void log_success(const char* header, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, ANSI_COLOR_GREEN"[%s] "ANSI_COLOR_RESET, header);
    vfprintf(stdout, format, args);
    va_end(args);
}

void log_error(const char* header, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, ANSI_COLOR_RED"[%s] "ANSI_COLOR_RESET, header);
    vfprintf(stdout, format, args);
    va_end(args);
}

void log_warn(const char* header, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, ANSI_COLOR_YELLOW"[%s] "ANSI_COLOR_RESET, header);
    vfprintf(stdout, format, args);
    va_end(args);
}

void log_info(const char* header, const char* format, ...) {
    fprintf(stdout, ANSI_COLOR_BLUE"[%s] "ANSI_COLOR_RESET, header);
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}