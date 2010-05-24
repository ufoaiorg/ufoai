/**
 * @file m_node_text.c
 * @todo add getter/setter to cleanup access to extradata from cl_*.c files (check "u.text.")
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
#include "m_node_text.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../../shared/parse.h"

#define EXTRADATA(node) MN_EXTRADATA(node, textExtraData_t)
#define EXTRADATACONST(node) MN_EXTRADATACONST(node, textExtraData_t)

static void MN_TextUpdateCache(menuNode_t *node);

static void MN_TextValidateCache (menuNode_t *node)
{
	int v;
	if (EXTRADATA(node).dataID == TEXT_NULL || node->text != NULL)
		return;

	v = MN_GetDataVersion(EXTRADATA(node).dataID);
	if (v != EXTRADATA(node).versionId) {
		MN_TextUpdateCache(node);
	}
}

/**
 * @brief Change the selected line
 */
void MN_TextNodeSelectLine (menuNode_t *node, int num)
{
	if (EXTRADATA(node).textLineSelected == num)
		return;
	EXTRADATA(node).textLineSelected = num;
	if (node->onChange)
		MN_ExecuteEventActions(node, node->onChange);
}

/**
 * @brief Scroll to the bottom
 * @param[in] nodePath absolute path
 */
void MN_TextScrollEnd (const char* nodePath)
{
	menuNode_t *node = MN_GetNodeByPath(nodePath);
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' could not be found\n", nodePath);
		return;
	}

	if (!MN_NodeInstanceOf(node, "text")) {
		Com_Printf("MN_TextScrollBottom: '%s' node is not an 'text'.\n", Cmd_Argv(1));
		return;
	}

	MN_TextValidateCache(node);

	if (EXTRADATA(node).super.scrollY.fullSize > EXTRADATA(node).super.scrollY.viewSize) {
		EXTRADATA(node).super.scrollY.viewPos = EXTRADATA(node).super.scrollY.fullSize - EXTRADATA(node).super.scrollY.viewSize;
		MN_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);
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

	lineHeight = EXTRADATACONST(node).lineHeight;
	if (lineHeight == 0) {
		const char *font = MN_GetFontFromNode(node);
		lineHeight = MN_FontGetHeight(font);
	}

	MN_NodeAbsoluteToRelativePos(node, &x, &y);
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

