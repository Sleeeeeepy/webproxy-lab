#include "proxy.h"

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/epoll.h>

#include "csapp.h"
#include "logger.h"
#include "string.h"
#include "cache.h"

#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define HTTP_VER_STRING "HTTP/1.0"
#define MAX_EVENTS      100

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static cache* http_cache;
static pthread_mutex_t mutex;
int main(int argc, char** argv) {
    int c = 0;
    int option_index = 0;
    context_t ctx;

    static struct option long_options[] = {{"host", required_argument, 0, 'h'},
                                           {"port", required_argument, 0, 'p'},
                                           {"help", no_argument, 0, '?'},
                                           {0, 0, 0, 0}};

    while ((c = getopt_long(argc, argv, "h:p:?", long_options,
                            &option_index)) != -1) {
        switch (c) {
            case 0:
                break;
            case 'h':
                strncpy(ctx.default_host, optarg, MAXLINE - 1);
                ctx.default_host[MAXLINE - 1] = '\0';
                break;
            case 'p':
                strncpy(ctx.default_port, optarg, MAXLINE - 1);
                break;
            case '?':
                print_usage(argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Unknown option: %c\n", c);
                print_usage(argv[0]);
                exit(1);
        }
    }

    int port_idx = optind;
    while (optind < argc) {
        optind++;
    }

    if (optind == port_idx) {
        print_usage(argv[0]);
        exit(1);
    }

    if (strlen(ctx.default_host) == 0) {
        log_warn("WARN", "No default host has been set\n");
        log_warn("WARN", "Relative path requests are forwarded to localhost\n");
        snprintf(ctx.default_host, sizeof(ctx.default_host), "localhost");
    }
    log_info("INFO", "default host: %s\n", ctx.default_host);

    if (strlen(ctx.default_port) == 0) {
        snprintf(ctx.default_port, sizeof(ctx.default_port), "80");
        log_warn("WARN", "No default port has benn set\n");
        log_warn("WARN", "Relative path requests are forwarded to port 80\n");
    }
    log_info("INFO", "default port: %s\n", ctx.default_port);

    if (Signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        unix_error("Failed to set sigpipe handler");
    }

    if (Signal(SIGINT, sigint_handler) == SIG_ERR) {
        unix_error("Failed to set sigint handler");
    }

    pthread_mutex_init(&mutex, NULL);
    http_cache = create_cache();
    start_proxy(argv[port_idx], &ctx);
}

void print_usage(char* program) {
    fprintf(stderr, "Usage: %s <PROXY_PORT> [OPTIONS]\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr,
            "  -h, --host=HOST      Set the default host of remote host\n");
    fprintf(stderr,
            "  -p, --port=PORT      Set the default port of remote host\n");
    fprintf(stderr, "  -?, --help           Show this help message\n");
}

static void start_proxy(char* proxy_port, context_t* ctx) {
    int listen_fd, client_fd, epoll_fd;
    struct sockaddr_storage client_addr, listen_addr;
    socklen_t client_len;
    char host[MAXLINE], port[MAXLINE];
    struct epoll_event event, events[MAX_EVENTS];

    listen_fd = Open_listenfd(proxy_port);
    log_info("INFO", "The proxy server is listening on port %s\n", proxy_port);
    epoll_fd = epoll_create1(0);
    ctx->epoll_fd = epoll_fd;
    ctx->events = &events;
    if (epoll_fd == -1) {
        log_error("ERROR", "Failed to create epoll\n");
        close(listen_fd);
        return;
    }

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
        log_error("ERROR", "Failed to add server socket to epoll");
        close(listen_fd);
        close(epoll_fd);
        return;
    }

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            if (errno == EINTR) {
                continue;
            }
            log_error("ERROR", "An error in epoll_wait\n");
            exit(1);
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == listen_fd) {
                client_len = sizeof(client_addr);
                client_fd = Accept(listen_fd, (struct sockaddr*)&client_addr,
                                   &client_len);

                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) ==
                    -1) {
                    log_error("ERROR", "Failed to add client to epoll\n");
                    close(client_fd);
                    continue;
                }

                Getnameinfo((struct sockaddr*)&client_addr, client_len, host,
                            sizeof(host), port, sizeof(host), 0);
                log_info("Connection", "%s:%s\n", host, port);
            } else {
                client_fd = events[i].data.fd;
                threaded_request(client_fd, ctx);
            }
        }
    }

    Close(listen_fd);
    return;
}

