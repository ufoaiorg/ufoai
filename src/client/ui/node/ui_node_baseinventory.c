/**
 * @file ui_node_baseinventory.c
 * @brief "Base inventory" is one of the container nodes. It allow to see and
 * drag and drop soldier items from a base to soldier equipments.
 * @todo extend it with aircraft equipment
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_dragndrop.h"
#include "../ui_tooltip.h"
#include "../ui_nodes.h"
#include "../ui_input.h"
#include "../ui_render.h"
#include "ui_node_baseinventory.h"
#include "ui_node_model.h"
#include "ui_node_container.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_mesh.h"
#include "../../cgame/cl_game.h"
#include "../../battlescape/cl_actor.h"
#include "../../cl_inventory.h"

#define EXTRADATA_TYPE baseInventoryExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * self cache for drag item
 * @note we can use a global variable because we only can have 1 source node at a time
 */
static int dragInfoFromX = -1;
static int dragInfoFromY = -1;

/**
 * The current invList pointer (only used for ignoring the dragged item
 * for finding free space right now)
 */
static const invList_t *dragInfoIC;

/**
 * @brief Searches if there is an item at location (x/y) in a scrollable container. You can also provide an item to search for directly (x/y is ignored in that case).
 * @note x = x-th item in a row, y = row. i.e. x/y does not equal the "grid" coordinates as used in those containers.
 * @param[in] node Context node
 * @param[in] item Item requested
 * @param[in] filterType Filter used.
 * @todo Remove filter it is not a generic concept, and here it mean nothing
 * @return invList_t Pointer to the invList_t/item that is located at x/y or equals "item".
 * @sa INVSH_SearchInInventory
 */
static invList_t *UI_ContainerNodeGetExistingItem (const uiNode_t *node, const objDef_t *item, const itemFilterTypes_t filterType)
{
	return INV_SearchInInventoryWithFilter(ui_inventory, EXTRADATACONST(node).super.container, item, filterType);
}

/**
 *  @brief Flag for containerItemIterator_t (CII) groupSteps
 */
#define CII_AMMOONLY 0x01
#define CII_WEAPONONLY 0x02		/**< it mean any soldier equipment, else ammo */
#define CII_AVAILABLEONLY 0x04
#define CII_NOTAVAILABLEONLY 0x08
#define CII_END 0x80

typedef struct {
	const uiNode_t* node;
	byte groupSteps[6];
	int groupID;
	itemFilterTypes_t filterEquipType;

	int itemID;				/**< ID into csi.ods array */
	invList_t *itemFound;	/**< If item foundID into csi.ods array */
} containerItemIterator_t;

/**
 * @brief Compute the next itemID
 * @note If something found, item type can be find with iterator->itemID
 * @note If item is available into the container iterator->itemFound point to this element
 * @note If nothing found (no next element) then iterator->itemID >= csi.numODs
 */
static void UI_ContainerItemIteratorNext (containerItemIterator_t *iterator)
{
	assert(iterator->groupSteps[iterator->groupID] != CII_END);

	/* iterate each groups */
	for (; iterator->groupSteps[iterator->groupID] != CII_END; iterator->groupID++) {
		int filter = iterator->groupSteps[iterator->groupID];
		/* next */
		iterator->itemID++;

		/* iterate all item type*/
		for (;iterator->itemID < csi.numODs; iterator->itemID++) {
			qboolean isAmmo;
			qboolean isWeapon;
			qboolean isArmour;
			const objDef_t *obj = INVSH_GetItemByIDX(iterator->itemID);

			/* gameplay filter */
			if (!GAME_ItemIsUseable(obj))
				continue;

			/* type filter */
			/** @todo not sure its the right check */
			isArmour = INV_IsArmour(obj);
			isAmmo = obj->numWeapons != 0 && INV_IsAmmo(obj);
			isWeapon = obj->weapon || obj->isMisc || isArmour;

			if ((filter & CII_WEAPONONLY) && !isWeapon)
				continue;
			if ((filter & CII_AMMOONLY) && !isAmmo)
				continue;
			if (!INV_ItemMatchesFilter(obj, iterator->filterEquipType))
				continue;

			/* exists in inventory filter */
			iterator->itemFound = UI_ContainerNodeGetExistingItem(iterator->node, obj, iterator->filterEquipType);
			if ((filter & CII_AVAILABLEONLY) && iterator->itemFound == NULL)
				continue;
			if ((filter & CII_NOTAVAILABLEONLY) && iterator->itemFound != NULL)
				continue;

			/* we found something */
			return;
		}

		/* can we search into another group? */
		if (iterator->groupSteps[iterator->groupID + 1] != CII_END)
			iterator->itemID = -1;
	}

	/* clean up */
	iterator->itemFound = NULL;
}

