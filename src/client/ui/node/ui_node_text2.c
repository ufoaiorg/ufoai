/**
 * @file ui_node_text2.c
 * Node to display article of text. The node pre-parsed the "brute" text into a
 * data structure. The rendering code only paint the data structure, without any parsing.
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
#include "ui_node_text2.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../../shared/parse.h"

#define EXTRADATA_TYPE text2ExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_TextNodeGenerateLineSplit (uiNode_t *node)
{
	const char *data;
	int bufferSize = 1024;
	char *buffer = (char *)Mem_Alloc(bufferSize);

	LIST_Delete(&EXTRADATA(node).lineSplit);

	if (node->text != NULL)
		data = UI_GetReferenceString(node, node->text);
	else if (EXTRADATA(node).super.dataID != TEXT_NULL) {
		const uiSharedData_t *shared;
		shared = &ui_global.sharedData[EXTRADATA(node).super.dataID];
		switch (shared->type) {
		case UI_SHARED_TEXT:
			data = UI_GetText(EXTRADATA(node).super.dataID);
			break;
		case UI_SHARED_LINKEDLISTTEXT:
			return;
		default:
			return;
		}
	} else
		return;

	if (data[0] == '_')
		data = _(data + 1);

	while (data[0] != '\0') {
		const char *next = strchr(data, '\n');
		int lineSize;
		if (next == NULL)
			lineSize = strlen(data);
		else
			lineSize = next - data;

		if (lineSize + 1 > bufferSize) {
			bufferSize = lineSize + 1;
			Mem_Free(buffer);
			buffer = (char *)Mem_Alloc(bufferSize);
		}

		Q_strncpyz(buffer, data, lineSize + 1);
		LIST_AddString(&EXTRADATA(node).lineSplit, buffer);

		if (next == NULL)
			break;
		data = next + 1;
	}

	Mem_Free(buffer);
}

/**
 * @brief Get the line number under an absolute position
 * @param[in] node a text node
 * @param[in] x position x on the screen
 * @param[in] y position y on the screen
 * @return The line number under the position (0 = first line)
 */
static int UI_TextNodeGetLine (const uiNode_t *node, int x, int y)
{
	int lineHeight;
	int line;
	assert(UI_NodeInstanceOf(node, "text"));

	lineHeight = EXTRADATACONST(node).super.lineHeight;
	if (lineHeight == 0) {
		const char *font = UI_GetFontFromNode(node);
		lineHeight = UI_FontGetHeight(font);
	}

	UI_NodeAbsoluteToRelativePos(node, &x, &y);
	y -= node->padding;

	/* skip position over the first line */
	if (y < 0)
		 return -1;
	line = (int) (y / lineHeight) + EXTRADATACONST(node).super.super.scrollY.viewPos;

	/* skip position under the last line */
	if (line >= EXTRADATACONST(node).super.super.scrollY.fullSize)
		return -1;

	return line;
}

static void UI_TextNodeMouseMove (uiNode_t *node, int x, int y)
{
	EXTRADATA(node).super.lineUnderMouse = UI_TextNodeGetLine(node, x, y);
}

/**
 * @brief Handles linked list of text. Each element of the list must be a line of text without line break.
 * A line break is between 2 element of the list. Text line is wrapped/truncated... according to
 * a node property
 * @param[in] node The context node
 * @param[in] list The test to draw else NULL
 * @param[in] noDraw If true, calling of this function only update the cache (real number of lines)
 */
static void UI_TextNodeDrawText (uiNode_t* node, const linkedList_t* list, qboolean noDraw)
{
	char newFont[MAX_VAR];
	const char* oldFont = NULL;
	int fullSizeY;
	int x1; /* variable x position */
	const char *font = UI_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width;
	int viewSizeY;

	UI_GetNodeAbsPos(node, pos);

	if (UI_AbstractScrollableNodeIsSizeChange(node)) {
		int lineHeight = EXTRADATA(node).super.lineHeight;
		if (lineHeight == 0) {
			const char *font = UI_GetFontFromNode(node);
			lineHeight = UI_FontGetHeight(font);
		}
		viewSizeY = node->size[1] / lineHeight;
	} else {
		viewSizeY = EXTRADATA(node).super.super.scrollY.viewSize;
	}

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;

	/* fix position of the start of the draw according to the align */
	switch (node->contentAlign % 3) {
	case 0:	/* left */
		break;
	case 1:	/* middle */
		x += width / 2;
		break;
	case 2:	/* right */
		x += width;
		break;
	}

	R_Color(node->color);

	fullSizeY = 0;
	while (list) {
		const char *cur = (const char*)list->data;

		/* new line starts from node x position */
		x1 = x;
		if (oldFont) {
			font = oldFont;
			oldFont = NULL;
		}

		/* text styles and inline images */
		if (cur[0] == '^') {
			switch (toupper(cur[1])) {
			case 'B':
				Com_sprintf(newFont, sizeof(newFont), "%s_bold", font);
				oldFont = font;
				font = newFont;
				cur += 2; /* don't print the format string */
				break;
			}
		}

		/* is it a white line? */
		if (!cur) {
			fullSizeY++;
		} else {
			if (noDraw) {
				int lines = 0;
				R_FontTextSize (font, cur, width, EXTRADATA(node).super.longlines, NULL, NULL, &lines, NULL);
				fullSizeY += lines;
			} else
				UI_DrawString(font, node->contentAlign, x1, y, x, width, EXTRADATA(node).super.lineHeight, cur, viewSizeY, EXTRADATA(node).super.super.scrollY.viewPos, &fullSizeY, qtrue, EXTRADATA(node).super.longlines);
		}

		list = list->next;
	}

	/* update scroll status */
	UI_AbstractScrollableNodeSetY(node, -1, viewSizeY, fullSizeY);

	R_Color(NULL);
}

