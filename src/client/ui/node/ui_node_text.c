/**
 * @file ui_node_text.c
 * @todo add getter/setter to cleanup access to extradata from cl_*.c files (check "u.text.")
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
#include "../ui_behaviour.h"
#include "../ui_render.h"
#include "ui_node_text.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../../shared/parse.h"

#define EXTRADATA_TYPE textExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_TextUpdateCache(uiNode_t *node);

/* Used for drag&drop-like scrolling */
static int mouseScrollX;
static int mouseScrollY;

void UI_TextValidateCache (uiNode_t *node, textUpdateCache_t update)
{
	int v;
	if (EXTRADATA(node).dataID == TEXT_NULL || node->text != NULL)
		return;

	v = UI_GetDataVersion(EXTRADATA(node).dataID);
	if (v != EXTRADATA(node).versionId) {
		update(node);
	}
}

/**
 * @brief Change the selected line
 */
void UI_TextNodeSelectLine (uiNode_t *node, int num)
{
	if (EXTRADATA(node).textLineSelected == num)
		return;
	EXTRADATA(node).textLineSelected = num;
	if (node->onChange)
		UI_ExecuteEventActions(node, node->onChange);
}

/**
 * @brief Scroll to the bottom
 * @param[in] nodePath absolute path
 */
void UI_TextScrollEnd (const char* nodePath)
{
	uiNode_t *node = UI_GetNodeByPath(nodePath);
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' could not be found\n", nodePath);
		return;
	}

	if (!UI_NodeInstanceOf(node, "text")) {
		Com_Printf("UI_TextScrollBottom: '%s' node is not an 'text'.\n", Cmd_Argv(1));
		return;
	}

	UI_TextValidateCache(node, UI_TextUpdateCache);

	if (EXTRADATA(node).super.scrollY.fullSize > EXTRADATA(node).super.scrollY.viewSize) {
		EXTRADATA(node).super.scrollY.viewPos = EXTRADATA(node).super.scrollY.fullSize - EXTRADATA(node).super.scrollY.viewSize;
		UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);
	}
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

	lineHeight = EXTRADATACONST(node).lineHeight;
	if (lineHeight == 0) {
		const char *font = UI_GetFontFromNode(node);
		lineHeight = UI_FontGetHeight(font);
	}

	UI_NodeAbsoluteToRelativePos(node, &x, &y);
	y -= node->padding;

	/* skip position over the first line */
	if (y < 0)
		 return -1;
	line = (int) (y / lineHeight) + EXTRADATACONST(node).super.scrollY.viewPos;

	/* skip position under the last line */
	if (line >= EXTRADATACONST(node).super.scrollY.fullSize)
		return -1;

	return line;
}

static void UI_TextNodeMouseMove (uiNode_t *node, int x, int y)
{
	EXTRADATA(node).lineUnderMouse = UI_TextNodeGetLine(node, x, y);
}

#define UI_TEXTNODE_BUFFERSIZE		32768

/**
 * @brief Handles line breaks and drawing for shared data id
 * @param[in] node The context node
 * @param[in] text The test to draw else NULL
 * @param[in] list The test to draw else NULL
 * @param[in] noDraw If true, calling of this function only update the cache (real number of lines)
 * @note text or list but be used, not both
 */
