/**
 * @file ui_node_container.c
 * @brief The container node refer to 3 different nodes merged into a singler one. Both
 * can drag and drop solider items from a container to another one. The first container
 * is a soldier slot. For example, the left arm, the bag pack... The second is the base
 * item list. And the last it a floor container used into the battlescape. The node name
 * itself is used to know the container role.
 * @todo Move base container list outside
 * @todo Move container role outside of the node name
 * @todo Link soldier container with a soldier
 * @todo Link base container with a base
 * @todo Link floor container with a map/cell...
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../ui_actions.h"
#include "../ui_dragndrop.h"
#include "../ui_tooltip.h"
#include "../ui_nodes.h"
#include "../ui_input.h"
#include "../ui_render.h"
#include "ui_node_model.h"
#include "ui_node_container.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_mesh.h"
#include "../../cl_game.h"
#include "../../cl_team.h"
#include "../../battlescape/cl_actor.h"
#include "../../cl_inventory.h"

/**
 * @todo need refactoring to remove, reduce use... of that var
 * Global access to many node content like that is very bad.
 */
inventory_t *ui_inventory = NULL;

#define EXTRADATA_TYPE containerExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * self cache for drag item
 * @note we can use a global variable because we only can have 1 source node at a time
 */
static int dragInfoFromX = -1;
static int dragInfoFromY = -1;

/**
 * self cache for the preview and dropped item
 * @note we can use a global variable because we only can have 1 target node at a time
 */
static int dragInfoToX = -1;
static int dragInfoToY = -1;

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
static invList_t *UI_ContainerNodeGetExistingItem (const uiNode_t *node, objDef_t *item, const itemFilterTypes_t filterType)
{
	return INVSH_SearchInInventoryWithFilter(ui_inventory, EXTRADATACONST(node).container, NONE, NONE, item, filterType);
}

/**
 * @brief Update display of scroll buttons.
 * @note The cvars "mn_cont_scroll_prev_hover" and "mn_cont_scroll_next_hover" are
 * set by the "in" and "out" functions of the scroll buttons.
 * @param[in] node Context node
 */
static void UI_ContainerNodeUpdateScroll (uiNode_t* node)
{
	if (EXTRADATA(node).onViewChange) {
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
	}
}

static inline qboolean UI_IsScrollContainerNode (const uiNode_t* const node)
{
	return EXTRADATACONST(node).container && EXTRADATACONST(node).container->scroll;
}

/**
 * @brief Fills the ground container of the ui_inventory with unused items from a given
 * equipment definition
 * @note Keep in mind that @c ed is changed here - so items are removed and the ground container
 * of a inventory definition is in general a temp container - that means you should make a copy
 * of the @c equipDef_t you want to add to the temp ground container of the given @c inv
 * @todo it's not obvious for the caller that @c ui_inventory pointer must be set
 * @param[in,out] inv The inventory to add the unused items from @c ed to
 * @param[in,out] ed The equipment definition to get the used items from that should be added
 * to the ground container of @c inv
 * @todo dont use, and dont called by the container node; should we move it outside?
 */
void UI_ContainerNodeUpdateEquipment (inventory_t *inv, equipDef_t *ed)
{
	int i;

	/* a 'tiny hack' to add the remaining equipment (not carried)
	 * correctly into buy categories, reloading at the same time;
	 * it is valid only due to the following property: */
	assert(MAX_CONTAINERS >= FILTER_AIRCRAFT);

	for (i = 0; i < csi.numODs; i++) {
		objDef_t *od = INVSH_GetItemByIDX(i);
		/* Don't allow to show unuseable items. */
		if (!GAME_ItemIsUseable(od))
			continue;

		while (ed->numItems[i]) {
			const item_t item = {NONE_AMMO, NULL, od, 0, 0};
			if (!cls.i.AddToInventory(&cls.i, inv, item, INVDEF(csi.idEquip), NONE, NONE, 1)) {
				/* no space left in the inventory */
				break;
			}
			ed->numItems[item.t->idx]--;
		}
	}

	/* First-time linking of ui_inventory. */
	if (ui_inventory && !ui_inventory->c[csi.idEquip]) {
		ui_inventory->c[csi.idEquip] = inv->c[csi.idEquip];
	}
}

/**
 * @brief Draws an item to the screen
 * @param[in] node Context node
 * @param[in] org Node position on the screen (pixel). Single nodes: Use the center of the node.
 * @param[in] item The item to draw.
 * @param[in] x Position in container. Set this to -1 if it's drawn in a single container.
 * @param[in] y Position in container. Set this to -1 if it's drawn in a single container.
 * @param[in] scale
 * @param[in] color
 * @sa SCR_DrawCursor
 * Used to draw an item to the equipment containers. First look whether the objDef_t
 * includes an image - if there is none then draw the model
 */
