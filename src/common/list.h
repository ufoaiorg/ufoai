#ifndef LIST_H_
#define LIST_H_

#include "../shared/ufotypes.h"

typedef struct linkedList_s {
	byte *data;
	struct linkedList_s *next;
	qboolean ptr;	/**< don't call Mem_Free for data if this is @c true */
} linkedList_t;

/** @brief Assign the next variable from the linked list or @c NULL if the list end is reached */
#define ASSIGN_VAR(l, type, var) ((var) = (((l) != NULL) ? (type*)(l)->data : NULL))
/** @brief check whether the next data pointer from the linked list is @c NULL or not, if it is @c NULL the end of the list is reached */
#define CHECK_LIST(l, type, var) ((((l) = (((ASSIGN_VAR(l, type, (var))) != NULL) ? (l)->next : NULL)) != NULL) || var != NULL)
/** @brief Iterates over a linked list, it's safe to delete the returned entry from the list while looping over it
 * @note Nesting these loop iterators can cause problems, because the local variable @c l is shadowed
 * @note Don't try to use the internal loop variable @c l. This variable is most likely not at the position you would expect it to be */
#define LIST_Foreach(list, type, var) for (linkedList_t *l = (list); CHECK_LIST(l, type, (var));)

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
