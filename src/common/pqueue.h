/**
 * @file pqueue.h
 * @brief Header file for the priority queue implementation.
 */

/*
Originally written by Justin-Heyes Jones
Modified by Shlomi Fish, 2000
Modified by Florian Festi, 2007

This file is in the public domain (it's uncopyrighted).

Check out Justin-Heyes Jones' A* page from which this code has
originated:
	http://www.geocities.com/jheyesjones/astar.html
*/

#ifndef __PQUEUE_H
#define __PQUEUE_H

#include "../game/q_shared.h"

typedef int pq_rating_t;

typedef struct struct_pq_element_t
{
	pos3_t item;
	pq_rating_t rating;
} pq_element_t;

typedef struct _PQUEUE
{
	uint32_t MaxSize;
	uint32_t CurrentSize;
	pq_element_t * Elements;
} PQUEUE;

void PQueueInitialise(PQUEUE *pq, uint32_t MaxElements);
void PQueueFree(PQUEUE *pq);

#define PQueueIsEmpty(pq) ((pq)->CurrentSize == 0)
void PQueuePush(PQUEUE *pq, pos3_t item, pq_rating_t rating);
void PQueuePop(PQUEUE *pq, pos3_t item);

#endif /* #ifdef __PQUEUE_H */