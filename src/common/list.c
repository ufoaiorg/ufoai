
/*
============================================================
LINKED LIST
============================================================
*/

#include "list.h"
#include "mem.h"
#include <assert.h>
#include <string.h>

extern struct memPool_s *com_genericPool;

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
	assert(list);

	while ((string != NULL) && (list != NULL)) {
		if (!strcmp((const char*)list->data, string))
			return list;
		list = list->next;
	}

	return NULL;
}


/**
 * @brief Searches for the first occurrence of a given string
 * @return the linkedList_t pointer if the string is found, otherwise @c NULL
 * @note if data is @c NULL, the function returns @c NULL
 */
linkedList_t* LIST_GetPointer (linkedList_t* list, const void* data)
{
	assert(list);

	while ((data != NULL) && (list != NULL)) {
		if (list->data == data)
			return list;
		list = list->next;
	}

	return NULL;
}

/**
 * @brief Adds an string to a new or to an already existing linked list. The string is copied here.
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
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
 */
void LIST_RemoveEntry (linkedList_t **list, linkedList_t *entry)
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
 * @brief Linked list iterator
 * @param list The linked list to iterate over
 * @param lastData The last data entry
 */
void *LIST_GetNext (linkedList_t *list, void *lastData)
{
	linkedList_t *entry;

	if (LIST_IsEmpty(list))
		return NULL;

	if (lastData == NULL)
		return list->data;

	entry = LIST_GetPointer(list, lastData);
	assert(entry);

	if (entry->next)
		return entry->next->data;

	return NULL;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
qboolean LIST_Remove (linkedList_t **list, const void *data)
{
	linkedList_t *l = LIST_GetPointer(*list, data);
	if (l != NULL)
		LIST_RemoveEntry(list, l);
	return l != NULL;
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
 * @brief Returns the last entry in the linked list
 * @param[in] list Linked list to get the entry from.
 */
void *LIST_GetLast (linkedList_t *list)
{
	while (list) {
		linkedList_t *next = list->next;
		if (next == NULL)
			return (void *)list->data;
		list = next;
	}

	return NULL;
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
