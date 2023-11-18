#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static void make_req_hdr(char* buf, size_t n, URL url);
int main() {
  printf("%s", user_agent_hdr);
  return 0;
}

static void make_req_hdr(char* buf, size_t n, URL url) {
    snprintf(buf, n, "%s", user_agent_hdr);
    snprintf(buf, n, "%sHost: %s\r\n", buf, url.host);
    snprintf(buf, n, "%sConnection: close\r\n", buf);
    snprintf(buf, n, "%sProxy-Connection: close\r\n", buf);
    snprintf(buf, n, "%s\r\n", buf);
}
