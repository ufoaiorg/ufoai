/**
 * @file pqueue.c
 * @brief Implementation of a priority queue by using a binary heap.
 * @note Manage a priority queue as a heap - the heap is implemented as a array.
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


/* initialise the priority queue with a maximum size of maxelements. */

void PQueueInitialise (PQUEUE *pq, uint32_t MaxElements)
{
	pq->MaxSize = MaxElements;
	pq->CurrentSize = 0;

	pq->Elements = (pq_element_t*) malloc(sizeof(pq_element_t) * (MaxElements + 1));

	if (pq->Elements == NULL)
		printf("Memory alloc failed!\n");
}

void PQueuePush (PQUEUE *pq, pos3_t item, pq_rating_t r)
{
	uint32_t i, j;
	pq_element_t * Elements = pq->Elements;
	uint32_t CurrentSize = pq->CurrentSize;

	if (CurrentSize == pq->MaxSize) {
		int new_size;
		new_size = pq->MaxSize * 2;
		pq->Elements = Elements = (pq_element_t *)realloc(Elements, sizeof(pq_element_t) * (new_size + 1));
		pq->MaxSize = new_size;
	}

	/* set i to the first unused element and increment CurrentSize */

	i = (++CurrentSize);

	/* while the parent of the space we're putting the new node into is worse than
	 * our new node, swap the space with the worse node. We keep doing that until we
	 * get to a worse node or until we get to the top
	 * note that we also can sort so that the minimum elements bubble up so we need to loops
	 * with the comparison operator flipped... */

	while (i > PQ_FIRST_ENTRY && (Elements[PQ_PARENT_INDEX(i)].rating > r)) {
		Elements[i] = Elements[PQ_PARENT_INDEX(i)];
		i = PQ_PARENT_INDEX(i);
	}

	/* then add the element at the space we created. */
	for (j = 0; j < 3; j++)
		Elements[i].item[j] = item[j];

	Elements[i].rating = r;

	pq->CurrentSize = CurrentSize;
}

/* free up memory for pqueue */
void PQueueFree (PQUEUE *pq)
{
	free(pq->Elements);
}

/* remove the first node from the pqueue and provide a pointer to it */

void PQueuePop (PQUEUE *pq, pos3_t item)
{
	uint32_t i, j;
	uint32_t child;
	pq_element_t * Elements = pq->Elements;
	uint32_t CurrentSize = pq->CurrentSize;

	pq_element_t pMaxElement;
	pq_element_t pLastElement;

	if (PQueueIsEmpty(pq))
		return; /* XXX */

	pMaxElement = Elements[PQ_FIRST_ENTRY];

	/* get pointer to last element in tree */
	pLastElement = Elements[CurrentSize--];

	for (j = 0; j < 3; j++)
		item[j] = pMaxElement.item[j];

	for (i = PQ_FIRST_ENTRY; (child = PQ_LEFT_CHILD_INDEX(i)) <= CurrentSize; i = child) {
		/* set child to the smaller of the two children... */

		if ((child != CurrentSize) && (Elements[child + 1].rating < Elements[child].rating))
			child ++;

		if (pLastElement.rating > Elements[ child ].rating)
			Elements[i] = Elements[child];
		else
			break;
	}

	Elements[i] = pLastElement;
	pq->CurrentSize = CurrentSize;
}
