#include <stdlib.h>
#include <stdbool.h>
#include <sys/epoll.h>

#include "csapp.h"

#define SMALL_MAXSIZE 255

typedef struct {
    char proto[SMALL_MAXSIZE];
    char host[SMALL_MAXSIZE];
    char path[SMALL_MAXSIZE];
    int port;
} URL;

typedef struct {
    URL url;
    char method[SMALL_MAXSIZE];
    char ver[SMALL_MAXSIZE];
    char header[MAXLINE];
} request_t;

typedef struct {
    bool succ;
    bool has_data;
    void* data;
} result_t;

typedef struct {
    char default_host[MAXLINE];
    char default_port[MAXLINE];
    int epoll_fd;
    struct epoll_event* events;
} context_t;

typedef struct {
    int fd;
    request_t request;
    context_t* ctx;
    char *raw_url[MAXLINE];
} targs_t;

void print_usage(char* program);
static void thread_start(void* targs);
static void sync_request(int fd, const context_t* ctx);
static void threaded_request(int fd, const context_t* ctx);
static void start_proxy(char* port, context_t* ctx);
static void handle_request(void* targs);
static void handle_request_cache__(targs_t* args, char* data, size_t size);
static void handle_request__(targs_t* args);
static result_t parse_url(const char* urlstr, URL* url);
static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void rio_writen__(int fd, char* buf, size_t n);
void sigpipe_handler(int signal);
void sigint_handler(int signal);