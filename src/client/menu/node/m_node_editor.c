/**
 * @file m_node_editor.c
 * @note type "mn_push editor" to use it, Escape button to close it, and "mn_extract" to extract a menu
 * @brief Editor is an invisible node used to create an edition mode. The edition mode
 * allow user to move and resize all visible nodes.
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
#include "../m_parse.h"
#include "../m_input.h"
#include "../m_nodes.h"
#include "../m_menus.h"
#include "../m_render.h"
#include "m_node_editor.h"
#include "m_node_abstractnode.h"

#include "../../input/cl_keys.h"
#include "../../input/cl_input.h"
#include "../../renderer/r_draw.h"

static menuNode_t* anchoredNode = NULL;
static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
static const vec4_t grey = {0.8, 0.8, 0.8, 1.0};
static const int anchorSize = 10;
static int status = -1;
static int startX;
static int startY;

static void MN_EditorNodeHighlightNode (menuNode_t *node, const vec4_t color, qboolean displayAnchor)
{
	vec2_t pos;
	MN_GetNodeAbsPos(node, pos);

	R_DrawRect(pos[0] - 1, pos[1] - 1, node->size[0] + 2, node->size[1] + 2, color, 1.0, 0x3333);

	if (displayAnchor) {
		MN_DrawFill(pos[0] - anchorSize, pos[1] - anchorSize, anchorSize, anchorSize, color);
		MN_DrawFill(pos[0] - anchorSize, pos[1] + node->size[1], anchorSize, anchorSize, color);
		MN_DrawFill(pos[0] + node->size[0], pos[1] + node->size[1], anchorSize, anchorSize, color);
		MN_DrawFill(pos[0] + node->size[0], pos[1] - anchorSize, anchorSize, anchorSize, color);
	}
}

static int MN_EditorNodeGetElementAtPosition (menuNode_t *node, int x, int y)
{
	MN_NodeAbsoluteToRelativePos(anchoredNode, &x, &y);

	if (x > 0 && x < node->size[0] && y > 0 && y < node->size[1]) {
		return 4;
	}

	if (y > -anchorSize && y < 0) {
		if (x > -anchorSize && x < 0) {
			return 0;
		} else if (x > node->size[0] && x < node->size[0] + anchorSize) {
			return 1;
		}
	} else if (y > node->size[1] && y < node->size[1] + anchorSize) {
		if (x > -anchorSize && x < 0) {
			return 2;
		} else if (x > node->size[0] && x < node->size[0] + anchorSize) {
			return 3;
		}
	}
	return -1;
}

static void MN_EditorNodeDraw (menuNode_t *node)
{
	menuNode_t* hovered = NULL;

	if (MN_GetMouseCapture() != node) {
		if (anchoredNode)
			MN_EditorNodeHighlightNode(anchoredNode, red, qfalse);
		return;
	}

	if (status == -1) {
#if 0	/* it does nothing, remember why we need that... */
		if (anchoredNode)
			MN_EditorNodeGetElementAtPosition(anchoredNode, mousePosX, mousePosY);
#endif
		hovered = MN_GetNodeAtPosition(mousePosX, mousePosY);
		/* do not edit node from editor window */
		if (hovered && hovered->root == node->root)
			hovered = NULL;
	}

	if (hovered && hovered != anchoredNode)
		MN_EditorNodeHighlightNode(hovered, grey, qtrue);

	if (anchoredNode)
		MN_EditorNodeHighlightNode(anchoredNode, red, qtrue);
}

static void MN_EditorNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	vec2_t size;
	const int diffX = x - startX;
	const int diffY = y - startY;
	startX = x;
	startY = y;

	if (anchoredNode == NULL)
		return;

	switch (status) {
	case -1:
		return;
	case 0:
		anchoredNode->pos[0] += diffX;
		anchoredNode->pos[1] += diffY;
		size[0] = anchoredNode->size[0] - diffX;
		size[1] = anchoredNode->size[1] - diffY;
		break;
	case 1:
		anchoredNode->pos[1] += y - startY;
		size[0] = anchoredNode->size[0] + diffX;
		size[1] = anchoredNode->size[1] - diffY;
		break;
	case 2:
		anchoredNode->pos[0] += x - startX;
		size[0] = anchoredNode->size[0] - diffX;
		size[1] = anchoredNode->size[1] + diffY;
		break;
	case 3:
		size[0] = anchoredNode->size[0] + diffX;
		size[1] = anchoredNode->size[1] + diffY;
		break;
	case 4:
		anchoredNode->pos[0] += diffX;
		anchoredNode->pos[1] += diffY;
		size[0] = anchoredNode->size[0];
		size[1] = anchoredNode->size[1];
		break;
	default:
		Sys_Error("MN_EditorNodeCapturedMouseMove: Invalid status of %i", status);
	}

	if (size[0] < 5)
		size[0] = 5;
	if (size[1] < 5)
		size[1] = 5;

	MN_NodeSetSize(anchoredNode, size);
}

