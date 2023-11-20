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
    int priority;           // the priority of every node
    struct node *nextnode;  // point to the next node
    void *data;             // the data stored in each node
}node_t;

//define the queue structure
typedef struct safequeue
{
    node_t *head;
    node_t *tail;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;

}safequeue_t;

extern void queue_init(safequeue_t *sq);
extern void enqueue(safequeue_t *sq, void *data, int priority);

#endif