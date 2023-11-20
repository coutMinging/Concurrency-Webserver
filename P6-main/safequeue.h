#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#include "proxyserver.h"
#include <pthread.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct{
    struct http_request *q;
    int size;
    int capacity;
    pthread_mutex_t mutex ;
    pthread_cond_t cond ;


}safequeue;

void safequeue_init(int capacity, safequeue *sq)
{
    sq->q = (struct http_request*)malloc(capacity * sizeof(struct http_request));
    if(sq->q == NULL){
        printf("malloc failed");
        exit(1);
    }
    sq->size = 0;
    sq->capacity = capacity;
    pthread_mutex_init(&(sq->mutex), NULL);
    pthread_cond_init(&(sq->cond), NULL);

}

#endif