static void MN_TextNodeMouseMove (menuNode_t *node, int x, int y)
{
	EXTRADATA(node).lineUnderMouse = MN_TextNodeGetLine(node, x, y);
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
static void MN_TextNodeDrawText (menuNode_t* node, const char *text, const linkedList_t* list, qboolean noDraw)
{
	char textCopy[MAX_MENUTEXTLEN];
	char newFont[MAX_VAR];
	const char* oldFont = NULL;
	vec4_t colorHover;
	vec4_t colorSelectedHover;
	char *cur, *tab, *end;
	int fullSizeY;
	int x1; /* variable x position */
	const char *font = MN_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width, height;
	int viewSizeY;

	MN_GetNodeAbsPos(node, pos);

	if (MN_AbstractScrollableNodeIsSizeChange(node)) {
		int lineHeight = EXTRADATA(node).lineHeight;
		if (lineHeight == 0) {
			const char *font = MN_GetFontFromNode(node);
			lineHeight = MN_FontGetHeight(font);
		}
		viewSizeY = node->size[1] / lineHeight;
	} else {
		viewSizeY = EXTRADATA(node).super.scrollY.viewSize;
	}

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;
	height = node->size[1] - node->padding - node->padding;

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

				/* use tab stop as given via menu definition format string
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
					MN_DrawString(font, node->textalign, x1, y, x1, y, tabwidth - 1, height, EXTRADATA(node).lineHeight, cur, viewSizeY, EXTRADATA(node).super.scrollY.viewPos, &fullSizeY, qfalse, LONGLINES_PRETTYCHOP);

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
					MN_DrawString(font, node->textalign, x1, y, x, y, width, height, EXTRADATA(node).lineHeight, cur, viewSizeY, EXTRADATA(node).super.scrollY.viewPos, &fullSizeY, qtrue, EXTRADATA(node).longlines);
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
	MN_AbstractScrollableNodeSetY(node, -1, viewSizeY, fullSizeY);

	R_Color(NULL);
}

static void MN_TextUpdateCache (menuNode_t *node)
{
	const menuSharedData_t *shared;

	if (EXTRADATA(node).dataID == TEXT_NULL && node->text != NULL)
		return;

	shared = &mn.sharedData[EXTRADATA(node).dataID];

	switch (shared->type) {
	case MN_SHARED_TEXT:
		{
			const char* t = shared->data.text;
			if (t[0] == '_')
				t = _(++t);
			MN_TextNodeDrawText(node, t, NULL, qtrue);
		}
		break;
	case MN_SHARED_LINKEDLISTTEXT:
		MN_TextNodeDrawText(node, NULL, shared->data.linkedListText, qtrue);
		break;
	default:
		break;
	}

	EXTRADATA(node).versionId = shared->versionId;
}

/**
 * @brief Draw a text node
 */
static void MN_TextNodeDraw (menuNode_t *node)
{
	const menuSharedData_t *shared;

	if (EXTRADATA(node).dataID == TEXT_NULL && node->text != NULL) {
		const char* t = MN_GetReferenceString(node, node->text);
		if (t[0] == '_')
			t = _(++t);
		MN_TextNodeDrawText(node, t, NULL, qfalse);
		return;
	}

	shared = &mn.sharedData[EXTRADATA(node).dataID];

	switch (shared->type) {
	case MN_SHARED_TEXT:
	{
		const char* t = shared->data.text;
		if (t[0] == '_')
			t = _(++t);
		MN_TextNodeDrawText(node, t, NULL, qfalse);
		break;
	}
	case MN_SHARED_LINKEDLISTTEXT:
		MN_TextNodeDrawText(node, NULL, shared->data.linkedListText, qfalse);
		break;
	default:
		break;
	}

	EXTRADATA(node).versionId = shared->versionId;
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa MN_TextNodeRightClick
 */
static void MN_TextNodeClick (menuNode_t * node, int x, int y)
{
	int line = MN_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.scrollY.fullSize)
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

	if (line < 0 || line >= EXTRADATA(node).super.scrollY.fullSize)
		return;

	MN_TextNodeSelectLine(node, line);

	if (node->onRightClick)
		MN_ExecuteEventActions(node, node->onRightClick);
}

/**
 */
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
	EXTRADATA(node).textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void MN_TextNodeLoaded (menuNode_t *node)
{
	int lineheight = EXTRADATA(node).lineHeight;
	/* auto compute lineheight */
	/* we don't overwrite EXTRADATA(node).lineHeight, because "0" is dynamically replaced by font height on draw function */
	if (lineheight == 0) {
		/* the font is used */
		const char *font = MN_GetFontFromNode(node);
		lineheight = MN_FontGetHeight(font);
	}

	/* auto compute rows (super.viewSizeY) */
	if (EXTRADATA(node).super.scrollY.viewSize == 0) {
		if (node->size[1] != 0 && lineheight != 0) {
			EXTRADATA(node).super.scrollY.viewSize = node->size[1] / lineheight;
		} else {
			EXTRADATA(node).super.scrollY.viewSize = 1;
			Com_Printf("MN_TextNodeLoaded: node '%s' has no rows value\n", MN_GetPath(node));
		}
	}

	/* auto compute height */
	if (node->size[1] == 0) {
		node->size[1] = EXTRADATA(node).super.scrollY.viewSize * lineheight;
	}

	/* is text slot exists */
	if (EXTRADATA(node).dataID >= MAX_MENUTEXTS)
		Com_Error(ERR_DROP, "Error in node %s - max menu num exceeded (num: %i, max: %i)", MN_GetPath(node), EXTRADATA(node).dataID, MAX_MENUTEXTS);

#ifdef DEBUG
	if (EXTRADATA(node).super.scrollY.viewSize != (int)(node->size[1] / lineheight)) {
		Com_Printf("MN_TextNodeLoaded: rows value (%i) of node '%s' differs from size (%.0f) and format (%i) values\n",
			EXTRADATA(node).super.scrollY.viewSize, MN_GetPath(node), node->size[1], lineheight);
	}
#endif

	if (node->text == NULL && EXTRADATA(node).dataID == TEXT_NULL)
		Com_Printf("MN_TextNodeLoaded: 'textid' property of node '%s' is not set\n", MN_GetPath(node));
}

static const value_t properties[] = {
	/* Current selected line  */
	{"lineselected", V_INT, MN_EXTRADATA_OFFSETOF(textExtraData_t, textLineSelected), MEMBER_SIZEOF(textExtraData_t, textLineSelected)},
	/* One of the list TEXT_STANDARD, TEXT_LIST, TEXT_UFOPEDIA, TEXT_BUILDINGS,
	 * TEXT_BUILDING_INFO, TEXT_RESEARCH, TEXT_RESEARCH_INFO, TEXT_POPUP,
	 * TEXT_POPUP_INFO, TEXT_AIRCRAFT_LIST, TEXT_AIRCRAFT, TEXT_AIRCRAFT_INFO,
	 * TEXT_MESSAGESYSTEM, TEXT_CAMPAIGN_LIST, TEXT_MULTISELECTION.
	 * There are more IDs in use - see mn_data.h for and up-to-date list.
	 * Display a shared content registered by the client code.
	 */
	{"dataid", V_UI_DATAID, MN_EXTRADATA_OFFSETOF(textExtraData_t, dataID), MEMBER_SIZEOF(textExtraData_t, dataID)},
	/* Size between two lines. Default value is 0, in this case it use a line height according to the font size. */
	{"lineheight", V_INT, MN_EXTRADATA_OFFSETOF(textExtraData_t, lineHeight), MEMBER_SIZEOF(textExtraData_t, lineHeight)},
	/* Bigger size of the width replacing a tab character. */
	{"tabwidth", V_INT, MN_EXTRADATA_OFFSETOF(textExtraData_t, tabWidth), MEMBER_SIZEOF(textExtraData_t, tabWidth)},
	/* What to do with text lines longer than node width. Default is to wordwrap them to make multiple lines. */
	{"longlines", V_LONGLINES, MN_EXTRADATA_OFFSETOF(textExtraData_t, longlines), MEMBER_SIZEOF(textExtraData_t, longlines)},

	/* Number of visible line we can display into the node height.
	 * Currently, it translate the scrollable property <code>viewSize</code>
	 * @todo For a smooth scroll we should split that
	 */
	{"rows", V_INT, MN_EXTRADATA_OFFSETOF(textExtraData_t, super.scrollY.viewSize), MEMBER_SIZEOF(textExtraData_t, super.scrollY.viewSize)},
	/* Number of lines contained into the node.
	 * Currently, it translate the scrollable property <code>fullSize</code>
	 * @todo For a smooth scroll we should split that
	 */
	{"lines", V_INT, MN_EXTRADATA_OFFSETOF(textExtraData_t, super.scrollY.fullSize), MEMBER_SIZEOF(textExtraData_t, super.scrollY.fullSize)},

	/** Highlight each node elements when the mouse move over the node.
	 * @todo delete it went its possible (need to create a textlist...)
	 */
	{"mousefx", V_BOOL, MN_EXTRADATA_OFFSETOF(textExtraData_t, mousefx), MEMBER_SIZEOF(textExtraData_t, mousefx)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterTextNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "text";
	behaviour->extends = "abstractscrollable";
	behaviour->draw = MN_TextNodeDraw;
	behaviour->leftClick = MN_TextNodeClick;
	behaviour->rightClick = MN_TextNodeRightClick;
	behaviour->mouseWheel = MN_TextNodeMouseWheel;
	behaviour->mouseMove = MN_TextNodeMouseMove;
	behaviour->loading = MN_TextNodeLoading;
	behaviour->loaded = MN_TextNodeLoaded;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA(0));
}
