/**
 * @file
 * @brief LINKED LIST interface
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

#pragma once

#include "../shared/ufotypes.h"

struct linkedList_t {
	void* data;
	linkedList_t* next;
	bool ptr; /**< don't call Mem_Free for data if this is @c true */
};

typedef int (*linkedListSort_t) (linkedList_t* entry1, linkedList_t* entry2, const void* userData);

/** @brief Iterates over a linked list, it's safe to delete the returned entry from the list while looping over it.
 * @note @c var must be a simple variable name, because it is declared in the loop macro and is also used to create the name of the internal variables.
 * @note Don't try to use the internal loop variable. This variable is most likely not at the position you would expect it to be. */
#define LIST_Foreach(list, type, var) \
	for (bool var##__break = false, var##__once = true; var##__once; var##__once = false) \
		for (linkedList_t const* var##__iter = (list); !var##__break && var##__iter;) \
			for (type* const var = (var##__break = var##__once = true, static_cast<type*>(var##__iter->data)); var##__once; var##__break = var##__once = false) \
				if (var##__iter = var##__iter->next, false) {} else

/**
 * @brief Will sort the list before loop over the sorted list. Make sure to free the sortedList after you are done with the loop.
 */
#define LIST_ForeachSorted(list, type, var, sorter, userdata, sortedlist) \
	sortedlist = LIST_CopyStructure(list); \
	LIST_Sort(&sortedlist, sorter, userdata); \
	LIST_Foreach(sortedlist, type, var)

void LIST_PrependString(linkedList_t** listDest, const char* data);
void LIST_AddString(linkedList_t** list, const char* data);
void LIST_AddStringSorted(linkedList_t** listDest, const char* data);
void LIST_AddPointer(linkedList_t** listDest, void* data);
linkedList_t* LIST_Add(linkedList_t** list, void const* data, size_t length);
const linkedList_t* LIST_ContainsString(const linkedList_t* list, const char* string);
linkedList_t* LIST_GetPointer(linkedList_t* list, const void* data);
void LIST_Delete(linkedList_t** list);
bool LIST_RemoveEntry(linkedList_t** list, linkedList_t* entry);
bool LIST_IsEmpty(const linkedList_t* list);
int LIST_Count(const linkedList_t* list);
linkedList_t* LIST_CopyStructure(linkedList_t* src);
void* LIST_GetByIdx(linkedList_t* list, int index);
bool LIST_Remove(linkedList_t** list, const void* data);
void LIST_Sort(linkedList_t** list, linkedListSort_t sorter, const void* userData);
void* LIST_GetRandom(linkedList_t* list);

/**
 * Add a copy of data to list and return a reference to the copied data.
 */
template<typename T> inline T& LIST_Add(linkedList_t** const list, T const& data)
{
	return *static_cast<T*>(LIST_Add(list, &data, sizeof(data))->data);
}