/**
 * @brief Use a container node to init an item iterator
 */
static void UI_ContainerItemIteratorInit (containerItemIterator_t *iterator, const uiNode_t* const node)
{
	int groupID = 0;
	iterator->itemID = -1;
	iterator->groupID = 0;
	iterator->node = node;
	iterator->filterEquipType = EXTRADATACONST(node).filterEquipType;

	if (EXTRADATACONST(node).displayAvailableOnTop) {
		/* available items */
		if (EXTRADATACONST(node).displayWeapon)
			iterator->groupSteps[groupID++] = CII_WEAPONONLY | CII_AVAILABLEONLY;
		if (EXTRADATACONST(node).displayAmmo)
			iterator->groupSteps[groupID++] = CII_AMMOONLY | CII_AVAILABLEONLY;
		/* unavailable items */
		if (EXTRADATACONST(node).displayUnavailableItem) {
			if (EXTRADATACONST(node).displayWeapon)
				iterator->groupSteps[groupID++] = CII_WEAPONONLY | CII_NOTAVAILABLEONLY;
			if (EXTRADATACONST(node).displayAmmo)
				iterator->groupSteps[groupID++] = CII_AMMOONLY | CII_NOTAVAILABLEONLY;
		}
	} else {
		const int filter = (EXTRADATACONST(node).displayUnavailableItem) ? 0 : CII_AVAILABLEONLY;
		if (EXTRADATACONST(node).displayWeapon)
			iterator->groupSteps[groupID++] = CII_WEAPONONLY | filter;
		if (EXTRADATACONST(node).displayAmmo)
			iterator->groupSteps[groupID++] = CII_AMMOONLY | filter;
	}
	iterator->groupSteps[groupID++] = CII_END;

	/* find the first item */
	UI_ContainerItemIteratorNext(iterator);
}

/**
 * @brief Update display of scroll buttons.
 * @param[in] node Context node
 */
static void UI_BaseInventoryNodeUpdateScroll (uiNode_t* node)
{
	if (EXTRADATA(node).onViewChange) {
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
	}
}

/**
 * @brief Generate tooltip text for an item.
 * @param[in] item The item we want to generate the tooltip text for.
 * @param[in,out] tooltipText Pointer to a string the information should be written into.
 * @param[in] stringMaxLength Max. string size of @c tooltipText.
 * @return Number of lines
 * @todo Merge it with node container -- copy-paste of it
 */
static void UI_GetItemTooltip (item_t item, char *tooltipText, size_t stringMaxLength)
{
	assert(item.t);

	if (item.amount > 1)
		Com_sprintf(tooltipText, stringMaxLength, "%i x %s\n", item.amount, _(item.t->name));
	else
		Com_sprintf(tooltipText, stringMaxLength, "%s\n", _(item.t->name));

	/* Only display further info if item.t is researched */
	if (GAME_ItemIsUseable(item.t)) {
		if (item.t->weapon) {
			/* Get info about used ammo (if there is any) */
			if (item.t == item.m) {
				/* Item has no ammo but might have shot-count */
				if (item.a) {
					Q_strcat(tooltipText, va(_("Ammo: %i\n"), item.a), stringMaxLength);
				}
			} else if (item.m) {
				/* Search for used ammo and display name + ammo count */
				Q_strcat(tooltipText, va(_("%s loaded\n"), _(item.m->name)), stringMaxLength);
				Q_strcat(tooltipText, va(_("Ammo: %i\n"),  item.a), stringMaxLength);
			}
		} else if (item.t->numWeapons) {
			/* Check if this is a non-weapon and non-ammo item */
			if (!(item.t->numWeapons == 1 && item.t->weapons[0] == item.t)) {
				int i;
				/* If it's ammo get the weapon names it can be used in */
				Q_strcat(tooltipText, _("Usable in:\n"), stringMaxLength);
				for (i = 0; i < item.t->numWeapons; i++) {
					const objDef_t *weapon = item.t->weapons[i];
					if (GAME_ItemIsUseable(weapon)) {
						Q_strcat(tooltipText, va("* %s\n", _(weapon->name)), stringMaxLength);
					}
				}
			}
		}
	}
}