// for debugging
static void sync_request(int fd, const context_t* ctx) {
    targs_t* args = calloc(1, sizeof(targs_t));
    args->ctx = ctx;
    args->fd = fd;
    thread_start(args);
}

static void threaded_request(int fd, const context_t* ctx) {
    pthread_t tid;
    targs_t* thread_args = calloc(1, sizeof(targs_t));
    thread_args->ctx = ctx;
    thread_args->fd = fd;
    if (pthread_create(&tid, NULL, (void* (*)(void*))thread_start,
                       (void*)thread_args) != 0) {
        log_error("ERROR", "Failed to create new thread\n");
        free(thread_args);
        return;
    }
}

static void thread_start(void* targs) {
    targs_t* args = (targs_t*)targs;
    if (pthread_detach(pthread_self()) < 0) {
        log_error("ERROR", "Failed to set detach mode\n");
        if (epoll_ctl(args->ctx->epoll_fd, EPOLL_CTL_DEL, args->fd, NULL) ==
            -1) {
            log_error("ERROR", "Failed to remove client to epoll\n");
        }
        free(targs);
        Close(args->fd);
        pthread_exit("error");
    }
    handle_request(targs);

    if (epoll_ctl(args->ctx->epoll_fd, EPOLL_CTL_DEL, args->fd, NULL) == -1) {
        log_error("ERROR", "Failed to remove client to epoll\n");
    }
    Close(args->fd);
    free(targs);
}

