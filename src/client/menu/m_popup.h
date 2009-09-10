/**
 * @file m_popup.h
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

#ifndef CLIENT_MENU_M_POPUP_H
#define CLIENT_MENU_M_POPUP_H

#include "node/m_node_text.h"

struct menuNode_s;	/* prototype */

#define POPUPLIST_MENU_NAME "popup_list"
#define POPUPLIST_NODE_NAME "popup_list"

extern char popupText[MAX_SMALLMENUTEXTLEN];

void MN_Popup(const char *title, const char *text);
struct menuNode_s *MN_PopupList(const char *title, const char *headline, linkedList_t* entries, const char *clickAction);
void MN_PopupButton(const char *title, const char *text,
	const char *clickAction1, const char *clickText1, const char *tooltip1,
	const char *clickAction2, const char *clickText2, const char *tooltip2,
	const char *clickAction3, const char *clickText3, const char *tooltip3);

#endif
