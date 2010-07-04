/**
 * @file m_node_text2.c
 * @todo add getter/setter to cleanup access to extradata from cl_*.c files (check "u.text2.")
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
#include "../m_internal.h"
#include "../m_font.h"
#include "../m_actions.h"
#include "../m_parse.h"
#include "../m_render.h"
#include "m_node_text2.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../../shared/parse.h"

#define EXTRADATA_TYPE text2ExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) MN_EXTRADATACONST(node, EXTRADATA_TYPE)

static void MN_TextUpdateCache(menuNode_t *node);

static void MN_TextNodeGenerateLineSplit (menuNode_t *node)
{
	const char *data;
	int bufferSize = 1024;
	char *buffer = Mem_Alloc(bufferSize);

	LIST_Delete(&EXTRADATA(node).lineSplit);

	if (node->text != NULL)
		data = MN_GetReferenceString(node, node->text);
	else if (EXTRADATA(node).super.dataID != TEXT_NULL) {
		const menuSharedData_t *shared;
		shared = &mn.sharedData[EXTRADATA(node).super.dataID];
		switch (shared->type) {
		case MN_SHARED_TEXT:
			data = MN_GetText(EXTRADATA(node).super.dataID);
			break;
		case MN_SHARED_LINKEDLISTTEXT:
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
			buffer =  Mem_Alloc(bufferSize);
		}

		Q_strncpyz(buffer, data, lineSize + 1);
		LIST_AddString(&EXTRADATA(node).lineSplit, buffer);

		if (next == NULL)
			break;
		data = next + 1;
	}

	Mem_Free(buffer);
}

static void MN_TextValidateCache (menuNode_t *node)
{
	int v;
	if (EXTRADATA(node).super.dataID == TEXT_NULL || node->text != NULL)
		return;

	v = MN_GetDataVersion(EXTRADATA(node).super.dataID);
	if (v != EXTRADATA(node).super.versionId) {
		MN_TextUpdateCache(node);
	}
}

/**
 * @brief Get the line number under an absolute position
 * @param[in] node a text node
 * @param[in] x position x on the screen
 * @param[in] y position y on the screen
 * @return The line number under the position (0 = first line)
 */
static int MN_TextNodeGetLine (const menuNode_t *node, int x, int y)
{
	int lineHeight;
	int line;
	assert(MN_NodeInstanceOf(node, "text"));

	lineHeight = EXTRADATACONST(node).super.lineHeight;
	if (lineHeight == 0) {
		const char *font = MN_GetFontFromNode(node);
		lineHeight = MN_FontGetHeight(font);
	}

	MN_NodeAbsoluteToRelativePos(node, &x, &y);
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

static void MN_TextNodeMouseMove (menuNode_t *node, int x, int y)
{
	EXTRADATA(node).super.lineUnderMouse = MN_TextNodeGetLine(node, x, y);
}

#define MAX_MENUTEXTLEN		32768

/**
 * @brief Handles line breaks and drawing for MN_TEXT menu nodes
 * @param[in] node The context node
 * @param[in] text The test to draw else NULL
 * @param[in] list The test to draw else NULL
 * @param[in] noDraw If true, calling of this function only update the cache (real number of lines)
 * @note text or list but be used, not both
 */
static void MN_TextNodeDrawText (menuNode_t* node, const linkedList_t* list, qboolean noDraw)
{
	char newFont[MAX_VAR];
	const char* oldFont = NULL;
	vec4_t colorHover;
	vec4_t colorSelectedHover;
	int fullSizeY;
	int x1; /* variable x position */
	const char *font = MN_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width, height;
	int viewSizeY;

	MN_GetNodeAbsPos(node, pos);

	if (MN_AbstractScrollableNodeIsSizeChange(node)) {
		int lineHeight = EXTRADATA(node).super.lineHeight;
		if (lineHeight == 0) {
			const char *font = MN_GetFontFromNode(node);
			lineHeight = MN_FontGetHeight(font);
		}
		viewSizeY = node->size[1] / lineHeight;
	} else {
		viewSizeY = EXTRADATA(node).super.super.scrollY.viewSize;
	}

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;
	height = node->size[1] - node->padding - node->padding;

	/* Hover darkening effect for normal text lines. */
	VectorScale(node->color, 0.8, colorHover);
	colorHover[3] = node->color[3];

	/* Hover darkening effect for selected text lines. */
	VectorScale(node->selectedColor, 0.8, colorSelectedHover);
	colorSelectedHover[3] = node->selectedColor[3];

	/* fix position of the start of the draw according to the align */
	switch (node->textalign % 3) {
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
				MN_DrawString(font, node->textalign, x1, y, x, y, width, height, EXTRADATA(node).super.lineHeight, cur, viewSizeY, EXTRADATA(node).super.super.scrollY.viewPos, &fullSizeY, qtrue, EXTRADATA(node).super.longlines);
		}

		list = list->next;
	}

	/* update scroll status */
	MN_AbstractScrollableNodeSetY(node, -1, viewSizeY, fullSizeY);

	R_Color(NULL);
}

