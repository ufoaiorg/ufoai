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

#define MAX_MENUTEXTLEN		32768
/* used to speed up buffer safe string copies */
#define MAX_SMALLMENUTEXTLEN	1024

/* bar and background have the same width */
#define MN_SCROLLBAR_WIDTH 10
/* actual height: node height * displayed lines / all lines * this multiplier, must be less than or equal one */
#define MN_SCROLLBAR_HEIGHT 1
/* space between text and scrollbar */
#define MN_SCROLLBAR_PADDING 10

/** @brief linked into mn.menuText - defined in menu scripts via num */
typedef enum {
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_UFOPEDIA,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH = 5,
	TEXT_RESEARCH_INFO,			/**< not used */
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT = 10,
	TEXT_AIRCRAFT_INFO,
	TEXT_MESSAGESYSTEM,			/**< just a dummy for messagesystem - we use the stack */
	TEXT_CAMPAIGN_LIST,
	TEXT_MULTISELECTION,
	TEXT_PRODUCTION_LIST = 15,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_EMPLOYEE,
	TEXT_MOUSECURSOR_RIGHT,
	TEXT_PRODUCTION_QUEUED = 20,
	TEXT_HOSPITAL,
	TEXT_STATS_1,
	TEXT_STATS_2,
	TEXT_STATS_3,
	TEXT_STATS_4 = 25,
	TEXT_STATS_5,
	TEXT_STATS_6,
	TEXT_STATS_7,
	TEXT_BASE_LIST,
	TEXT_BASE_INFO = 30,
	TEXT_TRANSFER_LIST,
	TEXT_MOUSECURSOR_PLAYERNAMES,
	TEXT_CARGO_LIST,
	TEXT_UFOPEDIA_MAILHEADER,
	TEXT_UFOPEDIA_MAIL = 35,
	TEXT_MARKET_NAMES,
	TEXT_MARKET_STORAGE,
	TEXT_MARKET_MARKET,
	TEXT_MARKET_PRICES,
	TEXT_CHAT_WINDOW = 40,
	TEXT_AIREQUIP_1,
	TEXT_AIREQUIP_2,
	TEXT_AIREQUIP_3,
	TEXT_BASEDEFENSE_LIST,
	TEXT_TIPOFTHEDAY = 45,
	TEXT_GENERIC,
	TEXT_XVI,

	MAX_MENUTEXTS
} menuTextIDs_t;

void MN_DrawTextNode(const char *text, const linkedList_t* list, const char* font, menuNode_t* node, int x, int y, int width, int height);
void MN_TextScrollBottom(const char* nodeName);
qboolean MN_TextScroll(menuNode_t *node, int offset);
void MN_MenuTextReset(int menuTextID);

void MN_NodeTextInit(void);

#endif
