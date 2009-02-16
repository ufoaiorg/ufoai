/**
 * @file m_node_textentry.c
 * @todo must we need to use command to interact with keyboard?
 * @todo allow to edit text without any cvar
 * @todo add a custom max size
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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_main.h"
#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "../m_actions.h"
#include "../../client.h"
#include "m_node_textentry.h"
#include "m_node_abstractnode.h"
#include "m_node_abstractnode.h"

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

static const char CURSOR = '|';		/**< Use as the cursor when we edit the text */
static const char HIDECHAR = '*';	/**< use as a mask for password */

/* global data */
static char cmdChanged[MAX_VAR];
static char cmdAborted[MAX_VAR];
static qboolean gotFocus = qfalse;

/**
 * @brief fire the change event
 */
static inline void MN_TextEntryNodeFireChange (menuNode_t *node)
{
	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief fire the abort event
 */
static inline void MN_TextEntryNodeFireAbort (menuNode_t *node)
{
	/* fire change event */
	if (node->u.textentry.onAbort) {
		MN_ExecuteEventActions(node, node->u.textentry.onAbort);
	}
}

/**
 * @brief callback from the keyboard
 */
static void MN_TextEntryNodeKeyboardChanged_f (void)
{
	menuNode_t *node = (menuNode_t *) Cmd_Userdata();
	MN_TextEntryNodeFireChange(node);
	gotFocus = qfalse;
	if (MN_HasFocus(node)) {
		Cmd_RemoveCommand(cmdChanged);
		Cmd_RemoveCommand(cmdAborted);
		cmdChanged[0] = '\0';
		MN_RemoveFocus();
	}
}

/**
 * @brief callback from the keyboard
 */
static void MN_TextEntryNodeKeyboardAborted_f (void)
{
	menuNode_t *node = (menuNode_t *) Cmd_Userdata();
	MN_TextEntryNodeFireAbort(node);

	/* remove the focus to show changes */
	if (MN_HasFocus(node)) {
		Cmd_RemoveCommand(cmdChanged);
		Cmd_RemoveCommand(cmdAborted);
		cmdChanged[0] = '\0';
		MN_RemoveFocus();
	}
}

/**
 * @brief force edition of a textentry node
 * @note the textentry must be on the active menu
 */
static void MN_EditTextEntry_f (void)
{
	menuNode_t *node;
	const char* name;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <textentrynode>\n", Cmd_Argv(0));
		return;
	}

	name = Cmd_Argv(1);
	node = MN_GetNode(MN_GetActiveMenu(), name);
	if (!node) {
		Com_Printf("MN_EditTextEntry_f: node '%s' dont exists on the current active menu '%s'\n", name, MN_GetActiveMenu()->name);
		return;
	}

	/* remove the focus to show changes */
	if (!MN_HasFocus(node)) {
		MN_RequestFocus(node);
	}
}

/**
 * @brief Called when the user click with the right mouse button
 */
static void MN_TextEntryNodeClick (menuNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no cvar */
	if (!node->text)
		return;
	if (Q_strncmp(node->text, "*cvar", 5))
		return;

	if (!MN_HasFocus(node)) {
		if (node->onClick) {
			MN_ExecuteEventActions(node, node->onClick);
		}
		MN_RequestFocus(node);
	}
}

/**
 * @brief Called when the node got the focus
 */
