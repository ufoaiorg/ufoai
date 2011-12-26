#ifndef LIST_H_
#define LIST_H_

#include "../shared/ufotypes.h"

typedef struct linkedList_s {
	byte *data;
	struct linkedList_s *next;
	qboolean ptr;	/**< don't call Mem_Free for data if this is @c true */
} linkedList_t;

typedef int (*linkedListSort_t) (linkedList_t *entry1, linkedList_t *entry2, const void *userData);

/** @brief Iterates over a linked list, it's safe to delete the returned entry from the list while looping over it.
 * @note @c var must be a simple variable name, because it is declared in the loop macro and is also used to create the name of the internal variables.
 * @note Don't try to use the internal loop variable. This variable is most likely not at the position you would expect it to be. */
#define LIST_Foreach(list, type, var) \
	for (qboolean var##__break = qfalse, var##__once = qtrue; var##__once; var##__once = qfalse) \
		for (linkedList_t const* var##__iter = (list); !var##__break && var##__iter;) \
			for (type* const var = (var##__break = var##__once = qtrue, (type*)var##__iter->data); var##__once; var##__break = var##__once = qfalse) \
				if (var##__iter = var##__iter->next, qfalse) {} else

/**
 * @brief Will sort the list before loop over the sorted list. Make sure the free the sortedList after you done with the loop.
 */
#define LIST_ForeachSorted(list, type, var, sorter, userdata, sortedlist) \
	sortedlist = LIST_CopyStructure(list); \
	LIST_Sort(&sortedlist, sorter, userdata); \
	LIST_Foreach(sortedlist, type, var)

void LIST_PrependString(linkedList_t** listDest, const char* data);
void LIST_AddString(linkedList_t** list, const char* data);
void LIST_AddStringSorted(linkedList_t** listDest, const char* data);
void LIST_AddPointer(linkedList_t** listDest, void* data);
linkedList_t* LIST_Add(linkedList_t** list, const byte* data, size_t length);
const linkedList_t* LIST_ContainsString(const linkedList_t* list, const char* string);
linkedList_t* LIST_GetPointer(linkedList_t* list, const void* data);
void LIST_Delete(linkedList_t **list);
qboolean LIST_RemoveEntry(linkedList_t **list, linkedList_t *entry);
qboolean LIST_IsEmpty(const linkedList_t *list);
int LIST_Count(const linkedList_t *list);
linkedList_t *LIST_CopyStructure(linkedList_t* src);
void *LIST_GetByIdx(linkedList_t *list, int index);
qboolean LIST_Remove(linkedList_t **list, const void *data);
void LIST_Sort(linkedList_t **list, linkedListSort_t sorter, const void* userData);

#endif /* LIST_H_ */
