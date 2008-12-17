/**
 * @file m_node_container.c
 * @todo need refactoring. one possible way:
 * @todo 1) generic drag-and-drop
 * @todo 2) move container list code out
 * @todo 3) improve the code genericity (remove baseCurrent...)
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../client.h"
#include "../../cl_actor.h"
#include "../../cl_map.h"
#include "../m_main.h"
#include "../m_draw.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_dragndrop.h"
#include "../m_tooltip.h"
#include "../m_nodes.h"
#include "m_node_model.h"
#include "m_node_container.h"
#include "m_node_abstractnode.h"

inventory_t *menuInventory = NULL;

/**
 * @brief Update display of scroll buttons.
 * @note The cvars "mn_cont_scroll_prev_hover" and "mn_cont_scroll_next_hover" are
 * set by the "in" and "out" functions of the scroll buttons.
 */
static void MN_ScrollContainerUpdate_f (void)
{
	Cbuf_AddText(va("mn_setnodeproperty equip_scroll current %i\n", menuInventory->scrollCur));
	Cbuf_AddText(va("mn_setnodeproperty equip_scroll viewsize %i\n", menuInventory->scrollNum));
	Cbuf_AddText(va("mn_setnodeproperty equip_scroll fullsize %i\n", menuInventory->scrollTotalNum));
#if 0
	/* "Previous"/"Backward" button. Are there items before the first displayed one? */
	if (menuInventory->scrollCur > 0) {
		/* Clicking the button will do something. */
		if (Cvar_VariableInteger("mn_cont_scroll_prev_hover"))
			Cbuf_AddText("cont_scroll_prev_high\n");
		else
			Cbuf_AddText("cont_scroll_prev_act\n");
	} else {
		/* The button is disabled. */
		Cbuf_AddText("cont_scroll_prev_ina\n");
	}

	/* "Next"/"Forward" button. Are there still items after the ones that get displayed? */
	if (menuInventory->scrollNum + menuInventory->scrollCur < menuInventory->scrollTotalNum) {
		/* Clicking the button will do something. */
		if (Cvar_VariableInteger("mn_cont_scroll_next_hover"))
			Cbuf_AddText("cont_scroll_next_high\n");
		else
			Cbuf_AddText("cont_scroll_next_act\n");
	} else {
		/* The button is disabled. */
		Cbuf_AddText("cont_scroll_next_ina\n");
	}
#endif
}

/**
 * @brief Scrolls one item forward in a scrollable container.
 * @sa Console command "scrollcont_next"
 * @todo Add check so we do only increase if there are still hidden items after the last displayed one.
 */
static void MN_ScrollContainerNext_f (void)
{
	const menuNode_t* const node = MN_GetNodeFromCurrentMenu("equip");

	/* Can be called from everywhere. */
	if (!baseCurrent || !node)
		return;

	if (!menuInventory)
		return;

	/* Check if the end of the currently visible items still is not the last of the displayable items. */
	if (menuInventory->scrollCur < menuInventory->scrollTotalNum
	 && menuInventory->scrollCur + menuInventory->scrollNum < menuInventory->scrollTotalNum) {
		menuInventory->scrollCur++;
		Com_DPrintf(DEBUG_CLIENT, "MN_ScrollContainerNext_f: Increased current scroll index: %i (num: %i, total: %i).\n",
			menuInventory->scrollCur,
			menuInventory->scrollNum,
			menuInventory->scrollTotalNum);
	} else
		Com_DPrintf(DEBUG_CLIENT, "MN_ScrollContainerNext_f: Current scroll index already %i (num: %i, total: %i)).\n",
			menuInventory->scrollCur,
			menuInventory->scrollNum,
			menuInventory->scrollTotalNum);

	/* Update display of scroll buttons. */
	MN_ScrollContainerUpdate_f();
}

/**
 * @brief Scrolls one item backwards in a scrollable container.
 * @sa Console command "scrollcont_prev"
 */
static void MN_ScrollContainerPrev_f (void)
{
	const menuNode_t* const node = MN_GetNodeFromCurrentMenu("equip");

	/* Can be called from everywhere. */
	if (!baseCurrent || !node)
		return;

	if (!menuInventory)
		return;

	if (menuInventory->scrollCur > 0) {
		menuInventory->scrollCur--;
		Com_DPrintf(DEBUG_CLIENT, "MN_ScrollContainerNext_f: Decreased current scroll index: %i (num: %i, total: %i).\n",
			menuInventory->scrollCur,
			menuInventory->scrollNum,
			menuInventory->scrollTotalNum);
	} else
		Com_DPrintf(DEBUG_CLIENT, "MN_ScrollContainerNext_f: Current scroll index already %i (num: %i, total: %i).\n",
			menuInventory->scrollCur,
			menuInventory->scrollNum,
			menuInventory->scrollTotalNum);

	/* Update display of scroll buttons. */
	MN_ScrollContainerUpdate_f();
}

/**
 * @brief Scrolls items in a scrollable container.
 */
static void MN_ScrollContainerScroll_f (void)
{
	int offset;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <+/-offset>\n", Cmd_Argv(0));
		return;
	}

	/* Can be called from everywhere. */
	if (!baseCurrent)
		return;

	if (!menuInventory)
		return;

	offset = atoi(Cmd_Argv(1));
	while (offset > 0) {
		MN_ScrollContainerNext_f();
		offset--;
	}
	while (offset < 0) {
		MN_ScrollContainerPrev_f();
		offset++;
	}
}

