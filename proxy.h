#include <stdlib.h>

typedef struct {
    char proto[255];
    char host[255];
    char path[255];
    int port;
} URL;

typedef struct {
    URL url;
    char* method;
    char* ver;
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

static void make_req_hdr(char* buf, size_t n, const URL* url);
static void make_request(char* buf, size_t n, const request_t* req);
static result_t parse_url(const char* urlstr, URL* url);