static void MN_TextUpdateCache (menuNode_t *node)
{
	const menuSharedData_t *shared;

	MN_TextNodeGenerateLineSplit(node);

	if (EXTRADATA(node).super.dataID == TEXT_NULL && node->text != NULL) {
		MN_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qtrue);
		return;
	}

	shared = &mn.sharedData[EXTRADATA(node).super.dataID];

	if (shared->type == MN_SHARED_LINKEDLISTTEXT) {
		MN_TextNodeDrawText(node, shared->data.linkedListText, qtrue);
	} else {
		MN_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qtrue);
	}

	EXTRADATA(node).super.versionId = shared->versionId;
}

/**
 * @brief Draw a text node
 */
static void MN_TextNodeDraw (menuNode_t *node)
{
	const menuSharedData_t *shared;

	MN_TextValidateCache(node);

	if (EXTRADATA(node).super.dataID == TEXT_NULL && node->text != NULL) {
		MN_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qfalse);
		return;
	}

	shared = &mn.sharedData[EXTRADATA(node).super.dataID];

	switch (shared->type) {
	case MN_SHARED_TEXT:
		MN_TextNodeDrawText(node, EXTRADATA(node).lineSplit, qfalse);
		break;
	case MN_SHARED_LINKEDLISTTEXT:
		MN_TextNodeDrawText(node, shared->data.linkedListText, qfalse);
		break;
	default:
		break;
	}

	EXTRADATA(node).super.versionId = shared->versionId;
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa MN_TextNodeRightClick
 */
static void MN_TextNodeClick (menuNode_t * node, int x, int y)
{
	int line = MN_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.super.scrollY.fullSize)
		return;

	MN_TextNodeSelectLine(node, line);

	if (node->onClick)
		MN_ExecuteEventActions(node, node->onClick);
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @sa MN_TextNodeClick
 */
static void MN_TextNodeRightClick (menuNode_t * node, int x, int y)
{
	int line = MN_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.super.scrollY.fullSize)
		return;

	MN_TextNodeSelectLine(node, line);

	if (node->onRightClick)
		MN_ExecuteEventActions(node, node->onRightClick);
}

static void MN_TextNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	MN_AbstractScrollableNodeScrollY(node, (down ? 1 : -1));
	if (node->onWheelUp && !down)
		MN_ExecuteEventActions(node, node->onWheelUp);
	if (node->onWheelDown && down)
		MN_ExecuteEventActions(node, node->onWheelDown);
	if (node->onWheel)
		MN_ExecuteEventActions(node, node->onWheel);
}

static void MN_TextNodeLoading (menuNode_t *node)
{
	EXTRADATA(node).super.textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void MN_TextNodeLoaded (menuNode_t *node)
{
	int lineheight = EXTRADATA(node).super.lineHeight;
	/* auto compute lineheight */
	/* we don't overwrite EXTRADATA(node).lineHeight, because "0" is dynamically replaced by font height on draw function */
	if (lineheight == 0) {
		/* the font is used */
		const char *font = MN_GetFontFromNode(node);
		lineheight = MN_FontGetHeight(font);
	}

	/* auto compute rows (super.viewSizeY) */
	if (EXTRADATA(node).super.super.scrollY.viewSize == 0) {
		if (node->size[1] != 0 && lineheight != 0) {
			EXTRADATA(node).super.super.scrollY.viewSize = node->size[1] / lineheight;
		} else {
			EXTRADATA(node).super.super.scrollY.viewSize = 1;
			Com_Printf("MN_TextNodeLoaded: node '%s' has no rows value\n", MN_GetPath(node));
		}
	}

	/* auto compute height */
	if (node->size[1] == 0) {
		node->size[1] = EXTRADATA(node).super.super.scrollY.viewSize * lineheight;
	}

	/* is text slot exists */
	if (EXTRADATA(node).super.dataID >= MAX_MENUTEXTS)
		Com_Error(ERR_DROP, "Error in node %s - max menu num exceeded (num: %i, max: %i)", MN_GetPath(node), EXTRADATA(node).super.dataID, MAX_MENUTEXTS);

#ifdef DEBUG
	if (EXTRADATA(node).super.super.scrollY.viewSize != (int)(node->size[1] / lineheight)) {
		Com_Printf("MN_TextNodeLoaded: rows value (%i) of node '%s' differs from size (%.0f) and format (%i) values\n",
			EXTRADATA(node).super.super.scrollY.viewSize, MN_GetPath(node), node->size[1], lineheight);
	}
#endif

	if (node->text == NULL && EXTRADATA(node).super.dataID == TEXT_NULL)
		Com_Printf("MN_TextNodeLoaded: 'textid' property of node '%s' is not set\n", MN_GetPath(node));
}

static const value_t properties[] = {
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterText2Node (nodeBehaviour_t *behaviour)
{
	behaviour->name = "text2";
	behaviour->extends = "text";
	behaviour->draw = MN_TextNodeDraw;
	behaviour->leftClick = MN_TextNodeClick;
	behaviour->rightClick = MN_TextNodeRightClick;
	behaviour->mouseWheel = MN_TextNodeMouseWheel;
	behaviour->mouseMove = MN_TextNodeMouseMove;
	behaviour->loading = MN_TextNodeLoading;
	behaviour->loaded = MN_TextNodeLoaded;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
