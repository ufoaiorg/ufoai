/**
 * @file ui_node_messagelist.c
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
#include "../ui_render.h"
#include "../ui_sprite.h"
#include "ui_node_text.h"
#include "ui_node_messagelist.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../cgame/campaign/cp_messages.h" /**< message_t */
#include "../../../shared/parse.h"

#define EXTRADATA(node) UI_EXTRADATA(node, abstractScrollableExtraData_t)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, abstractScrollableExtraData_t)

/** @todo use the font height? */
static const int LINEHEIGHT	= 20;

static const int DATETIME_COLUUI_SIZE = 180;

/* Used for drag&drop-like scrolling */
static int mouseScrollX;
static int mouseScrollY;

/**
 * @return Number of lines need to display this message
 */
static int UI_MessageGetLines (const uiNode_t *node, message_t *message, const char *fontID, int width)
{
	const int column1 = DATETIME_COLUUI_SIZE;
	const int column2 = width - DATETIME_COLUUI_SIZE - node->padding;
	int lines1;
	int lines2;
	R_FontTextSize(fontID, message->timestamp, column1, LONGLINES_WRAP, NULL, NULL, &lines1, NULL);
	R_FontTextSize(fontID, message->text, column2, LONGLINES_WRAP, NULL, NULL, &lines2, NULL);
	return max(lines1, lines2);
}

static char *lastDate;

/**
 * @todo do not hard code icons
 * @todo cache icon result
 */
static uiSprite_t *UI_MessageGetIcon (const message_t *message)
{
	const char* iconName;

	switch (message->type) {
	case MSG_DEBUG:			/**< only save them in debug mode */
		iconName = "icons/message_debug";
		break;
	case MSG_INFO:			/**< don't save these messages */
		iconName = "icons/message_info";
		break;
	case MSG_STANDARD:
		iconName = "icons/message_info";
		break;
	case MSG_RESEARCH_PROPOSAL:
		iconName = "icons/message_research";
		break;
	case MSG_RESEARCH_HALTED:
		iconName = "icons/message_research";
		break;
	case MSG_RESEARCH_FINISHED:
		iconName = "icons/message_research";
		break;
	case MSG_CONSTRUCTION:
		iconName = "icons/message_construction";
		break;
	case MSG_UFOSPOTTED:
		iconName = "icons/message_ufo";
		break;
	case MSG_TERRORSITE:
		iconName = "icons/message_ufo";
		break;
	case MSG_BASEATTACK:
		iconName = "icons/message_ufo";
		break;
	case MSG_TRANSFERFINISHED:
		iconName = "icons/message_transfer";
		break;
	case MSG_PROMOTION:
		iconName = "icons/message_promotion";
		break;
	case MSG_PRODUCTION:
		iconName = "icons/message_production";
		break;
	case MSG_NEWS:
		iconName = "icons/message_info";
		break;
	case MSG_DEATH:
		iconName = "icons/message_death";
		break;
	case MSG_CRASHSITE:
		iconName = "icons/message_ufo";
		break;
	case MSG_EVENT:
		iconName = "icons/message_info";
		break;
	default:
		iconName = "icons/message_info";
		break;
	}

	return UI_GetSpriteByName(iconName);
}

static void UI_MessageDraw (const uiNode_t *node, message_t *message, const char *fontID, int x, int y, int width, int *screenLines)
{
	const int column1 = DATETIME_COLUUI_SIZE;
	const int column2 = width - DATETIME_COLUUI_SIZE - node->padding;
	int lines1 = *screenLines;
	int lines2 = *screenLines;

	/* also display the first date on wraped message we only see the end */
	if (lines1 < 0)
		lines1 = 0;

	/* display the date */
	if (lastDate == NULL || !Q_streq(lastDate, message->timestamp)) {
		R_Color(node->color);
		UI_DrawString(fontID, ALIGN_UL, x, y, x, column1, LINEHEIGHT, message->timestamp, EXTRADATACONST(node).scrollY.viewSize, 0, &lines1, qtrue, LONGLINES_WRAP);
		R_Color(NULL);
	}

	x += DATETIME_COLUUI_SIZE + node->padding;

	/* identify the begin of a message with a mark */
	if (lines2 >= 0) {
		const uiSprite_t *icon = UI_MessageGetIcon(message);
		R_Color(NULL);
		UI_DrawSpriteInBox(qfalse, icon, SPRITE_STATUS_NORMAL, x - 25, y + LINEHEIGHT * lines2 - 1, 19, 19);
	}

	/* draw the message */
	R_Color(node->color);
	UI_DrawString(fontID, ALIGN_UL, x, y, x, column2, LINEHEIGHT, message->text, EXTRADATACONST(node).scrollY.viewSize, 0, &lines2, qtrue, LONGLINES_WRAP);
	R_Color(NULL);
	*screenLines = max(lines1, lines2);
	lastDate = message->timestamp;
}

