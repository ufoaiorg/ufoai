/**
 * @file
 * @brief This node allow to edit a cvar text with the keyboard. When we
 * click on the node, we active the edition, we can validate it with the ''RETURN'' key,
 * or abort it with ''ESCAPE'' key. A validation fire a scriptable callback event.
 * We can custom the mouse behaviour when we click outside the node in edition mode.
 * It can validate or abort the edition.
 * @todo allow to edit text without any cvar
 * @todo add a custom max size
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
#include "../ui_nodes.h"
#include "../ui_font.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_input.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "../ui_sprite.h"
#include "ui_node_textentry.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"

#include "../../client.h"
#include "../../../shared/utf8.h"

#define EXTRADATA_TYPE textEntryExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

static const char CURSOR = '|';		/**< Use as the cursor when we edit the text */
static const char HIDECHAR = '*';	/**< use as a mask for password */

/* limit the input for cvar editing (base name, save slots and so on) */
#define MAX_CVAR_EDITING_LENGTH 256 /* MAXCMDLINE */

/* global data */
static char cvarValueBackup[MAX_CVAR_EDITING_LENGTH];
static cvar_t *editedCvar = NULL;
static bool isAborted = false;

/**
 * @brief callback from the keyboard
 */
static void UI_TextEntryNodeValidateEdition (uiNode_t *node)
{
	/* invalidate cache */
	editedCvar = NULL;
	cvarValueBackup[0] = '\0';

	/* fire change event */
	if (node->onChange) {
		UI_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief callback from the keyboard
 */
static void UI_TextEntryNodeAbortEdition (uiNode_t *node)
{
	assert(editedCvar);

	/* set the old cvar value */
	Cvar_ForceSet(editedCvar->name, cvarValueBackup);

	/* invalidate cache */
	editedCvar = NULL;
	cvarValueBackup[0] = '\0';

	/* fire abort event */
	if (EXTRADATA(node).onAbort) {
		UI_ExecuteEventActions(node, EXTRADATA(node).onAbort);
	}
}

/**
 * @brief force edition of a textentry node
 * @note the textentry must be on the active window
 */
static void UI_TextEntryNodeFocus (uiNode_t *node, const uiCallContext_t *context)
{
	/* remove the focus to show changes */
	if (!UI_HasFocus(node)) {
		UI_RequestFocus(node);
	}
}

/**
 * @brief Called when the user click with the right mouse button
 */
void uiTextEntryNode::onLeftClick (uiNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no cvar */
	if (!node->text)
		return;
	if (!Q_strstart(node->text, "*cvar:"))
		return;

	if (!UI_HasFocus(node)) {
		if (node->onClick) {
			UI_ExecuteEventActions(node, node->onClick);
		}
		UI_RequestFocus(node);
	}
}

/**
 * @brief Called when the node got the focus
 */
void uiTextEntryNode::onFocusGained (uiNode_t *node)
{
	assert(editedCvar == NULL);
	/* skip '*cvar ' */
	editedCvar = Cvar_Get(&((char*)node->text)[6]);
	assert(editedCvar);
	Q_strncpyz(cvarValueBackup, editedCvar->string, sizeof(cvarValueBackup));
	isAborted = false;
}

/**
 * @brief Called when the node lost the focus
 */
void uiTextEntryNode::onFocusLost (uiNode_t *node)
{
	/* already aborted/changed with the keyboard */
	if (editedCvar == NULL)
		return;

	/* release the keyboard */
	if (isAborted || EXTRADATA(node).clickOutAbort) {
		UI_TextEntryNodeAbortEdition(node);
	} else {
		UI_TextEntryNodeValidateEdition(node);
	}
}

/**
 * @brief edit the current cvar with a char
 */
static void UI_TextEntryNodeEdit (uiNode_t *node, unsigned int key)
{
	char buffer[MAX_CVAR_EDITING_LENGTH];
	int length;

	/* copy the cvar */
	Q_strncpyz(buffer, editedCvar->string, sizeof(buffer));
	length = strlen(buffer);

	/* compute result */
	if (key == K_BACKSPACE) {
		length = UTF8_delete_char(buffer, length - 1);
	} else {
		int charLength = UTF8_encoded_len(key);
		/* is buffer full? */
		if (length + charLength >= sizeof(buffer))
			return;

		length += UTF8_insert_char(buffer, sizeof(buffer), length, key);
	}

	/* update the cvar */
	Cvar_ForceSet(editedCvar->name, buffer);
}

/**
 * @brief Called when we press a key when the node got the focus
 * @return True, if we use the event
 */
bool uiTextEntryNode::onKeyPressed (uiNode_t *node, unsigned int key, unsigned short unicode)
{
	switch (key) {
	/* remove the last char */
	case K_BACKSPACE:
		UI_TextEntryNodeEdit(node, K_BACKSPACE);
		return true;
	/* cancel the edition */
	case K_ESCAPE:
		isAborted = true;
		UI_RemoveFocus();
		return true;
	/* validate the edition */
	case K_ENTER:
	case K_KP_ENTER:
		UI_TextEntryNodeValidateEdition(node);
		UI_RemoveFocus();
		return true;
	}

	/* non printable */
	if (unicode < 32 || (unicode >= 127 && unicode < 192))
		return false;

	/* add a char */
	UI_TextEntryNodeEdit(node, unicode);
	return true;
}

void uiTextEntryNode::draw (uiNode_t *node)
{
	const float *textColor;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	const char *font = UI_GetFontFromNode(node);
	uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;

	if (node->disabled) {
		/** @todo need custom color when node is disabled */
		textColor = disabledColor;
		iconStatus = SPRITE_STATUS_DISABLED;
	} else if (node->state) {
		textColor = node->color;
		iconStatus = SPRITE_STATUS_HOVER;
	} else {
		textColor = node->color;
	}
	if (UI_HasFocus(node)) {
		textColor = node->selectedColor;
	}

	UI_GetNodeAbsPos(node, pos);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, iconStatus, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	}

	if (char const* const text = UI_GetReferenceString(node, node->text)) {
		char  buf[MAX_VAR];
		char* c = buf;
		if (EXTRADATA(node).isPassword) {
			size_t const size = UTF8_strlen(text);
			memset(buf, HIDECHAR, size);
			c += size;
		} else {
			size_t const size = strlen(text);
			memcpy(buf, text, size);
			c += size;
		}

		if (UI_HasFocus(node) && CL_Milliseconds() % 1000 < 500) {
			*c++ = CURSOR;
		}

		*c = '\0';

		if (*buf != '\0') {
			R_Color(textColor);
			UI_DrawStringInBox(font, (align_t)node->contentAlign,
				pos[0] + node->padding, pos[1] + node->padding,
				node->box.size[0] - node->padding - node->padding, node->box.size[1] - node->padding - node->padding,
				buf);
			R_Color(NULL);
		}
	}

}

/**
 * @brief Call before the script initialization of the node
 */
void uiTextEntryNode::onLoading (uiNode_t *node)
{
	node->padding = 8;
	node->contentAlign = ALIGN_CL;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
}

void UI_RegisterTextEntryNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "textentry";
	behaviour->manager = UINodePtr(new uiTextEntryNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Call back event called when we click on the node. If the click select the node,
	 * it called before we start the cvar edition.
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "onClick");

	/* Call back event (like click...) fired when the text is changed, after
	 * validation. An abort of the edition dont fire this event.
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "onChange");

	/* Custom the draw behaviour by hiding each character of the text with a star (''*''). */
	UI_RegisterExtradataNodeProperty(behaviour, "isPassword", V_BOOL, textEntryExtraData_t, isPassword);
	/* ustom the mouse event behaviour. When we are editing the text, if we click out of the node, the edition is aborted. Changes on
	 * the text are canceled, and no change event are fired.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "clickOutAbort", V_BOOL, textEntryExtraData_t, clickOutAbort);
	/* Call it when we abort the edition */
	UI_RegisterExtradataNodeProperty(behaviour, "onAbort", V_UI_ACTION, textEntryExtraData_t, onAbort);
	/* Call it to force node edition */
	UI_RegisterNodeMethod(behaviour, "edit", UI_TextEntryNodeFocus);
	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