/**
 * @brief Calculates the size of a container node and links the container
 * into the node (uses the @c invDef_t shape bitmask to determine the size)
 * @param[in,out] node The node to get the size for
 */
static void UI_BaseInventoryNodeLoaded (uiNode_t* const node)
{
	EXTRADATA(node).super.container = INVSH_GetInventoryDefinitionByID("equip");
}

static const vec3_t scale = {3.5, 3.5, 3.5};
/** @todo it may be nice to vectorise that */
static const vec4_t colorDefault = {1, 1, 1, 1};
static const vec4_t colorLoadable = {0.5, 1, 0.5, 1};
static const vec4_t colorDisabledHiden = {0.5, 0.5, 0.5, 0.5};
static const vec4_t colorDisabledLoadable = {0.5, 0.25, 0.25, 1.0};

/**
 * @brief Draw the base inventory
 * @return The full height requested by the current view (not the node height)
 */
static int UI_BaseInventoryNodeDrawItems (uiNode_t *node, const objDef_t *highlightType)
{
	qboolean outOfNode = qfalse;
	vec2_t nodepos;
	int items = 0;
	int rowHeight = 0;
	const int cellWidth = node->size[0] / EXTRADATA(node).columns;
	containerItemIterator_t iterator;
	int currentHeight = 0;
	UI_GetNodeAbsPos(node, nodepos);

	UI_ContainerItemIteratorInit(&iterator, node);
	for (; iterator.itemID < csi.numODs; UI_ContainerItemIteratorNext(&iterator)) {
		const int id = iterator.itemID;
		const objDef_t *obj = INVSH_GetItemByIDX(id);
		item_t tempItem = {1, NULL, obj, 0, 0};
		vec3_t pos;
		vec3_t ammopos;
		const float *color;
		qboolean isHighlight = qfalse;
		int amount;
		const int col = items % EXTRADATA(node).columns;
		int cellHeight = 0;
		const invList_t *icItem = iterator.itemFound;

		/* skip items over and bellow the node view */
		if (outOfNode || currentHeight < EXTRADATA(node).scrollY.viewPos) {
			int height;
			R_FontTextSize("f_verysmall", _(obj->name),
				cellWidth - 5, LONGLINES_WRAP, NULL, &height, NULL, NULL);
			height += obj->sy * C_UNIT + 10;
			if (height > rowHeight)
				rowHeight = height;

			if (outOfNode || currentHeight + rowHeight < EXTRADATA(node).scrollY.viewPos) {
				if (col == EXTRADATA(node).columns - 1) {
					currentHeight += rowHeight;
					rowHeight = 0;
				}
				items++;
				continue;
			}
		}

		Vector2Copy(nodepos, pos);
		pos[0] += cellWidth * col;
		pos[1] += currentHeight - EXTRADATA(node).scrollY.viewPos;
		pos[2] = 0;

		if (highlightType) {
			if (INV_IsAmmo(obj))
				isHighlight = INVSH_LoadableInWeapon(obj, highlightType);
			else
				isHighlight = INVSH_LoadableInWeapon(highlightType, obj);
		}

		if (icItem != NULL) {
			if (isHighlight)
				color = colorLoadable;
			else
				color = colorDefault;
		} else {
			if (isHighlight)
				color = colorDisabledLoadable;
			else
				color = colorDisabledHiden;
		}

		if (icItem)
			amount = icItem->item.amount;
		else
			amount = 0;

		/* draw item */
		pos[0] += obj->sx * C_UNIT / 2.0;
		pos[1] += obj->sy * C_UNIT / 2.0;
		UI_DrawItem(node, pos, &tempItem, -1, -1, scale, color);
		UI_DrawString("f_verysmall", ALIGN_LC,
			pos[0] + obj->sx * C_UNIT / 2.0, pos[1] + obj->sy * C_UNIT / 2.0,
			pos[0] + obj->sx * C_UNIT / 2.0, cellWidth - 5, /* maxWidth */
			0, va("x%i", amount), 0, 0, NULL, qfalse, 0);
		pos[0] -= obj->sx * C_UNIT / 2.0;
		pos[1] += obj->sy * C_UNIT / 2.0;
		cellHeight += obj->sy * C_UNIT;

		/* save position for ammo */
		Vector2Copy(pos, ammopos);
		ammopos[2] = 0;
		ammopos[0] += obj->sx * C_UNIT + 10;

		/* draw the item name. */
		cellHeight += UI_DrawString("f_verysmall", ALIGN_UL,
			pos[0], pos[1],
			pos[0], cellWidth - 5, /* max width */
			0, _(obj->name), 0, 0, NULL, qfalse, LONGLINES_WRAP);

		/* draw ammos of weapon */
		if (obj->weapon && EXTRADATA(node).displayAmmoOfWeapon) {
			int ammoIdx;
			for (ammoIdx = 0; ammoIdx < obj->numAmmos; ammoIdx++) {
				tempItem.t = obj->ammos[ammoIdx];

				/* skip unusable ammo */
				if (!GAME_ItemIsUseable(tempItem.t))
					continue;

				/* find and skip none existing ammo */
				icItem = UI_ContainerNodeGetExistingItem(node, tempItem.t, EXTRADATA(node).filterEquipType);
				if (!icItem)
					continue;

				/* Calculate the center of the item model/image. */
				ammopos[0] += icItem->item.t->sx * C_UNIT / 2.0;
				ammopos[1] -= icItem->item.t->sy * C_UNIT / 2.0;
				UI_DrawItem(node, ammopos, &tempItem, -1, -1, scale, colorDefault);
				UI_DrawString("f_verysmall", ALIGN_LC,
					ammopos[0] + icItem->item.t->sx * C_UNIT / 2.0, ammopos[1] + icItem->item.t->sy * C_UNIT / 2.0,
					ammopos[0] + icItem->item.t->sx * C_UNIT / 2.0, cellWidth - 5 - ammopos[0],	/* maxWidth */
					0, va("x%i", icItem->item.amount), 0, 0, NULL, qfalse, 0);
				ammopos[0] += icItem->item.t->sx * C_UNIT / 2.0;
				ammopos[1] += icItem->item.t->sy * C_UNIT / 2.0;
			}
		}
		cellHeight += 10;

		if (cellHeight > rowHeight) {
			rowHeight = cellHeight;
		}

		/* add a marge between rows */
		if (col == EXTRADATA(node).columns - 1) {
			currentHeight += rowHeight;
			rowHeight = 0;
			if (currentHeight - EXTRADATA(node).scrollY.viewPos >= node->size[1])
				outOfNode = qtrue;
		}

		/* count items */
		items++;
	}

	if (rowHeight != 0) {
		currentHeight += rowHeight;
	}
	return currentHeight;
}

