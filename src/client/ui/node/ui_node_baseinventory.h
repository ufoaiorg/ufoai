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

#include "ui_node_container.h"
#include "ui_node_abstractscrollable.h"

class uiBaseInventoryNode : public uiContainerNode {
	void draw(uiNode_t* node) override;
	void drawTooltip(const uiNode_t* node, int x, int y) const override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) override;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) override;
	void onWindowOpened(uiNode_t* node, linkedList_t* params) override;
	void onLoading(uiNode_t* node) override;
	void onLoaded(uiNode_t* node) override;
	bool onDndEnter(uiNode_t* node) override;
	bool onDndMove(uiNode_t* node, int x, int y) override;
	void onDndLeave(uiNode_t* node) override;
};

/* prototypes */
struct uiBehaviour_t;

typedef struct baseInventoryExtraData_s {
	containerExtraData_t super;

	/** A filter - see itemFilterTypes_t */
	int filterEquipType;

	int columns;
	bool displayWeapon;
	bool displayAmmo;
	bool displayImplant;
	bool displayUnavailableItem;
	bool displayAmmoOfWeapon;
	bool displayUnavailableAmmoOfWeapon;
	bool displayAvailableOnTop;

	/* scroll status */
	uiScroll_t scrollY;
	/* scroll callback when the status change */
	struct uiAction_s* onViewChange;

} baseInventoryExtraData_t;

/**
 * @note super must be at 0 else inherited function will crash.
 */
CASSERT(offsetof(baseInventoryExtraData_t, super) == 0);

void UI_RegisterBaseInventoryNode(uiBehaviour_t* behaviour);
