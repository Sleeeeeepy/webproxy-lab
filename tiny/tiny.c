/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include <time.h>
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
void handle_get(int fd, rio_t *rio, char *uri, char *filename, char *cgiargs);
void handle_head(int fd, rio_t *rio, char *uri, char *filename, char *cgiargs);
void rio_writen__(int fd, char *buf, size_t n);
int endsWith(const char *str, const char *suffix);

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (Signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        unix_error("Failed to set sigchld handler");
    }

    if (Signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
        unix_error("Failed to set sigpipe handler");
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr,
                        &clientlen);  // line:netp:tiny:accept
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port,
                    MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);   // line:netp:tiny:doit
        Close(connfd);  // line:netp:tiny:close
    }
}

void handle_get(int fd, rio_t *rio, char *uri, char *filename, char *cgiargs) {
    struct stat sbuf;
    read_requesthdrs(rio);
    int is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny coludn't find this file", GET);
        return;
    }

    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file", GET);
            return;
        }
        serve_static(fd, filename, &sbuf, GET);
    } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program", GET);
            return;
        }
        serve_dynamic(fd, filename, cgiargs, GET);
    }
}

void handle_head(int fd, rio_t *rio, char *uri, char *filename, char *cgiargs) {
    struct stat sbuf;
    read_requesthdrs(rio);
    int is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny coludn't find this file", HEAD);
        return;
    }

    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file", HEAD);
            return;
        }
        serve_static(fd, filename, sbuf.st_size, HEAD);
    } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program", HEAD);
            return;
        }
        serve_dynamic(fd, filename, cgiargs, HEAD);
    }
}

void doit(int fd) {
    char buf[MAXLINE] = {0};
    char method[MAXLINE] = {0};
    char uri[MAXLINE] = {0};
    char version[MAXLINE] = {0};
    char filename[MAXLINE] = {0};
    char cgiargs[MAXLINE] = {0};
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n%s\n", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strncasecmp(method, "GET", 4) == 0) {
        handle_get(fd, &rio, uri, filename, cgiargs); 
        return;
    } else if (strncasecmp(method, "HEAD", 5) == 0) {
        handle_head(fd, &rio, uri, filename, cgiargs);
        return;
    }

    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method", UNKOWN);   
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, http_method_t method) {
    char buf[MAXLINE], body[MAXBUF];

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen__(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen__(fd, buf, strlen(buf));
    
    if (method == HEAD) {
        sprintf(buf, "\r\n");
        rio_writen__(fd, buf, strlen(buf));
        return;
    }

    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen__(fd, buf, strlen(buf));
    rio_writen__(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    if (!endsWith(buf, "\r\n\r\n")) {
        printf("invlid header!\n");
        return;
    }

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    
    if (!strstr(uri, "cgi-bin")) {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri) - 1] == '/') {
            strcat(filename, "home.html");
        }

        return 1;
    }

    ptr = index(uri, '?');
    if (ptr) {
        strcpy(cgiargs, ptr + 1);
        *ptr = '\0';
    } else {
        strcpy(cgiargs, "");
    }

    strcpy(filename, ".");
    strcat(filename, uri);

    return 0;
}

void serve_static(int fd, char *filename, struct stat *sbuf, http_method_t method) {
    int srcfd;
    char *srcp;
    char buf[MAXLINE] = {0};
    char filetype[MAXLINE] = {0};
    int filesize = sbuf->st_size;
    char time_buf[100];
    strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(sbuf->st_mtime)));
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-type: %s\r\n", buf, filetype);
    sprintf(buf, "%sLast-Modified: %s\r\n", buf, time_buf);
    if (method == HEAD) {
        sprintf(buf, "%s\r\n", buf);
        rio_writen__(fd, buf, strlen(buf));
        return;
    }

    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, filesize);
    rio_writen__(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    srcfd = Open(filename, O_RDONLY, 0);
    srcp = (char *)Malloc(filesize);
    memset(srcp, 0x00, filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    rio_writen__(fd, srcp, filesize);
    //rio_writen__(STDOUT_FILENO, srcp, filesize);
    free(srcp);
}

void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    } else if (strstr(filename, "image/gif")) {
        strcpy(filetype, ".gif");
    } else if (strstr(filename, ".png")) {
        strcpy(filetype, ".png");
    } else if (strstr(filename, ".jpg")) {
        strcpy(filetype, "image/jpeg");
    } else if (strstr(filename, ".mpg")) {
        strcpy(filetype, "video/mpeg");
    } else if (strstr(filename, ".mp4")) {
        strcpy(filetype, "video/mp4");
    } else {
        strcpy(filetype, "text/plain");
    }
}

void serve_dynamic(int fd, char *filename, char *cgiargs, http_method_t method) {
    char buf[MAXLINE], *emptylist[] = { NULL };
    // char method_buf[100];
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen__(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    rio_writen__(fd, buf, strlen(buf));

    // TODO: HEAD 메소드를 사용하면 cgi까지 실행이 되어야 합니다.
    if (method == HEAD) {
        sprintf(buf, "\r\n");
        rio_writen__(fd, buf, strlen(buf));
        return;
    }

    if (Fork() == 0) {
        if (Signal(SIGPIPE, SIG_DFL) == SIG_ERR) {
            unix_error("Failed to reset SIGEPIPE handler");
        }
        // sprintf(method_buf, "%d", method);
        setenv("QUERY_STRING", cgiargs, 1);
        // setenv("METHOD", method_buf, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
}

void sigchld_handler(int signal) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fprintf(stderr, "[%d]child process exit\n", pid);
    }
}

void sigpipe_handler(int signal) {
    fprintf(stderr, "EPIE error ocurred.\n");
}

void rio_writen__(int fd, char *buf, size_t n) {
    if (rio_writen(fd, buf, n) == n) {
        return;
    }

    if (errno == EPIPE) {
        return;
    }

    unix_error("rio_writen error");
}

int endsWith(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (str_len < suffix_len) {
        return 0;
    }

    return strncmp(str + (str_len - suffix_len), suffix, suffix_len) == 0;
}