/**
 * @brief Draws an item to the screen
 *
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
void MN_DrawItem (menuNode_t *node, const vec3_t org, const item_t *item, int x, int y, const vec3_t scale, const vec4_t color)
{
	objDef_t *od;
	vec4_t col;
	vec3_t origin;

	assert(item);
	assert(item->t);
	od = item->t;

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

		/* Draw the image. */
		R_DrawNormPic(origin[0], origin[1], imgWidth, imgHeight, 0, 0, 0, 0, ALIGN_CC, qtrue, od->image);
	} else {
		menuModel_t *menuModel = NULL;
		const char *modelName = od->model;
		if (od->tech && od->tech->mdl) {
			menuModel = MN_GetMenuModel(od->tech->mdl);
			/* the model from the tech structure has higher priority, because the item model itself
			 * is mainly for the battlescape or the geoscape - only use that as a fallback */
			modelName = od->tech->mdl;
		}

		/* no model definition in the tech struct, not in the fallback object definition */
		if (modelName == NULL || modelName[0] == '\0') {
			Com_Printf("MN_DrawItem: No model given for item: '%s'\n", od->id);
			return;
		}

		if (menuModel && node) {
			const char* ref = MN_GetReferenceString(node->menu, node->dataImageOrModel);
			MN_DrawModelNode(node, ref, modelName);
		} else {
			modelInfo_t mi;
			vec3_t angles = {-10, 160, 70};
			vec3_t size = {scale[0], scale[1], scale[2]};

			if (item->rotated)
				angles[0] -= 90;

			memset(&mi, 0, sizeof(mi));
			mi.origin = origin;
			mi.angles = angles;
			mi.center = od->center;
			mi.scale = size;
			mi.color = col;
			mi.name = modelName;
			if (od->scale)
				VectorScale(size, od->scale, size);

			/* draw the model */
			R_DrawModelDirect(&mi, NULL, NULL);
		}
	}
}

/**
 * @brief Generate tooltip text for an item.
 * @param[in] item The item we want to generate the tooltip text for.
 * @param[in,out] tooltiptext Pointer to a string the information should be written into.
 * @param[in] string_maxlength Max. string size of tooltiptext.
 * @return Number of lines
 */
void MN_GetItemTooltip (item_t item, char *tooltiptext, size_t string_maxlength)
{
	int i;
	objDef_t *weapon;

	assert(item.t);

	if (item.amount > 1)
		Com_sprintf(tooltiptext, string_maxlength, "%i x %s\n", item.amount, item.t->name);
	else
		Com_sprintf(tooltiptext, string_maxlength, "%s\n", item.t->name);

	/* Only display further info if item.t is researched */
	if (RS_IsResearched_ptr(item.t->tech)) {
		if (item.t->weapon) {
			/* Get info about used ammo (if there is any) */
			if (item.t == item.m) {
				/* Item has no ammo but might have shot-count */
				if (item.a) {
					Q_strcat(tooltiptext, va(_("Ammo: %i\n"), item.a), string_maxlength);
				}
			} else if (item.m) {
				/* Search for used ammo and display name + ammo count */
				Q_strcat(tooltiptext, va(_("%s loaded\n"), item.m->name), string_maxlength);
				Q_strcat(tooltiptext, va(_("Ammo: %i\n"),  item.a), string_maxlength);
			}
		} else if (item.t->numWeapons) {
			/* Check if this is a non-weapon and non-ammo item */
			if (!(item.t->numWeapons == 1 && item.t->weapons[0] == item.t)) {
				/* If it's ammo get the weapon names it can be used in */
				Q_strcat(tooltiptext, _("Usable in:\n"), string_maxlength);
				for (i = 0; i < item.t->numWeapons; i++) {
					weapon = item.t->weapons[i];
					if (RS_IsResearched_ptr(weapon->tech)) {
						Q_strcat(tooltiptext, va("* %s\n", weapon->name), string_maxlength);
					}
				}
			}
		}
	}
}

/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
static void MN_DrawDisabled (const menuNode_t* node)
{
	const vec4_t color = { 0.3f, 0.3f, 0.3f, 0.7f };
	vec2_t nodepos;

	MN_GetNodeAbsPos(node, nodepos);
	R_DrawFill(nodepos[0], nodepos[1], node->size[0], node->size[1], ALIGN_UL, color);
}

/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
static void MN_DrawFree (int container, const menuNode_t *node, int posx, int posy, int sizex, int sizey, qboolean showTUs)
{
	const vec4_t color = { 0.0f, 1.0f, 0.0f, 0.7f };
	invDef_t* inv = &csi.ids[container];
	vec2_t nodepos;

	MN_GetNodeAbsPos(node, nodepos);
	R_DrawFill(posx, posy, sizex, sizey, ALIGN_UL, color);

	/* if showTUs is true (only the first time in none single containers)
	 * and we are connected to a game */
	if (showTUs && cls.state == ca_active){
		R_FontDrawString("f_verysmall", 0, nodepos[0] + 3, nodepos[1] + 3,
			nodepos[0] + 3, nodepos[1] + 3, node->size[0] - 6, 0, 0,
			va(_("In: %i Out: %i"), inv->in, inv->out), 0, 0, NULL, qfalse, 0);
	}
}

/**
 * @brief Draws the free and usable inventory positions when dragging an item.
 * @note Only call this function in dragging mode
 */
static void MN_ContainerNodeDrawFreeSpace (inventory_t *inv, const menuNode_t *node)
{
	const objDef_t *od = dragInfo.item.t;	/**< Get the 'type' of the dragged item. */
	vec2_t nodepos;

	/* Draw only in dragging-mode and not for the equip-floor */
	assert(MN_DNDIsDragging());
	assert(inv);

	MN_GetNodeAbsPos(node, nodepos);
	/* if single container (hands, extension, headgear) */
	if (node->container->single) {
		/* if container is free or the dragged-item is in it */
		if (node->container == dragInfo.from || Com_CheckToInventory(inv, od, node->container, 0, 0, dragInfo.ic))
			MN_DrawFree(node->container->id, node, nodepos[0], nodepos[1], node->size[0], node->size[1], qtrue);
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
				const int checkedTo = Com_CheckToInventory(inv, od, node->container, x, y, dragInfo.ic);
				if (checkedTo & INV_FITS)				/* Item can be placed normally. */
					Com_MergeShapes(free, (uint32_t)od->shape, x, y);
				if (checkedTo & INV_FITS_ONLY_ROTATED)	/* Item can be placed rotated. */
					Com_MergeShapes(free, Com_ShapeRotate((uint32_t)od->shape), x, y);

				/* Only draw on existing positions. */
				if (Com_CheckShape(node->container->shape, x, y)) {
					if (Com_CheckShape(free, x, y)) {
						MN_DrawFree(node->container->id, node, nodepos[0] + x * C_UNIT, nodepos[1] + y * C_UNIT, C_UNIT, C_UNIT, showTUs);
						showTUs = qfalse;
					}
				}
			}	/* for x */
		}	/* for y */
	}
}