/**
 * @brief Draw the inventory of the base
 */
static void UI_BaseInventoryNodeDraw2 (uiNode_t *node, const objDef_t *highlightType)
{
	qboolean updateScroll = qfalse;
	int visibleHeight = 0;
	int needHeight = 0;
	vec2_t screenPos;

	UI_GetNodeScreenPos(node, screenPos);
	R_PushClipRect(screenPos[0], screenPos[1], node->size[0], node->size[1]);

	needHeight = UI_BaseInventoryNodeDrawItems(node, highlightType);

	R_PopClipRect();
	visibleHeight = node->size[1];

#if 0
	R_FontDrawString("f_verysmall", ALIGN_UL,
		node->pos[0], node->pos[1], node->pos[0], node->pos[1],
		0,	0,	/* maxWidth/maxHeight */
		0, va("%i %i/%i", EXTRADATA(node).scrollCur, visibleRows, totalRows), 0, 0, NULL, qfalse, 0);
#endif

	/* Update display of scroll buttons if something changed. */
	if (visibleHeight != EXTRADATA(node).scrollY.viewSize || needHeight != EXTRADATA(node).scrollY.fullSize) {
		EXTRADATA(node).scrollY.fullSize = needHeight;
		EXTRADATA(node).scrollY.viewSize = visibleHeight;
		updateScroll = qtrue;
	}
	if (EXTRADATA(node).scrollY.viewPos > needHeight - visibleHeight) {
		EXTRADATA(node).scrollY.viewPos = needHeight - visibleHeight;
		updateScroll = qtrue;
	}
	if (EXTRADATA(node).scrollY.viewPos < 0) {
		EXTRADATA(node).scrollY.viewPos = 0;
		updateScroll = qtrue;
	}

	if (updateScroll)
		UI_BaseInventoryNodeUpdateScroll(node);
}

