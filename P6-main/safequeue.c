#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "safequeue.h"

safequeueItem_t **priority_queue;
int size = 0; //size of this priority queue
int max_size; //max size of the priority queue
pthread_mutex_t mutex;
pthread_cond_t cond;
//create a priority queue with given max size and assign it to max_size
void create_queue(int full_size) {
    max_size=full_size;
    priority_queue = malloc(full_size * sizeof(safequeueItem_t*));
    
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    
}

int add_work(int fd, int priority, char *path, int delay) {
    safequeueItem_t *new_item = malloc(sizeof(safequeueItem_t*));

    new_item->client_fd = fd;
    new_item->priority = priority;
    new_item->path = path;
    new_item->delay = delay;
    //add lock
    pthread_mutex_lock(&mutex);

    if (size >= max_size) {
        return -1;
    }

    priority_queue[size] = new_item;
    size++;

    pthread_cond_signal(&cond); // Signal
    
    pthread_mutex_unlock(&mutex);//unlock

    return 0;
}
//get the index of the highest priority item in the priority queue(helpper func)
int get_highest_priority() {
    int highest_p = -1;
    int index = -1;

    for (int i = 0; i < size; i++) {
        if (priority_queue[i]->priority > highest_p) {
            highest_p = priority_queue[i]->priority;
            index = i;
        }
    }

    return index;
}
//get the job with highest priority
safequeueItem_t *get_work() {
    pthread_mutex_lock(&mutex);

    while (size <= 0) {
        pthread_cond_wait(&cond, &mutex); // Wait for work
    }

    int rem_index = get_highest_priority();
    safequeueItem_t *item = priority_queue[rem_index];

    for (int i = rem_index; i < size - 1; i++) {
        priority_queue[i] = priority_queue[i + 1];
    }

    size--;

    pthread_mutex_unlock(&mutex);

    //return the removed item
    return item;
}

safequeueItem_t *get_work_nonblocking() {
    pthread_mutex_lock(&mutex);

    //if no element just return will not wait since non blocking
    if (size <= 0) {
        pthread_mutex_unlock(&mutex);
        return (safequeueItem_t*)0;
    }

    int rem_index = get_highest_priority();

    safequeueItem_t *item = priority_queue[rem_index];

    for (int i = rem_index; i < size - 1; i++) {
        priority_queue[i] = priority_queue[i + 1];
    }
    size--;

    pthread_mutex_unlock(&mutex);

    return item;
}

//Remember to free resources and destroy mutex and condition variable
void destroy_queue() {
     pthread_mutex_lock(&mutex);

     for (int i = 0; i < size; i++) {
         free(priority_queue[i]->path);
         free(priority_queue[i]);
     }

     free(priority_queue);
     size = 0;

     pthread_mutex_unlock(&mutex);
     pthread_mutex_destroy(&mutex);
     pthread_cond_destroy(&cond);
 }
