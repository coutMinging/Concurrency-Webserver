#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H
#include <pthread.h>

typedef struct safequeueItem {
    int priority;
    char *path;
    int delay;
    int client_fd;
    
} safequeueItem_t;

void create_queue(int full_size);
void dequeue();
int add_work(int priority, char *path, int delay, int client_fd);
int get_highest_priority();
safequeueItem_t *get_work();
safequeueItem_t *get_work_nonblocking();

#endif