/**
 * @brief Main function to draw a container node
 */
static void UI_BaseInventoryNodeDraw (uiNode_t *node)
{
	const objDef_t *highlightType = NULL;

	if (!EXTRADATA(node).super.container)
		return;
	if (!ui_inventory)
		return;
	/* is container invisible */
	if (node->color[3] < 0.001)
		return;

	/* Highlight weapons that the dragged ammo (if it is one) can be loaded into. */
	if (UI_DNDIsDragging() && UI_DNDGetType() == DND_ITEM) {
		highlightType = UI_DNDGetItem()->t;
	}

	UI_BaseInventoryNodeDraw2(node, highlightType);
}

/**
 * @note this function is a copy-paste of UI_ContainerNodeDrawItems (with remove of unneeded code)
 */
static invList_t *UI_BaseInventoryNodeGetItem (const uiNode_t* const node, int mouseX, int mouseY, int* contX, int* contY)
{
	qboolean outOfNode = qfalse;
	vec2_t nodepos;
	int items = 0;
	int rowHeight = 0;
	const int cellWidth = node->size[0] / EXTRADATACONST(node).columns;
	int tempX, tempY;
	containerItemIterator_t iterator;
	int currentHeight = 0;

	if (!contX)
		contX = &tempX;
	if (!contY)
		contY = &tempY;

	UI_GetNodeAbsPos(node, nodepos);

	UI_ContainerItemIteratorInit(&iterator, node);
	for (; iterator.itemID < csi.numODs; UI_ContainerItemIteratorNext(&iterator)) {
		const int id = iterator.itemID;
		const objDef_t *obj = INVSH_GetItemByIDX(id);
		vec2_t pos;
		vec2_t ammopos;
		const int col = items % EXTRADATACONST(node).columns;
		int cellHeight = 0;
		invList_t *icItem = iterator.itemFound;
		int height;

		/* skip items over and bellow the node view */
		if (outOfNode || currentHeight < EXTRADATACONST(node).scrollY.viewPos) {
			int outHeight;
			R_FontTextSize("f_verysmall", _(obj->name),
				cellWidth - 5, LONGLINES_WRAP, NULL, &outHeight, NULL, NULL);
			outHeight += obj->sy * C_UNIT + 10;
			if (outHeight > rowHeight)
				rowHeight = outHeight;

			if (outOfNode || currentHeight + rowHeight < EXTRADATACONST(node).scrollY.viewPos) {
				if (col == EXTRADATACONST(node).columns - 1) {
					currentHeight += rowHeight;
					rowHeight = 0;
				}
				items++;
				continue;
			}
		}

		Vector2Copy(nodepos, pos);
		pos[0] += cellWidth * col;
		pos[1] += currentHeight - EXTRADATACONST(node).scrollY.viewPos;

		/* check item */
		if (mouseY < pos[1])
			break;
		if (mouseX >= pos[0] && mouseX < pos[0] + obj->sx * C_UNIT
		 && mouseY >= pos[1] && mouseY < pos[1] + obj->sy * C_UNIT) {
			if (icItem) {
				*contX = icItem->x;
				*contY = icItem->y;
				return icItem;
			} else {
				return NULL;
			}
		}
		pos[1] += obj->sy * C_UNIT;
		cellHeight += obj->sy * C_UNIT;

		/* save position for ammo */
		Vector2Copy(pos, ammopos);
		ammopos[0] += obj->sx * C_UNIT + 10;

		/* draw the item name. */
		R_FontTextSize("f_verysmall", _(obj->name),
			cellWidth - 5, LONGLINES_WRAP, NULL, &height, NULL, NULL);
		cellHeight += height;

		/* draw ammos of weapon */
		if (obj->weapon && EXTRADATACONST(node).displayAmmoOfWeapon) {
			int ammoIdx;
			for (ammoIdx = 0; ammoIdx < obj->numAmmos; ammoIdx++) {
				const objDef_t *objammo = obj->ammos[ammoIdx];

				/* skip unusable ammo */
				if (!GAME_ItemIsUseable(objammo))
					continue;

				/* find and skip none existing ammo */
				icItem = UI_ContainerNodeGetExistingItem(node, objammo, EXTRADATACONST(node).filterEquipType);
				if (!icItem)
					continue;

				/* check ammo (ammopos in on the left-lower corner) */
				if (mouseX < ammopos[0] || mouseY >= ammopos[1])
					break;
				if (mouseX >= ammopos[0] && mouseX < ammopos[0] + objammo->sx * C_UNIT
				 && mouseY >= ammopos[1] - objammo->sy * C_UNIT && mouseY < ammopos[1]) {
					*contX = icItem->x;
					*contY = icItem->y;
					return icItem;
				}
				ammopos[0] += objammo->sx * C_UNIT;
			}
		}
		cellHeight += 10;

		if (cellHeight > rowHeight) {
			rowHeight = cellHeight;
		}

		/* add a margin between rows */
		if (col == EXTRADATACONST(node).columns - 1) {
			currentHeight += rowHeight;
			rowHeight = 0;
			if (currentHeight - EXTRADATACONST(node).scrollY.viewPos >= node->size[1])
				return NULL;
		}

		/* count items */
		items++;
	}

	*contX = NONE;
	*contY = NONE;
	return NULL;
}