/**
 * @brief Calculates the size of a container node and links the container
 * into the node (uses the @c invDef_t shape bitmask to determine the size)
 * @param[in,out] node The node to get the size for
 */
void MN_FindContainer (menuNode_t* const node)
{
	invDef_t *id;
	int i, j;

	/* already a container assigned - no need to recalculate the size */
	if (node->container)
		return;

	for (i = 0, id = csi.ids; i < csi.numIDs; id++, i++)
		if (!Q_strncmp(node->name, id->name, sizeof(node->name)))
			break;

	if (i == csi.numIDs)
		node->container = NULL;
	else
		node->container = &csi.ids[i];

	if (node->container && node->container->scroll) {
		/* No need to calculate the size - we directly define it in
		 * the "inventory" entry in the .ufo file anyway. */
		node->size[0] = node->container->scroll;
		node->size[1] = node->container->scrollHeight;
	} else {
		/* Start on the last bit of the shape mask. */
		for (i = 31; i >= 0; i--) {
			for (j = 0; j < SHAPE_BIG_MAX_HEIGHT; j++)
				if (id->shape[j] & (1 << i))
					break;
			if (j < SHAPE_BIG_MAX_HEIGHT)
				break;
		}
		node->size[0] = C_UNIT * (i + 1) + 0.01;

		/* start on the lower row of the shape mask */
		for (i = SHAPE_BIG_MAX_HEIGHT - 1; i >= 0; i--)
			if (id->shape[i] & ~0x0)
				break;
		node->size[1] = C_UNIT * (i + 1) + 0.01;
	}
}

/**
 * @brief Draw the container with all its items
 * @note The item tooltip is done after the menu is completely
 * drawn - because it must be on top of every other node and item
 * @return The item in the container to hover
 */
