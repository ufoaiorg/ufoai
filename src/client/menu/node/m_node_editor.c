/**
 * @file m_node_editor.c
 * @note type "mn_push editor" to use it, Escape button to close it, and "debug_mneditionextract" to extract a menu
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
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "../m_nodes.h"
#include "m_node_editor.h"
#include "m_node_abstractnode.h"

static menuNode_t* anchoredNode = NULL;
static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
static const vec4_t grey = {0.8, 0.8, 0.8, 1.0};
static const int anchorSize = 10;
static int status = -1;
static int startX;
static int startY;

static void MN_EditorNodeHilightNode (menuNode_t *node, const vec4_t color)
{
	vec2_t pos;
	MN_GetNodeAbsPos(node, pos);

	R_ColorBlend(color);
	R_FontDrawString("f_small_bold", ALIGN_UL, 20, 50, 20, 50, 400, 400, 0, va("%s (%s)", node->name, node->behaviour->name), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	R_ColorBlend(NULL);

	R_DrawFill(pos[0] - anchorSize, pos[1] - anchorSize, anchorSize, anchorSize, ALIGN_UL, color);
	R_DrawFill(pos[0] - anchorSize, pos[1] + node->size[1], anchorSize, anchorSize, ALIGN_UL, color);
	R_DrawFill(pos[0] + node->size[0], pos[1] + node->size[1], anchorSize, anchorSize, ALIGN_UL, color);
	R_DrawFill(pos[0] + node->size[0], pos[1] - anchorSize, anchorSize, anchorSize, ALIGN_UL, color);
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

	if (status == -1) {
		int s = -1;
		if (anchoredNode)
			MN_EditorNodeGetElementAtPosition(anchoredNode, mousePosX, mousePosY);
		if (s == -1 || s == 4)
			hovered = MN_GetNodeByPosition(mousePosX, mousePosY);
	}

	if (hovered && hovered != anchoredNode)
		MN_EditorNodeHilightNode(hovered, grey);

	if (anchoredNode)
		MN_EditorNodeHilightNode(anchoredNode, red);
}

static void MN_EditorNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	if (anchoredNode == NULL)
		return;

	switch (status) {
	case -1:
		return;
	case 0:
		anchoredNode->pos[0] += x - startX;
		anchoredNode->size[0] -= x - startX;
		anchoredNode->pos[1] += y - startY;
		anchoredNode->size[1] -= y - startY;
		startX = x;
		startY = y;
		break;
	case 1:
		anchoredNode->size[0] += x - startX;
		anchoredNode->pos[1] += y - startY;
		anchoredNode->size[1] -= y - startY;
		startX = x;
		startY = y;
		break;
	case 2:
		anchoredNode->pos[0] += x - startX;
		anchoredNode->size[0] -= x - startX;
		anchoredNode->size[1] += y - startY;
		startX = x;
		startY = y;
		break;
	case 3:
		anchoredNode->size[0] += x - startX;
		anchoredNode->size[1] += y - startY;
		startX = x;
		startY = y;
		break;
	case 4:
		anchoredNode->pos[0] += x - startX;
		anchoredNode->pos[1] += y - startY;
		startX = x;
		startY = y;
		break;
	default:
		assert(qfalse);
	}

	if (anchoredNode->size[0] < 5)
		anchoredNode->size[0] = 5;
	if (anchoredNode->size[1] < 5)
		anchoredNode->size[1] = 5;
}

static void MN_EditorNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	status = -1;
}

static void MN_EditorNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	menuNode_t* hovered;

	hovered = MN_GetNodeByPosition(mousePosX, mousePosY);
	if (hovered == anchoredNode)
		hovered = NULL;

	if (button != K_MOUSE1)
		return;
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

	if (hovered && hovered->menu != node->menu)
		anchoredNode = hovered;
}

#ifdef DEBUG

static void MN_EditorNodeStart_f (void)
{
	menuNode_t* node;
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nodepath>\n", Cmd_Argv(0));
		return;
	}
	node = MN_GetNodeByPath(Cmd_Argv(1));
	assert(node);
	MN_SetMouseCapture(node);
}

static void MN_EditorNodeStop_f (void)
{
	menuNode_t* node;
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nodepath>\n", Cmd_Argv(0));
		return;
	}
	node = MN_GetNodeByPath(Cmd_Argv(1));
	/* assert(MN_GetMouseCapture() == node);*/
	MN_MouseRelease();
}

static void MN_EditorNodeExtract_f (void)
{
	menu_t* menu;
	menuNode_t* node;
	FILE* fd;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <menuname>\n", Cmd_Argv(0));
		return;
	}
	menu = MN_GetMenu(Cmd_Argv(1));
	if (!menu) {
		Com_Printf("Menu '%s' not found\n", Cmd_Argv(1));
		return;
	}

	fd = fopen(va("menu_%s_extracted.ufo", menu->name), "wt");
	fprintf(fd, "menu %s {\n", menu->name);
	for (node = menu->firstChild; node; node = node->next) {
		fprintf(fd, "\t%s %s {\n", node->behaviour->name, node->name);
		fprintf(fd, "\t\tpos \"%d %d\"\n", (int)node->pos[0], (int)node->pos[1]);
		fprintf(fd, "\t\tsize \"%d %d\"\n", (int)node->size[0], (int)node->size[1]);
		fprintf(fd, "\t}\n");
	}
	fprintf(fd, "}\n");
	fclose(fd);
}

#endif

void MN_RegisterEditorNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "editor";
	behaviour->draw = MN_EditorNodeDraw;
	behaviour->mouseDown = MN_EditorNodeMouseDown;
	behaviour->mouseUp = MN_EditorNodeMouseUp;
	behaviour->capturedMouseMove = MN_EditorNodeCapturedMouseMove;
#ifdef DEBUG
	Cmd_AddCommand("debug_mneditionstart", MN_EditorNodeStart_f, "Start node edition");
	Cmd_AddCommand("debug_mneditionstop", MN_EditorNodeStop_f, "Stop node edition");
	Cmd_AddCommand("debug_mneditionextract", MN_EditorNodeExtract_f, "Extract position and size of nodes into a file");
#endif

}
