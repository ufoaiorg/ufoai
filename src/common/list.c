
/*
============================================================
LINKED LIST
============================================================
*/

#include "list.h"
#include "mem.h"
#include "../shared/shared.h"
#include <assert.h>
#include <string.h>

extern struct memPool_s *com_genericPool;

/**
 * @brief Adds an entry to a new or to an already existing linked list
 * @sa LIST_AddString
 * @sa LIST_Remove
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
	assert(list);

	while ((string != NULL) && (list != NULL)) {
		if (!strcmp((const char*)list->data, string))
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
 * @brief Adds an string to a new or to an already existing linked list. The string is copied here.
 * @sa LIST_Add
 * @sa LIST_Remove
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
 * @sa LIST_Remove
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
 */
void LIST_Remove (linkedList_t **list, linkedList_t *entry)
{
	linkedList_t* prev, *listPos;

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
	} else {
		while (listPos) {
			if (listPos == entry) {
				prev->next = listPos->next;
				if (!listPos->ptr)
					Mem_Free(listPos->data);
				Mem_Free(listPos);
				break;
			}
			prev = listPos;
			listPos = listPos->next;
		}
	}
}

/**
 * @sa LIST_Add
 * @sa LIST_Remove
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
 * @sa LIST_Remove
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
	const int count = LIST_Count(list);

	if (!count || !list)
		return NULL;

	if (index >= count || index < 0)
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