static const invList_t* MN_DrawContainerNode (menuNode_t *node)
{
	const vec3_t scale = {3.5, 3.5, 3.5};
	vec4_t color = {1, 1, 1, 1};
	vec4_t colorLoadable = {0.5, 1, 0.5, 1};
	qboolean drawLoadable = qfalse;
	vec2_t nodepos;

#if 0
	MN_FindContainer(node);
#endif
	if (!node->container)
		return NULL;

	assert(menuInventory);
	MN_GetNodeAbsPos(node, nodepos);

	/* Highlight weapons that the dragged ammo (if it is one) can be loaded into. */
	if (MN_DNDIsDragging())
		drawLoadable = qtrue;

	if (node->container->single) {
		/* Single container */
		vec2_t pos;
		Vector2Copy(nodepos, pos);
		pos[0] += node->size[0] / 2.0;
		pos[1] += node->size[1] / 2.0;

		/* Single item container (special case for left hand). */
		if (node->container->id == csi.idLeft && !menuInventory->c[csi.idLeft]) {
			color[3] = 0.5;
			colorLoadable[3] = 0.5;
			if (menuInventory->c[csi.idRight]) {
				const item_t *item = &menuInventory->c[csi.idRight]->item;
				assert(item);
				assert(item->t);

				if (item->t->holdTwoHanded) {
					if (drawLoadable && INVSH_LoadableInWeapon(dragInfo.item.t, item->t))
						MN_DrawItem(node, pos, item, -1, -1, scale, colorLoadable);
					else
						MN_DrawItem(node, pos, item, -1, -1, scale, color);
				}
			}
		} else if (menuInventory->c[node->container->id]) {
			const item_t *item;

			if (menuInventory->c[csi.idRight]) {
				item = &menuInventory->c[csi.idRight]->item;
				/* If there is a weapon in the right hand that needs two hands to shoot it
				 * and there is a weapon in the left, then draw a disabled marker for the
				 * fireTwoHanded weapon. */
				assert(item);
				assert(item->t);
				if (node->container->id == csi.idRight && item->t->fireTwoHanded && menuInventory->c[csi.idLeft]) {
					Vector4Set(color, 0.5, 0.5, 0.5, 0.5);
					Vector4Set(colorLoadable, 0.5, 0.25, 0.25, 0.5);
					MN_DrawDisabled(node);
				}
			}

			item = &menuInventory->c[node->container->id]->item;
			assert(item);
			assert(item->t);
			if (drawLoadable && INVSH_LoadableInWeapon(dragInfo.item.t, item->t))
				MN_DrawItem(node, pos, item, -1, -1, scale, colorLoadable);
			else
				MN_DrawItem(node, pos, item, -1, -1, scale, color);
		}
	} else {
		if (node->container->scroll) {
			/* Scrollable container */
			/** @todo We really should group similar weapons (using same ammo) and their ammo together. */
			invList_t *ic;
			byte itemType;
			int curWidth = 0;	/**< Combined width of all drawn item so far. */
			int curHeight = 0;	/**< Combined Height of all drawn item so far. */
			int maxHeight = 0;	/**< Max. height of a row. */
			int curCol = 0;		/**< Index of item in current row. */
			int curRow = 0;		/**< Current row. */
			int curItem = 0;	/**< Item counter for all items that get displayed. */
			qboolean lastItem = qfalse;	/**< Did we reach the last displayed/visible item? We only count the items after that.*/
			int rowOffset;

			/* Remember the last used scroll settings, so we know if we
			 * need to update the button-display later on. */
			const int cache_scrollCur = menuInventory->scrollCur;
			const int cache_scrollNum = menuInventory->scrollNum;
			const int cache_scrollTotalNum = menuInventory->scrollTotalNum;

			menuInventory->scrollNum = 0;
			menuInventory->scrollTotalNum = 0;

			/* Change row spacing according to vertical/horizontal setting. */
			rowOffset = node->container->scrollVertical ? C_ROW_OFFSET : 0;

			for (itemType = 0; itemType <= 1; itemType++) {	/**< 0==weapons, 1==ammo */
				for (ic = menuInventory->c[node->container->id]; ic; ic = ic->next) {
					if (ic->item.t
					 && ((!itemType && !(!Q_strncmp(ic->item.t->type, "ammo", 4))) || (itemType && !Q_strncmp(ic->item.t->type, "ammo", 4)))
					 && INV_ItemMatchesFilter(ic->item.t, baseCurrent->equipType)) {
						if (!lastItem && curItem >= menuInventory->scrollCur) {
							item_t tempItem = {1, NULL, NULL, 0, 0};
							qboolean newRow = qfalse;
							vec2_t pos;
							Vector2Copy(nodepos, pos);
							pos[0] += curWidth;
							pos[1] += curHeight;

							if (!node->container->scrollVertical && curWidth + ic->item.t->sx * C_UNIT <= node->container->scroll) {
								curWidth += ic->item.t->sx * C_UNIT;
							} else {
								/* New row */
								if (curHeight + maxHeight + rowOffset > node->container->scrollHeight) {
									/* Last item - We do not draw anything else. */
									lastItem = qtrue;
								} else {
									curHeight += maxHeight + rowOffset;
									curWidth = maxHeight = curCol = 0;
									curRow++;

									/* Re-calculate the position of this item in the new row. */
									Vector2Copy(nodepos, pos);
									pos[1] += curHeight;
									newRow = qtrue;
								}
							}

							maxHeight = max(ic->item.t->sy * C_UNIT, maxHeight);

							if (lastItem || curHeight + ic->item.t->sy + rowOffset > node->container->scrollHeight) {
								/* Last item - We do not draw anything else. */
								lastItem = qtrue;
							} else {
								vec2_t posName;
								Vector2Copy(pos, posName);	/* Save original coordiantes for drawing of the item name. */
								posName[1] -= rowOffset;

								/* Calculate the center of the item model/image. */
								pos[0] += ic->item.t->sx * C_UNIT / 2.0;
								pos[1] += ic->item.t->sy * C_UNIT / 2.0;

								assert(ic->item.t);

								/* Update location info for this entry. */
								ic->x = curCol;
								ic->y = curRow;

								/* Actually draw the item. */
								tempItem.t = ic->item.t;
								if (drawLoadable && INVSH_LoadableInWeapon(dragInfo.item.t, ic->item.t))
									MN_DrawItem(node, pos, &tempItem, -1, -1, scale, colorLoadable);
								else
									MN_DrawItem(node, pos, &tempItem, -1, -1, scale, color);

								if (node->container->scrollVertical) {
									/* Draw the item name. */
									R_FontDrawString("f_verysmall", ALIGN_UL,
										posName[0], posName[1],
										posName[0], posName[1],
										0,	/* maxWidth */
										0,	/* maxHeight */
										0, va("%s (%i)", ic->item.t->name,  ic->item.amount), 0, 0, NULL, qfalse, 0);
								} /* scrollVertical */

								if (newRow)
									curWidth += ic->item.t->sx * C_UNIT;

								/* This item uses ammo - lets display that as well. */
								if (node->container->scrollVertical && ic->item.t->numAmmos && ic->item.t != ic->item.t->ammos[0]) {
									int ammoIdx;
									invList_t *icAmmo;

									pos[0] += ic->item.t->sx * C_UNIT / 2.0;

									/* Loop through all ammo-types for this item. */
									for (ammoIdx = 0; ammoIdx < ic->item.t->numAmmos; ammoIdx++) {
										tempItem.t = ic->item.t->ammos[ammoIdx];
										if (tempItem.t->tech && RS_IsResearched_ptr(tempItem.t->tech)) {
											icAmmo = INV_SearchInScrollableContainer(menuInventory, node->container, NONE, NONE, tempItem.t, baseCurrent->equipType);

											/* If we've found a piece of this ammo in the inventory we draw it. */
											if (icAmmo) {
												/* Calculate the center of the item model/image. */
												pos[0] += tempItem.t->sx * C_UNIT / 2;
												pos[1] = nodepos[1] + curHeight + icAmmo->item.t->sy * C_UNIT / 2.0;

												curWidth += tempItem.t->sx * C_UNIT;

												MN_DrawItem(node, pos, &tempItem, -1, -1, scale, color);
												R_FontDrawString("f_verysmall", ALIGN_LC,
													pos[0] + icAmmo->item.t->sx * C_UNIT / 2.0, pos[1] + icAmmo->item.t->sy * C_UNIT / 2.0,
													pos[0] + icAmmo->item.t->sx * C_UNIT / 2.0, pos[1] + icAmmo->item.t->sy * C_UNIT / 2.0,
													C_UNIT,	/* maxWidth */
													0,	/* maxHeight */
													0, va("%i", icAmmo->item.amount), 0, 0, NULL, qfalse, 0);

												/* Add width of text and the rest of the item. */
												curWidth += C_UNIT / 2.0;
												pos[0] += tempItem.t->sx * C_UNIT / 2 + C_UNIT / 2.0;

											}
										}
									}
								}
							}

							/* Count displayed items */
							if (!lastItem) {
								menuInventory->scrollNum++;
								curCol++;
							}
						}

						/* Count items that are possible to be displayed. */
						menuInventory->scrollTotalNum++;
						curItem++;
					}
				}
			}

			/* Update display of scroll buttons if something changed. */
			if (cache_scrollCur != menuInventory->scrollCur
			 || cache_scrollNum != menuInventory->scrollNum
			 ||	cache_scrollTotalNum != menuInventory->scrollTotalNum)
				MN_ScrollContainerUpdate_f();
		} else {
			/* Grid container */
			const invList_t *ic;

			for (ic = menuInventory->c[node->container->id]; ic; ic = ic->next) {
				assert(ic->item.t);
				if (drawLoadable && INVSH_LoadableInWeapon(dragInfo.item.t, ic->item.t))
					MN_DrawItem(node, nodepos, &ic->item, ic->x, ic->y, scale, colorLoadable);
				else
					MN_DrawItem(node, nodepos, &ic->item, ic->x, ic->y, scale, color);
			}
		}
	}

	/* Draw free space if dragging - but not for idEquip */
	if (MN_DNDIsDragging() && node->container->id != csi.idEquip)
		MN_ContainerNodeDrawFreeSpace(menuInventory, node);

	return NULL;
}

