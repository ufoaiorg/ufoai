/**
 * @file
 * @brief This node implements the key binding change and display
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

#include "../ui_main.h"
#include "../ui_draw.h"
#include "../ui_nodes.h"
#include "../ui_font.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_input.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "../ui_sprite.h"
#include "ui_node_keybinding.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"

#include "../../client.h"

#define EXTRADATA_TYPE keyBindingExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

/**
 * @brief Called when the user click with the right mouse button
 */
void uiKeyBindingNode::onLeftClick (uiNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no binding given */
	if (!node->text)
		return;

	if (!Q_strstart(node->text, "*binding"))
		return;

	if (!UI_HasFocus(node)) {
		if (node->onClick)
			UI_ExecuteEventActions(node, node->onClick);
		UI_RequestFocus(node);
	}
}

/**
 * @brief Called when we press a key when the node got the focus
 * @return True, if we use the event
 */
bool uiKeyBindingNode::onKeyPressed (uiNode_t *node, unsigned int key, unsigned short unicode)
{
	const char *binding;

	UI_RemoveFocus();

	/** @todo what about macro expansion? */
	char const* const command = Q_strstart(node->text, "*binding:");
	if (!command)
		return false;

	/** @todo ensure that the binding for the key command is not executed */

	binding = Key_GetBinding(command, EXTRADATA(node).keySpace);
	if (binding[0] != '\0') {
		/* if it's the same command, do not change anything, otherwise
		 * show the reason why nothing was changed */
		if (!Q_streq(binding, command)) {
			const char *keyStr = Key_KeynumToString(key);
			UI_DisplayNotice(va(_("Key %s already bound"), keyStr), 2000, NULL);
		}
		return false;
	}

	/* fire change event */
	if (node->onChange)
		UI_ExecuteEventActions(node, node->onChange);

	Key_SetBinding(key, command, EXTRADATA(node).keySpace);

	return true;
}

void uiKeyBindingNode::draw (uiNode_t *node)
{
	const char *binding, *description, *command;
	const float *textColor;
	vec2_t pos;
	const char *font = UI_GetFontFromNode(node);
	const int bindingWidth = EXTRADATA(node).bindingWidth;
	const int descriptionWidth = node->box.size[0] - bindingWidth;
	vec2_t descriptionPos, descriptionSize;
	vec2_t bindingPos, bindingSize;
	uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;

	if (node->state) {
		textColor = node->color;
		iconStatus = SPRITE_STATUS_HOVER;
	} else {
		textColor = node->color;
	}
	if (UI_HasFocus(node))
		textColor = node->selectedColor;

	UI_GetNodeAbsPos(node, pos);

	Vector2Set(descriptionSize, descriptionWidth, node->box.size[1]);
	Vector2Set(bindingSize, bindingWidth, node->box.size[1]);
	Vector2Set(descriptionPos, pos[0], pos[1]);
	Vector2Set(bindingPos, pos[0] + descriptionWidth + node->padding, pos[1]);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, iconStatus, descriptionPos[0], descriptionPos[1], descriptionSize[0], descriptionSize[1]);
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, iconStatus, bindingPos[0], bindingPos[1], bindingSize[0], bindingSize[1]);
	}

	binding = UI_GetReferenceString(node, node->text);
	if (Q_strnull(binding))
		binding = _("NONE");

	/** @todo check that this is a keybinding value (with macro expansion) */
	command = node->text + 9;
	description = Cmd_GetCommandDesc(command);
	if (description[0] == '\0')
		description = command;

	R_Color(textColor);

	UI_DrawStringInBox(font, (align_t)node->contentAlign,
		descriptionPos[0] + node->padding, descriptionPos[1] + node->padding,
		descriptionSize[0] - node->padding - node->padding, descriptionSize[1] - node->padding - node->padding,
		description);

	UI_DrawStringInBox(font, (align_t)node->contentAlign,
		bindingPos[0] + node->padding, bindingPos[1] + node->padding,
		bindingSize[0] - node->padding - node->padding, bindingSize[1] - node->padding - node->padding,
		binding);

	R_Color(NULL);
}

/**
 * @brief Call before the script initialization of the node
 */
void uiKeyBindingNode::onLoading (uiNode_t *node)
{
	node->padding = 8;
	node->contentAlign = ALIGN_CL;
	EXTRADATA(node).bindingWidth = 50;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 0.5);
}

void UI_RegisterKeyBindingNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "keybinding";
	behaviour->manager = UINodePtr(new uiKeyBindingNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	UI_RegisterExtradataNodeProperty(behaviour, "keyspace", V_INT, keyBindingExtraData_t, keySpace);
	UI_RegisterExtradataNodeProperty(behaviour, "bindingwidth", V_INT, keyBindingExtraData_t, bindingWidth);
	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
