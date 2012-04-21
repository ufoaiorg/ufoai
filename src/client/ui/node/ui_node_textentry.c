/**
 * @file ui_node_textentry.c
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
#include "ui_node_textentry.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"

#include "../../client.h"
#include "../../../shared/utf8.h"

#define EXTRADATA_TYPE textEntryExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

static const char CURSOR = '|';		/**< Use as the cursor when we edit the text */
static const char HIDECHAR = '*';	/**< use as a mask for password */

/* limit the input for cvar editing (base name, save slots and so on) */
#define MAX_CVAR_EDITING_LENGTH 256 /* MAXCMDLINE */

/* global data */
static char cvarValueBackup[MAX_CVAR_EDITING_LENGTH];
static cvar_t *editedCvar = NULL;
static qboolean isAborted = qfalse;

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
static void UI_TextEntryNodeClick (uiNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no cvar */
	if (!node->text)
		return;
	if (!Q_strstart(node->text, "*cvar"))
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
static void UI_TextEntryFocusGained (uiNode_t *node)
{
	assert(editedCvar == NULL);
	/* skip '*cvar ' */
	editedCvar = Cvar_Get(&((char*)node->text)[6], "", 0, NULL);
	assert(editedCvar);
	Q_strncpyz(cvarValueBackup, editedCvar->string, sizeof(cvarValueBackup));
	isAborted = qfalse;
}

/**
 * @brief Called when the node lost the focus
 */
static void UI_TextEntryFocusLost (uiNode_t *node)
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
static qboolean UI_TextEntryNodeKeyPressed (uiNode_t *node, unsigned int key, unsigned short unicode)
{
	switch (key) {
	/* remove the last char */
	case K_BACKSPACE:
		UI_TextEntryNodeEdit(node, K_BACKSPACE);
		return qtrue;
	/* cancel the edition */
	case K_ESCAPE:
		isAborted = qtrue;
		UI_RemoveFocus();
		return qtrue;
	/* validate the edition */
	case K_ENTER:
	case K_KP_ENTER:
		UI_TextEntryNodeValidateEdition(node);
		UI_RemoveFocus();
		return qtrue;
	}

	/* non printable */
	if (unicode < 32 || (unicode >= 127 && unicode < 192))
		return qfalse;

	/* add a char */
	UI_TextEntryNodeEdit(node, unicode);
	return qtrue;
}

static void UI_TextEntryNodeDraw (uiNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *text;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	const char *font = UI_GetFontFromNode(node);

	if (node->disabled) {
		/** @todo need custom color when node is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
	} else if (node->state) {
		textColor = node->color;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}
	if (UI_HasFocus(node))
		textColor = node->selectedColor;

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image)
		UI_DrawPanel(pos, node->size, image, texX, texY, panelTemplate);

	text = UI_GetReferenceString(node, node->text);
	if (text != NULL) {
		/** @todo we don't need to edit the text to draw the cursor */
		if (UI_HasFocus(node)) {
			if (CL_Milliseconds() % 1000 < 500) {
				text = va("%s%c", text, CURSOR);
			}
		}

		if (EXTRADATA(node).isPassword) {
			char *c = va("%s", text);
			int size = UTF8_strlen(c);
			text = c;
			/* hide the text with a special char */
			assert(strlen(c) >= size);	/* trustable, but it can't be false */
			while (size) {
				*c++ = HIDECHAR;
				size--;
			}
			/* readd the cursor */
			if (UI_HasFocus(node)) {
				if (CL_Milliseconds() % 1000 < 500) {
					c--;
					*c++ = CURSOR;
				}
			}
			*c = '\0';
		}

		if (*text != '\0') {
			R_Color(textColor);
			UI_DrawStringInBox(font, node->contentAlign,
				pos[0] + node->padding, pos[1] + node->padding,
				node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
				text, LONGLINES_PRETTYCHOP);
			R_Color(NULL);
		}
	}

}

/**
 * @brief Call before the script initialization of the node
 */
static void UI_TextEntryNodeLoading (uiNode_t *node)
{
	node->padding = 8;
	node->contentAlign = ALIGN_CL;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
}

void UI_RegisterTextEntryNode (struct uiBehaviour_s *behaviour)
{
	behaviour->name = "textentry";
	behaviour->leftClick = UI_TextEntryNodeClick;
	behaviour->focusGained = UI_TextEntryFocusGained;
	behaviour->focusLost = UI_TextEntryFocusLost;
	behaviour->keyPressed = UI_TextEntryNodeKeyPressed;
	behaviour->draw = UI_TextEntryNodeDraw;
	behaviour->loading = UI_TextEntryNodeLoading;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Texture used by the button. It's a normalized texture of 128x128.
	 * Normal button start at 0x0, mouse over start at 64x0, mouse click
	 * start at 0x64 (but not yet implemented), and disabled start at 64x64.
	 * See the image to have a usable template for this node.
	 * @image html http://ufoai.org/wiki/images/Button_blue.png
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "image");

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
}