/**
 * @brief Custom tooltip for container node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void UI_BaseInventoryNodeDrawTooltip (uiNode_t *node, int x, int y)
{
	static char tooltiptext[MAX_VAR * 2];
	const invList_t *itemHover;
	vec2_t nodepos;

	UI_GetNodeAbsPos(node, nodepos);

	/* Find out where the mouse is. */
	itemHover = UI_BaseInventoryNodeGetItem(node, x, y, NULL, NULL);

	if (itemHover) {
		const int itemToolTipWidth = 250;

		/* Get name and info about item */
		UI_GetItemTooltip(itemHover->item, tooltiptext, sizeof(tooltiptext));
#ifdef DEBUG
		/* Display stored container-coordinates of the item. */
		Q_strcat(tooltiptext, va("\n%i/%i", itemHover->x, itemHover->y), sizeof(tooltiptext));
#endif
		UI_DrawTooltip(tooltiptext, x, y, itemToolTipWidth);
	}
}

/**
 * @brief Try to autoplace an item at a position
 * when right-click was used in the inventory.
 * @param[in] node The context node
 * @param[in] mouseX X mouse coordinates.
 * @param[in] mouseY Y mouse coordinates.
 */
static void UI_ContainerNodeAutoPlace (uiNode_t* node, int mouseX, int mouseY)
{
	invList_t *ic;
	int fromX, fromY;
	int sel;

	if (!ui_inventory)
		return;

	/* don't allow this in tactical missions */
	if (CL_BattlescapeRunning())
		return;

	sel = cl_selected->integer;
	if (sel < 0)
		return;

	assert(EXTRADATA(node).super.container);

	ic = UI_BaseInventoryNodeGetItem(node, mouseX, mouseY, &fromX, &fromY);
	Com_DPrintf(DEBUG_CLIENT, "UI_ContainerNodeAutoPlace: item %i/%i selected from scrollable container.\n", fromX, fromY);
	if (!ic)
		return;
	UI_ContainerNodeAutoPlaceItem(node, ic);

	/* Update display of scroll buttons. */
	UI_BaseInventoryNodeUpdateScroll(node);
}