void UI_DrawItem (uiNode_t *node, const vec3_t org, const item_t *item, int x, int y, const vec3_t scale, const vec4_t color)
{
	objDef_t *od = item->t;
	vec4_t col;
	vec3_t origin;

	assert(od);
	assert(org[2] > -1000 && org[2] < 1000); 	/*< prevent use of vec2_t for org */

	Vector4Copy(color, col);
	/* no ammo in this weapon - highlight this item */
	if (od->weapon && od->reload && !item->a) {
		col[1] *= 0.5;
		col[2] *= 0.5;
	}

	VectorCopy(org, origin);

	/* Calculate correct location of the image or the model (depends on rotation) */
	/** @todo Change the rotation of the image as well, right now only the location is changed.
	 * How is image-rotation handled right now? */
	if (x >= 0 || y >= 0) {
		/* Add offset of location in container. */
		origin[0] += x * C_UNIT;
		origin[1] += y * C_UNIT;

		/* Add offset for item-center (depends on rotation). */
		if (item->rotated) {
			origin[0] += od->sy * C_UNIT / 2.0;
			origin[1] += od->sx * C_UNIT / 2.0;
			/** @todo Image size calculation depends on handling of image-rotation.
			imgWidth = od->sy * C_UNIT;
			imgHeight = od->sx * C_UNIT;
			*/
		} else {
			origin[0] += od->sx * C_UNIT / 2.0;
			origin[1] += od->sy * C_UNIT / 2.0;
		}
	}

	/* don't handle the od->tech->image here - it's very ufopedia specific in most cases */
	if (od->image[0] != '\0') {
		const int imgWidth = od->sx * C_UNIT;
		const int imgHeight = od->sy * C_UNIT;
		origin[0] -= od->sx * C_UNIT / 2.0;
		origin[1] -= od->sy * C_UNIT / 2.0;

		/* Draw the image. */
		R_Color(color);
		UI_DrawNormImageByName(origin[0], origin[1], imgWidth, imgHeight, 0, 0, 0, 0, od->image);
		R_Color(NULL);
	} else {
		uiModel_t *model = NULL;
		const char *modelName = GAME_GetModelForItem(od, &model);

		/* no model definition in the tech struct, not in the fallback object definition */
		if (modelName == NULL || modelName[0] == '\0') {
			Com_Printf("UI_DrawItem: No model given for item: '%s'\n", od->id);
			return;
		}

		if (model && node) {
			UI_DrawModelNode(node, modelName);
		} else {
			modelInfo_t mi;
			vec3_t angles = {-10, 160, 70};
			vec3_t size = {scale[0], scale[1], scale[2]};
			vec3_t center;

			if (item->rotated)
				angles[0] -= 90;

			if (od->scale)
				VectorScale(size, od->scale, size);

			VectorNegate(od->center, center);

			memset(&mi, 0, sizeof(mi));
			mi.origin = origin;
			mi.angles = angles;
			mi.center = center;
			mi.scale = size;
			mi.color = col;
			mi.name = modelName;

			/* draw the model */
			R_DrawModelDirect(&mi, NULL, NULL);
		}
	}
}

/**
 * @brief Generate tooltip text for an item.
 * @param[in] item The item we want to generate the tooltip text for.
 * @param[in,out] tooltipText Pointer to a string the information should be written into.
 * @param[in] stringMaxLength Max. string size of @c tooltipText.
 * @return Number of lines
 */
static void UI_GetItemTooltip (item_t item, char *tooltipText, size_t stringMaxLength)
{
	objDef_t *weapon;

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
					weapon = item.t->weapons[i];
					if (GAME_ItemIsUseable(weapon)) {
						Q_strcat(tooltipText, va("* %s\n", _(weapon->name)), stringMaxLength);
					}
				}
			}
		}
	}
}

/**
 * @brief Search a child container node by the given container id
 * @note Only search with one depth
 */
uiNode_t *UI_GetContainerNodeByContainerIDX (const uiNode_t* const parent, const int index)
{
	const invDef_t* const container = INVDEF(index);
	uiNode_t *containerNode = UI_GetNode(parent, container->name);

	return containerNode;
}

/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
static void UI_DrawDisabled (const uiNode_t* node)
{
	const vec4_t color = { 0.3f, 0.3f, 0.3f, 0.7f };
	vec2_t nodepos;

	UI_GetNodeAbsPos(node, nodepos);
	UI_DrawFill(nodepos[0], nodepos[1], node->size[0], node->size[1], color);
}

/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
static void UI_DrawFree (containerIndex_t container, const uiNode_t *node, int posx, int posy, int sizex, int sizey, qboolean showTUs)
{
	const vec4_t color = { 0.0f, 1.0f, 0.0f, 0.7f };
	invDef_t* inv = INVDEF(container);
	vec2_t nodepos;

	UI_GetNodeAbsPos(node, nodepos);
	UI_DrawFill(posx, posy, sizex, sizey, color);

	/* if showTUs is true (only the first time in none single containers)
	 * and we are connected to a game */
	if (showTUs && CL_BattlescapeRunning()) {
		UI_DrawString("f_verysmall", ALIGN_UL, nodepos[0] + 3, nodepos[1] + 3,
			nodepos[0] + 3, node->size[0] - 6, 0,
			va(_("In: %i Out: %i"), inv->in, inv->out), 0, 0, NULL, qfalse, 0);
	}
}

/**
 * @brief Draws the free and usable inventory positions when dragging an item.
 * @note Only call this function in dragging mode
 */