static void MN_TextEntryGotFocus (menuNode_t *node)
{
	/* register keyboard callback */
	snprintf(cmdChanged, sizeof(cmdChanged), "%s_changed", &((char*)node->text)[6]);
	if (Cmd_Exists(cmdChanged)) {
		Sys_Error("MN_TextEntryNodeSetFocus: '%s' already used, code down yet allow context restitution. Plese clean up your script.\n", cmdChanged);
	}
	Cmd_AddCommand(cmdChanged, MN_TextEntryNodeKeyboardChanged_f, "Text entry callback");
	Cmd_AddUserdata(cmdChanged, node);
	snprintf(cmdAborted, sizeof(cmdAborted), "%s_aborted", &((char*)node->text)[6]);
	if (Cmd_Exists(cmdAborted)) {
		Sys_Error("MN_TextEntryNodeSetFocus: '%s' already used, code down yet allow context restitution. Plese clean up your script.\n", cmdAborted);
	}
	Cmd_AddCommand(cmdAborted, MN_TextEntryNodeKeyboardAborted_f, "Text entry callback");
	Cmd_AddUserdata(cmdAborted, node);

	/* start typing */
	Cbuf_AddText(va("mn_msgedit ?%s\n", &((char*)node->text)[6]));
}

/**
 * @brief Called when the node lost the focus
 */
static void MN_TextEntryLostFocus (menuNode_t *node)
{
	/* already aborted/changed with the keyboard */
	if (cmdChanged[0] == '\0') {
		return;
	}

	/* relese the keyboard */
	if (node->u.textentry.clickOutAbort) {
		Cmd_ExecuteString("mn_msgedit !\n");
	} else {
		Cmd_ExecuteString("mn_msgedit .\n");
	}
	Cmd_RemoveCommand(cmdChanged);
	Cmd_RemoveCommand(cmdAborted);
}

static void MN_TextEntryNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *text;
	const char *font;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
	} else if (MN_HasFocus(node)) {
		textColor = node->selectedColor;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image) {
		R_DrawPanel(pos, node->size, image, node->blend, texX, texY, panelTemplate);
	}

	text = MN_GetReferenceString(node, node->text);
	if (text != NULL) {
		/** @todo we dont need to edit the text to draw the cursor */
		if (MN_HasFocus(node)) {
			if (cl.time % 1000 < 500) {
				text = va("%s%c", text, CURSOR);
			}
		}

		if (node->u.textentry.isPassword) {
			char *c = va("%s", text);
			text = c;
			/* hide the text */
			/** @todo does it work with Unicode :/ dont we create to much char? */
			while (*c != '\0') {
				*c++ = HIDECHAR;
			}
			/* replace the cursor */
			if (MN_GetMouseCapture() == node) {
				if (cl.time % 1000 < 500) {
					*--c = CURSOR;
				}
			}
		}

		if (*text != '\0') {
			font = MN_GetFont(node);
			R_ColorBlend(textColor);
			R_FontDrawStringInBox(font, node->textalign,
				pos[0] + node->padding, pos[1] + node->padding,
				node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
				text, LONGLINES_PRETTYCHOP);
			R_ColorBlend(NULL);
		}
	}

}

/**
 * @brief Call before the script initialisation of the node
 */
static void MN_TextEntryNodeLoading (menuNode_t *node)
{
	node->padding = 8;
	node->textalign = ALIGN_CL;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
}

static const value_t properties[] = {
	{"ispassword", V_BOOL, offsetof(menuNode_t, u.textentry.isPassword), MEMBER_SIZEOF(menuNode_t, u.textentry.isPassword) },
	{"clickoutabort", V_BOOL, offsetof(menuNode_t, u.textentry.clickOutAbort), MEMBER_SIZEOF(menuNode_t, u.textentry.clickOutAbort)},
	{"abort", V_SPECIAL_ACTION, offsetof(menuNode_t, u.textentry.onAbort), MEMBER_SIZEOF(menuNode_t, u.textentry.onAbort)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterTextEntryNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "textentry";
	behaviour->leftClick = MN_TextEntryNodeClick;
	behaviour->gotFocus = MN_TextEntryGotFocus;
	behaviour->lostFocus = MN_TextEntryLostFocus;
	behaviour->draw = MN_TextEntryNodeDraw;
	behaviour->loading = MN_TextEntryNodeLoading;
	behaviour->properties = properties;

	Cmd_AddCommand("mn_edittextentry", MN_EditTextEntry_f, "Force edition of the textentry.");
}