static int oldMouseX = 0;
static int oldMouseY = 0;

static void UI_BaseInventoryNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	const int delta = abs(oldMouseX - x) + abs(oldMouseY - y);
	if (delta > 15) {
		UI_DNDDragItem(node, &(dragInfoIC->item));
		UI_MouseRelease();
	}
}

static void UI_BaseInventoryNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	switch (button) {
	case K_MOUSE1:
	{
		/* start drag and drop */
		int fromX, fromY;
		dragInfoIC = UI_BaseInventoryNodeGetItem(node, x, y, &fromX, &fromY);
		if (dragInfoIC) {
			dragInfoFromX = fromX;
			dragInfoFromY = fromY;
			oldMouseX = x;
			oldMouseY = y;
			UI_SetMouseCapture(node);
			EXTRADATA(node).super.lastSelectedId = dragInfoIC->item.t->idx;
			if (EXTRADATA(node).super.onSelect) {
				UI_ExecuteEventActions(node, EXTRADATA(node).super.onSelect);
			}
		}
		break;
	}
	case K_MOUSE2:
		if (UI_DNDIsDragging()) {
			UI_DNDAbort();
		} else {
			/* auto place */
			UI_ContainerNodeAutoPlace(node, x, y);
		}
		break;
	default:
		break;
	}
}

static void UI_BaseInventoryNodeMouseUp (uiNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	if (UI_GetMouseCapture() == node) {
		UI_MouseRelease();
	}
	if (UI_DNDIsDragging()) {
		UI_DNDDrop();
	}
}

static qboolean UI_BaseInventoryNodeWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean down = deltaY > 0;
	const int delta = 20;
	if (deltaY == 0)
		return qfalse;
	if (down) {
		const int lenght = EXTRADATA(node).scrollY.fullSize - EXTRADATA(node).scrollY.viewSize;
		if (EXTRADATA(node).scrollY.viewPos < lenght) {
			EXTRADATA(node).scrollY.viewPos += delta;
			if (EXTRADATA(node).scrollY.viewPos > lenght)
				EXTRADATA(node).scrollY.viewPos = lenght;
			UI_BaseInventoryNodeUpdateScroll(node);
		}
	} else {
		if (EXTRADATA(node).scrollY.viewPos > 0) {
			EXTRADATA(node).scrollY.viewPos -= delta;
			if (EXTRADATA(node).scrollY.viewPos < 0)
				EXTRADATA(node).scrollY.viewPos = 0;
			UI_BaseInventoryNodeUpdateScroll(node);
		}
	}
	return qtrue;
}

static void UI_BaseInventoryNodeLoading (uiNode_t *node)
{
	EXTRADATA(node).super.container = NULL;
	EXTRADATA(node).columns = 1;
	node->color[3] = 1.0;
}

/**
 * @brief Call when a DND enter into the node
 */
static qboolean UI_BaseInventoryNodeDNDEnter (uiNode_t *target)
{
	/* The node is invalide */
	if (EXTRADATA(target).super.container == NULL)
		return qfalse;
	/* accept items only, if we have a container */
	return UI_DNDGetType() == DND_ITEM && UI_DNDGetSourceNode() != target;
}

/**
 * @brief Call into the target when the DND hover it
 * @return True if the DND is accepted
 */
static qboolean UI_BaseInventoryNodeDNDMove (uiNode_t *target, int x, int y)
{
	return qtrue;
}

/**
 * @brief Call when a DND enter into the node
 */
static void UI_BaseInventoryNodeDNDLeave (uiNode_t *node)
{
}

/**
 * @brief Call when we open the window containing the node
 */
static void UI_BaseInventoryNodeInit (uiNode_t *node, linkedList_t *params)
{
}