static void UI_TextNodeDrawText (uiNode_t* node, const char *text, const linkedList_t* list, qboolean noDraw)
{
	/** @todo verry big, what about static, rendering is not multi threaded */
	char textCopy[UI_TEXTNODE_BUFFERSIZE];
	char newFont[MAX_VAR];
	const char* oldFont = NULL;
	vec4_t colorHover;
	vec4_t colorSelectedHover;
	char *cur, *tab, *end;
	int fullSizeY;
	int x1; /* variable x position */
	const char *font = UI_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width;
	int viewSizeY;

	UI_GetNodeAbsPos(node, pos);

	if (UI_AbstractScrollableNodeIsSizeChange(node)) {
		int lineHeight = EXTRADATA(node).lineHeight;
		if (lineHeight == 0) {
			const char *font = UI_GetFontFromNode(node);
			lineHeight = UI_FontGetHeight(font);
		}
		viewSizeY = node->size[1] / lineHeight;
	} else {
		viewSizeY = EXTRADATA(node).super.scrollY.viewSize;
	}

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;

	if (text) {
		Q_strncpyz(textCopy, text, sizeof(textCopy));
	} else if (list) {
		Q_strncpyz(textCopy, (const char*)list->data, sizeof(textCopy));
	} else
		return;	/**< Nothing to draw */

	cur = textCopy;

	/* Hover darkening effect for normal text lines. */
	VectorScale(node->color, 0.8, colorHover);
	colorHover[3] = node->color[3];

	/* Hover darkening effect for selected text lines. */
	VectorScale(node->selectedColor, 0.8, colorSelectedHover);
	colorSelectedHover[3] = node->selectedColor[3];

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
	do {
		qboolean haveTab;
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

		/* get the position of the next newline - otherwise end will be null */
		end = strchr(cur, '\n');
		if (end)
			/* set the \n to \0 to draw only this part (before the \n) with our font renderer */
			/* let end point to the next char after the \n (or \0 now) */
			*end++ = '\0';

		/* highlighting */
		if (fullSizeY == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0) {
			/* Draw current line in "selected" color (if the linenumber is stored). */
			R_Color(node->selectedColor);
		} else {
			R_Color(node->color);
		}

		if (node->state && EXTRADATA(node).mousefx && fullSizeY == EXTRADATA(node).lineUnderMouse) {
			/* Highlight line if mousefx is true. */
			/** @todo what about multiline text that should be highlighted completely? */
			if (fullSizeY == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0) {
				R_Color(colorSelectedHover);
			} else {
				R_Color(colorHover);
			}
		}

		/* tabulation, we assume all the tabs fit on a single line */
		haveTab = strchr(cur, '\t') != NULL;
		if (haveTab) {
			while (cur && *cur) {
				int tabwidth;

				tab = strchr(cur, '\t');

				/* use tab stop as given via property definition
				 * or use 1/3 of the node size (width) */
				if (!EXTRADATA(node).tabWidth)
					tabwidth = width / 3;
				else
					tabwidth = EXTRADATA(node).tabWidth;

				if (tab) {
					int numtabs = strspn(tab, "\t");
					tabwidth *= numtabs;
					while (*tab == '\t')
						*tab++ = '\0';
				} else {
					/* maximize width for the last element */
					tabwidth = width - (x1 - x);
					if (tabwidth < 0)
						tabwidth = 0;
				}

				/* minimize width for element outside node */
				if ((x1 - x) + tabwidth > width)
					tabwidth = width - (x1 - x);

				/* make sure it is positive */
				if (tabwidth < 0)
					tabwidth = 0;

				if (tabwidth != 0)
					UI_DrawString(font, node->contentAlign, x1, y, x1, tabwidth - 1, EXTRADATA(node).lineHeight, cur, viewSizeY, EXTRADATA(node).super.scrollY.viewPos, &fullSizeY, qfalse, LONGLINES_PRETTYCHOP);

				/* next */
				x1 += tabwidth;
				cur = tab;
			}
			fullSizeY++;
		}

		/*Com_Printf("until newline - lines: %i\n", lines);*/
		/* the conditional expression at the end is a hack to draw "/n/n" as a blank line */
		/* prevent line from being drawn if there is nothing that should be drawn after it */
		if (cur && (cur[0] || end || list)) {
			/* is it a white line? */
			if (!cur) {
				fullSizeY++;
			} else {
				if (noDraw) {
					int lines = 0;
					R_FontTextSize (font, cur, width, EXTRADATA(node).longlines, NULL, NULL, &lines, NULL);
					fullSizeY += lines;
				} else
					UI_DrawString(font, node->contentAlign, x1, y, x, width, EXTRADATA(node).lineHeight, cur, viewSizeY, EXTRADATA(node).super.scrollY.viewPos, &fullSizeY, qtrue, EXTRADATA(node).longlines);
			}
		}

		if (EXTRADATA(node).mousefx)
			R_Color(node->color); /* restore original color */

		/* now set cur to the next char after the \n (see above) */
		cur = end;
		if (!cur && list) {
			list = list->next;
			if (list) {
				Q_strncpyz(textCopy, (const char*)list->data, sizeof(textCopy));
				cur = textCopy;
			}
		}
	} while (cur);

	/* update scroll status */
	UI_AbstractScrollableNodeSetY(node, -1, viewSizeY, fullSizeY);

	R_Color(NULL);
}

static void UI_TextUpdateCache (uiNode_t *node)
{
	const uiSharedData_t *shared;

	if (EXTRADATA(node).dataID == TEXT_NULL && node->text != NULL)
		return;

	shared = &ui_global.sharedData[EXTRADATA(node).dataID];

	switch (shared->type) {
	case UI_SHARED_TEXT:
		{
			const char* t = shared->data.text;
			if (t[0] == '_')
				t = _(++t);
			UI_TextNodeDrawText(node, t, NULL, qtrue);
		}
		break;
	case UI_SHARED_LINKEDLISTTEXT:
		UI_TextNodeDrawText(node, NULL, shared->data.linkedListText, qtrue);
		break;
	default:
		break;
	}

	EXTRADATA(node).versionId = shared->versionId;
}

