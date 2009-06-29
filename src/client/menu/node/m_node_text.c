/**
 * @file m_node_text.c
 * @todo add getter/setter to cleanup access to extradata from cl_*.c files (check "u.text.")
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

#include "../m_main.h"
#include "../m_internal.h"
#include "../m_font.h"
#include "../m_actions.h"
#include "../m_parse.h"
#include "../m_render.h"
#include "m_node_text.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
/** @todo remove this dependency from the text node */
#include "../../campaign/cp_campaign.h" /**< message_t */
#include "../../../shared/parse.h"

#define EXTRADATA(node) (node->u.text)

static void MN_TextNodeDraw(menuNode_t *node);

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
 * @brief Scriptfunction that gets the wanted text node and scrolls the text.
 */
static void MN_TextScroll_f (void)
{
	int offset = 0;
	menuNode_t *node;
	menuNode_t *menu;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <nodename> <+/-offset>\n", Cmd_Argv(0));
		return;
	}

	menu = MN_GetActiveMenu();
	if (!menu) {
		Com_Printf("MN_TextScroll_f: No active menu\n");
		return;
	}

	node = MN_GetNodeByPath(va("%s.%s", menu->name, Cmd_Argv(1)));
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "MN_TextScroll_f: Node '%s' not found.\n", Cmd_Argv(1));
		return;
	}

	if (!strncmp(Cmd_Argv(2), "reset", 5)) {
		MN_AbstractScrollableNodeSetY(node, 0, -1, -1);
		return;
	}

	offset = atoi(Cmd_Argv(2));

	if (offset == 0)
		return;

	MN_AbstractScrollableNodeScrollY(node, offset);
}

/**
 * @brief Scroll to the bottom
 * @todo fix param to use absolute path
 */
void MN_TextScrollBottom (const char* nodeName)
{
	menuNode_t *node = MN_GetNode(MN_GetActiveMenu(), nodeName);
	if (!node) {
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' could not be found\n", nodeName);
		return;
	}

	if (EXTRADATA(node).super.fullSizeY > EXTRADATA(node).super.viewSizeY) {
		Com_DPrintf(DEBUG_CLIENT, "\nMN_TextScrollBottom: Scrolling to line %i\n", EXTRADATA(node).super.fullSizeY - EXTRADATA(node).super.viewSizeY + 1);
		EXTRADATA(node).super.viewPosY = EXTRADATA(node).super.fullSizeY - EXTRADATA(node).super.viewSizeY + 1;
	}
}

/**
 * @brief Draw a scrollbar, if necessary
 * @note Needs node->super.fullSizeY to be accurate
 */
static void MN_DrawScrollBar (const menuNode_t *node)
{
	static const vec4_t scrollbarBackground = {0.03, 0.41, 0.05, 0.5};
	static const vec4_t scrollbarColor = {0.03, 0.41, 0.05, 1.0};

	if (EXTRADATA(node).scrollbar && EXTRADATA(node).super.viewSizeY && EXTRADATA(node).super.fullSizeY > EXTRADATA(node).super.viewSizeY) {
		vec2_t nodepos;
		int scrollbarX;
		float scrollbarY;

		MN_GetNodeAbsPos(node, nodepos);
		scrollbarX = nodepos[0] + node->size[0] - MN_SCROLLBAR_WIDTH;
		scrollbarY = node->size[1] * EXTRADATA(node).super.viewSizeY / EXTRADATA(node).super.fullSizeY * MN_SCROLLBAR_HEIGHT;

		MN_DrawFill(scrollbarX, nodepos[1],
			MN_SCROLLBAR_WIDTH, node->size[1],
			scrollbarBackground);

		MN_DrawFill(scrollbarX, nodepos[1] + (node->size[1] - scrollbarY) * EXTRADATA(node).super.viewPosY / (EXTRADATA(node).super.fullSizeY - EXTRADATA(node).super.viewSizeY),
			MN_SCROLLBAR_WIDTH, scrollbarY,
			scrollbarColor);
	}
}

int MN_TextNodeGetLines (const struct menuNode_s *node)
{
	return EXTRADATA(node).super.fullSizeY;
}

/**
 * @brief Get the line number under an absolute position
 * @param[in] node a text node
 * @param[in] x position x on the screen
 * @param[in] y position y on the screen
 * @return The line number under the position (0 = first line)
 */
int MN_TextNodeGetLine (const menuNode_t *node, int x, int y)
{
	assert(MN_NodeInstanceOf(node, "text"));

	/* if no texh, its not a text list, result is not important */
	if (!node->u.text.lineHeight)
		return 0;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);
	if (EXTRADATA(node).super.viewPosY)
		return (int) (y / node->u.text.lineHeight) + EXTRADATA(node).super.viewPosY;
	else
		return (int) (y / node->u.text.lineHeight);
}

