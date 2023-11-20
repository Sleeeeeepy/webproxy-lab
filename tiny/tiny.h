#include "csapp.h"

typedef enum {
    UNKOWN = 0,
    GET = 1,
    HEAD = 2
} http_method_t;

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, struct stat *sbuf, http_method_t method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, http_method_t method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg, http_method_t method);
void sigchld_handler(int signal);
void sigpipe_handler(int signal);
void handle_get(int fd, rio_t *rio, char *uri, char *filename, char *cgiargs, http_method_t method);
void handle_head(int fd, rio_t *rio, char *uri, char *filename, char *cgiargs);
void rio_writen__(int fd, char *buf, size_t n);
int endsWith(const char *str, const char *suffix);