void UI_RegisterBaseInventoryNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "baseinventory";
	behaviour->extends = "container";
	behaviour->draw = UI_BaseInventoryNodeDraw;
	behaviour->drawTooltip = UI_BaseInventoryNodeDrawTooltip;
	behaviour->mouseDown = UI_BaseInventoryNodeMouseDown;
	behaviour->mouseUp = UI_BaseInventoryNodeMouseUp;
	behaviour->scroll = UI_BaseInventoryNodeWheel;
	behaviour->capturedMouseMove = UI_BaseInventoryNodeCapturedMouseMove;
	behaviour->windowOpened = UI_BaseInventoryNodeInit;
	behaviour->loading = UI_BaseInventoryNodeLoading;
	behaviour->loaded = UI_BaseInventoryNodeLoaded;
	behaviour->dndEnter = UI_BaseInventoryNodeDNDEnter;
	behaviour->dndMove = UI_BaseInventoryNodeDNDMove;
	behaviour->dndLeave = UI_BaseInventoryNodeDNDLeave;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Display/hide weapons. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayweapon", V_BOOL, baseInventoryExtraData_t, displayWeapon);
	/* Display/hide ammo. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayammo", V_BOOL, baseInventoryExtraData_t, displayAmmo);
	/* Display/hide out of stock items. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayunavailableitem", V_BOOL, baseInventoryExtraData_t, displayUnavailableItem);
	/* Sort the list to display in stock items on top of the list. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayavailableontop", V_BOOL, baseInventoryExtraData_t, displayAvailableOnTop);
	/* Display/hide ammo near weapons. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayammoofweapon", V_BOOL, baseInventoryExtraData_t, displayAmmoOfWeapon);
	/* Display/hide out of stock ammo near weapons. <code>displayammoofweapon</code> must be activated first. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayunavailableammoofweapon", V_BOOL, baseInventoryExtraData_t, displayUnavailableAmmoOfWeapon);
	/* Custom the number of column we must use to display items. */
	UI_RegisterExtradataNodeProperty(behaviour, "columns", V_INT, baseInventoryExtraData_t, columns);
	/* Filter items by a category. */
	UI_RegisterExtradataNodeProperty(behaviour, "filter", V_INT, baseInventoryExtraData_t, filterEquipType);

	/* Position of the vertical view (into the full number of elements the node contain) */
	UI_RegisterExtradataNodeProperty(behaviour, "viewpos", V_INT, baseInventoryExtraData_t, scrollY.viewPos);
	/* Size of the vertical view (proportional to the number of elements the node can display without moving) */
	UI_RegisterExtradataNodeProperty(behaviour, "viewsize", V_INT, baseInventoryExtraData_t, scrollY.viewSize);
	/* Full vertical size (proportional to the number of elements the node contain) */
	UI_RegisterExtradataNodeProperty(behaviour, "fullsize", V_INT, baseInventoryExtraData_t, scrollY.fullSize);
	/* Called when one of the properties viewpos/viewsize/fullsize change */
	UI_RegisterExtradataNodeProperty(behaviour, "onviewchange", V_UI_ACTION, baseInventoryExtraData_t, onViewChange);

	Com_RegisterConstInt("FILTER_S_PRIMARY", FILTER_S_PRIMARY);
	Com_RegisterConstInt("FILTER_S_SECONDARY", FILTER_S_SECONDARY);
	Com_RegisterConstInt("FILTER_S_HEAVY", FILTER_S_HEAVY);
	Com_RegisterConstInt("FILTER_S_MISC", FILTER_S_MISC);
	Com_RegisterConstInt("FILTER_S_ARMOUR", FILTER_S_ARMOUR);
	Com_RegisterConstInt("FILTER_CRAFTITEM", FILTER_CRAFTITEM);
	Com_RegisterConstInt("FILTER_UGVITEM", FILTER_UGVITEM);
	Com_RegisterConstInt("FILTER_AIRCRAFT", FILTER_AIRCRAFT);
	Com_RegisterConstInt("FILTER_DUMMY", FILTER_DUMMY);
	Com_RegisterConstInt("FILTER_DISASSEMBLY", FILTER_DISASSEMBLY);
}