/**
 * @brief Draw a preview of the DND item dropped into the node
 * @todo function create from a merge; computation can be cleanup (maybe)
 */
static void MN_ContainerNodeDrawDropPreview (menuNode_t *node)
{
	vec2_t nodepos;
	qboolean exists;
	int itemX = 0;
	int itemY = 0;
	int checkedTo = INV_DOES_NOT_FIT;
	vec3_t org;
	vec4_t color = { 1, 1, 1, 1 };
	const vec3_t scale = { 3.5, 3.5, 3.5 };

	MN_GetNodeAbsPos(node, nodepos);

	/** We calculate the position of the top-left corner of the dragged
	 * item in oder to compensate for the centered-drawn cursor-item.
	 * Or to be more exact, we calculate the relative offset from the cursor
	 * location to the middle of the top-left square of the item.
	 * @sa MN_LeftClick */
	if (dragInfo.item.t) {
		itemX = C_UNIT * dragInfo.item.t->sx / 2;	/* Half item-width. */
		itemY = C_UNIT * dragInfo.item.t->sy / 2;	/* Half item-height. */

		/* Place relative center in the middle of the square. */
		itemX -= C_UNIT / 2;
		itemY -= C_UNIT / 2;
	}

	/* Store information for preview drawing of dragged items. */
	if (MN_CheckNodeZone(node, mousePosX, mousePosY)
	 ||	MN_CheckNodeZone(node, mousePosX - itemX, mousePosY - itemY)) {
		dragInfo.toNode = node;
		dragInfo.to = node->container;

		dragInfo.toX = (mousePosX - nodepos[0] - itemX) / C_UNIT;
		dragInfo.toY = (mousePosY - nodepos[1] - itemY) / C_UNIT;

		/** Check if the items already exists in the container. i.e. there is already at least one item.
		 * @sa Com_AddToInventory */
		exists = qfalse;
		if (dragInfo.to && dragInfo.toNode
		 && (dragInfo.to->id == csi.idFloor || dragInfo.to->id == csi.idEquip)
		 && (dragInfo.toX  < 0 || dragInfo.toY < 0 || dragInfo.toX >= SHAPE_BIG_MAX_WIDTH || dragInfo.toY >= SHAPE_BIG_MAX_HEIGHT)
		 && Com_ExistsInInventory(menuInventory, dragInfo.to, dragInfo.item)) {
				exists = qtrue;
		 }

		/** Search for a suitable position to render the item at if
		 * the container is "single", the cursor is out of bound of the container.
		 */
		 if (!exists && dragInfo.item.t && (dragInfo.to->single
		  || dragInfo.toX  < 0 || dragInfo.toY < 0
		  || dragInfo.toX >= SHAPE_BIG_MAX_WIDTH || dragInfo.toY >= SHAPE_BIG_MAX_HEIGHT)) {
#if 0
/* ... or there is something in the way. */
/* We would need to check for weapon/ammo as well here, otherwise a preview would be drawn as well when hovering over the correct weapon to reload. */
		  || (Com_CheckToInventory(menuInventory, dragInfo.item.t, dragInfo.to, dragInfo.toX, dragInfo.toY) == INV_DOES_NOT_FIT)) {
#endif
			Com_FindSpace(menuInventory, &dragInfo.item, dragInfo.to, &dragInfo.toX, &dragInfo.toY, dragInfo.ic);
		}
	}


	/* draw the drop preview item */

	/** Revert the rotation info for the cursor-item in case it
	 * has been changed (can happen in rare conditions).
	 * @todo Maybe we can later change this to reflect "manual" rotation?
	 * @todo Check if this causes problems when letting the item snap back to its original location. */
	dragInfo.item.rotated = qfalse;

	/* Draw "preview" of placed (&rotated) item. */
	if (dragInfo.toNode && !dragInfo.to->scroll) {
		const int oldRotated = dragInfo.item.rotated;

		checkedTo = Com_CheckToInventory(menuInventory, dragInfo.item.t, dragInfo.to, dragInfo.toX, dragInfo.toY, dragInfo.ic);

		if (checkedTo == INV_FITS_ONLY_ROTATED)
			dragInfo.item.rotated = qtrue;

		if (checkedTo && Q_strncmp(dragInfo.item.t->type, "armour", MAX_VAR)) {	/* If the item fits somehow and it's not armour */
			vec2_t nodepos;

			MN_GetNodeAbsPos(dragInfo.toNode, nodepos);
			if (dragInfo.to->single) { /* Get center of single container for placement of preview item */
				VectorSet(org,
					nodepos[0] + dragInfo.toNode->size[0] / 2.0,
					nodepos[1] + dragInfo.toNode->size[1] / 2.0,
					-40);
			} else {
				/* This is a "grid" container - we need to calculate the item-position
				 * (on the screen) from stored placement in the container and the
				 * calculated rotation info. */
				if (dragInfo.item.rotated)
					VectorSet(org,
						nodepos[0] + (dragInfo.toX + dragInfo.item.t->sy / 2.0) * C_UNIT,
						nodepos[1] + (dragInfo.toY + dragInfo.item.t->sx / 2.0) * C_UNIT,
						-40);
				else
					VectorSet(org,
						nodepos[0] + (dragInfo.toX + dragInfo.item.t->sx / 2.0) * C_UNIT,
						nodepos[1] + (dragInfo.toY + dragInfo.item.t->sy / 2.0) * C_UNIT,
						-40);
			}
			Vector4Set(color, 0.5, 0.5, 1, 1);	/**< Make the preview item look blueish */
			MN_DrawItem(NULL, org, &dragInfo.item, -1, -1, scale, color);	/**< Draw preview item. */
		}

		dragInfo.item.rotated = oldRotated ;
	}

	dragInfo.isPlaceFound = checkedTo != 0;
}

