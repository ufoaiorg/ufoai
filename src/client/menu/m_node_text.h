/**
 * @file m_node_text.h
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

#ifndef CLIENT_MENU_M_NODE_TEXT_H
#define CLIENT_MENU_M_NODE_TEXT_H

#include "../../common/common.h"

#include "m_nodes.h"
#include "m_messages.h"

#define MAX_MENUTEXTLEN		32768
/* used to speed up buffer safe string copies */
#define MAX_SMALLMENUTEXTLEN	1024

/* bar and background have the same width */
#define MN_SCROLLBAR_WIDTH 10
/* actual height: node height * displayed lines / all lines * this multiplier, must be less than or equal one */
#define MN_SCROLLBAR_HEIGHT 1
/* space between text and scrollbar */
#define MN_SCROLLBAR_PADDING 10

void MN_DrawTextNode(const char *text, const linkedList_t *list, const char *font, menuNode_t *node, int x, int y, int width, int height);
void MN_DrawMessageList(const message_t *messageStack, const char *font, menuNode_t *node, int x, int y, int width, int height);
void MN_TextScrollBottom(const char* nodeName);
qboolean MN_TextScroll(menuNode_t *node, int offset);
void MN_MenuTextReset(int menuTextID);

void MN_NodeTextInit(void);

#endif
