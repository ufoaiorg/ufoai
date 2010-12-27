#ifndef LIST_H_
#define LIST_H_

#include "../shared/ufotypes.h"

typedef struct linkedList_s {
	byte *data;
	struct linkedList_s *next;
	qboolean ptr;	/**< don't call Mem_Free for data if this is @c true */
} linkedList_t;

/** @brief Iterates over a linked list, it's safe to delete the returned entry from the list while looping over it.
 * @note @c var must be a simple variable name, because it is also used to create the name of the internal iterator variable.
 * @note Don't try to use the internal loop variable. This variable is most likely not at the position you would expect it to be. */
#define LIST_Foreach(list, type, var) \
	for (linkedList_t *var##__iter = (list); var##__iter;) \
		if (var = (type*)var##__iter->data, var##__iter = var##__iter->next, 0) {} else

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
void *LIST_GetByIdx(linkedList_t *list, int index);
qboolean LIST_Remove(linkedList_t **list, const void *data);

#endif /* LIST_H_ */
