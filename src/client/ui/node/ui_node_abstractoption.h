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

#include "../../../shared/shared.h"
#include "ui_node_abstractscrollable.h"

struct uiAction_s;

class uiAbstractOptionNode : public uiLocatedNode {
	void doLayout(uiNode_t* node) override;
	int getCellWidth(uiNode_t* node) override;
	int getCellHeight(uiNode_t* node) override;
};

typedef struct {
	/* link to shared data (can be used if internal data is null) */
	int dataId;							/**< Shared data id where we can find option */
	int versionId;						/**< Cached version of the shared data, to check update */
	const char* cvar;					/**< Cvar containing current value */

	/* information */
	int count;							/**< number of elements */
	int lineHeight;

	uiScroll_t scrollY;					/**< Scroll position, if need */

	struct uiAction_s* onViewChange;	/**< called when view change (number of elements...) */

	/** Sprite used as a background */
	uiSprite_t* background;
} abstractOptionExtraData_t;

struct uiBehaviour_t;

void UI_RegisterAbstractOptionNode(uiBehaviour_t* behaviour);
void UI_OptionNodeSortOptions(uiNode_t* node);
uiNode_t* UI_AbstractOptionGetFirstOption(uiNode_t* node);
const char* UI_AbstractOptionGetCurrentValue(uiNode_t* node);
void UI_AbstractOptionSetCurrentValue(uiNode_t* node, const char* value);
