/* Matan Gazit
 * CS 4760
 * 04/04/20
 *
 * queue.c
 *
 * defines various queue utitlity methods used by the oss
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

Queue* new_queue(unsigned int _size)
{
	Queue *n_queue = (Queue *)malloc(sizeof(Queue)); // allocate memory for queue
	n_queue -> size = _size;
	n_queue -> head = n_queue -> num_occupied = 0;
	n_queue -> tail = _size - 1;
	n_queue -> arr = (unsigned int *)malloc(n_queue -> size * sizeof(unsigned int));
	return n_queue;
}

void q_push(Queue* queue, unsigned int item)
{
	if (q_full(queue)) return;
	queue -> tail = (queue -> tail + 1) % queue -> size;
	queue -> arr[queue -> tail] = item;
	queue -> num_occupied += 1;
}

unsigned int q_pop(Queue* queue)
{
		if(q_empty(queue)) return -1;
		int item = queue -> arr[queue -> head];
		memset(&queue -> arr[queue -> head], 0, sizeof(unsigned int));
		queue -> head = (queue -> head + 1) % queue -> size;
		queue -> num_occupied -= 1;
		return item;
}

unsigned int q_peek(Queue* queue)
{
	if(q_empty(queue)) return -1;
	int item = queue -> arr[queue -> head];
	return item;
}

bool q_full(Queue* queue)
{
	return queue -> num_occupied == queue -> size;
}

bool q_empty(Queue* queue)
{
	return queue -> num_occupied == 0;
}

void display_q(Queue *q)
{
	printf("Displayed Queue(%d): \n", q -> num_occupied);
	int i;
	for (i = 0; i < q -> num_occupied; i++)
	{
		printf("Element %d : %d\n", i, q -> arr[(q -> head + i) % q -> size]);
	}
}