/**
 * @brief Called when the node have lost the captured node
 */
static void MN_EditorNodeCapturedMouseLost (menuNode_t *node)
{
	status = -1;
}

static void MN_EditorNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (MN_GetMouseCapture() != node)
		return;
	if (button != K_MOUSE1)
		return;
	status = -1;
}

static void MN_EditorNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	menuNode_t* hovered;

	if (MN_GetMouseCapture() != node)
		return;
	if (button != K_MOUSE1)
		return;

	hovered = MN_GetNodeAtPosition(mousePosX, mousePosY);

	/* stop the capture */
	if (hovered && hovered->root == node->root) {
		MN_MouseRelease();
		return;
	}

	if (hovered == anchoredNode)
		hovered = NULL;

	if (anchoredNode) {
		status = MN_EditorNodeGetElementAtPosition(anchoredNode, x, y);
		if (status == 4) {
			if (hovered == NULL) {
				startX = x;
				startY = y;
				return;
			}
		} else if (status != -1) {
			startX = x;
			startY = y;
			return;
		}
	}

	/* select the node */
	if (hovered && hovered->root != node->root) {
		anchoredNode = hovered;
		Cvar_Set("mn_sys_editor_node", anchoredNode->name);
		Cvar_Set("mn_sys_editor_window", anchoredNode->root->name);
	}
}

static void MN_EditorNodeStart (menuNode_t *node)
{
	MN_SetMouseCapture(node);
}

static void MN_EditorNodeStop (menuNode_t *node)
{
	MN_MouseRelease();
}

static void MN_EditorNodeExtractNode (qFILE *file, menuNode_t *node, int depth)
{
	int i;
	char tab[16];
	menuNode_t *child;
	assert(depth < 16);

	for (i = 0; i < depth; i++) {
		tab[i] = '\t';
	}
	tab[i] = '\0';

	FS_Printf(file, "%s%s %s {\n", tab, node->behaviour->name, node->name);
	child = node->firstChild;

	/* properties */
	if (child) {
		FS_Printf(file, "%s\t{\n", tab);
		FS_Printf(file, "%s\t\tpos\t\"%d %d\"\n", tab, (int)node->pos[0], (int)node->pos[1]);
		FS_Printf(file, "%s\t\tsize\t\"%d %d\"\n", tab, (int)node->size[0], (int)node->size[1]);
		FS_Printf(file, "%s\t}\n", tab);
	} else {
		FS_Printf(file, "%s\tpos\t\"%d %d\"\n", tab, (int)node->pos[0], (int)node->pos[1]);
		FS_Printf(file, "%s\tsize\t\"%d %d\"\n", tab, (int)node->size[0], (int)node->size[1]);
	}

	/* child */
	while (child) {
		MN_EditorNodeExtractNode(file, child, depth + 1);
		child = child->next;
	}

	FS_Printf(file, "%s}\n", tab);
}

/**
 * @note not moved into V_UI_NODEMETHOD because it is more a generic
 * tool than a method of the node editor
 */
static void MN_EditorNodeExtract_f (void)
{
	menuNode_t* menu;
	qFILE file;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <menuname>\n", Cmd_Argv(0));
		return;
	}
	menu = MN_GetMenu(Cmd_Argv(1));
	if (!menu) {
		Com_Printf("Menu '%s' not found\n", Cmd_Argv(1));
		return;
	}

	FS_OpenFile(va("menu_%s_extracted.ufo", menu->name), &file, FILE_WRITE);
	MN_EditorNodeExtractNode(&file, menu, 0);
	FS_CloseFile(&file);

	Com_Printf("Menu '%s' extracted.\n", Cmd_Argv(1));
}

static const value_t properties[] = {
	/* start edition mode */
	{"start", V_UI_NODEMETHOD, ((size_t) MN_EditorNodeStart), 0},
	/* stop edition mode */
	{"stop", V_UI_NODEMETHOD, ((size_t) MN_EditorNodeStop), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterEditorNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "editor";
	behaviour->extends = "special";
	behaviour->draw = MN_EditorNodeDraw;
	behaviour->mouseDown = MN_EditorNodeMouseDown;
	behaviour->mouseUp = MN_EditorNodeMouseUp;
	behaviour->capturedMouseMove = MN_EditorNodeCapturedMouseMove;
	behaviour->capturedMouseLost = MN_EditorNodeCapturedMouseLost;
	behaviour->properties = properties;

	Cmd_AddCommand("mn_extract", MN_EditorNodeExtract_f, "Extract position and size of nodes into a file");
	Cmd_AddParamCompleteFunction("mn_extract", MN_CompleteWithMenu);
}
