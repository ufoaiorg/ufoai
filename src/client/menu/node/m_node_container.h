/**
 * @file m_node_container.h
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

#ifndef CLIENT_MENU_M_NODE_CONTAINER_H
#define CLIENT_MENU_M_NODE_CONTAINER_H

/** @brief One unit in the containers is 25x25. */
#define C_UNIT				25

/**
 * @brief Offset between two rows (and the top of the container to
 * the first row) of items in a scrollable container.
 * Right now only used for vertical containers.
 */
#define	C_ROW_OFFSET		15

/* prototypes */
struct base_s;
struct inventory_s;
struct nodeBehaviour_s;
struct menuNode_s;
struct invList_s;
struct item_s;
struct invDef_s;

extern struct inventory_s *menuInventory;

void MN_RegisterContainerNode(struct nodeBehaviour_s *behaviour);
struct invList_s *MN_GetItemFromScrollableContainer (const struct menuNode_s* const node, int mouseX, int mouseY, int* contX, int* contY);
void MN_DrawItem(struct menuNode_s *node, const vec3_t org, const struct item_s *item, int x, int y, const vec3_t scale, const vec4_t color);
void MN_ContainerNodeSetFilter(struct menuNode_s* node, int num);

/**
 * @brief extradata for container widget
 * @note everything is not used by all container's type (grid, single, scrolled)
 */
typedef struct containerExtraData_s {
	/* for all containers */
	struct invDef_s *container;	/**< The container linked to this node. */

	/* for scrolled container */
	int filterEquipType;	/**< A filter */

	int columns;
	qboolean displayWeapon;
	qboolean displayAmmo;
	qboolean displayUnavailableItem;
	qboolean displayAmmoOfWeapon;
	qboolean displayUnavailableAmmoOfWeapon;

	int scrollCur;			/**< Index of first item that is displayed. */
	int scrollNum;			/**< Number of items that are displayed. */
	int scrollTotalNum;		/**< Total number of displayable items. */
	struct menuAction_s *onViewChange;	/**< called when view change (scrollCur, scrollNum, scrollTotalNum) */
} containerExtraData_t;


#endif
