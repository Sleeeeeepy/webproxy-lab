#include "cache.h"

cacheline* create_cacheline(const char* url, const char* content) {
    cacheline* c = (cacheline*)calloc(1, sizeof(cacheline));
    size_t url_len = strlen(url);
    size_t content_len = strlen(content);
    c->url = (char*)calloc(1, url_len);
    c->content = (char*)calloc(1, content_len);
    c->last_request = time(NULL);
    c->size = content_len;
    c->prev = NULL;
    c->next = NULL;

    strncpy(c->url, url, url_len);
    strncpy(c->content, content, content_len);
    
    return c;
}

cache* create_cache() {
    cache* new_cache = (cache*)malloc(sizeof(cache));
    new_cache->head = NULL;
    new_cache->tail = NULL;
    new_cache->total_size = 0;
    return new_cache;
}

void free_cache(cache* c) {
    cacheline* cl = c->head;
    while (cl != NULL) {
        cacheline* temp = cl;
        cl = cl->next;
        free_cacheline(temp);
    }
}

void free_cacheline(cacheline* c) {
    free(c->url);
    free(c->content);
}

cacheline* find(cache* c, const char* url) {
    cacheline* current = c->head;
    while (current != NULL) {
        if (strcmp(current->url, url) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void add_head(cache* c, cacheline* new_line) {
    if (new_line->size > MAX_OBJECT_SIZE) {
        return;
    }

    while (c->total_size + new_line->size > MAX_CACHE_SIZE) {
        kill_victim(c);
    }

    if (c->head == NULL) {
        c->head = new_line;
        c->tail = new_line;
    } else {
        new_line->next = c->head;
        c->head->prev = new_line;
        c->head = new_line;
    }
    c->total_size += new_line->size;
    c->len += 1;
}

void add_tail(cache* c, cacheline* new_line) {
    if (new_line->size > MAX_OBJECT_SIZE) {
        return;
    }

    while (c->total_size + new_line->size > MAX_CACHE_SIZE) {
        kill_victim(c);
    }

    if (c->tail == NULL) {
        c->head = new_line;
        c->tail = new_line;
    } else {
        new_line->prev = c->tail;
        c->tail->next = new_line;
        c->tail = new_line;
    }
    c->total_size += new_line->size;
    c->len += 1;
}

void delete_head(cache* c) {
    if (c->head != NULL) {
        cacheline* temp = c->head;
        c->head = c->head->next;
        if (c->head != NULL) {
            c->head->prev = NULL;
        } else {
            c->tail = NULL;
        }
        c->total_size -= temp->size;
        c->len -= 1;
        free_cacheline(temp);
    }
}

void delete_tail(cache* c) {
    if (c->tail != NULL) {
        cacheline* temp = c->tail;
        c->tail = c->tail->prev;
        if (c->tail != NULL) {
            c->tail->next = NULL;
        } else {
            c->head = NULL;
        }
        c->total_size -= temp->size;
        c->len -= 1;
        free_cacheline(temp);
    }
}

void kill_victim(cache* c) {
    srand(time(NULL));
    int idx = rand() % c->len;

    if (c->len == 0) {
        return;
    } 

    cacheline* cl = c->head;
    for (size_t i = 0; i < idx; i++) {
        if (cl != NULL) {
            cl = cl->next;
        }
    }

    if (cl->prev != NULL) {
        cl->prev->next = cl->next;
    } else {
        c->head = cl->next;
    }

    if (cl->next != NULL) {
        cl->next->prev = cl->prev;
    } else {
        c->tail = cl->prev;
    }

    c->total_size -= cl->size;
    c->len -= 1;
    free_cacheline(cl);
}