/**
 * @brief Draw a text node
 */
static void UI_TextNodeDraw (uiNode_t *node)
{
	const uiSharedData_t *shared;

	if (EXTRADATA(node).dataID == TEXT_NULL && node->text != NULL) {
		const char* t = UI_GetReferenceString(node, node->text);
		if (t[0] == '_')
			t = _(++t);
		UI_TextNodeDrawText(node, t, NULL, qfalse);
		return;
	}

	shared = &ui_global.sharedData[EXTRADATA(node).dataID];

	switch (shared->type) {
	case UI_SHARED_TEXT:
	{
		const char* t = shared->data.text;
		if (t[0] == '_')
			t = _(++t);
		UI_TextNodeDrawText(node, t, NULL, qfalse);
		break;
	}
	case UI_SHARED_LINKEDLISTTEXT:
		UI_TextNodeDrawText(node, NULL, shared->data.linkedListText, qfalse);
		break;
	default:
		break;
	}

	EXTRADATA(node).versionId = shared->versionId;
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa UI_TextNodeRightClick
 */
static void UI_TextNodeClick (uiNode_t * node, int x, int y)
{
	int line = UI_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.scrollY.fullSize)
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

	if (line < 0 || line >= EXTRADATA(node).super.scrollY.fullSize)
		return;

	UI_TextNodeSelectLine(node, line);

	if (node->onRightClick)
		UI_ExecuteEventActions(node, node->onRightClick);
}

/**
 */
static qboolean UI_TextNodeMouseWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean updated;
	qboolean down = deltaY > 0;
	if (deltaY == 0)
		return qfalse;
	updated = UI_AbstractScrollableNodeScrollY(node, (down ? 1 : -1));

	/* @todo use super behaviour */
	if (node->onWheelUp && !down) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		updated = qtrue;
	}
	if (node->onWheelDown && down) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		updated = qtrue;
	}
	if (node->onWheel) {
		UI_ExecuteEventActions(node, node->onWheel);
		updated = qtrue;
	}
	return updated;
}

static void UI_TextNodeLoading (uiNode_t *node)
{
	EXTRADATA(node).textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void UI_TextNodeLoaded (uiNode_t *node)
{
	int lineheight = EXTRADATA(node).lineHeight;
	/* auto compute lineheight */
	/* we don't overwrite EXTRADATA(node).lineHeight, because "0" is dynamically replaced by font height on draw function */
	if (lineheight == 0) {
		/* the font is used */
		const char *font = UI_GetFontFromNode(node);
		lineheight = UI_FontGetHeight(font);
	}

	/* auto compute rows (super.viewSizeY) */
	if (EXTRADATA(node).super.scrollY.viewSize == 0) {
		if (node->size[1] != 0 && lineheight != 0) {
			EXTRADATA(node).super.scrollY.viewSize = node->size[1] / lineheight;
		} else {
			EXTRADATA(node).super.scrollY.viewSize = 1;
			Com_Printf("UI_TextNodeLoaded: node '%s' has no rows value\n", UI_GetPath(node));
		}
	}

	/* auto compute height */
	if (node->size[1] == 0) {
		node->size[1] = EXTRADATA(node).super.scrollY.viewSize * lineheight;
	}

	/* is text slot exists */
	if (EXTRADATA(node).dataID >= UI_MAX_DATAID)
		Com_Error(ERR_DROP, "Error in node %s - max shared data id num exceeded (num: %i, max: %i)", UI_GetPath(node), EXTRADATA(node).dataID, UI_MAX_DATAID);

#ifdef DEBUG
	if (EXTRADATA(node).super.scrollY.viewSize != (int)(node->size[1] / lineheight)) {
		Com_Printf("UI_TextNodeLoaded: rows value (%i) of node '%s' differs from size (%.0f) and format (%i) values\n",
			EXTRADATA(node).super.scrollY.viewSize, UI_GetPath(node), node->size[1], lineheight);
	}
#endif

	if (node->text == NULL && EXTRADATA(node).dataID == TEXT_NULL)
		Com_Printf("UI_TextNodeLoaded: 'textid' property of node '%s' is not set\n", UI_GetPath(node));
}

/**
 * @brief Track mouse down/up events to implement drag&drop-like scrolling, for touchscreen devices
 * @sa UI_TextNodeMouseUp, UI_TextNodeCapturedMouseMove
*/
static void UI_TextNodeMouseDown (struct uiNode_s *node, int x, int y, int button)
{
	if (!UI_GetMouseCapture() && button == K_MOUSE1 &&
		EXTRADATA(node).super.scrollY.fullSize > EXTRADATA(node).super.scrollY.viewSize) {
		UI_SetMouseCapture(node);
		mouseScrollX = x;
		mouseScrollY = y;
	}
}

static void UI_TextNodeMouseUp (struct uiNode_s *node, int x, int y, int button)
{
	if (UI_GetMouseCapture() == node)  /* More checks can never hurt */
		UI_MouseRelease();
}

static void UI_TextNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	const int lineHeight = node->behaviour->getCellHeight(node);
	const int deltaY = (mouseScrollY - y) / lineHeight;
	/* We're doing only vertical scroll, that's enough for the most instances */
	if (abs(mouseScrollY - y) >= lineHeight) {
		UI_AbstractScrollableNodeScrollY(node, deltaY);
		/* @todo not accurate */
		mouseScrollX = x;
		mouseScrollY = y;
	}
	if (node->behaviour->mouseMove)
		node->behaviour->mouseMove(node, x, y);
}