static void handle_request(void* targs) {
    char buf[MAXLINE];
    char url_buf[MAXLINE];
    targs_t* args = (targs_t*)targs;
    rio_t rio;
    bool has_connhdr = false, has_hosthdr = false, has_pconnhdr = false,
         has_useragent = false;
    size_t host_len;
    bool need_update_cache = false;
    cacheline* found = NULL;

    rio_readinitb(&rio, args->fd);
    if (rio_readlineb(&rio, buf, sizeof(buf)) < 0) {
        log_error("Error", "Failed to read data from %d\n", args->fd);
        return;
    }

    sscanf(buf, "%s %s %s", args->request.method, url_buf, args->request.ver);
    log_info("REQUEST", "%s %s %s\n", args->request.method, url_buf, args->request.ver);

    result_t parse_result = parse_url(url_buf, &(args->request.url));
    if (!parse_result.succ || strlen(args->request.ver) == 0) {
        clienterror(args->fd, "Bad Request", "400", "Proxy Error",
                    "Bad Request");
        return;
    }

    strcpy(args->request.ver, HTTP_VER_STRING);
    host_len = strnlen(args->request.url.host, sizeof(args->request.url.host));

    while (rio_readlineb(&rio, buf, sizeof(buf)) > 0) {
        if (fast_strstr(buf, "\r\n") == NULL) {
            log_warn("HEADER", "Invalid header\n");
            break;
        }

        if (!has_useragent && fast_strstr(buf, "User-Agent") != NULL) {
            strncat(args->request.header, user_agent_hdr,
                    sizeof(args->request.header));
            has_useragent = true;
            continue;
        }

        if (!has_hosthdr && fast_strstr(buf, "Host") != NULL) {
            if (host_len == 0) {
                log_warn("WARN", "relative path received\n");
                log_warn("WARN", "forward to default host\n");
                strncatf(args->request.header, sizeof(args->request.header),
                         "Host: http://%s:%s", args->ctx->default_host,
                         args->ctx->default_port);
                strcpy(args->request.url.proto, "http");
                strcpy(args->request.url.host, "localhost");
                args->request.url.port = 8081;
                continue;
            }
            strncatf(args->request.header, sizeof(args->request.header),
                     "Host: %s\r\n", args->request.url.host);
            has_hosthdr = true;
            log_info("HEADER", "%s", buf);
            continue;
        }

        if (!has_pconnhdr && fast_strstr(buf, "Proxy-Connection") != NULL) {
            strncatf(args->request.header, sizeof(args->request.header),
                     "Proxy-Connection: close\r\n");
            has_pconnhdr = true;
            log_info("HEADER", "%s", buf);
            continue;
        }

        if (!has_connhdr && fast_strstr(buf, "Connection") != NULL) {
            strncatf(args->request.header, sizeof(args->request.header),
                     "Connection: close\r\n");
            has_connhdr = true;
            log_info("HEADER", "%s", buf);
            continue;
        }

        strncatf(args->request.header, sizeof(args->request.header), "%s", buf);
        if (strcmp(buf, "\r\n") == 0) {
            log_info("HEADER", "end of headers\n");
            break;
        }
        log_info("HEADER", "%s", buf);
    }

    if (!has_useragent) {
        strncat(args->request.header, user_agent_hdr,
                sizeof(args->request.header));
    }

    if (!has_hosthdr) {
        strncatf(args->request.header, sizeof(args->request.header),
                 "Host: %s\r\n", args->request.url.host);
    }

    if (!has_connhdr) {
        strncatf(args->request.header, sizeof(args->request.header),
                 "Connection: close\r\n");
    }

    if (!has_pconnhdr) {
        strncatf(args->request.header, sizeof(args->request.header),
                 "Proxy-Connection: close\r\n");
    }

    if (args->request.url.port == 0) {
        args->request.url.port = atoi(args->ctx->default_port);
    }

    if (strlen(args->request.url.host) == 0) {
        strcpy(args->request.url.host, args->ctx->default_host);
    }

    if ((found = find(http_cache, url_buf)) == NULL) {
        need_update_cache = true;
    }

    strncpy(args->raw_url, url_buf, sizeof(args->raw_url));
    if (need_update_cache) {
    handle_request__(args);
    } else {
        handle_request_cache__(args, found->content, found->size);
        return;
    }
}

static void handle_request_cache__(targs_t* args, char* data, size_t size) {
    log_info("INFO", "Send cached content\n");
    if (rio_writen(args->fd, data, size) != size) {
        log_error("ERROR", "Failed to response to the client\n");
        return;
    }
    log_success("SUCCESS", "Send response successfully\n");
}

static void handle_request__(targs_t* args) {
    int server_fd, n;
    char write_buf[MAXLINE], read_buf[MAXLINE], port[SMALL_MAXSIZE];
    rio_t rio;
    size_t write_len;
    char cache_buf[MAX_OBJECT_SIZE];
    size_t total_len = 0;

    snprintf(port, SMALL_MAXSIZE, "%d", args->request.url.port);
    server_fd = open_clientfd(args->request.url.host, port);
    if (server_fd < 0) {
        log_error("ERROR", "Failed to connect to server\n",
                  args->request.url.host, args->request.url.port);
        log_error("ERROR", "host: %s:%s\n", args->request.url.host, port);
        clienterror(args->fd, "Internal Server Error", "500", "Proxy Error",
                    "Failed to connect to server");
        return;
    }
    rio_readinitb(&rio, server_fd);

    snprintf(write_buf, sizeof(write_buf), "%s %s %s\r\n%s",
             args->request.method, args->request.url.path, args->request.ver,
             args->request.header);

    write_len = strnlen(write_buf, sizeof(write_buf));
    if (rio_writen(server_fd, write_buf, write_len) != write_len) {
        log_error("ERROR", "Failed to request to the server\n");
        clienterror(args->fd, "Internal Server Error", "500", "Proxy Error",
                    "Failed to request to server");
        return;
    }

    while ((n = rio_readlineb(&rio, read_buf, sizeof(read_buf))) != 0) {
        if (rio_writen(args->fd, read_buf, n) != n) {
            log_error("ERROR", "Failed to response to the client\n");
            break;
        }
        total_len += n;
        if (total_len < sizeof(cache_buf)) {
            strcat(cache_buf, read_buf);
        }
    }

    if (total_len < MAX_OBJECT_SIZE) {
        pthread_mutex_lock(&mutex);
        add_head(http_cache, create_cacheline(args->raw_url, cache_buf));
        pthread_mutex_unlock(&mutex);
    }
    Close(server_fd);
    log_success("SUCCESS", "Send response successfully\n");
}

