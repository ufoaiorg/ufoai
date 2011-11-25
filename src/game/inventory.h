#ifndef INVENTORY_H_
#define INVENTORY_H_

#include "q_shared.h"

typedef struct inventoryImport_s {
	void (*Free) (void* data);

	void (*FreeAll) (void);

	void *(*Alloc) (size_t size);
} inventoryImport_t;

typedef struct inventoryInterface_s
{
	/* private */
	const inventoryImport_t *import;

	invList_t* invList;

	item_t cacheItem;

	const csi_t* csi;

	const char *name;

	/* public */
	qboolean (*RemoveFromInventory) (struct inventoryInterface_s* self, inventory_t* const i, const invDef_t * container, invList_t *fItem) __attribute__((nonnull(1), nonnull(2), warn_unused_result));

	invList_t* (*AddToInventory) (struct inventoryInterface_s* self, inventory_t * const i, const item_t* const item, const invDef_t * container, int x, int y,
			int amount) __attribute__((nonnull(1), nonnull(2), warn_unused_result));

	int (*MoveInInventory) (struct inventoryInterface_s* self, inventory_t* const i, const invDef_t * from, invList_t *item, const invDef_t * to,
			int tx, int ty, int *TU, invList_t ** icp) __attribute__((nonnull(1), nonnull(2)));

	qboolean (*TryAddToInventory) (struct inventoryInterface_s* self, inventory_t* const inv, const item_t * const item, const invDef_t * container);

	void (*DestroyInventory) (struct inventoryInterface_s* self, inventory_t* const i) __attribute__((nonnull(1), nonnull(2)));

	void (*EmptyContainer) (struct inventoryInterface_s* self, inventory_t* const i, const invDef_t * container)__attribute__((nonnull(1), nonnull(2)));

	void (*EquipActor) (struct inventoryInterface_s* self, inventory_t* const inv, const equipDef_t *ed, const teamDef_t* td) __attribute__((nonnull(1), nonnull(2)));

	void (*EquipActorMelee) (struct inventoryInterface_s* self, inventory_t* const inv, const teamDef_t* td) __attribute__((nonnull(1)));

	void (*EquipActorRobot) (struct inventoryInterface_s* self, inventory_t* const inv, const objDef_t* weapon) __attribute__((nonnull(1), nonnull(2)));

	int (*GetUsedSlots) (struct inventoryInterface_s* self);
} inventoryInterface_t;

void INV_InitInventory(const char *name, inventoryInterface_t *ii, const csi_t* csi, const inventoryImport_t *iimport);
void INV_DestroyInventory(inventoryInterface_t *ii);

#endif /* INVENTORY_H_ */