/**
 * @todo need to think about a common mechanism from drag-drop
 * @todo need a cleanup/marge/refactoring with MN_DrawContainerNode
 */
static void MN_ContainerNodeDraw (menuNode_t *node)
{
	if (!menuInventory)
		return;

	if (node->color[3] > 0.001) {
		MN_DrawContainerNode(node);
	}

	if (MN_DNDIsDestinationNode(node))
		MN_ContainerNodeDrawDropPreview(node);
}

/**
 * @brief Custom tooltip for container node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 * @todo Why "MAX_VAR * 2"? MAX_VAR is already very big for tooltip no?
 */
static void MN_ContainerNodeDrawTooltip (menuNode_t *node, int x, int y)
{
	static char tooltiptext[MAX_VAR * 2];
	const invList_t *itemHover;
	vec2_t nodepos;

	MN_GetNodeAbsPos(node, nodepos);

	/** Find out where the mouse is. */
	if (node->container->scroll) {
		itemHover = MN_GetItemFromScrollableContainer(node, x, y, NULL, NULL);
	} else {
		itemHover = Com_SearchInInventory(menuInventory,
			node->container,
			(x - nodepos[0]) / C_UNIT,
			(y - nodepos[1]) / C_UNIT);
	}

	if (itemHover) {
		const int itemToolTipWidth = 250;

		/* Get name and info about item */
		MN_GetItemTooltip(itemHover->item, tooltiptext, sizeof(tooltiptext));
#ifdef DEBUG
		/* Display stored container-coordinates of the item. */
		Q_strcat(tooltiptext, va("\n%i/%i", itemHover->x, itemHover->y), sizeof(tooltiptext));
#endif
		MN_DrawTooltip("f_small", tooltiptext, x, y, itemToolTipWidth, 0);
	}
}

/**
 * @brief Gets location of the item the mouse is over (in a scrollable container)
 * @param[in] node	The container-node.
 * @param[in] mouseX	X location of the mouse.
 * @param[in] mouseY	Y location of the mouse.
 * @param[out] contX	X location in the container (index of item in row).
 * @param[out] contY	Y location in the container (row).
 * @sa INV_SearchInScrollableContainer
 */
