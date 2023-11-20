#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "string.h"

int endsWith(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (str_len < suffix_len) {
        return 0;
    }

    return strncmp(str + (str_len - suffix_len), suffix, suffix_len) == 0;
}

char* strncatf(char* c, size_t n,  char* format, ...) {
    char buf[n];
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buf, n, format, args);
    va_end(args);
    return strncat(c, buf, n);
}