static result_t parse_url(const char* url, URL* parsedURL) {
    char *token, *rest;
    bool no_proto = false;
    memset(parsedURL, 0x00, sizeof(*parsedURL));
    result_t result;
    result.succ = true;
    result.data = NULL;
    char* url_copy = strdup(url);

    // Parse proto
    if (fast_strstr(url_copy, "://") != NULL) {
    token = strtok_r(url_copy, ":", &rest);
    if (token != NULL && rest != NULL && *rest != '\0') {
        size_t len = MIN(strlen(token), sizeof(parsedURL->proto) - 1);
        strncpy(parsedURL->proto, token, len);
    }
    } else {
        rest = url_copy;
        no_proto = true;
        }

    // parse relative path
    if (no_proto && rest != NULL && rest[0] == '/') {
        size_t len = MIN(strlen(rest), sizeof(parsedURL->path) - 1);
        strncpy(parsedURL->path, rest, len);
    } else {
        // Parse host
    token = strtok_r(rest, "/", &rest);
    if (token != NULL) {
        size_t len = MIN(strlen(token), sizeof(parsedURL->host) - 1);
        strncpy(parsedURL->host, token, len);
    }

        // Parse path
    if (rest != NULL) {
        size_t len = MIN(strlen(rest), sizeof(parsedURL->path) - 1);
        if (rest[0] != '/') {
            strncpy(parsedURL->path, "/", 2);
        }
        strncat(parsedURL->path, rest, len);
    } else {
        strncpy(parsedURL->path, "/", 2);
        }
    }

    // Parse port
    // Remove invalid port
    size_t host_len = strlen(parsedURL->host);
    if (host_len > 0 && parsedURL->host[host_len - 1] == ':') {
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

    // Can't parse ipv6
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
        if (pos != NULL) {
        *pos = '\0';
        }
        free(host_copy);
    }

    // prevent directory traversal
    if (fast_strstr(parsedURL->path, "../") != NULL ||
        fast_strstr(parsedURL->path, "//") != NULL) {
        result.succ = false;
    }

    if (!no_proto) {
    result.has_data = (result.data != NULL);
    free(url_copy);
        return result;
    }

    // if there is no proto, determine proto using known port number
    switch (parsedURL->port) {
        case 0:
        case 80:
            strcpy(parsedURL->proto, "http");
            break;
        case 443:
            strcpy(parsedURL->proto, "https");
            break;
        default:
            result.succ = false;
            break;
    }

    return result;
}

static void clienterror(int fd, char* cause, char* errnum, char* shortmsg,
                        char* longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen__(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen__(fd, buf, strlen(buf));
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body,
            "%s<body bgcolor="
            "ffffff"
            ">\r\n",
            body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Proxy</em>\r\n", body);
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen__(fd, buf, strlen(buf));
    rio_writen__(fd, body, strlen(body));
}

void rio_writen__(int fd, char* buf, size_t n) {
    if (rio_writen(fd, buf, n) == n) {
        return;
    }

    if (errno == EPIPE) {
        return;
    }

    unix_error("rio_writen error");
}

void sigpipe_handler(int signal) { log_warn("WARN", "Broken pipe\n"); }
void sigint_handler(int signal) {
    log_info("INFO", "Closing server...\n");
    free_cache(http_cache);
    pthread_mutex_destroy(&mutex);
    log_info("INFO", "Bye\n");
    exit(0);
}