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

/**
 * @brief LIST_ForeachGetNextWithCheck callback function that checks whether a value should be used in the foreach loop, or whether it should be skipped
 * @param data The list data
 * @param userdata The userdata that is send to perform the check
 * @return @c true if the value should be used, @c false if it should be skipped
 */
typedef qboolean (*listCheck_t) (void* data, const void *userdata);

linkedList_t* LIST_GetNextWithCheck(linkedList_t* list, listCheck_t check, const void *userdata);

/** @brief check whether the next data pointer from the linked list is @c NULL or not, if it is @c NULL the end of the list is reached */
#define CHECK_LIST_CHECK(l, type, var, check, userdata) ((((l) = (((ASSIGN_VAR(l, type, (var))) != NULL) ? LIST_GetNextWithCheck((l)->next, (check), (userdata)) : NULL)) != NULL) || var != NULL)

/**
 * @brief Iterates over a linked list and perform a check whether the entry should be part of the loop or not.
 * @param list The linked list to iterate over
 * @param type The type the data pointer in the list entry should be cast into
 * @param check The check callback
 * @param userdata Userdata that is passed to the check callback
 */
#define LIST_ForeachCheck(list, type, var, check, userdata) for (linkedList_t *l = LIST_GetNextWithCheck((list), (check), (userdata)); CHECK_LIST_CHECK(l, type, (var), (check), (userdata));)

void LIST_AddString(linkedList_t** list, const char* data);
void LIST_AddStringSorted(linkedList_t** listDest, const char* data);
void LIST_AddPointer(linkedList_t** listDest, void* data);
linkedList_t* LIST_Add(linkedList_t** list, const byte* data, size_t length);
const linkedList_t* LIST_ContainsString(const linkedList_t* list, const char* string);
linkedList_t* LIST_GetPointer(linkedList_t* list, const void* data);
void LIST_Delete(linkedList_t **list);
void LIST_RemoveEntry(linkedList_t **list, linkedList_t *entry);
qboolean LIST_IsEmpty(const linkedList_t *list);
int LIST_Count(const linkedList_t *list);
void *LIST_GetByIdx(linkedList_t *list, int index);
void *LIST_GetLast(linkedList_t *list);
qboolean LIST_Remove(linkedList_t **list, const void *data);

#endif /* LIST_H_ */
