#include <stdlib.h>

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
    int fd;
    request_t request;
} targs_t;

typedef struct {
    bool succ;
    bool has_data;
    void* data;
} result_t;

static void handle_request(int fd);
static void handle_request__(void* arg);
static result_t parse_url(const char* urlstr, URL* url);
static void read_requesthdrs(rio_t *rp);
static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void sigpipe_handler(int signal);