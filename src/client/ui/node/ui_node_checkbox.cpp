/**
 * @file
 * @brief The checkbox node is a three state widget. If the value is 0,
 * checkbox is unchecked, if value is bigger than 0, the value is checked;
 * but if the value is under 0, the checkbox display an "invalidate" status.
 * @code
 * checkbox check_item {
 *   cvar "*cvar foobar"
 *   pos  "410 100"
 * }
 * @endcode
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_main.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "../ui_sound.h"
#include "../ui_sprite.h"
#include "ui_node_checkbox.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractvalue.h"

#define EXTRADATA_TYPE checkboxExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

void uiCheckBoxNode::draw (uiNode_t* node)
{
	const float value = getValue(node);
	vec2_t pos;
	uiSprite_t* icon = nullptr;
	uiSpriteStatus_t status = SPRITE_STATUS_NORMAL;

	/* outer status */
	if (node->disabled) {
		status = SPRITE_STATUS_DISABLED;
	} else if (node->state) {
		status = SPRITE_STATUS_HOVER;
	} else {
		status = SPRITE_STATUS_NORMAL;
	}

	/* inner status */
	if (value == 0) {
		icon = EXTRADATA(node).iconUnchecked;
	} else if (value > 0) {
		icon = EXTRADATA(node).iconChecked;
	} else { /* value < 0 */
		icon = EXTRADATA(node).iconIndeterminate;
	}

	UI_GetNodeAbsPos(node, pos);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, status, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	}
	if (icon) {
		UI_DrawSpriteInBox(false, icon, status, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	}
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
void uiCheckBoxNode::onActivate (uiNode_t* node)
{
	const float last = getValue(node);
	float value;

	if (node->disabled)
		return;

	/* update value */
	value = (last > 0) ? 0 : 1;
	if (last == value)
		return;

	/* save result */
	setValue(node, value);
}

static void UI_CheckBoxNodeCallActivate (uiNode_t* node, const uiCallContext_t* context)
{
	UI_Node_Activate(node);
}

/**
 * @brief Handles checkboxes clicks
 */
void uiCheckBoxNode::onLeftClick (uiNode_t* node, int x, int y)
{
	if (node->disabled)
		return;

	onActivate(node);
	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);
	UI_PlaySound("click1");
}

/**
 * @brief Handled before the begin of the load of the node from the script
 */
void uiCheckBoxNode::onLoading (uiNode_t* node)
{
	uiAbstractValueNode::onLoading(node);
	setRange(node, -1, 1);
}

void UI_RegisterCheckBoxNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "checkbox";
	behaviour->extends = "abstractvalue";
	behaviour->manager = UINodePtr(new uiCheckBoxNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/** Sprite used as an icon for checked state */
	UI_RegisterExtradataNodeProperty(behaviour, "iconChecked", V_UI_SPRITEREF, EXTRADATA_TYPE, iconChecked);
	/** Sprite used as an icon for unchecked state */
	UI_RegisterExtradataNodeProperty(behaviour, "iconUnchecked", V_UI_SPRITEREF, EXTRADATA_TYPE, iconUnchecked);
	/** Sprite used as an icon for indeterminate state */
	UI_RegisterExtradataNodeProperty(behaviour, "iconIndeterminate", V_UI_SPRITEREF, EXTRADATA_TYPE, iconIndeterminate);
	/** Sprite used as a background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);

	/* Call it to toggle the node status. */
	UI_RegisterNodeMethod(behaviour, "toggle", UI_CheckBoxNodeCallActivate);
}