static void UI_ContainerNodeDrawFreeSpace (uiNode_t *node, inventory_t *inv)
{
	const objDef_t *od = UI_DNDGetItem()->t;	/**< Get the 'type' of the dragged item. */
	vec2_t nodepos;

	/* Draw only in dragging-mode and not for the equip-floor */
	assert(UI_DNDIsDragging());
	assert(inv);

	UI_GetNodeAbsPos(node, nodepos);
	/* if single container (hands, extension, headgear) */
	if (EXTRADATA(node).container->single) {
		/* if container is free or the dragged-item is in it */
		if (UI_DNDIsSourceNode(node) || INVSH_CheckToInventory(inv, od, EXTRADATA(node).container, 0, 0, dragInfoIC))
			UI_DrawFree(EXTRADATA(node).container->id, node, nodepos[0], nodepos[1], node->size[0], node->size[1], qtrue);
	} else {
		/* The shape of the free positions. */
		uint32_t free[SHAPE_BIG_MAX_HEIGHT];
		qboolean showTUs = qtrue;
		int x, y;

		memset(free, 0, sizeof(free));

		for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++) {
			for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++) {
				/* Check if the current position is usable (topleft of the item). */

				/* Add '1's to each position the item is 'blocking'. */
				const int checkedTo = INVSH_CheckToInventory(inv, od, EXTRADATA(node).container, x, y, dragInfoIC);
				if (checkedTo & INV_FITS)				/* Item can be placed normally. */
					INVSH_MergeShapes(free, (uint32_t)od->shape, x, y);
				if (checkedTo & INV_FITS_ONLY_ROTATED)	/* Item can be placed rotated. */
					INVSH_MergeShapes(free, INVSH_ShapeRotate((uint32_t)od->shape), x, y);

				/* Only draw on existing positions. */
				if (INVSH_CheckShape(EXTRADATA(node).container->shape, x, y)) {
					if (INVSH_CheckShape(free, x, y)) {
						UI_DrawFree(EXTRADATA(node).container->id, node, nodepos[0] + x * C_UNIT, nodepos[1] + y * C_UNIT, C_UNIT, C_UNIT, showTUs);
						showTUs = qfalse;
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
static void UI_ContainerNodeLoaded (uiNode_t* const node)
{
	const char *name;
	invDef_t *container;

	/** @todo find a better way to add more equip node, without this hack */
	name = node->name;
	if (!strncmp(node->name, "equip_", 6))
		name = "equip";

	container = INVSH_GetInventoryDefinitionByID(name);
	if (container == NULL)
		return;

	EXTRADATA(node).container = container;

	if (UI_IsScrollContainerNode(node)) {
		/* No need to compute the size, the script provide it */
	} else {
		int i, j;
		/* Start on the last bit of the shape mask. */
		for (i = SHAPE_BIG_MAX_WIDTH - 1; i >= 0; i--) {
			for (j = 0; j < SHAPE_BIG_MAX_HEIGHT; j++)
				if (container->shape[j] & (1 << i))
					break;
			if (j < SHAPE_BIG_MAX_HEIGHT)
				break;
		}
		node->size[0] = C_UNIT * (i + 1) + 0.01;

		/* start on the lower row of the shape mask */
		for (i = SHAPE_BIG_MAX_HEIGHT - 1; i >= 0; i--)
			if (container->shape[i] & ~0x0)
				break;
		node->size[1] = C_UNIT * (i + 1) + 0.01;
	}
}

static const vec3_t scale = {3.5, 3.5, 3.5};
/** @todo it may be nice to vectorise that */
static const vec4_t colorDefault = {1, 1, 1, 1};
static const vec4_t colorLoadable = {0.5, 1, 0.5, 1};
static const vec4_t colorDisabled = {0.5, 0.5, 0.5, 1};
static const vec4_t colorDisabledHiden = {0.5, 0.5, 0.5, 0.5};
static const vec4_t colorDisabledLoadable = {0.5, 0.25, 0.25, 1.0};
static const vec4_t colorPreview = { 0.5, 0.5, 1, 1 };	/**< Make the preview item look bluish */

/**
 * @brief Draw a container which only contains one item
 * @param node
 * @param highlightType
 */
static void UI_ContainerNodeDrawSingle (uiNode_t *node, objDef_t *highlightType)
{
	vec4_t color;
	vec3_t pos;

	UI_GetNodeAbsPos(node, pos);
	pos[0] += node->size[0] / 2.0;
	pos[1] += node->size[1] / 2.0;
	pos[2] = 0;

	/* Single item container (special case for left hand). */
	if (INV_IsLeftDef(EXTRADATA(node).container) && !ui_inventory->c[csi.idLeft]) {
		if (ui_inventory->c[csi.idRight]) {
			const item_t *item = &ui_inventory->c[csi.idRight]->item;
			assert(item);
			assert(item->t);

			if (item->t->holdTwoHanded) {
				if (highlightType && INVSH_LoadableInWeapon(highlightType, item->t))
					memcpy(color, colorLoadable, sizeof(vec4_t));
				else
					memcpy(color, colorDefault, sizeof(vec4_t));
				color[3] = 0.5;
				UI_DrawItem(node, pos, item, -1, -1, scale, color);
			}
		}
	} else if (ui_inventory->c[EXTRADATA(node).container->id]) {
		qboolean disabled = qfalse;
		const item_t *item;

		if (ui_inventory->c[csi.idRight]) {
			item = &ui_inventory->c[csi.idRight]->item;
			/* If there is a weapon in the right hand that needs two hands to shoot it
			 * and there is a weapon in the left, then draw a disabled marker for the
			 * fireTwoHanded weapon. */
			assert(item);
			assert(item->t);
			if (INV_IsRightDef(EXTRADATA(node).container) && item->t->fireTwoHanded && ui_inventory->c[csi.idLeft]) {
				disabled = qtrue;
				UI_DrawDisabled(node);
			}
		}

		item = &ui_inventory->c[EXTRADATA(node).container->id]->item;
		assert(item);
		assert(item->t);
		if (highlightType && INVSH_LoadableInWeapon(highlightType, item->t)) {
			if (disabled)
				Vector4Copy(colorDisabledLoadable, color);
			else
				Vector4Copy(colorLoadable, color);
		} else {
			if (disabled)
				Vector4Copy(colorDisabled, color);
			else
				Vector4Copy(colorDefault, color);
		}
		if (disabled)
			color[3] = 0.5;
		UI_DrawItem(node, pos, item, -1, -1, scale, color);
	}
}

/**
 * @brief Draw a grip container
 */
static void UI_ContainerNodeDrawGrid (uiNode_t *node, objDef_t *highlightType)
{
	const invList_t *ic;
	vec3_t pos;

	UI_GetNodeAbsPos(node, pos);
	pos[2] = 0;

	for (ic = ui_inventory->c[EXTRADATA(node).container->id]; ic; ic = ic->next) {
		assert(ic->item.t);
		if (highlightType && INVSH_LoadableInWeapon(highlightType, ic->item.t))
			UI_DrawItem(node, pos, &ic->item, ic->x, ic->y, scale, colorLoadable);
		else
			UI_DrawItem(node, pos, &ic->item, ic->x, ic->y, scale, colorDefault);
	}
}

/**
 * @brief Draw a preview of the DND item dropped into the node
 */
static void UI_ContainerNodeDrawDropPreview (uiNode_t *target)
{
	item_t previewItem;
	int checkedTo;
	vec3_t origine;

	/* no preview into scrollable list */
	if (UI_IsScrollContainerNode(target))
		return;

	/* copy the DND item to not change the original one */
	previewItem = *UI_DNDGetItem();
	previewItem.rotated = qfalse;
	checkedTo = INVSH_CheckToInventory(ui_inventory, previewItem.t, EXTRADATA(target).container, dragInfoToX, dragInfoToY, dragInfoIC);
	if (checkedTo == INV_FITS_ONLY_ROTATED)
		previewItem.rotated = qtrue;

	/* no place found */
	if (!checkedTo)
		return;

	/* Hack, no preview for armour, we don't want it out of the armour container (and armour container is not visible) */
	if (INV_IsArmour(previewItem.t))
		return;

	UI_GetNodeAbsPos(target, origine);
	origine[2] = -40;

	/* Get center of single container for placement of preview item */
	if (EXTRADATA(target).container->single) {
		origine[0] += target->size[0] / 2.0;
		origine[1] += target->size[1] / 2.0;
	/* This is a "grid" container - we need to calculate the item-position
	 * (on the screen) from stored placement in the container and the
	 * calculated rotation info. */
	} else {
		if (previewItem.rotated) {
			origine[0] += (dragInfoToX + previewItem.t->sy / 2.0) * C_UNIT;
			origine[1] += (dragInfoToY + previewItem.t->sx / 2.0) * C_UNIT;
		} else {
			origine[0] += (dragInfoToX + previewItem.t->sx / 2.0) * C_UNIT;
			origine[1] += (dragInfoToY + previewItem.t->sy / 2.0) * C_UNIT;
		}
	}

	UI_DrawItem(NULL, origine, &previewItem, -1, -1, scale, colorPreview);
}

/**
 * @brief Main function to draw a container node
 */
static void UI_ContainerNodeDraw (uiNode_t *node)
{
	objDef_t *highlightType = NULL;

	if (!EXTRADATA(node).container)
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

	if (EXTRADATA(node).container->single) {
		UI_ContainerNodeDrawSingle(node, highlightType);
	} else {
		if (UI_IsScrollContainerNode(node)) {
			assert(qfalse);
		} else {
			UI_ContainerNodeDrawGrid(node, highlightType);
		}
	}

	/* Draw free space if dragging - but not for idEquip */
	if (UI_DNDIsDragging() && EXTRADATA(node).container->id != csi.idEquip)
		UI_ContainerNodeDrawFreeSpace(node, ui_inventory);

	if (UI_DNDIsTargetNode(node))
		UI_ContainerNodeDrawDropPreview(node);
}

/**
 * @brief Gets location of the item the mouse is over
 * @param[in] node The container-node.
 * @param[in] mouseX X location of the mouse.
 * @param[in] mouseY Y location of the mouse.
 * @param[out] contX X location in the container (index of item in row).
 * @param[out] contY Y location in the container (row).
 * @sa UI_ContainerNodeSearchInScrollableContainer
 */
static invList_t *UI_ContainerNodeGetItemAtPosition (const uiNode_t* const node, int mouseX, int mouseY, int* contX, int* contY)
{
	invList_t *result = NULL;
	/* Get coordinates inside a scrollable container (if it is one). */
	if (UI_IsScrollContainerNode(node)) {
		assert(qfalse);
	} else {
		vec2_t nodepos;
		int fromX, fromY;

		UI_GetNodeAbsPos(node, nodepos);
		/* Normalize screen coordinates to container coordinates. */
		fromX = (int) (mouseX - nodepos[0]) / C_UNIT;
		fromY = (int) (mouseY - nodepos[1]) / C_UNIT;
		if (contX)
			*contX = fromX;
		if (contY)
			*contY = fromY;

		result = INVSH_SearchInInventory(ui_inventory, EXTRADATACONST(node).container, fromX, fromY);
	}
	return result;
}

/**
 * @brief Custom tooltip for container node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void UI_ContainerNodeDrawTooltip (uiNode_t *node, int x, int y)
{
	static char tooltiptext[MAX_VAR * 2];
	const invList_t *itemHover;
	vec2_t nodepos;

	UI_GetNodeAbsPos(node, nodepos);

	/* Find out where the mouse is. */
	itemHover = UI_ContainerNodeGetItemAtPosition(node, x, y, NULL, NULL);

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
 * @todo None generic function. Not sure we can do it in a generic way
 */
static void UI_ContainerNodeAutoPlace (uiNode_t* node, int mouseX, int mouseY)
{
	int sel, target;
#if 0	/* see bellow #1 */
	int id;
#endif
	invList_t *ic;
	int fromX, fromY;
	uiNode_t *targetNode;
	qboolean ammoChanged = qfalse;

	if (!ui_inventory)
		return;

	/* don't allow this in tactical missions */
	if (CL_BattlescapeRunning())
		return;

	sel = cl_selected->integer;
	if (sel < 0)
		return;

	assert(EXTRADATA(node).container);

	ic = UI_ContainerNodeGetItemAtPosition(node, mouseX, mouseY, &fromX, &fromY);
	Com_DPrintf(DEBUG_CLIENT, "UI_ContainerNodeAutoPlace: item %i/%i selected from scrollable container.\n", fromX, fromY);
	if (!ic)
		return;
#if 0	/* see bellow #1 */
	id = ic->item.t->idx;
#endif

	/* Right click: automatic item assignment/removal. */
	if (EXTRADATA(node).container->id != csi.idEquip) {
		if (ic->item.m && ic->item.m != ic->item.t && ic->item.a) {
			/* Remove ammo on removing weapon from a soldier */
			target = csi.idEquip;
			ammoChanged = INV_UnloadWeapon(ic, ui_inventory, INVDEF(target));
		} else {
			/* Move back to idEquip (ground, floor) container. */
			target = csi.idEquip;
			INV_MoveItem(ui_inventory, INVDEF(target), NONE, NONE, EXTRADATA(node).container, ic);
		}
	} else {
		qboolean packed = qfalse;
		int px, py;
		assert(ic->item.t);
		/* armour can only have one target */
		if (INV_IsArmour(ic->item.t)) {
			target = csi.idArmour;
			packed = INV_MoveItem(ui_inventory, INVDEF(target), 0, 0, EXTRADATA(node).container, ic);
		/* ammo or item */
		} else if (INV_IsAmmo(ic->item.t)) {
			target = csi.idBelt;
			INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
			packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
			if (!packed) {
				target = csi.idHolster;
				INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
				packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
			}
			if (!packed) {
				target = csi.idBackpack;
				INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
				packed = INV_MoveItem( ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
			}
			/* Finally try left and right hand. There is no other place to put it now. */
			if (!packed) {
				const invList_t *rightHand = INVSH_SearchInInventory(ui_inventory, INVDEF(csi.idRight), 0, 0);

				/* Only try left hand if right hand is empty or no twohanded weapon/item is in it. */
				if (!rightHand || !rightHand->item.t->fireTwoHanded) {
					target = csi.idLeft;
					INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
					packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
				}
			}
			if (!packed) {
				target = csi.idRight;
				INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
				packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
			}
		} else {
			if (ic->item.t->headgear) {
				target = csi.idHeadgear;
				INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
				packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
			} else {
				/* left and right are single containers, but this might change - it's cleaner to check
				 * for available space here, too */
				target = csi.idRight;
				INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
				packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
				if (ic->item.t->weapon && !ic->item.a && packed)
					ammoChanged = INV_LoadWeapon(ic, ui_inventory, EXTRADATA(node).container, INVDEF(target));
				if (!packed) {
					const invList_t *rightHand = INVSH_SearchInInventory(ui_inventory, INVDEF(csi.idRight), 0, 0);

					/* Only try left hand if right hand is empty or no twohanded weapon/item is in it. */
					if (!rightHand || (rightHand && !rightHand->item.t->fireTwoHanded)) {
						target = csi.idLeft;
						INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
						packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
						if (ic->item.t->weapon && !ic->item.a && packed)
							ammoChanged = INV_LoadWeapon(ic, ui_inventory, EXTRADATA(node).container, INVDEF(target));
					}
				}
				if (!packed) {
					target = csi.idBelt;
					INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
					packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
					if (ic->item.t->weapon && !ic->item.a && packed)
						ammoChanged = INV_LoadWeapon(ic, ui_inventory, EXTRADATA(node).container, INVDEF(target));
				}
				if (!packed) {
					target = csi.idHolster;
					INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
					packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
					if (ic->item.t->weapon && !ic->item.a && packed)
						ammoChanged = INV_LoadWeapon(ic, ui_inventory, EXTRADATA(node).container, INVDEF(target));
				}
				if (!packed) {
					target = csi.idBackpack;
					INVSH_FindSpace(ui_inventory, &ic->item, INVDEF(target), &px, &py, NULL);
					packed = INV_MoveItem(ui_inventory, INVDEF(target), px, py, EXTRADATA(node).container, ic);
					if (ic->item.t->weapon && !ic->item.a && packed)
						ammoChanged = INV_LoadWeapon(ic, ui_inventory, EXTRADATA(node).container, INVDEF(target));
				}
			}
		}
		/* no need to continue here - placement wasn't successful at all */
		if (!packed)
			return;
	}

	/* Run onChange events */
	targetNode = UI_GetContainerNodeByContainerIDX(node->root, target);
	if (node->onChange)
		UI_ExecuteEventActions(node, node->onChange);
	if (node != targetNode && targetNode->onChange)
		UI_ExecuteEventActions(targetNode, targetNode->onChange);
	/* Also call onChange for equip_ammo if ammo moved
	Maybe there's a better way to do this? */
	if (INV_IsAmmo(ic->item.t) || ammoChanged) {
		uiNode_t *ammoNode = UI_GetNode(node->root, "equip_ammo");
		if (node != ammoNode && ammoNode->onChange)
			UI_ExecuteEventActions(ammoNode, ammoNode->onChange);
	}

	/* Update display of scroll buttons. */
	if (UI_IsScrollContainerNode(node))
		UI_ContainerNodeUpdateScroll(node);
}

static int oldMouseX = 0;
static int oldMouseY = 0;

static void UI_ContainerNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	const int delta = abs(oldMouseX - x) + abs(oldMouseY - y);
	if (delta > 15) {
		UI_DNDDragItem(node, &(dragInfoIC->item));
		UI_MouseRelease();
	}
}

static void UI_ContainerNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	switch (button) {
	case K_MOUSE1:
	{
		/* start drag and drop */
		int fromX, fromY;
		dragInfoIC = UI_ContainerNodeGetItemAtPosition(node, x, y, &fromX, &fromY);
		if (dragInfoIC) {
			dragInfoFromX = fromX;
			dragInfoFromY = fromY;
			oldMouseX = x;
			oldMouseY = y;
			UI_SetMouseCapture(node);
			EXTRADATA(node).lastSelectedId = dragInfoIC->item.t->idx;
			if (EXTRADATA(node).onSelect) {
				UI_ExecuteEventActions(node, EXTRADATA(node).onSelect);
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

static void UI_ContainerNodeMouseUp (uiNode_t *node, int x, int y, int button)
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
static void UI_ContainerNodeWheel (uiNode_t *node, qboolean down, int x, int y)
{
	if (UI_IsScrollContainerNode(node)) {
		const int delta = 20;
		if (down) {
			const int lenght = EXTRADATA(node).scrollTotalNum - EXTRADATA(node).scrollNum;
			if (EXTRADATA(node).scrollCur < lenght) {
				EXTRADATA(node).scrollCur += delta;
				if (EXTRADATA(node).scrollCur > lenght)
					EXTRADATA(node).scrollCur = lenght;
				UI_ContainerNodeUpdateScroll(node);
			}
		} else {
			if (EXTRADATA(node).scrollCur > 0) {
				EXTRADATA(node).scrollCur -= delta;
				if (EXTRADATA(node).scrollCur < 0)
					EXTRADATA(node).scrollCur = 0;
				UI_ContainerNodeUpdateScroll(node);
			}
		}
	}
}

static void UI_ContainerNodeLoading (uiNode_t *node)
{
	EXTRADATA(node).container = NULL;
	EXTRADATA(node).columns = 1;
	node->color[3] = 1.0;
}

/**
 * @brief Call when a DND enter into the node
 */
static qboolean UI_ContainerNodeDNDEnter (uiNode_t *target)
{
	/* accept items only, if we have a container */
	return UI_DNDGetType() == DND_ITEM && EXTRADATA(target).container && (!UI_IsScrollContainerNode(target) || UI_DNDGetSourceNode() !=  target);
}

/**
 * @brief Call into the target when the DND hover it
 * @return True if the DND is accepted
 */
static qboolean UI_ContainerNodeDNDMove (uiNode_t *target, int x, int y)
{
	vec2_t nodepos;
	qboolean exists;
	int itemX = 0;
	int itemY = 0;
	item_t *dragItem = UI_DNDGetItem();

	/* we already check it when the node accept the DND */
	assert(EXTRADATA(target).container);

	UI_GetNodeAbsPos(target, nodepos);

	/** We calculate the position of the top-left corner of the dragged
	 * item in oder to compensate for the centered-drawn cursor-item.
	 * Or to be more exact, we calculate the relative offset from the cursor
	 * location to the middle of the top-left square of the item.
	 * @sa UI_LeftClick */
	if (dragItem->t) {
		itemX = C_UNIT * dragItem->t->sx / 2;	/* Half item-width. */
		itemY = C_UNIT * dragItem->t->sy / 2;	/* Half item-height. */

		/* Place relative center in the middle of the square. */
		itemX -= C_UNIT / 2;
		itemY -= C_UNIT / 2;
	}

	dragInfoToX = (mousePosX - nodepos[0] - itemX) / C_UNIT;
	dragInfoToY = (mousePosY - nodepos[1] - itemY) / C_UNIT;

	/* Check if the items already exists in the container. i.e. there is already at least one item. */
	exists = qfalse;
	if ((INV_IsFloorDef(EXTRADATA(target).container) || INV_IsEquipDef(EXTRADATA(target).container))
		&& (dragInfoToX < 0 || dragInfoToY < 0 || dragInfoToX >= SHAPE_BIG_MAX_WIDTH || dragInfoToY >= SHAPE_BIG_MAX_HEIGHT)
		&& INVSH_ExistsInInventory(ui_inventory, EXTRADATA(target).container, *dragItem)) {
		exists = qtrue;
	}

	/* Search for a suitable position to render the item at if
	 * the container is "single", the cursor is out of bound of the container. */
	if (!exists && dragItem->t && (EXTRADATA(target).container->single
		|| dragInfoToX < 0 || dragInfoToY < 0
		|| dragInfoToX >= SHAPE_BIG_MAX_WIDTH || dragInfoToY >= SHAPE_BIG_MAX_HEIGHT)) {
#if 0
/* ... or there is something in the way. */
/* We would need to check for weapon/ammo as well here, otherwise a preview would be drawn as well when hovering over the correct weapon to reload. */
		|| (INVSH_CheckToInventory(ui_inventory, dragItem->t, EXTRADATA(target).container, dragInfoToX, dragInfoToY) == INV_DOES_NOT_FIT)) {
#endif
		INVSH_FindSpace(ui_inventory, dragItem, EXTRADATA(target).container, &dragInfoToX, &dragInfoToY, dragInfoIC);
	}

	/* we can drag every thing */
	if (UI_IsScrollContainerNode(target)) {
		return qtrue;
	}

	{
		invList_t *fItem;

		/* is there empty slot? */
		const int checkedTo = INVSH_CheckToInventory(ui_inventory, dragItem->t, EXTRADATA(target).container, dragInfoToX, dragInfoToY, dragInfoIC);
		if (checkedTo != INV_DOES_NOT_FIT)
			return qtrue;

		/* can we equip dragging item into the target item? */
		fItem = INVSH_SearchInInventory(ui_inventory, EXTRADATA(target).container, dragInfoToX, dragInfoToY);
		if (!fItem)
			return qfalse;
		if (EXTRADATA(target).container->single)
			return qtrue;
		return INVSH_LoadableInWeapon(dragItem->t, fItem->item.t);
	}
}

/**
 * @brief Call when a DND enter into the node
 */
static void UI_ContainerNodeDNDLeave (uiNode_t *node)
{
	dragInfoToX = -1;
	dragInfoToY = -1;
}

/**
 * @brief Call into the source when the DND end
 */
static qboolean UI_ContainerNodeDNDFinished (uiNode_t *source, qboolean isDropped)
{
	item_t *dragItem = UI_DNDGetItem();

	/* if the target can't finalize the DND we stop */
	if (!isDropped) {
		return qfalse;
	}

	/* on tactical mission */
	if (CL_BattlescapeRunning()) {
		const uiNode_t *target = UI_DNDGetTargetNode();
		assert(EXTRADATA(source).container);
		assert(target);
		assert(EXTRADATACONST(target).container);
		assert(selActor);
		CL_ActorInvMove(selActor, EXTRADATA(source).container->id, dragInfoFromX, dragInfoFromY,
			EXTRADATACONST(target).container->id, dragInfoToX, dragInfoToY);
	} else {
		uiNode_t *target = UI_DNDGetTargetNode();
		if (target) {
			invList_t *fItem;
			/** @todo Is filterEquipType needed here?, we can use anyway INVSH_SearchInInventory if we disable dragInfoFromX/Y when we start DND */
			if (UI_IsScrollContainerNode(source)) {
				const int equipType = EXTRADATA(source).filterEquipType;
				fItem = UI_ContainerNodeGetExistingItem(source, dragItem->t, equipType);
			} else
				fItem = INVSH_SearchInInventory(ui_inventory, EXTRADATA(source).container, dragInfoFromX, dragInfoFromY);

			/** @todo We must split the move in two. Here, we should not know how to add the item to the target (see dndDrop) */
			assert(EXTRADATA(target).container);
			assert(fItem);

			/* Remove ammo on removing weapon from a soldier */
			if (UI_IsScrollContainerNode(target) && fItem->item.m && fItem->item.m != fItem->item.t)
				INV_UnloadWeapon(fItem, ui_inventory, EXTRADATA(target).container);
			/* move the item */
			INV_MoveItem(ui_inventory,
				EXTRADATA(target).container, dragInfoToX, dragInfoToY,
				EXTRADATA(source).container, fItem);
			/* Add ammo on adding weapon to a soldier  */
			if (UI_IsScrollContainerNode(source) && fItem->item.t->weapon && !fItem->item.a)
				INV_LoadWeapon(fItem, ui_inventory, EXTRADATA(source).container, EXTRADATA(target).container);

			/* Run onChange events */
			if (source->onChange)
				UI_ExecuteEventActions(source, source->onChange);
			if (source != target && target->onChange)
				UI_ExecuteEventActions(target, target->onChange);
		}
	}

	dragInfoFromX = -1;
	dragInfoFromY = -1;
	return qtrue;
}

static const value_t properties[] = {
	/* Base container only. Display/hide weapons. */
	{"displayweapon", V_BOOL, UI_EXTRADATA_OFFSETOF(containerExtraData_t, displayWeapon),  MEMBER_SIZEOF(containerExtraData_t, displayWeapon)},
	/* Base container only. Display/hide ammo. */
	{"displayammo", V_BOOL, UI_EXTRADATA_OFFSETOF(containerExtraData_t, displayAmmo),  MEMBER_SIZEOF(containerExtraData_t, displayAmmo)},
	/* Base container only. Display/hide out of stock items. */
	{"displayunavailableitem", V_BOOL, UI_EXTRADATA_OFFSETOF(containerExtraData_t, displayUnavailableItem),  MEMBER_SIZEOF(containerExtraData_t, displayUnavailableItem)},
	/* Base container only. Sort the list to display in stock items on top of the list. */
	{"displayavailableontop", V_BOOL, UI_EXTRADATA_OFFSETOF(containerExtraData_t, displayAvailableOnTop),  MEMBER_SIZEOF(containerExtraData_t, displayAvailableOnTop)},
	/* Base container only. Display/hide ammo near weapons. */
	{"displayammoofweapon", V_BOOL, UI_EXTRADATA_OFFSETOF(containerExtraData_t, displayAmmoOfWeapon),  MEMBER_SIZEOF(containerExtraData_t, displayAmmoOfWeapon)},
	/* Base container only. Display/hide out of stock ammo near weapons. <code>displayammoofweapon</code> must be activated first. */
	{"displayunavailableammoofweapon", V_BOOL, UI_EXTRADATA_OFFSETOF(containerExtraData_t, displayUnavailableAmmoOfWeapon),  MEMBER_SIZEOF(containerExtraData_t, displayUnavailableAmmoOfWeapon)},
	/* Base container only. Custom the number of column we must use to display items. */
	{"columns", V_INT, UI_EXTRADATA_OFFSETOF(containerExtraData_t, columns),  MEMBER_SIZEOF(containerExtraData_t, columns)},
	/* Base container only. Filter items by a category. */
	{"filter", V_INT, UI_EXTRADATA_OFFSETOF(containerExtraData_t, filterEquipType),  MEMBER_SIZEOF(containerExtraData_t, filterEquipType)},

	/* Callback value set before calling onSelect. It is used to know the item selected */
	{"lastselectedid", V_INT, UI_EXTRADATA_OFFSETOF(containerExtraData_t, lastSelectedId),  MEMBER_SIZEOF(containerExtraData_t, lastSelectedId)},
	/* Callback event called when the user select an item */
	{"onselect", V_UI_ACTION, UI_EXTRADATA_OFFSETOF(containerExtraData_t, onSelect),  MEMBER_SIZEOF(containerExtraData_t, onSelect)},

	/* Base container only. Position of the vertical view (into the full number of elements the node contain)
	 * @todo Rename it viewpos (like scrollable node)
	 */
	{"scrollpos", V_INT, UI_EXTRADATA_OFFSETOF(containerExtraData_t, scrollCur),  MEMBER_SIZEOF(containerExtraData_t, scrollCur)},
	/* Base container only. Size of the vertical view (proportional to the number of elements the node can display without moving) */
	{"viewsize", V_INT, UI_EXTRADATA_OFFSETOF(containerExtraData_t, scrollNum),  MEMBER_SIZEOF(containerExtraData_t, scrollNum)},
	/* Base container only. Full vertical size (proportional to the number of elements the node contain) */
	{"fullsize", V_INT, UI_EXTRADATA_OFFSETOF(containerExtraData_t, scrollTotalNum),  MEMBER_SIZEOF(containerExtraData_t, scrollTotalNum)},
	/* Base container only. Called when one of the properties viewpos/viewsize/fullsize change */
	{"onviewchange", V_UI_ACTION, UI_EXTRADATA_OFFSETOF(containerExtraData_t, onViewChange), MEMBER_SIZEOF(containerExtraData_t, onViewChange)},

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterContainerNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "container";
	behaviour->draw = UI_ContainerNodeDraw;
	behaviour->drawTooltip = UI_ContainerNodeDrawTooltip;
	behaviour->mouseDown = UI_ContainerNodeMouseDown;
	behaviour->mouseUp = UI_ContainerNodeMouseUp;
	behaviour->capturedMouseMove = UI_ContainerNodeCapturedMouseMove;
	behaviour->loading = UI_ContainerNodeLoading;
	behaviour->loaded = UI_ContainerNodeLoaded;
	behaviour->dndEnter = UI_ContainerNodeDNDEnter;
	behaviour->dndFinished = UI_ContainerNodeDNDFinished;
	behaviour->dndMove = UI_ContainerNodeDNDMove;
	behaviour->dndLeave = UI_ContainerNodeDNDLeave;
	behaviour->mouseWheel = UI_ContainerNodeWheel;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
