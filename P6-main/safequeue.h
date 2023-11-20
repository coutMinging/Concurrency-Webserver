#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#include "proxyserver.h"
#include <pthread.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

//define the node structure
typedef struct node
{
    int priority;           
    struct node *nextnode;  
    void *data;
}node_t;

//define the queue structure
typedef struct safequeue
{
    node_t *head;
    node_t *tail;
    int size;
    int full_size;
    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;

}safequeue_t;

extern void queue_init(safequeue_t *sq, int full_size);
extern void enqueue(safequeue_t *sq, void *data, int priority);
extern void *dequeue(safequeue_t *sq);
extern void remove_all_node(safequeue_t *sq);

#endif