/**
 * @brief Return size of the cell, which is the size (in virtual "pixel") which represent 1 in the scroll values.
 * Here we guess the widget can scroll pixel per pixel.
 * @return Size in pixel.
 */
static int UI_TextNodeGetCellHeight (uiNode_t *node)
{
	int lineHeight = EXTRADATA(node).lineHeight;
	if (lineHeight == 0)
		lineHeight = UI_FontGetHeight(UI_GetFontFromNode(node));
	return lineHeight;
}

void UI_RegisterTextNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "text";
	behaviour->extends = "abstractscrollable";
	behaviour->draw = UI_TextNodeDraw;
	behaviour->leftClick = UI_TextNodeClick;
	behaviour->rightClick = UI_TextNodeRightClick;
	behaviour->scroll = UI_TextNodeMouseWheel;
	behaviour->mouseMove = UI_TextNodeMouseMove;
	behaviour->mouseDown = UI_TextNodeMouseDown;
	behaviour->mouseUp = UI_TextNodeMouseUp;
	behaviour->capturedMouseMove = UI_TextNodeCapturedMouseMove;
	behaviour->loading = UI_TextNodeLoading;
	behaviour->loaded = UI_TextNodeLoaded;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->getCellHeight = UI_TextNodeGetCellHeight;

	/* Current selected line  */
	UI_RegisterExtradataNodeProperty(behaviour, "lineselected", V_INT, textExtraData_t, textLineSelected);
	/* One of the list TEXT_STANDARD, TEXT_LIST, TEXT_UFOPEDIA, TEXT_BUILDINGS,
	 * TEXT_BUILDING_INFO, TEXT_RESEARCH, TEXT_RESEARCH_INFO, TEXT_POPUP,
	 * TEXT_POPUP_INFO, TEXT_AIRCRAFT_LIST, TEXT_AIRCRAFT, TEXT_AIRCRAFT_INFO,
	 * TEXT_MESSAGESYSTEM, TEXT_CAMPAIGN_LIST, TEXT_MULTISELECTION.
	 * There are more IDs in use - see ui_data.h for an up-to-date list.
	 * Display a shared content registered by the client code.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "dataid", V_UI_DATAID, textExtraData_t, dataID);
	/* Size between two lines. Default value is 0, in this case it use a line height according to the font size. */
	UI_RegisterExtradataNodeProperty(behaviour, "lineheight", V_INT, textExtraData_t, lineHeight);
	/* Bigger size of the width replacing a tab character. */
	UI_RegisterExtradataNodeProperty(behaviour, "tabwidth", V_INT, textExtraData_t, tabWidth);
	/* What to do with text lines longer than node width. Default is to wordwrap them to make multiple lines.
	 * It can be LONGLINES_WRAP, LONGLINES_CHOP, LONGLINES_PRETTYCHOP
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "longlines", V_INT, textExtraData_t, longlines);

	/* Number of visible line we can display into the node height.
	 * Currently, it translate the scrollable property <code>viewSize</code>
	 * @todo For a smooth scroll we should split that
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "rows", V_INT, textExtraData_t, super.scrollY.viewSize);
	/* Number of lines contained into the node.
	 * Currently, it translate the scrollable property <code>fullSize</code>
	 * @todo For a smooth scroll we should split that
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "lines", V_INT, textExtraData_t, super.scrollY.fullSize);

	/** Highlight each node elements when the mouse move over the node.
	 * @todo delete it went its possible (need to create a textlist...)
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "mousefx", V_BOOL, textExtraData_t, mousefx);

	Com_RegisterConstInt("LONGLINES_WRAP", LONGLINES_WRAP);
	Com_RegisterConstInt("LONGLINES_CHOP", LONGLINES_CHOP);
	Com_RegisterConstInt("LONGLINES_PRETTYCHOP", LONGLINES_PRETTYCHOP);
}
