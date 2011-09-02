/**
 * @file ui_node_container.h
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

#ifndef CLIENT_UI_UI_NODE_CONTAINER_H
#define CLIENT_UI_UI_NODE_CONTAINER_H

/** @brief One unit in the containers is 25x25. */
#define C_UNIT				25

/* prototypes */
struct base_s;
struct uiBehaviour_s;
struct uiNode_s;

extern inventory_t *ui_inventory;

void UI_RegisterContainerNode(struct uiBehaviour_s *behaviour);
void UI_DrawItem(struct uiNode_s *node, const vec3_t org, const struct item_s *item, int x, int y, const vec3_t scale, const vec4_t color);
void UI_ContainerNodeUpdateEquipment(inventory_t *inv, const equipDef_t *ed);
uiNode_t *UI_GetContainerNodeByContainerIDX(const uiNode_t* const parent, const int index);
void UI_ContainerNodeAutoPlaceItem (uiNode_t* node, invList_t *ic);

/**
 * @brief extradata for container widget
 * @note everything is not used by all container's type (grid, single, scrolled)
 */
typedef struct containerExtraData_s {
	/* for all containers */
	const invDef_t *container;	/**< The container linked to this node. */

	int lastSelectedId;		/**< id oject the object type selected */
	struct uiAction_s *onSelect;	/**< call when we select an item */
} containerExtraData_t;


#endif
