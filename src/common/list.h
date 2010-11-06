#ifndef LIST_H_
#define LIST_H_

#include "../shared/ufotypes.h"

typedef struct linkedList_s {
	byte *data;
	struct linkedList_s *next;
	qboolean ptr;	/**< don't call Mem_Free for data */
} linkedList_t;

#define LIST_Foreach(list, type, var) for (linkedList_t *l = list; (var = (l != NULL) ? (type*)l->data : NULL) != NULL; l = l->next)

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
void *LIST_GetNext(linkedList_t *list, void *lastData);

#endif /* LIST_H_ */
