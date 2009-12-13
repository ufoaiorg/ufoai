/**
 * @file m_node_keybinding.c
 * @brief This node implements the key binding change and display
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_draw.h"
#include "../m_nodes.h"
#include "../m_font.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "../m_actions.h"
#include "../m_render.h"
#include "m_node_keybinding.h"
#include "m_node_abstractnode.h"
#include "m_node_panel.h"

#include "../../client.h"

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

#define EXTRADATA(node) (node->u.keybinding)

/**
 * @brief Called when the user click with the right mouse button
 */
static void MN_KeyBindingNodeClick (menuNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no binding given */
	if (!node->text)
		return;

	if (strncmp(node->text, "*binding", 8))
		return;

	if (!MN_HasFocus(node)) {
		if (node->onClick)
			MN_ExecuteEventActions(node, node->onClick);
		MN_RequestFocus(node);
	}
}

/**
 * @brief Called when we press a key when the node got the focus
 * @return True, if we use the event
 */
static qboolean MN_KeyBindingNodeKeyPressed (menuNode_t *node, unsigned int key, unsigned short unicode)
{
	const char *command;
	char keyStr[MAX_VAR];
	const char *binding;

	MN_RemoveFocus();

	/** @todo what about macro expansion? */
	if (strncmp(node->text, "*binding:", 9))
		return qfalse;

	Q_strncpyz(keyStr, Key_KeynumToString(key), sizeof(keyStr));
	command = node->text + 9;

	/** @todo ensure that the binding for the key command is not executed */

	binding = Key_GetBinding(command, EXTRADATA(node).keySpace);
	if (binding[0] != '\0') {
		/* if it's the same command, do not change anything, otherwise
		 * show the reason why nothing was changed */
		if (strcmp(binding, command))
			MN_DisplayNotice(va(_("Key %s already bound"), keyStr), 2000, NULL);
		return qfalse;
	}

	/* fire change event */
	if (node->onChange)
		MN_ExecuteEventActions(node, node->onChange);

	Key_SetBinding(key, command, EXTRADATA(node).keySpace);

	return qtrue;
}

static void MN_KeyBindingNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *binding, *description, *command;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	const char *font = MN_GetFontFromNode(node);
	const int bindingWidth = EXTRADATA(node).bindingWidth;
	const int descriptionWidth = node->size[0] - bindingWidth;
	vec2_t descriptionPos, descriptionSize;
	vec2_t bindingPos, bindingSize;

	if (node->state) {
		textColor = node->color;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}
	if (MN_HasFocus(node))
		textColor = node->selectedColor;

	MN_GetNodeAbsPos(node, pos);

	Vector2Set(descriptionSize, descriptionWidth, node->size[1]);
	Vector2Set(bindingSize, bindingWidth, node->size[1]);
	Vector2Set(descriptionPos, pos[0], pos[1]);
	Vector2Set(bindingPos, pos[0] + descriptionWidth + node->padding, pos[1]);

	image = MN_GetReferenceString(node, node->image);
	if (image) {
		MN_DrawPanel(descriptionPos, descriptionSize, image, texX, texY, panelTemplate);
		MN_DrawPanel(bindingPos, bindingSize, image, texX, texY, panelTemplate);
	}

	binding = MN_GetReferenceString(node, node->text);
	if (binding == NULL || binding[0] == '\0')
		binding = _("NONE");

	/** @todo check that this is a keybinding value (with macro expansion) */
	command = node->text + 9;
	description = Cmd_GetCommandDesc(command);
	if (description[0] == '\0')
		description = command;

	R_Color(textColor);

	MN_DrawStringInBox(font, node->textalign,
		descriptionPos[0] + node->padding, descriptionPos[1] + node->padding,
		descriptionSize[0] - node->padding - node->padding, descriptionSize[1] - node->padding - node->padding,
		description, LONGLINES_PRETTYCHOP);

	MN_DrawStringInBox(font, node->textalign,
		bindingPos[0] + node->padding, bindingPos[1] + node->padding,
		bindingSize[0] - node->padding - node->padding, bindingSize[1] - node->padding - node->padding,
		binding, LONGLINES_PRETTYCHOP);

	R_Color(NULL);
}

/**
 * @brief Call before the script initialization of the node
 */
static void MN_KeyBindingNodeLoading (menuNode_t *node)
{
	node->padding = 8;
	node->textalign = ALIGN_CL;
	EXTRADATA(node).bindingWidth = 50;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 0.5);
}

static const value_t properties[] = {
	{"keyspace", V_INT, MN_EXTRADATA_OFFSETOF(keyBindingExtraData_t, keySpace), MEMBER_SIZEOF(keyBindingExtraData_t, keySpace)},
	{"bindingwidth", V_INT, MN_EXTRADATA_OFFSETOF(keyBindingExtraData_t, bindingWidth), MEMBER_SIZEOF(keyBindingExtraData_t, bindingWidth)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterKeyBindingNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "keybinding";
	behaviour->leftClick = MN_KeyBindingNodeClick;
	behaviour->keyPressed = MN_KeyBindingNodeKeyPressed;
	behaviour->draw = MN_KeyBindingNodeDraw;
	behaviour->loading = MN_KeyBindingNodeLoading;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(keyBindingExtraData_t);
}
