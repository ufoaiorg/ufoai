/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

/** @brief One unit in the containers is 25x25. */
#define C_UNIT				25

/* prototypes */
struct base_s;
struct uiBehaviour_t;
struct uiNode_t;

class uiContainerNode : public uiLocatedNode {
public:
	void draw(uiNode_t* node) override;
	void drawTooltip(const uiNode_t* node, int x, int y) const override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) override;
	void onLoading(uiNode_t* node) override;
	void onLoaded(uiNode_t* node) override;
	bool onDndEnter(uiNode_t* node) override;
	bool onDndMove(uiNode_t* node, int x, int y) override;
	void onDndLeave(uiNode_t* node) override;
	bool onDndFinished(uiNode_t* node, bool isDroped) override;
};

extern Inventory* ui_inventory;

void UI_RegisterContainerNode(uiBehaviour_t* behaviour);
void UI_DrawItem(uiNode_t* node, const vec3_t org, const Item* item, int x, int y, const vec3_t scale, const vec4_t color);
void UI_ContainerNodeUpdateEquipment(Inventory* inv, const equipDef_t* ed);
uiNode_t* UI_GetContainerNodeByContainerIDX(const uiNode_t* const parent, const int index);
void UI_ContainerNodeAutoPlaceItem (uiNode_t* node, Item* ic);
void UI_GetItemTooltip (const Item& item, char* tooltipText, size_t stringMaxLength);

/**
 * @brief extradata for container widget
 * @note everything is not used by all container's type (grid, single, scrolled)
 */
typedef struct containerExtraData_s {
	/* for all containers */
	const invDef_t* container;		/**< The container linked to this node. */

	int lastSelectedId;				/**< id oject the object type selected */
	struct uiAction_s* onSelect;	/**< call when we select an item */
} containerExtraData_t;
