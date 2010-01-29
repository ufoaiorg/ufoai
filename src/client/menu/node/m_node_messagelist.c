/**
 * @file m_node_messagelist.c
 * @todo add getter/setter to cleanup access to extradata from cl_*.c files (check "u.text.")
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
#include "../m_internal.h"
#include "../m_font.h"
#include "../m_actions.h"
#include "../m_parse.h"
#include "../m_render.h"
#include "../m_icon.h"
#include "m_node_text.h"
#include "m_node_messagelist.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../campaign/cp_messages.h" /**< message_t */
#include "../../../shared/parse.h"

#define EXTRADATA(node) (node->u.abstractscrollable)

/** @todo use the font height? */
static const int LINEHEIGHT	= 20;

static const int DATETIME_COLUMN_SIZE = 180;

/**
 * @return Number of lines need to display this message
 */
static int MN_MessageGetLines (const menuNode_t *node, message_t *message, const char *fontID, int width)
{
	const int column1 = DATETIME_COLUMN_SIZE;
	const int column2 = width - DATETIME_COLUMN_SIZE - node->padding;
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
static menuIcon_t *MN_MessageGetIcon (const message_t *message)
{
	const char* iconName;

	switch (message->type) {
	case MSG_DEBUG:			/**< only save them in debug mode */
		iconName = "message_debug";
		break;
	case MSG_INFO:			/**< don't save these messages */
		iconName = "message_info";
		break;
	case MSG_STANDARD:
		iconName = "message_info";
		break;
	case MSG_RESEARCH_PROPOSAL:
		iconName = "message_research";
		break;
	case MSG_RESEARCH_HALTED:
		iconName = "message_research";
		break;
	case MSG_RESEARCH_FINISHED:
		iconName = "message_research";
		break;
	case MSG_CONSTRUCTION:
		iconName = "message_construction";
		break;
	case MSG_UFOSPOTTED:
		iconName = "message_ufo";
		break;
	case MSG_TERRORSITE:
		iconName = "message_ufo";
		break;
	case MSG_BASEATTACK:
		iconName = "message_ufo";
		break;
	case MSG_TRANSFERFINISHED:
		iconName = "message_transfer";
		break;
	case MSG_PROMOTION:
		iconName = "message_promotion";
		break;
	case MSG_PRODUCTION:
		iconName = "message_production";
		break;
	case MSG_NEWS:
		iconName = "message_info";
		break;
	case MSG_DEATH:
		iconName = "message_death";
		break;
	case MSG_CRASHSITE:
		iconName = "message_ufo";
		break;
	case MSG_EVENT:
		iconName = "message_info";
		break;
	default:
		iconName = "message_info";
		break;
	}

	return MN_GetIconByName(iconName);
}

static void MN_MessageDraw (const menuNode_t *node, message_t *message, const char *fontID, int x, int y, int width, int height, int *screenLines)
{
	const int column1 = DATETIME_COLUMN_SIZE;
	const int column2 = width - DATETIME_COLUMN_SIZE - node->padding;
	int lines1 = *screenLines;
	int lines2 = *screenLines;

	/* also display the first date on wraped message we only see the end */
	if (lines1 < 0)
		lines1 = 0;

	/* display the date */
	if (lastDate == NULL || strcmp(lastDate, message->timestamp) != 0)
		MN_DrawString(fontID, ALIGN_UL, x, y, x, y, column1, height, LINEHEIGHT, message->timestamp, EXTRADATA(node).scrollY.viewSize, 0, &lines1, qtrue, LONGLINES_WRAP);

	x += DATETIME_COLUMN_SIZE + node->padding;

	/* identify the begin of a message with a mark */
	if (lines2 >= 0) {
		const menuIcon_t *icon;
		icon = MN_MessageGetIcon(message);
		MN_DrawIconInBox(icon, ICON_STATUS_NORMAL, x - 25, y + LINEHEIGHT * lines2 - 1, 19, 19);
	}

	/* draw the message */
	MN_DrawString(fontID, ALIGN_UL, x, y, x, y, column2, height, LINEHEIGHT, message->text, EXTRADATA(node).scrollY.viewSize, 0, &lines2, qtrue, LONGLINES_WRAP);
	*screenLines = max(lines1, lines2);
	lastDate = message->timestamp;
}

/**
 * @brief Draws the messagesystem node
 * @param[in] node The context menu node
 */
static void MN_MessageListNodeDraw (menuNode_t *node)
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

	defaultHeight = LINEHEIGHT;

#ifdef AUTOSCROLL
	autoscroll = (EXTRADATA(node).scrollY.viewPos + EXTRADATA(node).scrollY.viewSize == EXTRADATA(node).scrollY.fullSize)
		|| (EXTRADATA(node).scrollY.fullSize < EXTRADATA(node).scrollY.viewSize);
#endif

	/* text box */
	x = pos[0] + node->padding;
	y = pos[1] + node->padding;
	width = node->size[0] - node->padding - node->padding;
	height = node->size[1] - node->padding - node->padding;

	/* update message cache */
	if (MN_AbstractScrollableNodeIsSizeChange(node)) {
		/* recompute all line size */
		message = cp_messageStack;
		while (message) {
			message->lineUsed = MN_MessageGetLines(node, message, font, width);
			lineNumber += message->lineUsed;
			message = message->next;
		}
	} else {
		/* only check unvalidated messages */
		message = cp_messageStack;
		while (message) {
			if (message->lineUsed == 0)
				message->lineUsed = MN_MessageGetLines(node, message, font, width);
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
		MN_MessageDraw(node, message, font, x, y, width, height, &screenLines);
		if (screenLines >= EXTRADATA(node).scrollY.viewSize)
			break;
		message = message->next;
	}
}

static void MN_MessageListNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	MN_AbstractScrollableNodeScrollY(node, (down ? 1 : -1));
	if (node->onWheelUp && !down)
		MN_ExecuteEventActions(node, node->onWheelUp);
	if (node->onWheelDown && down)
		MN_ExecuteEventActions(node, node->onWheelDown);
	if (node->onWheel)
		MN_ExecuteEventActions(node, node->onWheel);
}

#ifdef DEBUG
static void MN_MessageDebugUseAllIcons_f (void)
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

void MN_RegisterMessageListNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "messagelist";
	behaviour->extends = "abstractscrollable";
	behaviour->draw = MN_MessageListNodeDraw;
	behaviour->mouseWheel = MN_MessageListNodeMouseWheel;
#ifdef DEBUG
	Cmd_AddCommand("debug_mn_message_useallicons", MN_MessageDebugUseAllIcons_f, "Update message to use all icons");
#endif
}