static void UI_TextUpdateCache (uiNode_t *node)
{
	const uiSharedData_t *shared;

	UI_TextNodeGenerateLineSplit(node);

	if (EXTRADATA(node).super.dataID == TEXT_NULL && node->text != NULL) {
		UI_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qtrue);
		return;
	}

	shared = &ui_global.sharedData[EXTRADATA(node).super.dataID];

	if (shared->type == UI_SHARED_LINKEDLISTTEXT) {
		UI_TextNodeDrawText(node, shared->data.linkedListText, qtrue);
	} else {
		UI_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qtrue);
	}

	EXTRADATA(node).super.versionId = shared->versionId;
}

/**
 * @brief Draw a text node
 */
static void UI_TextNodeDraw (uiNode_t *node)
{
	const uiSharedData_t *shared;

	UI_TextValidateCache(node, UI_TextUpdateCache);

	if (EXTRADATA(node).super.dataID == TEXT_NULL && node->text != NULL) {
		UI_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qfalse);
		return;
	}

	shared = &ui_global.sharedData[EXTRADATA(node).super.dataID];

	switch (shared->type) {
	case UI_SHARED_TEXT:
		UI_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qfalse);
		break;
	case UI_SHARED_LINKEDLISTTEXT:
		UI_TextNodeDrawText(node, shared->data.linkedListText, qfalse);
		break;
	default:
		break;
	}

	EXTRADATA(node).super.versionId = shared->versionId;
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa UI_TextNodeRightClick
 */
static void UI_TextNodeClick (uiNode_t * node, int x, int y)
{
	int line = UI_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.super.scrollY.fullSize)
		return;

	UI_TextNodeSelectLine(node, line);

	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @sa UI_TextNodeClick
 */
static void UI_TextNodeRightClick (uiNode_t * node, int x, int y)
{
	int line = UI_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.super.scrollY.fullSize)
		return;

	UI_TextNodeSelectLine(node, line);

	if (node->onRightClick)
		UI_ExecuteEventActions(node, node->onRightClick);
}

static void UI_TextNodeLoading (uiNode_t *node)
{
	EXTRADATA(node).super.textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void UI_TextNodeLoaded (uiNode_t *node)
{
	int lineheight = EXTRADATA(node).super.lineHeight;
	/* auto compute lineheight */
	/* we don't overwrite EXTRADATA(node).lineHeight, because "0" is dynamically replaced by font height on draw function */
	if (lineheight == 0) {
		/* the font is used */
		const char *font = UI_GetFontFromNode(node);
		lineheight = UI_FontGetHeight(font);
	}

	/* auto compute rows (super.viewSizeY) */
	if (EXTRADATA(node).super.super.scrollY.viewSize == 0) {
		if (node->size[1] != 0 && lineheight != 0) {
			EXTRADATA(node).super.super.scrollY.viewSize = node->size[1] / lineheight;
		} else {
			EXTRADATA(node).super.super.scrollY.viewSize = 1;
			Com_Printf("UI_TextNodeLoaded: node '%s' has no rows value\n", UI_GetPath(node));
		}
	}

	/* auto compute height */
	if (node->size[1] == 0) {
		node->size[1] = EXTRADATA(node).super.super.scrollY.viewSize * lineheight;
	}

	/* is text slot exists */
	if (EXTRADATA(node).super.dataID >= UI_MAX_DATAID)
		Com_Error(ERR_DROP, "Error in node %s - max shared data id exceeded (num: %i, max: %i)", UI_GetPath(node), EXTRADATA(node).super.dataID, UI_MAX_DATAID);

#ifdef DEBUG
	if (EXTRADATA(node).super.super.scrollY.viewSize != (int)(node->size[1] / lineheight)) {
		Com_Printf("UI_TextNodeLoaded: rows value (%i) of node '%s' differs from size (%.0f) and format (%i) values\n",
			EXTRADATA(node).super.super.scrollY.viewSize, UI_GetPath(node), node->size[1], lineheight);
	}
#endif

	if (node->text == NULL && EXTRADATA(node).super.dataID == TEXT_NULL)
		Com_Printf("UI_TextNodeLoaded: 'textid' property of node '%s' is not set\n", UI_GetPath(node));
}

void UI_RegisterText2Node (uiBehaviour_t *behaviour)
{
	behaviour->name = "text2";
	behaviour->extends = "text";
	behaviour->draw = UI_TextNodeDraw;
	behaviour->leftClick = UI_TextNodeClick;
	behaviour->rightClick = UI_TextNodeRightClick;
	behaviour->mouseMove = UI_TextNodeMouseMove;
	behaviour->loading = UI_TextNodeLoading;
	behaviour->loaded = UI_TextNodeLoaded;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
