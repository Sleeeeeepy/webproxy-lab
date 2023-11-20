#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "csapp.h"
#include "proxy.h"
#include "logger.h"
#include "string.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE  1049000
#define MAX_OBJECT_SIZE 102400
#define MIN(a, b)       ((a) < (b) ? (a) : (b))


/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char** argv) {
    int listen_fd, client_fd;
    struct sockaddr_storage client_addr, listen_addr;
    socklen_t client_len;
    char host[MAXLINE], port[MAXLINE];
    URL url;
    parse_url("/winter.mp4", &url);
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    listen_fd = Open_listenfd(argv[1]);
    log_info("Info", "The proxy server is listening on port %s\n", argv[1]);

    while (true) {
        client_len = sizeof(client_addr);
        client_fd = Accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        Getnameinfo((struct sockaddr*)&client_addr, client_len, host, sizeof(host), port, sizeof(host), 0);
        log_info("Connection", "%s:%s\n", host, port);
        handle_request(client_fd);
        Close(client_fd);
    }
    Close(listen_fd);
}

static void handle_request(int fd) {
    char buf[MAXLINE];
    char url_buf[MAXLINE];
    targs_t args;
    rio_t rio;
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, sizeof(buf)) < 0) {
        log_error("Error", "Failed to read data from %d\n", fd);
        return;
    }

    sscanf(buf, "%s %s %s", args.request.method, url_buf, args.request.ver);
    
    args.fd = fd;
    handle_request__((void *)&args);
}

static void handle_request__(void* arg) {

}

static void make_req_hdr(char* buf, size_t n, const URL* url) {
    snprintf(buf, n, "%s", user_agent_hdr);
    snprintf(buf, n, "%sHost: %s\r\n", buf, url->host);
    snprintf(buf, n, "%sConnection: close\r\n", buf);
    snprintf(buf, n, "%sProxy-Connection: close\r\n", buf);
    snprintf(buf, n, "%s\r\n", buf);
}

static void make_request(char* buf, size_t n, const request_t* req) {
    make_req_hdr(buf, n, &(req->url));
    snprintf(buf, n, "%s %s %s\r\n%s", req->method, req->url, req->ver, buf);
}

static result_t parse_url(const char* url, URL* parsedURL) {
    char *token, *rest;
    memset(parsedURL, 0x00, sizeof(*parsedURL));
    result_t result;

    result.succ = true;
    result.data = NULL;
    char* url_copy = strdup(url);

    token = strtok_r(url_copy, ":", &rest);
    if (token != NULL && rest != NULL && *rest != '\0') {
        size_t len = MIN(strlen(token), sizeof(parsedURL->proto) - 1);
        strncpy(parsedURL->proto, token, len);
    }

    if ((rest != NULL && *rest == '\0') || rest == NULL) {
        if (strstr(parsedURL->path, "../") != NULL || strstr(parsedURL->path, "//") != NULL) {
            result.succ = false;
            return result;
        }
        size_t len = MIN(strlen(token), sizeof(parsedURL->path) - 1);
        strncpy(parsedURL->path, token, len);
        return result;
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

    // prevent directory traversal
    if (strstr(parsedURL->path, "../") != NULL || strstr(parsedURL->path, "//") != NULL) {
        result.succ = false;
    }

    result.has_data = (result.data != NULL);
    free(url_copy);

    return result;
}

void read_requesthdrs(rio_t *rp) {
    char *buf = rp->rio_buf;
    if (!endsWith(buf, "\r\n\r\n")) {
        log_error("ERROR", "Failed to read line from %d\n", rp->rio_fd);
        return;
    }

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        if (rio_readlineb(rp, buf, MAXLINE) < 0) {
            log_error("ERROR", "Failed to read line from %d\n", rp->rio_fd);
            return;
        }
        log_info("HEADER", "%s", buf);
    }
}