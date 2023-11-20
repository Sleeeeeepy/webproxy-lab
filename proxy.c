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
#define HTTP_VER_STRING "HTTP/1.0"

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char** argv) {
    int listen_fd, client_fd;
    struct sockaddr_storage client_addr, listen_addr;
    socklen_t client_len;
    char host[MAXLINE], port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    
    if (Signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        unix_error("Failed to set sigpipe handler");
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
    bool has_connhdr = false, has_hosthdr = false, has_pconnhdr = false;
    size_t host_len;
    
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, sizeof(buf)) < 0) {
        log_error("Error", "Failed to read data from %d\n", fd);
        return;
    }

    sscanf(buf, "%s %s %s", args.request.method, url_buf, args.request.ver);
    result_t parse_result = parse_url(url_buf, &(args.request.url));
    if (!parse_result.succ) {
        return;
    }

    args.fd = fd;
    strcpy(args.request.ver, HTTP_VER_STRING);
    host_len = strnlen(args.request.url.host, sizeof(args.request.url.host));

    // TODO: read timeout with epoll
    while (rio_readlineb(&rio, buf, sizeof(buf)) > 0) {
        if (strstr(buf, "\r\n") == NULL) {
            log_warn("HEADER", "Invalid header\n");
            break;
        }

        if (strstr(buf, "Host") != NULL) {
            if (host_len == 0) {
                log_warn("WARN", "host_len is zero.\n");
                strncat(args.request.header, "Host: http://127.0.0.1:8081", sizeof(args.request.header));
                strcpy(args.request.url.proto, "http");
                strcpy(args.request.url.host, "localhost");
                args.request.url.port = 8081;
                continue;
            }
            strncatf(args.request.header, sizeof(args.request.header), "Host: %s\r\n", args.request.url.host);
            has_hosthdr = true;
            log_info("HEADER", "%s", buf);
            continue;
        }

        if (strstr(buf, "Connection") != NULL) {
            strncatf(args.request.header, sizeof(args.request.header), "Connection: close\r\n");
            has_connhdr = true;
            log_info("HEADER", "%s", buf);
            continue;
        }

        if (strstr(buf, "Proxy-Connection") != NULL) {
            strncatf(args.request.header, sizeof(args.request.header), "Proxy-Connection: close\r\n");
            has_pconnhdr = true;
            log_info("HEADER", "%s", buf);
            continue;
        }

        strncatf(args.request.header, sizeof(args.request.header), "%s", buf);
        if (strcmp(buf, "\r\n") == 0) {
            log_info("HEADER", "end of headers\n");
            break; // Exit the loop when an empty line is encountered
        }
        log_info("HEADER", "%s", buf);
    }

    if (!has_hosthdr) {
        strncatf(args.request.header, sizeof(args.request.header), "Host: %s\r\n", args.request.url.host);
    }

    if (!has_connhdr) {
        strncatf(args.request.header, sizeof(args.request.header), "Connection: close\r\n");
    }

    if (!has_pconnhdr) {
        strncatf(args.request.header, sizeof(args.request.header), "Proxy-Connection: close\r\n");
    }

    if (args.request.url.port == 0) {
        args.request.url.port = 80;
    }
    handle_request__((void *)&args);
}

static void handle_request__(void* arg) {
    int server_fd, n;
    char write_buf[MAXLINE], read_buf[MAXLINE], port[SMALL_MAXSIZE];
    targs_t* args = (targs_t*)arg;
    rio_t rio;

    snprintf(port, SMALL_MAXSIZE, "%d", args->request.url.port);
    server_fd = open_clientfd(args->request.url.host, port);
    if (server_fd < 0) {
        // log_error("Error", "Failed to connect to server", args->request.url.host, args->request.url.port);
        clienterror(args->fd, "Internal Server Error", "500", "Proxy Error", "Failed to connect tot server");
        return;
    }
    rio_readinitb(&rio, server_fd);

    snprintf(write_buf, sizeof(write_buf), "%s %s %s\r\n%s", args->request.method, args->request.url.path, args->request.ver, args->request.header);
    Rio_writen(server_fd, write_buf, strnlen(write_buf, sizeof(write_buf)));
    while ((n = rio_readlineb(&rio, read_buf, sizeof(read_buf))) != 0) {
        Rio_writen(args->fd, read_buf, n);
    }

    Close(server_fd);
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
        result.has_data = false;
        return result;
    }

    token = strtok_r(rest, "/", &rest);
    if (token != NULL) {
        size_t len = MIN(strlen(token), sizeof(parsedURL->host) - 1);
        strncpy(parsedURL->host, token, len);
    }

    if (rest != NULL) {
        size_t len = MIN(strlen(rest), sizeof(parsedURL->path) - 1);
        if (rest[0] != '/') {
            strcpy(parsedURL->path, "/");
        }
        strncat(parsedURL->path, rest, len);
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
        
        char* pos = strchr(parsedURL->host, ':');
        *pos = '\0';
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

static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Proxy</em>\r\n", body);
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

void sigpipe_handler(int signal) {
    log_warn("WARN", "PIPE Error");
}