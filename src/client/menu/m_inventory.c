/**
 * @file m_inventory.c
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

#include "../client.h"
#include "../cl_actor.h"
#include "m_inventory.h"
#include "m_draw.h"

inventory_t *menuInventory = NULL;

dragInfo_t dragInfo = {
	{NONE_AMMO, NULL, NULL, 1, 0},	/* item */
	NULL,	/* from */
	-1,		/* fromX */
	-1,		/* fromY */

	NULL,	/* toNode */
	NULL,	/* to */
	-1,		/* toX */
	-1		/* toY */

}; /**< To crash as soon as possible. */


/**
 * @brief Update display of scroll buttons.
 * @note The cvars "mn_cont_scroll_prev_hover" and "mn_cont_scroll_next_hover" are
 * set by the "in" and "out" functions of the scroll buttons.
 */
void MN_ScrollContainerUpdate_f (void)
{
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
}

/**
 * @brief Scrolls one item forward in a scrollable container.
 * @sa Console command "scrollcont_next"
 * @todo Add check so we do only increase if there are still hidden items after the last displayed one.
 */
void MN_ScrollContainerNext_f (void)
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
void MN_ScrollContainerPrev_f (void)
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
 * @brief Gets location of the item the mouse is over (in a scrollable container)
 * @param[in] node	The container-node.
 * @param[in] mouseX	X location of the mouse.
 * @param[in] mouseY	Y location of the mouse.
 * @param[out] contX	X location in the container (index of item in row).
 * @param[out] contY	Y location in the container (row).
 * @sa INV_SearchInScrollableContainer
 */
