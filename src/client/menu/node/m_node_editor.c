/**
 * @file m_node_editor.c
 * @note type "mn_push editor" to use it, Escape button to close it, and "mn_extract" to extract a menu
 * @brief Editor is an invisible node used to create an edition mode. The edition mode
 * allow user to move and resize all visible nodes.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../m_draw.h"
#include "../m_input.h"
#include "../m_nodes.h"
#include "../m_windows.h"
#include "../m_render.h"
#include "../m_actions.h"
#include "m_node_editor.h"
#include "m_node_abstractnode.h"

#include "../../input/cl_keys.h"
#include "../../input/cl_input.h"
#include "../../renderer/r_draw.h"

typedef enum {
	ZONE_NONE = -1,
	ZONE_TOPRIGHT_CORNER,
	ZONE_TOPLEFT_CORNER,
	ZONE_BOTTOMRIGHT_CORNER,
	ZONE_BOTTOMLEFT_CORNER,
	ZONE_BODY
} zoneNode_t;


static uiNode_t* anchoredNode = NULL;
static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
static const vec4_t grey = {0.8, 0.8, 0.8, 1.0};
static const int anchorSize = 10;
static zoneNode_t dragStatus = ZONE_NONE;
static int startX;
static int startY;

static void MN_EditorNodeHighlightNode (uiNode_t *node, const vec4_t color, qboolean displayAnchor)
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

static zoneNode_t MN_EditorNodeGetElementAtPosition (uiNode_t *node, int x, int y)
{
	MN_NodeAbsoluteToRelativePos(anchoredNode, &x, &y);

	if (x > 0 && x < node->size[0] && y > 0 && y < node->size[1]) {
		return ZONE_BODY;
	}

	if (y > -anchorSize && y < 0) {
		if (x > -anchorSize && x < 0) {
			return ZONE_TOPLEFT_CORNER;
		} else if (x > node->size[0] && x < node->size[0] + anchorSize) {
			return ZONE_TOPRIGHT_CORNER;
		}
	} else if (y > node->size[1] && y < node->size[1] + anchorSize) {
		if (x > -anchorSize && x < 0) {
			return ZONE_BOTTOMLEFT_CORNER;
		} else if (x > node->size[0] && x < node->size[0] + anchorSize) {
			return ZONE_BOTTOMRIGHT_CORNER;
		}
	}
	return ZONE_NONE;
}

static void MN_EditorNodeDraw (uiNode_t *node)
{
	MN_CaptureDrawOver(node);
}

static void MN_EditorNodeDrawOverMenu (uiNode_t *node)
{
	uiNode_t* hovered = NULL;

	if (MN_GetMouseCapture() != node) {
		if (anchoredNode)
			MN_EditorNodeHighlightNode(anchoredNode, red, qfalse);
		return;
	}

	if (dragStatus == ZONE_NONE) {
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

static void MN_EditorNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	vec2_t size;
	const int diffX = x - startX;
	const int diffY = y - startY;
	startX = x;
	startY = y;

	if (anchoredNode == NULL)
		return;

	switch (dragStatus) {
	case ZONE_NONE:
		return;
	case ZONE_TOPLEFT_CORNER:
		anchoredNode->pos[0] += diffX;
		anchoredNode->pos[1] += diffY;
		size[0] = anchoredNode->size[0] - diffX;
		size[1] = anchoredNode->size[1] - diffY;
		break;
	case ZONE_TOPRIGHT_CORNER:
		anchoredNode->pos[1] += diffY;
		size[0] = anchoredNode->size[0] + diffX;
		size[1] = anchoredNode->size[1] - diffY;
		break;
	case ZONE_BOTTOMLEFT_CORNER:
		anchoredNode->pos[0] += diffX;
		size[0] = anchoredNode->size[0] - diffX;
		size[1] = anchoredNode->size[1] + diffY;
		break;
	case ZONE_BOTTOMRIGHT_CORNER:
		size[0] = anchoredNode->size[0] + diffX;
		size[1] = anchoredNode->size[1] + diffY;
		break;
	case ZONE_BODY:
		anchoredNode->pos[0] += diffX;
		anchoredNode->pos[1] += diffY;
		size[0] = anchoredNode->size[0];
		size[1] = anchoredNode->size[1];
		break;
	default:
		Sys_Error("MN_EditorNodeCapturedMouseMove: Invalid status of %i", dragStatus);
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
static void MN_EditorNodeCapturedMouseLost (uiNode_t *node)
{
	dragStatus = ZONE_NONE;
}

static void MN_EditorNodeMouseUp (uiNode_t *node, int x, int y, int button)
{
	if (MN_GetMouseCapture() != node)
		return;
	if (button != K_MOUSE1)
		return;
	dragStatus = ZONE_NONE;
}

static void MN_EditorNodeSelectNode (uiNode_t *node, uiNode_t *selected)
{
	if (selected == NULL)
		return;
	/* do not select a node from the editor window */
	if (selected->root == node->root)
		return;
	anchoredNode = selected;
	Cvar_Set("mn_sys_editor_node", anchoredNode->name);
	Cvar_Set("mn_sys_editor_window", anchoredNode->root->name);
}

