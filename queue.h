/* Matan Gazit
 * CS 4760
 * 04/04/21
 *
 * queue.h
 *
 * header for queue.c
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct // queue struct
{
	unsigned int head; // head of the queue 
	unsigned int tail; // tail of the queue
	unsigned int num_occupied; // number of slots in queue being used
	unsigned int size; // total size of the queue
	unsigned int* arr; // array used by queue struct
} Queue;

Queue *new_queue(unsigned int);
void q_push(Queue*, unsigned int);
unsigned int q_pop(Queue*);
unsigned int q_peek(Queue*);
bool q_full(Queue*);
bool q_empty(Queue*);
void display_q(Queue*);

#endif