static invList_t *MN_GetItemFromScrollableContainer (const menuNode_t* const node, int mouseX, int mouseY, int* contX, int* contY)
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
					Vector2Copy(node->pos, posMin);
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
						Vector2Copy(node->pos, posMin);
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
void MN_Drag (const menuNode_t* const node, base_t *base, int mouseX, int mouseY, qboolean rightClick)
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

	assert(base);

	if (mouseSpace == MS_MENU) {
		invList_t *ic;
		int fromX, fromY;

		assert(node->container);
		/* Get coordinates inside a scrollable container (if it is one). */
		if (node->container->scroll) {
			ic = MN_GetItemFromScrollableContainer(node, mouseX, mouseY, &fromX, &fromY);
			Com_DPrintf(DEBUG_CLIENT, "MN_Drag: item %i/%i selected from scrollable container.\n", fromX, fromY);
		} else {
			/* Normalize screen coordinates to container coordinates. */
			fromX = (int) (mouseX - node->pos[0]) / C_UNIT;
			fromY = (int) (mouseY - node->pos[1]) / C_UNIT;

			ic = Com_SearchInInventory(menuInventory, node->container, fromX, fromY);
		}

		/* Start drag  */
		if (ic) {
			if (!rightClick) {
				/* Found item to drag. Prepare for drag-mode. */
				mouseSpace = MS_DRAG;
				dragInfo.item = ic->item;
				dragInfo.from = node->container;

				/* Store grid-position (in the container) of the mouse. */
				dragInfo.fromX = fromX;
				dragInfo.fromY = fromY;
			} else {
				/* Right click: automatic item assignment/removal. */
				if (node->container->id != csi.idEquip) {
					/* Move back to idEquip (ground, floor) container. */
					INV_MoveItem(base, menuInventory, &csi.ids[csi.idEquip], NONE, NONE, node->container, ic);
				} else {
					qboolean packed = qfalse;
					int px, py;
					assert(ic->item.t);
					/* armour can only have one target */
					if (!Q_strncmp(ic->item.t->type, "armour", MAX_VAR)) {
						packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idArmour], 0, 0, node->container, ic);
					/* ammo or item */
					} else if (!Q_strncmp(ic->item.t->type, "ammo", MAX_VAR)) {
						Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBelt], &px, &py);
						packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idBelt], px, py, node->container, ic);
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idHolster], &px, &py);
							packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idHolster], px, py, node->container, ic);
						}
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBackpack], &px, &py);
							packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idBackpack], px, py, node->container, ic);
						}
						/* Finally try left and right hand. There is no other place to put it now. */
						if (!packed) {
							const invList_t *rightHand = Com_SearchInInventory(menuInventory, &csi.ids[csi.idRight], 0, 0);

							/* Only try left hand if right hand is empty or no twohanded weapon/item is in it. */
							if (!rightHand || (rightHand && !rightHand->item.t->fireTwoHanded)) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idLeft], &px, &py);
								packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idLeft], px, py, node->container, ic);
							}
						}
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idRight], &px, &py);
							packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idRight], px, py, node->container, ic);
						}
					} else {
						if (ic->item.t->headgear) {
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idHeadgear], &px, &py);
							packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idHeadgear], px, py, node->container, ic);
						} else {
							/* left and right are single containers, but this might change - it's cleaner to check
							 * for available space here, too */
							Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idRight], &px, &py);
							packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idRight], px, py, node->container, ic);
							if (!packed) {
								const invList_t *rightHand = Com_SearchInInventory(menuInventory, &csi.ids[csi.idRight], 0, 0);

								/* Only try left hand if right hand is empty or no twohanded weapon/item is in it. */
								if (!rightHand || (rightHand && !rightHand->item.t->fireTwoHanded)) {
									Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idLeft], &px, &py);
									packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idLeft], px, py, node->container, ic);
								}
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBelt], &px, &py);
								packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idBelt], px, py, node->container, ic);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idHolster], &px, &py);
								packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idHolster], px, py, node->container, ic);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, &csi.ids[csi.idBackpack], &px, &py);
								packed = INV_MoveItem(base, menuInventory, &csi.ids[csi.idBackpack], px, py, node->container, ic);
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
		mouseSpace = MS_NULL;

		/* tactical mission */
		if (selActor) {
			assert(node->container);
			assert(dragInfo.from);
			MSG_Write_PA(PA_INVMOVE, selActor->entnum, dragInfo.from->id,
				dragInfo.fromX, dragInfo.fromY, node->container->id, dragInfo.toX, dragInfo.toY);

			return;
		/* menu */
		}

		if (dragInfo.from->scroll)
			fItem = INV_SearchInScrollableContainer(menuInventory, dragInfo.from, NONE, NONE, dragInfo.item.t, baseCurrent->equipType);
		else
			fItem = Com_SearchInInventory(menuInventory, dragInfo.from, dragInfo.fromX, dragInfo.fromY);

		INV_MoveItem(base, menuInventory, node->container,	/**< Can we use dragInfo.to here? */
			dragInfo.toX, dragInfo.toY, dragInfo.from, fItem);
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
	}
}

