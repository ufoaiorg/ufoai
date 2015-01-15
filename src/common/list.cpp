/**
 * @file
 * @brief LINKED LIST implementation
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "list.h"
#include "common.h"
#include "mem.h"
#include <assert.h>
#include <string.h>

static linkedList_t* LIST_AllocateEntry(void* const data, linkedList_t* const next = 0)
{
	linkedList_t* const e = Mem_PoolAllocType(linkedList_t, com_genericPool);
	e->data = data;
	e->next = next;
	return e;
}

/** Append entry to end of list. */
static void LIST_AppendEntry(linkedList_t** list, linkedList_t* const entry)
{
	while (*list) list = &(*list)->next;
	*list = entry;
}

/**
 * @brief Adds an entry to a new or to an already existing linked list
 * @sa LIST_AddString
 * @sa LIST_RemoveEntry
 * @return Returns a pointer to the data that has been added, wrapped in a linkedList_t
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
linkedList_t* LIST_Add (linkedList_t** listDest, void const* data, size_t length)
{
	assert(listDest);
	assert(data);

	void*         const dataCopy = memcpy(Mem_PoolAllocTypeN(byte, length, com_genericPool), data, length);
	linkedList_t* const newEntry = LIST_AllocateEntry(dataCopy);

	LIST_AppendEntry(listDest, newEntry);

	return newEntry;
}

/**
 * @brief Searches for the first occurrence of a given string
 * @return the linkedList_t pointer if the string is found, otherwise @c nullptr
 * @note if string is @c nullptr, the function returns @c nullptr
 * @sa LIST_AddString
 */
const linkedList_t* LIST_ContainsString (const linkedList_t* list, const char* string)
{
	while ((string != nullptr) && (list != nullptr)) {
		if (Q_streq(static_cast<char const*>(list->data), string))
			return list;
		list = list->next;
	}

	return nullptr;
}

/**
 * @brief Searches for the first occurrence of a given pointer
 * @return the linkedList_t pointer if the string is found, otherwise @c nullptr
 * @note if data is @c nullptr, the function returns @c nullptr
 * @note O(n)
 * @note Only use this for small linked lists
 */
linkedList_t* LIST_GetPointer (linkedList_t* list, const void* data)
{
	while ((data != nullptr) && (list != nullptr)) {
		if (list->data == data)
			return list;
		list = list->next;
	}

	return nullptr;
}

static linkedList_t* LIST_AllocateString(char const* data, linkedList_t* const next = 0)
{
	return LIST_AllocateEntry(Mem_StrDup(data), next);
}

void LIST_AddStringSorted (linkedList_t** listDest, const char* data)
{
	assert(listDest);
	assert(data);

	for (; *listDest; listDest = &(*listDest)->next) {
		if (Q_StringSort(data, static_cast<char const*>((*listDest)->data)) < 0)
			break;
	}

	*listDest = LIST_AllocateString(data, *listDest);
}

/**
 * @brief Adds a string as first entry to a linked list
 * @param listDest The linked list to add the string, too. If this is @c nullptr, a new list is created
 * @param data The string to add to the list
 * @sa LIST_AddString
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_PrependString (linkedList_t** listDest, const char* data)
{
	*listDest = LIST_AllocateString(data, *listDest);
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
	assert(listDest);
	assert(data);

	LIST_AppendEntry(listDest, LIST_AllocateString(data));
}

/**
 * @brief Adds just a pointer to a new or to an already existing linked list
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_AddPointer (linkedList_t** listDest, void* data)
{
	assert(listDest);
	assert(data);

	linkedList_t* const newEntry = LIST_AllocateEntry(data);
	newEntry->ptr = true;

	LIST_AppendEntry(listDest, newEntry);
}

/**
 * @brief Removes one entry from the linked list
 * @sa LIST_AddString
 * @sa LIST_Add
 * @sa LIST_AddPointer
 * @sa LIST_Delete
 * @return @c true if the removal was successful, @c false otherwise.
 */
bool LIST_RemoveEntry (linkedList_t** list, linkedList_t* entry)
{
	assert(list);
	assert(entry);

	for (; *list; list = &(*list)->next) {
		if (*list == entry) {
			/* Unlink the entry. */
			*list = entry->next;
			/* Delete the entry. */
			if (!entry->ptr) Mem_Free(entry->data);
			Mem_Free(entry);
			return true;
		}
	}

	return false;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
void LIST_Delete (linkedList_t** list)
{
	linkedList_t* next;
	linkedList_t* l = *list;

	while (l) {
		next = l->next;
		if (!l->ptr)
			Mem_Free(l->data);
		Mem_Free(l);
		l = next;
	}
	*list = nullptr;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
bool LIST_Remove (linkedList_t** list, const void* data)
{
	linkedList_t* l = LIST_GetPointer(*list, data);
	if (l != nullptr)
		return LIST_RemoveEntry(list, l);
	return false;
}

/**
 * @brief This is the actual sort function. Notice that it returns the new
 * head of the list. (It has to, because the head will not
 * generally be the same element after the sort.)
 * @note see http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 */
static linkedList_t* _LIST_Sort (linkedList_t* list, linkedListSort_t sorter, const void* userData)
{
	linkedList_t* p, *q, *e;
	int insize, psize, qsize, i;

	/*
	 * Silly special case: if `list' was passed in as nullptr, return
	 * nullptr immediately.
	 */
	if (!list)
		return nullptr;

	insize = 1;

	while (1) {
		p = list;
		list = nullptr;
		linkedList_t* tail = nullptr;

		int nmerges = 0; /* count number of merges we do in this pass */

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
		tail->next = nullptr;

		/* If we have done only one merge, we're finished. */
		if (nmerges <= 1) /* allow for nmerges==0, the empty list case */
			return list;

		/* Otherwise repeat, merging lists twice the size */
		insize *= 2;
	}
}

void LIST_Sort (linkedList_t** list, linkedListSort_t sorter, const void* userData)
{
	if (LIST_IsEmpty(*list))
		return;
	*list = _LIST_Sort(*list, sorter, userData);
}

/**
 * @brief Copies the list structure data - but not the content from the original list.
 * We are only using pointers here.
 */
linkedList_t* LIST_CopyStructure (linkedList_t* src)
{
	linkedList_t* dest = nullptr;
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
bool LIST_IsEmpty (const linkedList_t* list)
{
	return list == nullptr;
}

/**
 * @sa LIST_Add
 * @sa LIST_RemoveEntry
 */
int LIST_Count (const linkedList_t* list)
{
	const linkedList_t* l = list;
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
void* LIST_GetByIdx (linkedList_t* list, int index)
{
	int i;

	if (LIST_IsEmpty(list))
		return nullptr;

	if (index < 0)
		return nullptr;

	i = 0;
	while (list) {
		if (i == index)
			return list->data;
		i++;
		list = list->next;
	}

	return nullptr;
}

void* LIST_GetRandom (linkedList_t* list)
{
	const int elements = LIST_Count(list);
	if (elements == 0)
		return nullptr;

	const int element = rand() % elements;
	return LIST_GetByIdx(list, element);
}