static void MN_TextNodeMouseMove (menuNode_t *node, int x, int y)
{
	EXTRADATA(node).lineUnderMouse = MN_TextNodeGetLine(node, x, y);
}

/**
 * @brief Handles line breaks and drawing for MN_TEXT menu nodes
 * @param[in] text Text to draw
 * @param[in] font Font string to use
 * @param[in] node The current menu node
 * @param[in] x The fixed x position every new line starts
 * @param[in] y The fixed y position the text node starts
 */
static void MN_TextNodeDrawText (menuNode_t* node, const char *text, const linkedList_t* list)
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
		int lineheight = node->u.text.lineHeight;
		if (lineheight == 0) {
			const char *font = MN_GetFontFromNode(node);
			lineheight = MN_FontGetHeight(font) / 2;
		}
		viewSizeY = node->size[1] / lineheight;
	} else {
		viewSizeY = EXTRADATA(node).super.viewSizeY;
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

	/* scrollbar space */
	if (EXTRADATA(node).scrollbar)
		width -= MN_SCROLLBAR_WIDTH + MN_SCROLLBAR_PADDING;

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

	/*Com_Printf("\n\n\nEXTRADATA(node).super.fullSizeY: %i \n", EXTRADATA(node).super.fullSizeY);*/
	fullSizeY = 0;
	do {
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
		} else if (!strncmp(cur, TEXT_IMAGETAG, strlen(TEXT_IMAGETAG))) {
			const char *token;
			const image_t *image;
			int y1 = y;
			/* cut the image tag */
			cur += strlen(TEXT_IMAGETAG);
			token = Com_Parse((const char **)&cur);
			/** @todo fix scrolling images */
			if (fullSizeY > EXTRADATA(node).super.viewPosY)
				y1 += (fullSizeY - EXTRADATA(node).super.viewPosY) * node->u.text.lineHeight;
			/* don't draw images that would be out of visible area */
			if (y + height > y1 && fullSizeY >= EXTRADATA(node).super.viewPosY) {
				/** @todo (menu) we should scale the height here with font->height, too */
				image = MN_DrawNormImageByName(x1, y1, 0, 0, 0, 0, 0, 0, token);
				if (image)
					x1 += image->height;
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

		if (node->state && node->mousefx && fullSizeY == EXTRADATA(node).lineUnderMouse) {
			/* Highlight line if mousefx is true. */
			/** @todo what about multiline text that should be highlighted completely? */
			if (fullSizeY == EXTRADATA(node).textLineSelected && EXTRADATA(node).textLineSelected >= 0) {
				R_Color(colorSelectedHover);
			} else {
				R_Color(colorHover);
			}
		}

		/* tabulation, we assume all the tabs fit on a single line */
		do {
			int tabwidth;
			int numtabs;

			tab = strchr(cur, '\t');
			if (!tab)
				break;

			/* use tab stop as given via menu definition format string
			 * or use 1/3 of the node size (width) */
			if (!node->u.text.tabWidth)
				tabwidth = width / 3;
			else
				tabwidth = node->u.text.tabWidth;

			numtabs = strspn(tab, "\t");
			tabwidth *= numtabs;
			while (*tab == '\t')
				*tab++ = '\0';

			/*Com_Printf("tab - first part - lines: %i \n", lines);*/
			MN_DrawString(font, node->textalign, x1, y, x, y, tabwidth - 1, height, node->u.text.lineHeight, cur, viewSizeY, EXTRADATA(node).super.viewPosY, &fullSizeY, qfalse, LONGLINES_PRETTYCHOP);
			x1 += tabwidth;
			/* now skip to the first char after the \t */
			cur = tab;
		} while (1);

		/*Com_Printf("until newline - lines: %i\n", lines);*/
		/* the conditional expression at the end is a hack to draw "/n/n" as a blank line */
		/* prevent line from being drawn if there is nothing that should be drawn after it */
		if (cur && (cur[0] || end || list)) {
			/* is it a white line? */
			if (!cur) {
				fullSizeY++;
			} else {
				MN_DrawString(font, node->textalign, x1, y, x, y, width, height, node->u.text.lineHeight, cur, viewSizeY, EXTRADATA(node).super.viewPosY, &fullSizeY, qtrue, EXTRADATA(node).longlines);
			}
		}

		if (node->mousefx)
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
	MN_DrawScrollBar(node);
}

/**
 * @brief Draws the TEXT_MESSAGESYSTEM node
 * @param[in] font Font string to use
 * @param[in] node The current menu node
 * @param[in] x The fixed x position every new line starts
 * @param[in] y The fixed y position the text node starts
 * @note For efficiency, scrolling is based on the count of messages
 * rather than a count of linewrapped lines. The result is that
 * scrolling of the message window scrolls message by message,
 * which looks better anyway.
 * @todo Campaign mode only function
 */
static void MN_TextNodeDrawMessageList (menuNode_t *node, message_t *messageStack)
{
	message_t *message;
	int screenLines;
	const char *font = MN_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width, height;
	int defaultHeight;
	int lineNumber = 0;
	int posY;

/* #define AUTOSCROLL */		/**< if newer messages are on top, autoscroll is not need */
#ifdef AUTOSCROLL
	qboolean autoscroll;
#endif
	MN_GetNodeAbsPos(node, pos);

	if (node->u.text.lineHeight == 0)
		defaultHeight = MN_FontGetHeight(font);
	else
		defaultHeight = node->u.text.lineHeight;

#ifdef AUTOSCROLL
	autoscroll = (EXTRADATA(node).super.viewPosY + EXTRADATA(node).super.viewSizeY == EXTRADATA(node).super.fullSizeY)
		|| (EXTRADATA(node).super.fullSizeY < EXTRADATA(node).super.viewSizeY);
#endif

	/* update message cache */
	if (MN_AbstractScrollableNodeIsSizeChange(node)) {
		/* recompute all line size */
		message = messageStack;
		while (message) {
			const char* text = va("%s%s", message->timestamp, message->text);
			R_FontTextSize(font, text, node->size[0], EXTRADATA(node).longlines, NULL, NULL, &message->lineUsed, NULL);
			lineNumber += message->lineUsed;
			message = message->next;
		}
	} else {
		/* only check unvalidated messages */
		message = messageStack;
		while (message) {
			if (message->lineUsed == 0) {
				const char* text = va("%s%s", message->timestamp, message->text);
				R_FontTextSize(font, text, node->size[0], EXTRADATA(node).longlines, NULL, NULL, &message->lineUsed, NULL);
			}
			lineNumber += message->lineUsed;
			message = message->next;
		}
	}

	/* update scroll status */
#ifdef AUTOSCROLL
	if (autoscroll)
		MN_AbstractScrollableNodeSetY(node, lineNumber, node->size[1] / defaultHeight, lineNumber);
	else
		MN_AbstractScrollableNodeSetY(node, -1, node->size[1] / defaultHeight, lineNumber);
#else
	MN_AbstractScrollableNodeSetY(node, -1, node->size[1] / defaultHeight, lineNumber);
#endif

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;
	height = node->size[1] - node->padding - node->padding;

	/* scrollbar space */
	if (EXTRADATA(node).scrollbar)
		width -= MN_SCROLLBAR_WIDTH + MN_SCROLLBAR_PADDING;

	/* found the first message we must display */
	message = messageStack;
	posY = EXTRADATA(node).super.viewPosY;
	while (message && posY > 0) {
		posY -= message->lineUsed;
		if (posY < 0)
			break;
		message = message->next;
	}

	/* draw */
	/** @note posY can be negative (if we must display last line of the first message) */
	screenLines = posY;
	while (message) {
		const char* text = va("%s%s", message->timestamp, message->text);
		MN_DrawString(font, node->textalign, x, y, x, y, width, height, node->u.text.lineHeight, text, EXTRADATA(node).super.viewSizeY, 0, &screenLines, qtrue, EXTRADATA(node).longlines);
		if (screenLines - EXTRADATA(node).super.viewPosY >= EXTRADATA(node).super.viewSizeY)
			break;
		message = message->next;
	}

	if (EXTRADATA(node).scrollbar && EXTRADATA(node).super.viewSizeY)
		MN_DrawScrollBar(node);
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
		MN_TextNodeDrawText(node, t, NULL);
		return;
	}

	/** @todo Very special case, merge it with shared type, or split it into another node */
	if (EXTRADATA(node).dataID == TEXT_MESSAGESYSTEM) {
		MN_TextNodeDrawMessageList(node, cp_messageStack);
		return;
	}

	shared = &mn.sharedData[EXTRADATA(node).dataID];

	switch (shared->type) {
	case MN_SHARED_TEXT:
		{
			const char* t = shared->data.text;
			if (t[0] == '_')
				t = _(++t);
			MN_TextNodeDrawText(node, t, NULL);
		}
		break;
	case MN_SHARED_LINKEDLISTTEXT:
		MN_TextNodeDrawText(node, NULL, shared->data.linkedListText);
		break;
	default:
		break;
	}
}

/**
 * @brief Calls the script command for a text node that is clickable
 * @sa MN_TextNodeRightClick
 */
static void MN_TextNodeClick (menuNode_t * node, int x, int y)
{
	int line = MN_TextNodeGetLine(node, x, y);

	if (line < 0 || line >= EXTRADATA(node).super.fullSizeY)
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

	if (line < 0 || line >= EXTRADATA(node).super.fullSizeY)
		return;

	MN_TextNodeSelectLine(node, line);

	if (node->onRightClick)
		MN_ExecuteEventActions(node, node->onRightClick);
}

/**
 * @todo we should anyway scroll the text (if its possible)
 */
static void MN_TextNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	if (node->onWheelUp && node->onWheelDown) {
		MN_ExecuteEventActions(node, (down ? node->onWheelDown : node->onWheelUp));
	} else {
		MN_AbstractScrollableNodeScrollY(node, (down ? 1 : -1));
		/* they can also have script commands assigned */
		MN_ExecuteEventActions(node, node->onWheel);
	}
}