void MN_FindContainer (menuNode_t* const node)
{
	invDef_t *id;
	int i, j;

	for (i = 0, id = csi.ids; i < csi.numIDs; id++, i++)
		if (!Q_strncmp(node->name, id->name, sizeof(node->name)))
			break;

	if (i == csi.numIDs)
		node->container = NULL;
	else
		node->container = &csi.ids[i];

	if (node->container->scroll) {
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
void MN_DrawItem (const vec3_t org, const item_t *item, int x, int y, const vec3_t scale, const vec4_t color)
{
	objDef_t *od;

	assert(item);
	assert(item->t);
	od = item->t;

	if (od->image[0]) {
		vec3_t imgOrg;
		int imgWidth = item->t->sx * C_UNIT;
		int imgHeight = item->t->sy * C_UNIT;

		VectorCopy(org, imgOrg);

		/* Calculate correct location of the image (depends on rotation) */
		/** @todo Change the rotation of the image as well, right now only the location is changed.
		 * How is image-rotation handled right now? */
		if (x >= 0 || y >= 0) {
			/* Add offset of location in container. */
			imgOrg[0] += x * C_UNIT;
			imgOrg[1] += y * C_UNIT;

			/* Add offset for item-center (depends on rotation). */
			if (item->rotated) {
				imgOrg[0] += item->t->sy * C_UNIT / 2.0;
				imgOrg[1] += item->t->sx * C_UNIT / 2.0;
				/** @todo Image size calculation depends on handling of image-roation.
				imgWidth = item->t->sy * C_UNIT;
				imgHeight = item->t->sx * C_UNIT;
				*/
			} else {
				imgOrg[0] += item->t->sx * C_UNIT / 2.0;
				imgOrg[1] += item->t->sy * C_UNIT / 2.0;
			}
		}

		/* Draw the image. */
		R_DrawNormPic(imgOrg[0], imgOrg[1], imgWidth, imgHeight, 0, 0, 0, 0, ALIGN_CC, qtrue, od->image);
	} else if (od->model[0]) {
		modelInfo_t mi;
		vec3_t angles = { -10, 160, 70 };
		vec3_t origin;
		vec3_t size;
		vec4_t col;

		if (item->rotated)
			angles[0] -= 90;

		/* Draw model, if there is no image. */
		mi.name = od->model;
		mi.origin = origin;
		mi.angles = angles;
		mi.center = od->center;
		mi.scale = size;

		if (od->scale) {
			VectorScale(scale, od->scale, size);
		} else {
			VectorCopy(scale, size);
		}

		VectorCopy(org, origin);
		/* Calculate correct location of the model (depends on rotation) */
		if (x >= 0 || y >= 0) {
			/* Add offset of location in container. */
			origin[0] += C_UNIT * x;
			origin[1] += C_UNIT * y;

			/* Add offset for item-center (depends on rotation). */
			if (item->rotated) {
				origin[0] += item->t->sy * C_UNIT / 2.0;
				origin[1] += item->t->sx * C_UNIT / 2.0;
			} else {
				origin[0] += item->t->sx * C_UNIT / 2.0;
				origin[1] += item->t->sy * C_UNIT / 2.0;
			}
		}

		mi.frame = 0;
		mi.oldframe = 0;
		mi.backlerp = 0;
		mi.skin = 0;

		Vector4Copy(color, col);
		/* no ammo in this weapon - highlight this item */
		if (od->weapon && od->reload && !item->a) {
			col[1] *= 0.5;
			col[2] *= 0.5;
		}

		mi.color = col;

		/* draw the model */
		R_DrawModelDirect(&mi, NULL, NULL);
	}
}


/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
void MN_DrawDisabled (const menuNode_t* node)
{
	const vec4_t color = { 0.3f, 0.3f, 0.3f, 0.7f };
	R_DrawFill(node->pos[0], node->pos[1], node->size[0], node->size[1], ALIGN_UL, color);
}

/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
void MN_DrawFree (int container, const menuNode_t *node, int posx, int posy, int sizex, int sizey, qboolean showTUs)
{
	const vec4_t color = { 0.0f, 1.0f, 0.0f, 0.7f };
	invDef_t* inv = &csi.ids[container];

	R_DrawFill(posx, posy, sizex, sizey, ALIGN_UL, color);

	/* if showTUs is true (only the first time in none single containers)
	 * and we are connected to a game */
	if (showTUs && cls.state == ca_active)
		R_FontDrawString("f_verysmall", 0, node->pos[0] + 3, node->pos[1] + 3,
			node->pos[0] + 3, node->pos[1] + 3, node->size[0] - 6, 0, 0,
			va(_("In: %i Out: %i"), inv->in, inv->out), 0, 0, NULL, qfalse);
}

/**
 * @brief Draws the free and usable inventory positions when dragging an item.
 * @note Only call this function in dragging mode (mouseSpace == MS_DRAG)
 */
void MN_InvDrawFree (inventory_t *inv, const menuNode_t *node)
{
	objDef_t *od = dragInfo.item.t;	/**< Get the 'type' of the dragged item. */

	qboolean showTUs = qtrue;

	/* The shape of the free positions. */
	uint32_t free[SHAPE_BIG_MAX_HEIGHT];
	int x, y;
	int checkedTo;

	/* Draw only in dragging-mode and not for the equip-floor */
	assert(mouseSpace == MS_DRAG);
	assert(inv);

	/* if single container (hands, extension, headgear) */
	if (node->container->single) {
		/* if container is free or the dragged-item is in it */
		if (node->container == dragInfo.from || Com_CheckToInventory(inv, od, node->container, 0, 0))
			MN_DrawFree(node->container->id, node, node->pos[0], node->pos[1], node->size[0], node->size[1], qtrue);
	} else {
		memset(free, 0, sizeof(free));
		for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++) {
			for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++) {
				/* Check if the current position is usable (topleft of the item). */

				/* Add '1's to each position the item is 'blocking'. */
				checkedTo = Com_CheckToInventory(inv, od, node->container, x, y);
				if (checkedTo & INV_FITS)				/* Item can be placed normally. */
					Com_MergeShapes(free, (uint32_t)od->shape, x, y);
				if (checkedTo & INV_FITS_ONLY_ROTATED)	/* Item can be placed rotated. */
					Com_MergeShapes(free, Com_ShapeRotate((uint32_t)od->shape), x, y);

				/* Only draw on existing positions. */
				if (Com_CheckShape(node->container->shape, x, y)) {
					if (Com_CheckShape(free, x, y)) {
						MN_DrawFree(node->container->id, node, node->pos[0] + x * C_UNIT, node->pos[1] + y * C_UNIT, C_UNIT, C_UNIT, showTUs);
						showTUs = qfalse;
					}
				}
			}	/* for x */
		}	/* for y */
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
 * @brief Draw the container with all its items
 * @note The item tooltip is done after the menu is completely
 * drawn - because it must be on top of every other node and item
 * @return The item in the container to hover
 */
const invList_t* MN_DrawContainerNode (menuNode_t *node)
{
	const vec3_t scale = {3.5, 3.5, 3.5};
	vec4_t color = {1, 1, 1, 1};
	vec4_t colorLoadable = {0.5, 1, 0.5, 1};
	qboolean drawLoadable = qfalse;

	if (!node->container)
		MN_FindContainer(node);
	if (!node->container)
		return NULL;

	assert(menuInventory);

	/* Highlight weapons that the dragged ammo (if it is one) can be loaded into. */
	if (mouseSpace == MS_DRAG && dragInfo.item.t)
		drawLoadable = qtrue;

	if (node->container->single) {
		/* Single container */
		vec2_t pos;
		Vector2Copy(node->pos, pos);
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
						MN_DrawItem(pos, item, -1, -1, scale, colorLoadable);
					else
						MN_DrawItem(pos, item, -1, -1, scale, color);
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
				MN_DrawItem(pos, item, -1, -1, scale, colorLoadable);
			else
				MN_DrawItem(pos, item, -1, -1, scale, color);
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
							Vector2Copy(node->pos, pos);
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
									Vector2Copy(node->pos, pos);
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
									MN_DrawItem(pos, &tempItem, -1, -1, scale, colorLoadable);
								else
									MN_DrawItem(pos, &tempItem, -1, -1, scale, color);

								if (node->container->scrollVertical) {
									/* Draw the item name. */
									R_FontDrawString("f_verysmall", ALIGN_UL,
										posName[0], posName[1],
										posName[0], posName[1],
										0,	/* maxWidth */
										0,	/* maxHeight */
										0, va("%s (%i)", ic->item.t->name,  ic->item.amount), 0, 0, NULL, qfalse);
								} /* scrollVertical */

								if (newRow)
									curWidth += ic->item.t->sx * C_UNIT;

								/* This item uses ammo - lets display that as well. */
								if (node->container->scrollVertical && ic->item.t->numAmmos && ic->item.t != ic->item.t->ammos[0]) {
									int ammoIdx;
									invList_t *icAmmo;

									pos[0] += ic->item.t->sx * C_UNIT / 2.0;

									/* Loop through all ammo-types for this item. */
									for (ammoIdx = 0; ammoIdx <ic->item.t->numAmmos; ammoIdx++) {
										tempItem.t = ic->item.t->ammos[ammoIdx];
										if (tempItem.t->tech && RS_IsResearched_ptr(tempItem.t->tech)) {
											icAmmo = INV_SearchInScrollableContainer(menuInventory, node->container, NONE, NONE, tempItem.t, baseCurrent->equipType);

											/* If we've found a piece of this ammo in the inventory we draw it. */
											if (icAmmo) {
												/* Calculate the center of the item model/image. */
												pos[0] += tempItem.t->sx * C_UNIT / 2;
												pos[1] = node->pos[1] + curHeight + icAmmo->item.t->sy * C_UNIT / 2.0;

												curWidth += tempItem.t->sx * C_UNIT;

												MN_DrawItem(pos, &tempItem, -1, -1, scale, color);
												R_FontDrawString("f_verysmall", ALIGN_LC,
													pos[0] + icAmmo->item.t->sx * C_UNIT / 2.0, pos[1] + icAmmo->item.t->sy * C_UNIT / 2.0,
													pos[0] + icAmmo->item.t->sx * C_UNIT / 2.0, pos[1] + icAmmo->item.t->sy * C_UNIT / 2.0,
													C_UNIT,	/* maxWidth */
													0,	/* maxHeight */
													0, va("%i", icAmmo->item.amount), 0, 0, NULL, qfalse);

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
					MN_DrawItem(node->pos, &ic->item, ic->x, ic->y, scale, colorLoadable);
				else
					MN_DrawItem(node->pos, &ic->item, ic->x, ic->y, scale, color);
			}
		}
	}

	/* Draw free space if dragging - but not for idEquip */
	if (mouseSpace == MS_DRAG && node->container->id != csi.idEquip)
		MN_InvDrawFree(menuInventory, node);

	/**@todo Draw tooltips for dragged ammo (and info about weapon it can be loaded in when hovering over it). */
	/* Draw tooltip for weapon or ammo */
	if (mouseSpace != MS_DRAG && node->state && mn_show_tooltips->integer) {
		assert(node->container);

		/** Find out where the mouse is. */
		if (node->container->scroll) {
			return MN_GetItemFromScrollableContainer(node, mousePosX, mousePosY, NULL, NULL);
		} else {
			return Com_SearchInInventory(menuInventory,
				node->container,
				(mousePosX - node->pos[0]) / C_UNIT,
				(mousePosY - node->pos[1]) / C_UNIT);
		}
	}

	return NULL;
}

void MN_DrawItemNode (menuNode_t *node, const char *itemName)
{
	int i;
	item_t item = {1, NULL, NULL, 0, 0}; /* 1 so it's not red-ish; fake item anyway */
	const vec4_t color = {1, 1, 1, 1};
	vec2_t pos;

	for (i = 0; i < csi.numODs; i++)
		if (!Q_strncmp(itemName, csi.ods[i].id, MAX_VAR))
			break;
	if (i == csi.numODs)
		return;

	item.t = &csi.ods[i];

	/* We position the model of the item ourself (in the middle of the item
	 * node). See the "-1, -1" parameter of MN_DrawItem. */
	Vector2Copy(node->pos, pos);
	pos[0] += node->size[0] / 2.0;
	pos[1] += node->size[1] / 2.0;
	MN_DrawItem(pos, &item, -1, -1, node->scale, color);
}
