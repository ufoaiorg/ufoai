/**
 * @file pqueue.c
 * @brief Implementation of a priority queue by using a binary heap.
 * @note Manage a priority queue as a heap - the heap is implemented as an array.
 */

/*
Originally written by Justin-Heyes Jones
Modified by Shlomi Fish, 2000
Modified by Florian Festi, 2007

This file is in the public domain (it's uncopyrighted).

Bases on Justin-Heyes Jones' A* tutorial
*/

#include "common.h"
#include "pqueue.h"

/* given an index to any element in a binary tree stored in a linear array with the root at 1 and
 * a "sentinel" value at 0 these macros are useful in making the code clearer */

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i)>>1)
#define PQ_FIRST_ENTRY (1)

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i)<<1)
#define PQ_RIGHT_CHILD_INDEX(i) (((i)<<1)+1)
#define PGetRating(elem) ((elem).rating)


/**
 * @brief initialise the priority queue with a maximum size of maxelements.
 */
void PQueueInitialise (priorityQueue_t *pq, uint32_t maxElements)
{
	pq->maxSize = maxElements;
	pq->currentSize = 0;

	pq->elements = (priorityQueueElement_t*) Mem_Alloc(sizeof(priorityQueueElement_t) * (maxElements + 1));

	if (pq->elements == NULL)
		Sys_Error("PQueueInitialise: Memory alloc failed!");
}

void PQueuePush (priorityQueue_t *pq, const pos4_t item, priorityQueueRating_t r)
{
	uint32_t i, j;
	uint32_t currentSize = pq->currentSize;

	if (currentSize == pq->maxSize) {
		const int new_size = pq->maxSize * 2;
		pq->elements = (priorityQueueElement_t *)Mem_ReAlloc(pq->elements, sizeof(*pq->elements) * (new_size + 1));
		pq->maxSize = new_size;
	}

	/* set i to the first unused element and increment CurrentSize */
	i = (++currentSize);

	/* while the parent of the space we're putting the new node into is worse than
	 * our new node, swap the space with the worse node. We keep doing that until we
	 * get to a worse node or until we get to the top
	 * note that we also can sort so that the minimum elements bubble up so we need to loops
	 * with the comparison operator flipped... */

	while (i > PQ_FIRST_ENTRY && pq->elements[PQ_PARENT_INDEX(i)].rating > r) {
		pq->elements[i] = pq->elements[PQ_PARENT_INDEX(i)];
		i = PQ_PARENT_INDEX(i);
	}

	/* then add the element at the space we created. */
	for (j = 0; j < 4; j++)
		pq->elements[i].item[j] = item[j];

	pq->elements[i].rating = r;

	pq->currentSize = currentSize;
}

/**
 * @brief free up memory for pqueue
 */
void PQueueFree (priorityQueue_t *pq)
{
	Mem_Free(pq->elements);
}

/**
 * @brief remove the first node from the pqueue and provide a pointer to it
 */
void PQueuePop (priorityQueue_t *pq, pos4_t item)
{
	uint32_t i, j;
	uint32_t child;
	priorityQueueElement_t * elements = pq->elements;
	uint32_t currentSize = pq->currentSize;
	priorityQueueElement_t pMaxElement;
	priorityQueueElement_t pLastElement;

	if (PQueueIsEmpty(pq))
		return;

	pMaxElement = elements[PQ_FIRST_ENTRY];

	/* get pointer to last element in tree */
	pLastElement = elements[currentSize--];

	for (j = 0; j < 4; j++)
		item[j] = pMaxElement.item[j];

	for (i = PQ_FIRST_ENTRY; (child = PQ_LEFT_CHILD_INDEX(i)) <= currentSize; i = child) {
		/* set child to the smaller of the two children... */

		if ((child != currentSize) && (elements[child + 1].rating < elements[child].rating))
			child ++;

		if (pLastElement.rating > elements[child].rating)
			elements[i] = elements[child];
		else
			break;
	}

	elements[i] = pLastElement;
	pq->currentSize = currentSize;
}
