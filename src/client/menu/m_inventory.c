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

int dragFrom, dragFromX, dragFromY;
item_t dragItem = {NONE_AMMO, NULL, NULL, 1, 0}; /* to crash as soon as possible */

/**
 * @note: node->mousefx is the container id
 */
void MN_Drag (const menuNode_t* const node, int x, int y, qboolean rightClick)
{
	int px, py, sel;

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

		/* normalize it */
		px = (int) (x - node->pos[0]) / C_UNIT;
		py = (int) (y - node->pos[1]) / C_UNIT;

		/* start drag (mousefx represents container number) */
		ic = Com_SearchInInventory(menuInventory, node->mousefx, px, py);
		if (ic) {
			if (!rightClick) {
				/* found item to drag */
				mouseSpace = MS_DRAG;
				dragItem = ic->item;
				/* mousefx is the container (see hover code) */
				dragFrom = node->mousefx;
				dragFromX = ic->x;
				dragFromY = ic->y;
			} else {
				if (node->mousefx != csi.idEquip) {
					/* back to idEquip (ground, floor) container */
					INV_MoveItem(baseCurrent, menuInventory, csi.idEquip, -1, -1, node->mousefx, ic->x, ic->y);
				} else {
					qboolean packed = qfalse;

					assert(ic->item.t);
					/* armour can only have one target */
					if (!Q_strncmp(ic->item.t->type, "armour", MAX_VAR)) {
						packed = INV_MoveItem(baseCurrent, menuInventory, csi.idArmour, 0, 0, node->mousefx, ic->x, ic->y);
					/* ammo or item */
					} else if (!Q_strncmp(ic->item.t->type, "ammo", MAX_VAR)) {
						Com_FindSpace(menuInventory, &ic->item, csi.idBelt, &px, &py);
						packed = INV_MoveItem(baseCurrent, menuInventory, csi.idBelt, px, py, node->mousefx, ic->x, ic->y);
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, csi.idHolster, &px, &py);
							packed = INV_MoveItem(baseCurrent, menuInventory, csi.idHolster, px, py, node->mousefx, ic->x, ic->y);
						}
						if (!packed) {
							Com_FindSpace(menuInventory, &ic->item, csi.idBackpack, &px, &py);
							packed = INV_MoveItem(baseCurrent, menuInventory, csi.idBackpack, px, py, node->mousefx, ic->x, ic->y);
						}
					} else {
						if (ic->item.t->headgear) {
							Com_FindSpace(menuInventory, &ic->item, csi.idHeadgear, &px, &py);
							packed = INV_MoveItem(baseCurrent, menuInventory, csi.idHeadgear, px, py, node->mousefx, ic->x, ic->y);
						} else {
							/* left and right are single containers, but this might change - it's cleaner to check
							 * for available space here, too */
							Com_FindSpace(menuInventory, &ic->item, csi.idRight, &px, &py);
							packed = INV_MoveItem(baseCurrent, menuInventory, csi.idRight, px, py, node->mousefx, ic->x, ic->y);
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, csi.idLeft, &px, &py);
								packed = INV_MoveItem(baseCurrent, menuInventory, csi.idLeft, px, py, node->mousefx, ic->x, ic->y);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, csi.idHolster, &px, &py);
								packed = INV_MoveItem(baseCurrent, menuInventory, csi.idHolster, px, py, node->mousefx, ic->x, ic->y);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, csi.idBelt, &px, &py);
								packed = INV_MoveItem(baseCurrent, menuInventory, csi.idBelt, px, py, node->mousefx, ic->x, ic->y);
							}
							if (!packed) {
								Com_FindSpace(menuInventory, &ic->item, csi.idBackpack, &px, &py);
								packed = INV_MoveItem(baseCurrent, menuInventory, csi.idBackpack, px, py, node->mousefx, ic->x, ic->y);
							}
						}
					}
					/* no need to continue here - placement wasn't successful at all */
					if (!packed)
						return;
				}
			}
			UP_ItemDescription(ic->item.t->idx);
