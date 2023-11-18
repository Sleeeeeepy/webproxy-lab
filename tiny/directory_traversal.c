#include "csapp.h"

int main(int argc, char **argv) {
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;
    size_t len;

    char *target = "GET //../.clang-format\r\n\r\n";

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];

    printf("Sending request:\n%s", target);

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    len = strlen(target);
    Rio_writen(clientfd, target, len);

    while (Rio_readlineb(&rio, buf, MAXLINE) > 0) {
        Fputs(buf, stdout);
    }

    Close(clientfd);
    exit(0);
}