invList_t *MN_GetItemFromScrollableContainer (const menuNode_t* const node, int mouseX, int mouseY, int* contX, int* contY)
{
	invList_t *ic;
	byte itemType;		/**< Variable to seperate weapons&other items (0) and ammo (1). */
	int curWidth = 0;	/**< Combined width of all drawn item so far. */
	int curHeight = 0;	/**< Combined height of all drawn item so far. */
	int maxHeight = 0;	/**< Max. height of a row. */
	int curItem = 0;	/**< Item counter for all items that _could_ get displayed in the current view (baseCurrent->equipType). */
	int curDispItem = 0;	/**< Item counter for all items that actually get displayed. */
	int rowOffset;

	int tempX, tempY;

	if (!contX)	contX = &tempX;
	if (!contY)	contY = &tempY;

	/* Change row spacing according to vertical/horizontal setting. */
	rowOffset = node->container->scrollVertical ? C_ROW_OFFSET : 0;

	for (itemType = 0; itemType <= 1; itemType++) {	/**< 0==weapons, 1==ammo */
		for (ic = menuInventory->c[node->container->id]; ic; ic = ic->next) {
			if (ic->item.t
			 && ((!itemType && !(!Q_strncmp(ic->item.t->type, "ammo", 4))) || (itemType && !Q_strncmp(ic->item.t->type, "ammo", 4)))
			 && INV_ItemMatchesFilter(ic->item.t, baseCurrent->equipType)) {
				if (curItem >= menuInventory->scrollCur
				&& curDispItem < menuInventory->scrollNum) {
					qboolean newRow = qfalse;
					vec2_t posMin;
					vec2_t posMax;
					MN_GetNodeAbsPos(node, posMin);
					posMin[0] += curWidth;
					posMin[1] += curHeight;
					Vector2Copy(posMin, posMax);

					assert(ic->item.t);

					if (!node->container->scrollVertical && curWidth + ic->item.t->sx * C_UNIT <= node->container->scroll) {
						curWidth += ic->item.t->sx * C_UNIT;
					} else {
						/* New row */
						if (curHeight + maxHeight + rowOffset > node->container->scrollHeight)
								return NULL;

						curHeight += maxHeight + rowOffset;
						curWidth = maxHeight = 0;

						/* Re-calculate the min & max values for this item in the new row. */
						MN_GetNodeAbsPos(node, posMin);
						posMin[1] += curHeight;
						Vector2Copy(posMin, posMax);
						newRow = qtrue;
					}

					maxHeight = max(ic->item.t->sy * C_UNIT, maxHeight);
					if (curHeight + ic->item.t->sy + rowOffset > node->container->scrollHeight)
						return NULL;

					posMax[0] += ic->item.t->sx * C_UNIT;
					posMax[1] += ic->item.t->sy * C_UNIT;

					/* If the mouse coordinate is within the area of this item/invList we return its pointer. */
					if (mouseX >= posMin[0]	&& mouseX <= posMax[0]
					 && mouseY >= posMin[1] && mouseY <= posMax[1]) {
						*contX = ic->x;
						*contY = ic->y;
						return ic;
					}

					if (newRow)
						curWidth += ic->item.t->sx * C_UNIT;

					/* This item uses ammo - lets check that as well. */
					if (node->container->scrollVertical && ic->item.t->numAmmos && ic->item.t != ic->item.t->ammos[0]) {
						int ammoIdx;
						objDef_t *ammoItem;
						posMin[0] += ic->item.t->sx * C_UNIT;
						Vector2Copy(posMin, posMax);

						/* Loop through all ammo-types for this item. */
						for (ammoIdx = 0; ammoIdx < ic->item.t->numAmmos; ammoIdx++) {
							ammoItem = ic->item.t->ammos[ammoIdx];
							/* Only check for researched ammo (although this is implyed in most cases). */
							if (ammoItem->tech && RS_IsResearched_ptr(ammoItem->tech)) {
								invList_t *icAmmo = INV_SearchInScrollableContainer(menuInventory, node->container, NONE, NONE, ammoItem, baseCurrent->equipType);

								/* Only check the item (and calculate its size) if it's in the container. */
								if (icAmmo) {
									posMax[0] += ammoItem->sx * C_UNIT;
									posMax[1] += ammoItem->sy * C_UNIT;

									/* If the mouse coordinate is within the area of this ammo item/invList we return its pointer. */
									if (mouseX >= posMin[0]	&& mouseX <= posMax[0]
									 && mouseY >= posMin[1] && mouseY <= posMax[1]) {
										*contX = icAmmo->x;
										*contY = icAmmo->y;
										return icAmmo;
									}

									curWidth += ammoItem->sx * C_UNIT + C_UNIT / 2.0;
									posMin[0] += ammoItem->sx * C_UNIT + C_UNIT / 2.0;
									Vector2Copy(posMin, posMax);
								}
							}
						}
					}

					/* Count displayed/visible items */
					curDispItem++;
				}

				/* Count all items that could get displayed. */
				curItem++;
			}
		}
	}

	*contX = *contY = NONE;
	return NULL;
}

/**
 * @brief Tries to switch to drag mode or auto-assign items
 * when right-click was used in the inventory.
 * @param[in] node The node in the menu that the mouse is over (i.e. a container node).
 * @param[in] base The base we are in.
 * @param[in] mouseX/mouseY Mouse coordinates.
 * @param[in] rightClick If we want to auto-assign items instead of dragging them this has to be qtrue.
 */
static void MN_Drag (menuNode_t* node, base_t *base, int mouseX, int mouseY, qboolean rightClick)
{
	int sel;

	if (!menuInventory)
		return;

	/* don't allow this in tactical missions */
	if (selActor && rightClick)
		return;

	sel = cl_selected->integer;
	if (sel < 0)
		return;

	if (mouseSpace == MS_MENU) {
		invList_t *ic;
		int fromX, fromY;
		assert(node->container);
		/* Get coordinates inside a scrollable container (if it is one). */
		if (node->container->scroll) {
			ic = MN_GetItemFromScrollableContainer(node, mouseX, mouseY, &fromX, &fromY);
			Com_DPrintf(DEBUG_CLIENT, "MN_Drag: item %i/%i selected from scrollable container.\n", fromX, fromY);
		} else {
			vec2_t nodepos;

			MN_GetNodeAbsPos(node, nodepos);
			/* Normalize screen coordinates to container coordinates. */
			fromX = (int) (mouseX - nodepos[0]) / C_UNIT;
			fromY = (int) (mouseY - nodepos[1]) / C_UNIT;

			ic = Com_SearchInInventory(menuInventory, node->container, fromX, fromY);
		}

		/* Start drag  */
		if (ic) {
			if (!rightClick) {
				/* Found item to drag. Prepare for drag-mode. */
				MN_DNDStart(node);
				MN_DNDDragItem(&(ic->item), ic);
				MN_DNDFromContainer(node->container, fromX, fromY);
			} else {
				/* Right click: automatic item assignment/removal. */
				if (node->container->id != csi.idEquip) {
					/* Move back to idEquip (ground, floor) container. */
					INV_MoveItem(menuInventory, &csi.ids[csi.idEquip], NONE, NONE, node->container, ic);
				} else {
					qboolean packed = qfalse;
					int px, py;
					assert(ic->item.t);
					/* armour can only have one target */
					if (!Q_strncmp(ic->item.t->type, "armour", MAX_VAR)) {
						packed = INV_MoveItem(menuInventory, &csi.ids[csi.idArmour], 0, 0, node->container, ic);
					/* ammo or item */
					} else if (!Q_strncmp(ic->item.t->type, "ammo", MAX_VAR)) {
						Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBelt], &px, &py, NULL);
						packed = INV_MoveItem(menuInventory, &csi.ids[csi.idBelt], px, py, node->container, ic);
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idHolster], &px, &py, NULL);
							packed = INV_MoveItem(menuInventory, &csi.ids[csi.idHolster], px, py, node->container, ic);
						}
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBackpack], &px, &py, NULL);
							packed = INV_MoveItem( menuInventory, &csi.ids[csi.idBackpack], px, py, node->container, ic);
						}
						/* Finally try left and right hand. There is no other place to put it now. */
						if (!packed) {
							const invList_t *rightHand = Com_SearchInInventory(menuInventory, &csi.ids[csi.idRight], 0, 0);

							/* Only try left hand if right hand is empty or no twohanded weapon/item is in it. */
							if (!rightHand || (rightHand && !rightHand->item.t->fireTwoHanded)) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idLeft], &px, &py, NULL);
								packed = INV_MoveItem(menuInventory, &csi.ids[csi.idLeft], px, py, node->container, ic);
							}
						}
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idRight], &px, &py, NULL);
							packed = INV_MoveItem(menuInventory, &csi.ids[csi.idRight], px, py, node->container, ic);
						}
					} else {
						if (ic->item.t->headgear) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idHeadgear], &px, &py, NULL);
							packed = INV_MoveItem(menuInventory, &csi.ids[csi.idHeadgear], px, py, node->container, ic);
						} else {
							/* left and right are single containers, but this might change - it's cleaner to check
							 * for available space here, too */
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idRight], &px, &py, NULL);
							packed = INV_MoveItem(menuInventory, &csi.ids[csi.idRight], px, py, node->container, ic);
							if (!packed) {
								const invList_t *rightHand = Com_SearchInInventory(menuInventory, &csi.ids[csi.idRight], 0, 0);

								/* Only try left hand if right hand is empty or no twohanded weapon/item is in it. */
								if (!rightHand || (rightHand && !rightHand->item.t->fireTwoHanded)) {
									Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idLeft], &px, &py, NULL);
									packed = INV_MoveItem(menuInventory, &csi.ids[csi.idLeft], px, py, node->container, ic);
								}
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBelt], &px, &py, NULL);
								packed = INV_MoveItem(menuInventory, &csi.ids[csi.idBelt], px, py, node->container, ic);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idHolster], &px, &py, NULL);
								packed = INV_MoveItem(menuInventory, &csi.ids[csi.idHolster], px, py, node->container, ic);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBackpack], &px, &py, NULL);
								packed = INV_MoveItem(menuInventory, &csi.ids[csi.idBackpack], px, py, node->container, ic);
							}
						}
					}
					/* no need to continue here - placement wasn't successful at all */
					if (!packed)
						return;
				}
			}
			UP_ItemDescription(ic->item.t);