/*			MN_DrawTooltip("f_verysmall", csi.ods[dragItem.t].name, px, py, 0);*/
		}
	} else {
		/* end drag */
		mouseSpace = MS_NULL;
		px = (int) ((x - node->pos[0] - C_UNIT * ((dragItem.t->sx - 1) / 2.0)) / C_UNIT);
		py = (int) ((y - node->pos[1] - C_UNIT * ((dragItem.t->sy - 1) / 2.0)) / C_UNIT);

		/* tactical mission */
		if (selActor) {
			MSG_Write_PA(PA_INVMOVE, selActor->entnum, dragFrom, dragFromX, dragFromY, node->mousefx, px, py);
			return;
		/* menu */
		}

		INV_MoveItem(baseCurrent, menuInventory, node->mousefx, px, py, dragFrom, dragFromX, dragFromY);
	}

	/* We are in the base or multiplayer inventory */
	if (sel < chrDisplayList.num) {
		assert(chrDisplayList.chr[sel]);
		if (chrDisplayList.chr[sel]->empl_type == EMPL_ROBOT)
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
		node->mousefx = NONE;
	else
		node->mousefx = id - csi.ids;

	/* start on the last bit of the shape mask */
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

/**
 * @brief Draws an item to the screen
 *
 * @param[in] org Node position on the screen (pixel)
 * @param[in] item The item to draw
 * @param[in] sx Size in x direction (no pixel but container space)
 * @param[in] sy Size in y direction (no pixel but container space)
 * @param[in] x Position in container
 * @param[in] y Position in container
 * @param[in] scale
 * @param[in] color
 * @sa SCR_DrawCursor
 * Used to draw an item to the equipment containers. First look whether the objDef_t
 * includes an image - if there is none then draw the model
 */
void MN_DrawItem (const vec3_t org, item_t item, int sx, int sy, int x, int y, const vec3_t scale, const vec4_t color)
{
	objDef_t *od;

	assert(item.t);
	od = item.t;

	if (od->image[0]) {
		/* draw the image */
		R_DrawNormPic(
			org[0] + C_UNIT / 2.0 * sx + C_UNIT * x,
			org[1] + C_UNIT / 2.0 * sy + C_UNIT * y,
			C_UNIT * sx, C_UNIT * sy, 0, 0, 0, 0, ALIGN_CC, qtrue, od->image);
	} else if (od->model[0]) {
		modelInfo_t mi;
		vec3_t angles = { -10, 160, 70 };
		vec3_t origin;
		vec3_t size;
		vec4_t col;

		if (item.rotated)
			angles[0] -= 90;

		/* draw model, if there is no image */
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
		if (x || y || sx || sy) {
			origin[0] += C_UNIT / 2.0 * sx;
			origin[1] += C_UNIT / 2.0 * sy;
			origin[0] += C_UNIT * x;
			origin[1] += C_UNIT * y;
		}

		mi.frame = 0;
		mi.oldframe = 0;
		mi.backlerp = 0;
		mi.skin = 0;

		Vector4Copy(color, col);
		/* no ammo in this weapon - highlight this item */
		if (od->weapon && od->reload && !item.a) {
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
	objDef_t *od = dragItem.t;	/**< Get the 'type' of the dragged item. */
	int container = node->mousefx;

	qboolean showTUs = qtrue;

	/* The shape of the free positions. */
	uint32_t free[SHAPE_BIG_MAX_HEIGHT];
	int x, y;

	/* Draw only in dragging-mode and not for the equip-floor */
	assert(mouseSpace == MS_DRAG);
	assert(inv);

	/* if single container (hands, extension, headgear) */
	if (csi.ids[container].single) {
		/* if container is free or the dragged-item is in it */
		if (node->mousefx == dragFrom || Com_CheckToInventory(inv, od->idx, container, 0, 0))
			MN_DrawFree(container, node, node->pos[0], node->pos[1], node->size[0], node->size[1], qtrue);
	} else {
		memset(free, 0, sizeof(free));
		for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++) {
			for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++) {
				/* Check if the current position is usable (topleft of the item) */
				if (Com_CheckToInventory(inv, od->idx, container, x, y)) {
					/* add '1's to each position the item is 'blocking' */
					Com_MergeShapes(free, (uint32_t)od->shape, x, y);
				}
				/* Only draw on existing positions */
				if (Com_CheckShape(csi.ids[container].shape, x, y)) {
					if (Com_CheckShape(free, x, y)) {
						MN_DrawFree(container, node, node->pos[0] + x * C_UNIT, node->pos[1] + y * C_UNIT, C_UNIT, C_UNIT, showTUs);
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
 * @param[in|out] tooltiptext Pointer to a string the information should be written into.
 * @param[in] Max. string size of tooltiptext.
 * @return Number of lines
 */
void MN_GetItemTooltip (item_t item, char *tooltiptext, size_t string_maxlength)
{
	int i;
	objDef_t *weapon;

	assert(item.t);

	if (item.amount > 1)
		Q_strncpyz(tooltiptext, va("%i x %s\n", item.amount, item.t->name), string_maxlength);
	else
		Q_strncpyz(tooltiptext, va("%s\n", item.t->name), string_maxlength);

	/* Only display further info if item.t is researched */
	if (RS_ItemIsResearched(item.t->id)) {
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
					if (RS_ItemIsResearched(weapon->id)) {
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
	invList_t *ic;

	if (node->mousefx == C_UNDEFINED)
		MN_FindContainer(node);
	if (node->mousefx == NONE)
		return NULL;

	if (csi.ids[node->mousefx].single) {
		/* single item container (special case for left hand) */
		if (node->mousefx == csi.idLeft && !menuInventory->c[csi.idLeft]) {
			color[3] = 0.5;
			if (menuInventory->c[csi.idRight] && menuInventory->c[csi.idRight]->item.t->holdTwoHanded)
				MN_DrawItem(node->pos, menuInventory->c[csi.idRight]->item, node->size[0] / C_UNIT,
							node->size[1] / C_UNIT, 0, 0, scale, color);
		} else if (menuInventory->c[node->mousefx]) {
			if (node->mousefx == csi.idRight &&
					menuInventory->c[csi.idRight]->item.t->fireTwoHanded &&
					menuInventory->c[csi.idLeft]) {
				color[0] = color[1] = color[2] = color[3] = 0.5;
				MN_DrawDisabled(node);
			}
			MN_DrawItem(node->pos, menuInventory->c[node->mousefx]->item, node->size[0] / C_UNIT,
						node->size[1] / C_UNIT, 0, 0, scale, color);
		}
	} else {
		/* standard container */
		for (ic = menuInventory->c[node->mousefx]; ic; ic = ic->next) {
			MN_DrawItem(node->pos, ic->item, ic->item.t->sx, ic->item.t->sy, ic->x, ic->y, scale, color);
		}
	}
	/* draw free space if dragging - but not for idEquip */
	if (mouseSpace == MS_DRAG && node->mousefx != csi.idEquip)
		MN_InvDrawFree(menuInventory, node);

	/* Draw tooltip for weapon or ammo */
	if (mouseSpace != MS_DRAG && node->state && mn_show_tooltips->integer)
		/* Find out where the mouse is */
		return Com_SearchInInventory(menuInventory,
			node->mousefx, (mousePosX - node->pos[0]) / C_UNIT, (mousePosY - node->pos[1]) / C_UNIT);
	return NULL;
}

void MN_DrawItemNode (menuNode_t *node, const char *itemName)
{
	int i;
	item_t item = {1, NULL, NULL, 0, 0}; /* 1 so it's not reddish; fake item anyway */
	const vec4_t color = {1, 1, 1, 1};

	if (node->mousefx == C_UNDEFINED)
		MN_FindContainer(node);
	if (node->mousefx == NONE) {
		Com_Printf("no container for drawing this item (%s)...\n", itemName);
		return;
	}

	for (i = 0; i < csi.numODs; item.t++)
		if (!Q_strncmp(itemName, csi.ods[i].id, MAX_VAR))
			break;
	if (i == csi.numODs)
		return;

	item.t = &csi.ods[i];

	MN_DrawItem(node->pos, item, 0, 0, 0, 0, node->scale, color);
}
