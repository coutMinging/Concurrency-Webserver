#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H
#include <pthread.h>

typedef struct safequeueItem {
    int client_fd;
    int priority;
    char *path;
    int delay;
    
} safequeueItem_t;

void create_queue(int full_size);
int add_work(int fd, int priority, char *path, int delay);
int get_highest_priority();
safequeueItem_t *get_work();
safequeueItem_t *get_work_nonblocking();
void destroy_queue();
#endif