static void MN_EditorNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	uiNode_t* hovered;

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
		dragStatus = MN_EditorNodeGetElementAtPosition(anchoredNode, x, y);
		if (dragStatus == ZONE_BODY) {
			if (hovered == NULL) {
				startX = x;
				startY = y;
				return;
			}
		} else if (dragStatus != ZONE_NONE) {
			startX = x;
			startY = y;
			return;
		}
	}

	/* select the node */
	MN_EditorNodeSelectNode(node, hovered);
}

static void MN_EditorNodeStart (uiNode_t *node, const uiCallContext_t *context)
{
	MN_SetMouseCapture(node);
}

static void MN_EditorNodeStop (uiNode_t *node, const uiCallContext_t *context)
{
	MN_MouseRelease();
}

static void MN_EditorNodeSelectNext (uiNode_t *node, const uiCallContext_t *context)
{
	if (dragStatus != ZONE_NONE)
		return;
	if (anchoredNode == NULL)
		return;
	MN_EditorNodeSelectNode(node, anchoredNode->next);
}

static void MN_EditorNodeSelectParent (uiNode_t *node, const uiCallContext_t *context)
{
	if (dragStatus != ZONE_NONE)
		return;
	if (anchoredNode == NULL)
		return;
	MN_EditorNodeSelectNode(node, anchoredNode->parent);
}

static void MN_EditorNodeSelectFirstChild (uiNode_t *node, const uiCallContext_t *context)
{
	if (dragStatus != ZONE_NONE)
		return;
	if (anchoredNode == NULL)
		return;
	MN_EditorNodeSelectNode(node, anchoredNode->firstChild);
}

static void MN_EditorNodeExtractNode (qFILE *file, uiNode_t *node, int depth)
{
	int i;
	char tab[16];
	uiNode_t *child;
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
	uiNode_t* menu;
	qFILE file;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <menuname>\n", Cmd_Argv(0));
		return;
	}
	menu = MN_GetWindow(Cmd_Argv(1));
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
	/* select the next node (according to the current one) */
	{"selectnext", V_UI_NODEMETHOD, ((size_t) MN_EditorNodeSelectNext), 0},
	/* select the parent node (according to the current one) */
	{"selectparent", V_UI_NODEMETHOD, ((size_t) MN_EditorNodeSelectParent), 0},
	/* select first child node (according to the current one) */
	{"selectfirstchild", V_UI_NODEMETHOD, ((size_t) MN_EditorNodeSelectFirstChild), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterEditorNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "editor";
	behaviour->extends = "special";
	behaviour->draw = MN_EditorNodeDraw;
	behaviour->drawOverMenu = MN_EditorNodeDrawOverMenu;
	behaviour->mouseDown = MN_EditorNodeMouseDown;
	behaviour->mouseUp = MN_EditorNodeMouseUp;
	behaviour->capturedMouseMove = MN_EditorNodeCapturedMouseMove;
	behaviour->capturedMouseLost = MN_EditorNodeCapturedMouseLost;
	behaviour->properties = properties;

	Cmd_AddCommand("mn_extract", MN_EditorNodeExtract_f, "Extract position and size of nodes into a file");
	Cmd_AddParamCompleteFunction("mn_extract", MN_CompleteWithWindow);
}
