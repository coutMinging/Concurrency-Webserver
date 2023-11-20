#include "safequeue.h"
#include <stdio.h>
#include <stdlib.h>

//initialize the safe queue
void queue_init(safequeue_t *sq, int full_size)
{
    sq->head = NULL;
    sq->tail = NULL;
    sq->size = 0;
    sq->full_size = full_size;
    pthread_mutex_init(&sq->mutex, NULL);
    pthread_cond_init(&sq->empty, NULL);
    pthread_cond_init(&sq->full, NULL);

}

// add new node to the link list depend on the priority
void enqueue(safequeue_t *sq, void *data, int priority)
{
    pthread_mutex_lock(&sq->mutex);
    while(sq->size == sq->full_size)
    {
        pthread_cond_wait(&sq->empty, &sq->mutex);
    }
    node_t *new_node = (node_t*)malloc(sizeof(node_t));
    if(new_node == NULL){
        printf("malloc failed\n");
        exit(1);
    }
    new_node->data = data;
    new_node->priority = priority;
    new_node->nextnode = NULL;
    //insert the node based on the priority
    // when the linkedlist is empty
    if(sq->head == NULL || sq->head->priority>priority)
    {
        new_node->nextnode = sq->head;
        sq->head = new_node;
    }else{
        //if the linked list is not empty
        node_t *current = sq->head;
        while(current->nextnode != NULL && current->nextnode->priority<=priority)
        {
            current = current->nextnode;
        }
        new_node->nextnode = current->nextnode;
        current->nextnode = new_node;
    }
    if(sq->tail == NULL || sq->tail->nextnode == new_node)
    {
        sq->tail = new_node;
    }

    sq->size ++;
    pthread_cond_signal(&sq->full);

    pthread_mutex_unlock(&sq->mutex);

}
// get the data with highest priority
void *dequeue(safequeue_t *sq)
{
    //get the lock, ensure the thread safe
    pthread_mutex_lock(&sq->mutex);
    
    //using condition variable check, if empty, then wait
    while(sq->size == 0)
    {
        pthread_cond_wait(&sq->full, &sq->mutex);
    }

    //get the data stored in the highest priority node
    node_t *highest_priority_node = sq->head;
    void *data = sq->head->data;
    //delete the head node
    sq->head = sq->head->nextnode;
    //check if the link list is empty after remove the node
    //if the link list is empty then update the tail
    if(sq->head == NULL)
    {
        sq->tail == NULL;
    }
    free(highest_priority_node);
    sq->size--;
    //when the link list is not full, the can wake up the enqueue
    //to add more data into it
    pthread_cond_signal(&sq->empty);

    pthread_mutex_unlock(&sq->mutex);
    return data;



}

//make the linked list NULL
void remove_all_node(safequeue_t *sq)
{
    while(sq->head!=NULL)
    {
        node_t *current = sq->head;
        sq->head = sq->head->nextnode;
        free(current);
    }

}