/*			MN_DrawTooltip("f_verysmall", csi.ods[dragInfo.item.t].name, fromX, fromY, 0);*/
		}
	} else {
		invList_t *fItem;
		/* End drag */

		/* tactical mission */
		if (selActor) {
			assert(node->container);
			assert(dragInfo.from);
			MSG_Write_PA(PA_INVMOVE, selActor->entnum,
				dragInfo.from->id, dragInfo.fromX, dragInfo.fromY,
				node->container->id, dragInfo.toX, dragInfo.toY);

			MN_DNDStop();
			return;
		}

		assert(base);

		/* menu */
		if (dragInfo.from->scroll)
			fItem = INV_SearchInScrollableContainer(menuInventory, dragInfo.from, NONE, NONE, dragInfo.item.t, base->equipType);
		else
			fItem = Com_SearchInInventory(menuInventory, dragInfo.from, dragInfo.fromX, dragInfo.fromY);


		if (node->container->id == csi.idArmour) {
			/** hackhack @todo This is only because armour containers (and their nodes) are
			 * handled differently than normal containers somehow.
			 * dragInfo is not updated in MN_DrawMenus for them, this needs to be fixed.
			 * In a perfect world node->container would always be the same as dragInfo.to here. */
			if (dragInfo.toNode == node) {
				dragInfo.to = node->container;
				dragInfo.toX = 0;
				dragInfo.toY = 0;
			}
		}
		if (dragInfo.toNode) {
			INV_MoveItem(menuInventory,
				dragInfo.to, dragInfo.toX, dragInfo.toY,
				dragInfo.from, fItem);
		}
		MN_DNDStop();
	}

	/* Update display of scroll buttons. */
	MN_ScrollContainerUpdate_f();

	/* We are in the base or multiplayer inventory */
	if (sel < chrDisplayList.num) {
		assert(chrDisplayList.chr[sel]);
		if (chrDisplayList.chr[sel]->emplType == EMPL_ROBOT)
			CL_UGVCvars(chrDisplayList.chr[sel]);
		else
			CL_CharacterCvars(chrDisplayList.chr[sel]);

		/* Update object info */
		if (dragInfo.from) {
			Cvar_Set("mn_item", dragInfo.item.t->id);
		}
	}
}

static void MN_ContainerClick (menuNode_t *node, int x, int y)
{
	MN_Drag(node, baseCurrent, x, y, qfalse);
}

static void MN_ContainerRightClick (menuNode_t *node, int x, int y)
{
	MN_Drag(node, baseCurrent, x, y, qtrue);
}

static void MN_ContainerLoading (menuNode_t *node)
{
	node->container = NULL;
	node->color[3] = 1.0;
}

void MN_RegisterContainerNode (nodeBehaviour_t* behaviour)
{
	behaviour->name = "container";
	behaviour->id = MN_CONTAINER;
	behaviour->draw = MN_ContainerNodeDraw;
	behaviour->drawTooltip = MN_ContainerNodeDrawTooltip;
	behaviour->leftClick = MN_ContainerClick;
	behaviour->rightClick = MN_ContainerRightClick;
	behaviour->loading = MN_ContainerLoading;
	behaviour->loaded = MN_FindContainer;

	Cmd_AddCommand("scrollcont_update", MN_ScrollContainerUpdate_f, "Update display of scroll buttons.");
	Cmd_AddCommand("scrollcont_next", MN_ScrollContainerNext_f, "Scrolls the current container (forward).");
	Cmd_AddCommand("scrollcont_prev", MN_ScrollContainerPrev_f, "Scrolls the current container (backward).");
	Cmd_AddCommand("scrollcont_scroll", MN_ScrollContainerScroll_f, "Scrolls the current container.");
}
