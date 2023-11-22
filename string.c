#include "string.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int endsWith(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (str_len < suffix_len) {
        return 0;
    }

    return strncmp(str + (str_len - suffix_len), suffix, suffix_len) == 0;
}

char* strncatf(char* c, size_t n, char* format, ...) {
    char buf[n];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, n, format, args);
    va_end(args);
    return strncat(c, buf, n);
}

static void fill_lps_arr__(const char* pattern, int n, int* lps) {
    int len = 0;
    int i = 1;
    lps[0] = 0;

    while (i < n) {
        if (pattern[i] == pattern[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

char* fast_strstr(const char* haystack, const char* needle) {
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);

    if (needle_len == 0) {
        return (char*)haystack;
    }

    int* lps = (int*)malloc(sizeof(int) * needle_len);
    fill_lps_arr__(needle, needle_len, lps);

    int i = 0;
    int j = 0;

    while (i < haystack_len) {
        if (needle[j] == haystack[i]) {
            j++;
            i++;
        }

        if (j == needle_len) {
            free(lps);
            return (char*)(haystack + i - j);
        } else if (i < haystack_len && needle[j] != haystack[i]) {
            if (j != 0) {
                j = lps[j - 1];
            } else {
                i++;
            }
        }
    }

    free(lps);
    return NULL;
}