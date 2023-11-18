#include <stdbool.h>
#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE  1049000
#define MAX_OBJECT_SIZE 102400
#define MIN(a, b)       ((a) < (b) ? (a) : (b))

typedef struct {
    char proto[255];
    char host[255];
    char path[255];
    int port;
} URL;

typedef struct {
    bool succ;
    bool has_data;
    void* data;
} result_t;

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static void make_req_hdr(char* buf, size_t n, URL url);
static result_t parse_url(const char* url, URL* parsedURL);

int main() {
    printf("%s", user_agent_hdr);
    URL url;
    result_t result =
        parse_url("http://www.cool.com.au/ersdfs?dfd=dfgd@s=1l", &url);
    printf("result = %s\n", (result.succ) ? "true" : "fail");
    printf("Protocol: %s\nHost: %s\nPath: %s\nPort: %d", url.proto, url.host,
           url.path, url.port);
    return 0;
}

static void make_req_hdr(char* buf, size_t n, URL url) {
    snprintf(buf, n, "%s", user_agent_hdr);
    snprintf(buf, n, "%sHost: %s\r\n", buf, url.host);
    snprintf(buf, n, "%sConnection: close\r\n", buf);
    snprintf(buf, n, "%sProxy-Connection: close\r\n", buf);
    snprintf(buf, n, "%s\r\n", buf);
}

result_t parse_url(const char* url, URL* parsedURL) {
    char *token, *rest;
    memset(parsedURL, 0x00, sizeof(*parsedURL));
    result_t result;

    result.succ = true;
    result.data = NULL;
    char* url_copy = strdup(url);
    strcpy(url_copy, url);

    token = strtok_r(url_copy, ":", &rest);
    if (token != NULL) {
        size_t len = MIN(strlen(token), sizeof(parsedURL->proto) - 1);
        strncpy(parsedURL->proto, token, len);
    }

    token = strtok_r(rest, "/", &rest);
    if (token != NULL) {
        size_t len = MIN(strlen(token), sizeof(parsedURL->host) - 1);
        strncpy(parsedURL->host, token, len);
    }

    if (rest != NULL) {
        size_t len = MIN(strlen(rest), sizeof(parsedURL->path) - 1);
        strncpy(parsedURL->path, rest, len);
    } else {
        strcpy(parsedURL->path, "/");
    }

    // Parse port
    size_t host_len = strlen(parsedURL->host);
    if (parsedURL->host[host_len - 1] == ':') {
        parsedURL->host[host_len - 1] = '\0';
    }

    unsigned int cnt = 0;
    for (size_t i = 0; i < host_len; i++) {
        if (parsedURL->host[i] == ':') {
            cnt += 1;
            if (cnt > 1) {
                break;
            }
        }
    }

    if (cnt > 1) {
        result.succ = false;
    } else {
        char* saveptr;
        char* host_copy = strdup(parsedURL->host);
        strtok_r(host_copy, ":", &saveptr);

        if (saveptr != NULL) {
            parsedURL->port = atoi(saveptr);
        }

        free(host_copy);
    }

    // TODO: Parse path to prevent directory traversal

    result.has_data = (result.data != NULL);
    free(url_copy);

    return result;
}