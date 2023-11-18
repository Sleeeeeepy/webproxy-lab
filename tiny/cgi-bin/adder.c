/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;

    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p + 1);
        sscanf(arg1, "num1=%d", &n1);
        sscanf(arg2, "num2=%d", &n2);
    }

    sprintf(content, "QUERY_STRING=%s", buf);
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%s THE Internet addition portal.\r\n", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n", content, n1, n2,
            n1 + n2);
    sprintf(content, "%sThanks for visiting\r\n", content);

    printf("Connection: close\r\n");
    printf("Content-length: %d", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);

    exit(0);
}
/* $end adder */
