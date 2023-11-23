#include <stdbool.h>
#include <time.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE  1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cache_line {
    char* url;
    char* content;
    time_t last_request;
    size_t size;
    struct cache_line* prev;
    struct cache_line* next;
} cacheline;

typedef struct {
    cacheline* head;
    cacheline* tail;
    size_t total_size;
    size_t len;
} cache;

cacheline* create_cacheline(const char* url, const char* content);
cache* create_cache();
void free_cache(cache* c);
void free_cacheline(cacheline* c);
cacheline* find(cache* c, const char* url);
void add_head(cache* c, cacheline* new_line);
void add_tail(cache* c, cacheline* new_line);
void delete_head(cache* c);
void delete_tail(cache* c);
void kill_victim(cache* c);