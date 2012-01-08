
/*
============================================================
LINKED LIST
============================================================
*/

#include "list.h"
#include "common.h"
#include "mem.h"
#include <assert.h>
#include <string.h>

/**
 * @brief Adds an entry to a new or to an already existing linked list
 * @sa LIST_AddString
 * @sa LIST_RemoveEntry
 * @return Returns a pointer to the data that has been added, wrapped in a linkedList_t
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
linkedList_t* LIST_Add (linkedList_t** listDest, const byte* data, size_t length)
{
	linkedList_t *newEntry;
	linkedList_t *list;

	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest) {
		*listDest = (linkedList_t*)Mem_PoolAlloc(sizeof(**listDest), com_genericPool, 0);
		(*listDest)->data = (byte*)Mem_PoolAlloc(length, com_genericPool, 0);
		memcpy(((*listDest)->data), data, length);
		(*listDest)->next = NULL; /* not really needed - but for better readability */
		return *listDest;
	} else
		list = *listDest;

	while (list->next)
		list = list->next;

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
	list->next = newEntry;
	newEntry->data = (byte*)Mem_PoolAlloc(length, com_genericPool, 0);
	memcpy(newEntry->data, data, length);
	newEntry->next = NULL; /* not really needed - but for better readability */

	return newEntry;
}

/**
 * @brief Searches for the first occurrence of a given string
 * @return the linkedList_t pointer if the string is found, otherwise @c NULL
 * @note if string is @c NULL, the function returns @c NULL
 * @sa LIST_AddString
 */
const linkedList_t* LIST_ContainsString (const linkedList_t* list, const char* string)
{
	while ((string != NULL) && (list != NULL)) {
		if (Q_streq((const char*)list->data, string))
			return list;
		list = list->next;
	}

	return NULL;
}

/**
 * @brief Searches for the first occurrence of a given pointer
 * @return the linkedList_t pointer if the string is found, otherwise @c NULL
 * @note if data is @c NULL, the function returns @c NULL
 * @note O(n)
 * @note Only use this for small linked lists
 */
linkedList_t* LIST_GetPointer (linkedList_t* list, const void* data)
{
	while ((data != NULL) && (list != NULL)) {
		if (list->data == data)
			return list;
		list = list->next;
	}

	return NULL;
}

void LIST_AddStringSorted (linkedList_t** listDest, const char* data)
{
	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest)
		LIST_AddString(listDest, data);
	else {
		linkedList_t *list, *tmp;

		list = *listDest;
		tmp = NULL;
		while (list) {
			if (Q_StringSort(data, (const char *)list->data) < 0) {
				linkedList_t *newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
				newEntry->data = (byte*)Mem_StrDup(data);
				newEntry->next = list;
				if (tmp != NULL)
					tmp->next = newEntry;
				else
					*listDest = newEntry;
				return;
			}
			tmp = list;
			list = list->next;
		}
		assert(tmp);
		tmp->next = (linkedList_t*)Mem_PoolAlloc(sizeof(**listDest), com_genericPool, 0);
		tmp->next->data = (byte*)Mem_StrDup(data);
		tmp->next->next = NULL; /* not really needed - but for better readability */
	}
}

/**
 * @brief Adds a string as first entry to a linked list
 * @param listDest The linked list to add the string, too. If this is @c NULL, a new list is created
 * @param data The string to add to the list
 * @sa LIST_AddString
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_PrependString (linkedList_t** listDest, const char* data)
{
	linkedList_t *newEntry;

	/* create the list */
	if (!*listDest) {
		LIST_AddString(listDest, data);
		return;
	}

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
	newEntry->next = *listDest;
	*listDest = newEntry;
	newEntry->data = (byte*)Mem_StrDup(data);
}

/**
 * @brief Adds an string to a new or to an already existing linked list. The string is copied here.
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 * @see LIST_PrependString
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_AddString (linkedList_t** listDest, const char* data)
{
	linkedList_t *newEntry;
	linkedList_t *list;

	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest) {
		*listDest = (linkedList_t*)Mem_PoolAlloc(sizeof(**listDest), com_genericPool, 0);
		(*listDest)->data = (byte*)Mem_StrDup(data);
		(*listDest)->next = NULL; /* not really needed - but for better readability */
		return;
	} else
		list = *listDest;

	while (list->next)
		list = list->next;

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
	list->next = newEntry;
	newEntry->data = (byte*)Mem_StrDup(data);
	newEntry->next = NULL; /* not really needed - but for better readability */
}

/**
 * @brief Adds just a pointer to a new or to an already existing linked list
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_AddPointer (linkedList_t** listDest, void* data)
{
	linkedList_t *newEntry;
	linkedList_t *list;

	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest) {
		*listDest = (linkedList_t*)Mem_PoolAlloc(sizeof(**listDest), com_genericPool, 0);
		(*listDest)->data = (byte *)data;
		(*listDest)->ptr = qtrue;
		(*listDest)->next = NULL; /* not really needed - but for better readability */
		return;
	} else
		list = *listDest;

	while (list->next)
		list = list->next;

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
	list->next = newEntry;
	newEntry->data = (byte *)data;
	newEntry->ptr = qtrue;
	newEntry->next = NULL; /* not really needed - but for better readability */
}

