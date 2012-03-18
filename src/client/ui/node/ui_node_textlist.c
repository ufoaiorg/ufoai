/**
 * @file ui_node_textlist.c
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
#include "../ui_internal.h"
#include "../ui_font.h"
#include "../ui_actions.h"
#include "../ui_parse.h"
#include "../ui_render.h"
#include "../ui_data.h"
#include "ui_node_text.h"
#include "ui_node_textlist.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../../shared/parse.h"

#define EXTRADATA(node) UI_EXTRADATA(node, textExtraData_t)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, textExtraData_t)

/**
 * @brief Get the line number under an absolute position
 * @param[in] node a text node
 * @param[in] x position x on the screen
 * @param[in] y position y on the screen
 * @return The line number under the position (0 = first line)
 */
static int UI_TextListNodeGetLine (const uiNode_t *node, int x, int y)
{
	int lineHeight = EXTRADATACONST(node).lineHeight;
	if (lineHeight == 0) {
		const char *font = UI_GetFontFromNode(node);
		lineHeight = UI_FontGetHeight(font);
	}

	UI_NodeAbsoluteToRelativePos(node, &x, &y);
	y -= node->padding;
	return (int) (y / lineHeight) + EXTRADATACONST(node).super.scrollY.viewPos;
}

static void UI_TextListNodeMouseMove (uiNode_t *node, int x, int y)
{
	EXTRADATA(node).lineUnderMouse = UI_TextListNodeGetLine(node, x, y);
}

/**
 * @brief Handles line breaks and drawing for shared data text
 * @param[in] node The context node
 * @param[in] list The text list to draw
 */
static void UI_TextLineNodeDrawText (uiNode_t* node, const linkedList_t* list)
{
	vec4_t colorHover;
	vec4_t colorSelectedHover;
	int count; /* variable x position */
	const char *font = UI_GetFontFromNode(node);
	vec2_t pos;
	int currentY;
	int viewSizeY;
	int lineHeight;
	int maxHeight;

	lineHeight = EXTRADATA(node).lineHeight;
	if (lineHeight == 0)
		lineHeight = UI_FontGetHeight(font);

	if (UI_AbstractScrollableNodeIsSizeChange(node))
		viewSizeY = node->size[1] / lineHeight;
	else
		viewSizeY = EXTRADATA(node).super.scrollY.viewSize;

	/* text box */
	UI_GetNodeAbsPos(node, pos);
	pos[0] += node->padding;
	pos[1] += node->padding;

	/* prepare colors */
	VectorScale(node->color, 0.8, colorHover);
	colorHover[3] = node->color[3];
	VectorScale(node->selectedColor, 0.8, colorSelectedHover);
	colorSelectedHover[3] = node->selectedColor[3];

	/* skip lines over the scroll */
	count = 0;
	while (list && count < EXTRADATA(node).super.scrollY.viewPos) {
		list = list->next;
		count++;
	}

	currentY = pos[1];
	maxHeight = currentY + node->size[1] - node->padding - node->padding - node->padding - lineHeight;
	while (list) {
		const int width = node->size[0] - node->padding - node->padding;
		const char* text = (const char*) list->data;
		if (currentY > maxHeight)
			break;

		/* highlight selected line */
		if (count == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0)
			R_Color(node->selectedColor);
		else
			R_Color(node->color);

		/* highlight line under mouse */
		if (node->state && count == EXTRADATA(node).lineUnderMouse) {
			if (count == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0)
				R_Color(colorSelectedHover);
			else
				R_Color(colorHover);
		}

		UI_DrawStringInBox(font, node->contentAlign, pos[0], currentY, width, lineHeight, text, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += lineHeight;


		list = list->next;
		count++;
	}

	/* count all elements */
	while (list) {
		list = list->next;
		count++;
	}

	/* update scroll status */
	UI_AbstractScrollableNodeSetY(node, -1, viewSizeY, count);

	R_Color(NULL);
}

/**
 * @brief Draw a text node
 */
static void UI_TextListNodeDraw (uiNode_t *node)
{
	const uiSharedData_t *shared;
	shared = &ui_global.sharedData[EXTRADATA(node).dataID];

	/* nothing set yet? */
	if (shared->type == UI_SHARED_NONE)
		return;
	if (shared->type != UI_SHARED_LINKEDLISTTEXT) {
		Com_Printf("UI_TextListNodeDraw: Only linkedlist text supported (dataid %d).\n", EXTRADATA(node).dataID);
		UI_ResetData(EXTRADATA(node).dataID);
		return;
	}

	UI_TextLineNodeDrawText(node, shared->data.linkedListText);
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa UI_TextNodeRightClick
 */
static void UI_TextListNodeClick (uiNode_t * node, int x, int y)
{
	const int line = UI_TextListNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.scrollY.fullSize)
		return;

	if (line != EXTRADATA(node).textLineSelected) {
		EXTRADATA(node).textLineSelected = line;
		if (node->onChange)
			UI_ExecuteEventActions(node, node->onChange);
	}

	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @todo we should delete that function
 */
static void UI_TextListNodeRightClick (uiNode_t * node, int x, int y)
{
	const int line = UI_TextListNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.scrollY.fullSize)
		return;

	if (line != EXTRADATA(node).textLineSelected) {
		EXTRADATA(node).textLineSelected = line;
		if (node->onChange)
			UI_ExecuteEventActions(node, node->onChange);
	}

	if (node->onRightClick)
		UI_ExecuteEventActions(node, node->onRightClick);
}

static void UI_TextListNodeLoading (uiNode_t *node)
{
	EXTRADATA(node).textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
	node->contentAlign = ALIGN_CL;	/**< left center of each cells */
}

void UI_RegisterTextListNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "textlist";
	behaviour->extends = "text";
	behaviour->draw = UI_TextListNodeDraw;
	behaviour->leftClick = UI_TextListNodeClick;
	behaviour->rightClick = UI_TextListNodeRightClick;
	behaviour->mouseMove = UI_TextListNodeMouseMove;
	behaviour->loading = UI_TextListNodeLoading;
}