static void MN_TextNodeLoading (menuNode_t *node)
{
	EXTRADATA(node).textLineSelected = -1; /**< Invalid/no line selected per default. */
	Vector4Set(node->selectedColor, 1.0, 1.0, 1.0, 1.0);
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void MN_TextNodeLoaded (menuNode_t *node)
{
	int lineheight = node->u.text.lineHeight;
	/* auto compute lineheight */
	/* we don't overwrite node->u.text.lineHeight, because "0" is dynamically replaced by font height on draw function */
	if (lineheight == 0) {
		/* the font is used */
		const char *font = MN_GetFontFromNode(node);
		lineheight = MN_FontGetHeight(font) / 2;
	}

	/* auto compute rows (super.viewSizeY) */
	if (EXTRADATA(node).super.viewSizeY == 0) {
		if (node->size[1] != 0 && lineheight != 0) {
			EXTRADATA(node).super.viewSizeY = node->size[1] / lineheight;
		} else {
			EXTRADATA(node).super.viewSizeY = 1;
			Com_Printf("MN_TextNodeLoaded: node '%s' has no rows value\n", MN_GetPath(node));
		}
	}

	/* auto compute height */
	if (node->size[1] == 0) {
		node->size[1] = EXTRADATA(node).super.viewSizeY * lineheight;
	}

	/* is text slot exists */
	if (EXTRADATA(node).dataID >= MAX_MENUTEXTS)
		Com_Error(ERR_DROP, "Error in node %s - max menu num exceeded (num: %i, max: %i)", MN_GetPath(node), EXTRADATA(node).dataID, MAX_MENUTEXTS);

#ifdef DEBUG
	if (EXTRADATA(node).super.viewSizeY != (int)(node->size[1] / lineheight)) {
		Com_Printf("MN_TextNodeLoaded: rows value (%i) of node '%s' differs from size (%.0f) and format (%i) values\n",
			EXTRADATA(node).super.viewSizeY, MN_GetPath(node), node->size[1], lineheight);
	}
#endif

	if (node->text == NULL && EXTRADATA(node).dataID == TEXT_NULL)
		Com_Printf("MN_TextNodeLoaded: 'textid' property of node '%s' is not set\n", MN_GetPath(node));
}

static const value_t properties[] = {
	{"scrollbar", V_BOOL, offsetof(menuNode_t, u.text.scrollbar), MEMBER_SIZEOF(menuNode_t, u.text.scrollbar)},
	{"lineselected", V_INT, offsetof(menuNode_t, u.text.textLineSelected), MEMBER_SIZEOF(menuNode_t, u.text.textLineSelected)},
	{"dataid", V_UI_DATAID, offsetof(menuNode_t, u.text.dataID), MEMBER_SIZEOF(menuNode_t, u.text.dataID)},
	{"lineheight", V_INT, offsetof(menuNode_t, u.text.lineHeight), MEMBER_SIZEOF(menuNode_t, u.text.lineHeight)},
	{"tabwidth", V_INT, offsetof(menuNode_t, u.text.tabWidth), MEMBER_SIZEOF(menuNode_t, u.text.tabWidth)},
	{"longlines", V_LONGLINES, offsetof(menuNode_t, u.text.longlines), MEMBER_SIZEOF(menuNode_t, u.text.longlines)},

	/* translate text properties into the scrollable data; for a smoth scroll we should split that */
	{"rows", V_INT, offsetof(menuNode_t, u.text.super.viewSizeY), MEMBER_SIZEOF(menuNode_t, u.text.super.viewSizeY)},
	{"lines", V_INT, offsetof(menuNode_t, u.text.super.fullSizeY), MEMBER_SIZEOF(menuNode_t, u.text.super.fullSizeY)},

	/** @todo delete it went its possible (need to create a textlist) */
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
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

	/** @todo we should not need anymore this function */
	Cmd_AddCommand("mn_textscroll", MN_TextScroll_f, NULL);
}
