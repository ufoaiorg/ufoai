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

#include "../shared/shared.h"

/** @defgroup pqueue Priority Queue (priorityQueue_t)
 * @ingroup datastructure
 * Implementation of a priority queue by using a binary heap. Manage a priority
 * queue as a heap - the heap is implemented as an array.
 */

typedef int priorityQueueRating_t;

typedef struct priorityQueueElement_s {
	pos4_t item;
	priorityQueueRating_t rating;
} priorityQueueElement_t;

/**
 * @brief the priority queue struct
 * the actual data is stored in @c priorityQueueElement_t
 */
typedef struct priorityQueue_s {
	uint32_t maxSize;
	uint32_t currentSize;
	priorityQueueElement_t *elements;
} priorityQueue_t;

void PQueueInitialise(priorityQueue_t *pq, uint32_t maxElements);
void PQueueFree(priorityQueue_t *pq);

#define PQueueIsEmpty(pq) ((pq)->currentSize == 0)
void PQueuePush(priorityQueue_t *pq, const pos4_t item, priorityQueueRating_t rating);
void PQueuePop(priorityQueue_t *pq, pos4_t item);

#endif /* #ifdef __PQUEUE_H */