/**
 * @brief Removes one entry from the linked list
 * @sa LIST_AddString
 * @sa LIST_Add
 * @sa LIST_AddPointer
 * @sa LIST_Delete
 * @return @c true if the removal was successful, @c false otherwise.
 */
qboolean LIST_RemoveEntry (linkedList_t **list, linkedList_t *entry)
{
	linkedList_t* prev;
	linkedList_t* listPos;

	assert(list);
	assert(entry);

	prev = *list;
	listPos = *list;

	/* first entry */
	if (*list == entry) {
		if (!(*list)->ptr)
			Mem_Free((*list)->data);
		listPos = (*list)->next;
		Mem_Free(*list);
		*list = listPos;
		return qtrue;
	} else {
		while (listPos) {
			if (listPos == entry) {
				prev->next = listPos->next;
				if (!listPos->ptr)
					Mem_Free(listPos->data);
				Mem_Free(listPos);
				return qtrue;
			}
			prev = listPos;
			listPos = listPos->next;
		}
	}
	return qfalse;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
void LIST_Delete (linkedList_t **list)
{
	linkedList_t *next;
	linkedList_t *l = *list;

	while (l) {
		next = l->next;
		if (!l->ptr)
			Mem_Free(l->data);
		Mem_Free(l);
		l = next;
	}
	*list = NULL;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
qboolean LIST_Remove (linkedList_t **list, const void *data)
{
	linkedList_t *l = LIST_GetPointer(*list, data);
	if (l != NULL)
		return LIST_RemoveEntry(list, l);
	return qfalse;
}

/**
 * @brief This is the actual sort function. Notice that it returns the new
 * head of the list. (It has to, because the head will not
 * generally be the same element after the sort.)
 * @note see http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 */
static linkedList_t *_LIST_Sort (linkedList_t *list, linkedListSort_t sorter, const void* userData)
{
	linkedList_t *p, *q, *e, *tail;
	int insize, nmerges, psize, qsize, i;

	/*
	 * Silly special case: if `list' was passed in as NULL, return
	 * NULL immediately.
	 */
	if (!list)
		return NULL;

	insize = 1;

	while (1) {
		p = list;
		list = NULL;
		tail = NULL;

		nmerges = 0; /* count number of merges we do in this pass */

		while (p) {
			nmerges++; /* there exists a merge to be done */
			/* step `insize' places along from p */
			q = p;
			psize = 0;
			for (i = 0; i < insize; i++) {
				psize++;
				q = q->next;
				if (!q)
					break;
			}

			/* if q hasn't fallen off end, we have two lists to merge */
			qsize = insize;

			/* now we have two lists; merge them */
			while (psize > 0 || (qsize > 0 && q)) {
				/* decide whether next element of merge comes from p or q */
				if (psize == 0) {
					/* p is empty; e must come from q. */
					e = q;
					q = q->next;
					qsize--;
				} else if (qsize == 0 || !q) {
					/* q is empty; e must come from p. */
					e = p;
					p = p->next;
					psize--;
				} else if (sorter(p, q, userData) <= 0) {
					/* First element of p is lower (or same);
					 * e must come from p. */
					e = p;
					p = p->next;
					psize--;
				} else {
					/* First element of q is lower; e must come from q. */
					e = q;
					q = q->next;
					qsize--;
				}

				/* add the next element to the merged list */
				if (tail) {
					tail->next = e;
				} else {
					list = e;
				}
				tail = e;
			}

			/* now p has stepped `insize' places along, and q has too */
			p = q;
		}
		tail->next = NULL;

		/* If we have done only one merge, we're finished. */
		if (nmerges <= 1) /* allow for nmerges==0, the empty list case */
			return list;

		/* Otherwise repeat, merging lists twice the size */
		insize *= 2;
	}
}

void LIST_Sort (linkedList_t **list, linkedListSort_t sorter, const void* userData)
{
	if (LIST_IsEmpty(*list))
		return;
	*list = _LIST_Sort(*list, sorter, userData);
}

/**
 * @brief Copies the list structure data - but not the content from the original list.
 * We are only using pointers here.
 */
linkedList_t *LIST_CopyStructure (linkedList_t* src)
{
	linkedList_t *dest = NULL;
	LIST_Foreach(src, void, data) {
		LIST_AddPointer(&dest, data);
	}
	return dest;
}

/**
 * @brief Checks whether the given list is empty
 * @param[in] list The linked list to check
 * @return @c true if empty, @c false otherwise
 */
qboolean LIST_IsEmpty (const linkedList_t *list)
{
	return list == NULL;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
int LIST_Count (const linkedList_t *list)
{
	const linkedList_t *l = list;
	int count = 0;

	while (l) {
		count++;
		l = l->next;
	}
	return count;
}

/**
 * @brief Get an entry of a linked list by its index in the list.
 * @param[in] list Linked list to get the entry from.
 * @param[in] index The index of the entry in the linked list.
 * @return A void pointer of the content in the list-entry.
 */
void *LIST_GetByIdx (linkedList_t *list, int index)
{
	int i;

	if (LIST_IsEmpty(list))
		return NULL;

	if (index < 0)
		return NULL;

	i = 0;
	while (list) {
		if (i == index)
			return (void *)list->data;
		i++;
		list = list->next;
	}

	return NULL;
}