/**
 * @brief Draws the messagesystem node
 * @param[in] node The context node
 */
static void UI_MessageListNodeDraw (uiNode_t *node)
{
	message_t *message;
	int screenLines;
	const char *font = UI_GetFontFromNode(node);
	vec2_t pos;
	int x, y, width;
	int defaultHeight;
	int lineNumber = 0;
	int posY;

/* #define AUTOSCROLL */		/**< if newer messages are on top, autoscroll is not need */
#ifdef AUTOSCROLL
	qboolean autoscroll;
#endif
	UI_GetNodeAbsPos(node, pos);

	defaultHeight = LINEHEIGHT;

#ifdef AUTOSCROLL
	autoscroll = (EXTRADATA(node).scrollY.viewPos + EXTRADATA(node).scrollY.viewSize == EXTRADATA(node).scrollY.fullSize)
		|| (EXTRADATA(node).scrollY.fullSize < EXTRADATA(node).scrollY.viewSize);
#endif

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;

	/* update message cache */
	if (UI_AbstractScrollableNodeIsSizeChange(node)) {
		/* recompute all line size */
		message = cp_messageStack;
		while (message) {
			message->lineUsed = UI_MessageGetLines(node, message, font, width);
			lineNumber += message->lineUsed;
			message = message->next;
		}
	} else {
		/* only check unvalidated messages */
		message = cp_messageStack;
		while (message) {
			if (message->lineUsed == 0)
				message->lineUsed = UI_MessageGetLines(node, message, font, width);
			lineNumber += message->lineUsed;
			message = message->next;
		}
	}

	/* update scroll status */
#ifdef AUTOSCROLL
	if (autoscroll)
		UI_AbstractScrollableNodeSetY(node, lineNumber, node->size[1] / defaultHeight, lineNumber);
	else
		UI_AbstractScrollableNodeSetY(node, -1, node->size[1] / defaultHeight, lineNumber);
#else
	UI_AbstractScrollableNodeSetY(node, -1, node->size[1] / defaultHeight, lineNumber);
#endif

	/* found the first message we must display */
	message = cp_messageStack;
	posY = EXTRADATA(node).scrollY.viewPos;
	while (message && posY > 0) {
		posY -= message->lineUsed;
		if (posY < 0)
			break;
		message = message->next;
	}

	/* draw */
	/** @note posY can be negative (if we must display last line of the first message) */
	lastDate = NULL;
	screenLines = posY;
	while (message) {
		UI_MessageDraw(node, message, font, x, y, width, &screenLines);
		if (screenLines >= EXTRADATA(node).scrollY.viewSize)
			break;
		message = message->next;
	}
}

static qboolean UI_MessageListNodeMouseWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean down = deltaY > 0;
	qboolean updated;
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

#ifdef DEBUG
static void UI_MessageDebugUseAllIcons_f (void)
{
	message_t *message;
	int i = 0;
	message = cp_messageStack;
	while (message) {
		message->type = i % MSG_MAX;
		message = message->next;
		i++;
	}

}
#endif

static void UI_MessageListNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

/**
 * @brief Track mouse down/up events to implement drag&drop-like scrolling, for touchscreen devices
 * @sa UI_TextNodeMouseUp, UI_TextNodeCapturedMouseMove
*/
static void UI_MessageListNodeMouseDown (struct uiNode_s *node, int x, int y, int button)
{
	if (!UI_GetMouseCapture() && button == K_MOUSE1 &&
		EXTRADATA(node).scrollY.fullSize > EXTRADATA(node).scrollY.viewSize) {
		UI_SetMouseCapture(node);
		mouseScrollX = x;
		mouseScrollY = y;
	}
}

static void UI_MessageListNodeMouseUp (struct uiNode_s *node, int x, int y, int button)
{
	if (UI_GetMouseCapture() == node)  /* More checks can never hurt */
		UI_MouseRelease();
}

static void UI_MessageListNodeCapturedMouseMove (uiNode_t *node, int x, int y)
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
static int UI_MessageListNodeGetCellHeight (uiNode_t *node)
{
	return LINEHEIGHT;
}

void UI_RegisterMessageListNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "messagelist";
	behaviour->extends = "abstractscrollable";
	behaviour->draw = UI_MessageListNodeDraw;
	behaviour->loading = UI_MessageListNodeLoading;
	behaviour->scroll = UI_MessageListNodeMouseWheel;
	behaviour->mouseDown = UI_MessageListNodeMouseDown;
	behaviour->mouseUp = UI_MessageListNodeMouseUp;
	behaviour->capturedMouseMove = UI_MessageListNodeCapturedMouseMove;
	behaviour->getCellHeight = UI_MessageListNodeGetCellHeight;

#ifdef DEBUG
	Cmd_AddCommand("debug_ui_message_useallicons", UI_MessageDebugUseAllIcons_f, "Update message to use all icons");
#endif
}
