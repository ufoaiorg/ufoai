/**
 * @file m_node_text.h
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

#ifndef CLIENT_MENU_M_NODE_TEXT_H
#define CLIENT_MENU_M_NODE_TEXT_H

#include "m_node_abstractscrollable.h"

/* used to speed up buffer safe string copies */
#define MAX_SMALLMENUTEXTLEN	1024

struct nodeBehaviour_s;
struct menuAction_s;

void MN_TextScrollBottom(const char* nodePath);
void MN_TextNodeSelectLine(struct menuNode_s *node, int num);

void MN_RegisterTextNode(struct nodeBehaviour_s *behaviour);

typedef struct {
	abstractScrollableExtraData_t super;

	int dataID;					/**< ID of a shared data @sa src/client/menu/m_data.h */

	int textLineSelected;		/**< Which line is currenlty selected? This counts only visible lines). Add textScroll to this value to get total linecount. @sa selectedColor below.*/
	int lineUnderMouse;			/**< MN_TEXT: The line under the mouse, when the mouse is over the node */
	int lineHeight;				/**< size between two lines */
	int tabWidth;				/**< max size of a tabulation */
	byte longlines;				/**< what to do with long lines */

} textExtraData_t;

/**
 * @note text node inherite scrollable node. Scrollable (super) extradata
 * must not move, else we can't call scrollable functions.
 */
CASSERT(offsetof(textExtraData_t, super) == 0);

#endif
