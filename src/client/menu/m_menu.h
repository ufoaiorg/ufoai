/**
 * @file m_menu.h
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

#ifndef CLIENT_MENU_M_MENU_H
#define CLIENT_MENU_M_MENU_H

#include "../../game/q_shared.h"

/** @brief menu with all it's nodes linked in */
typedef struct menu_s {
	char name[MAX_VAR];
	int eventTime;
	vec2_t pos;	/**< the position of the menu */
	vec2_t size;	/**< the size of the menu */
	vec2_t noticePos; /**< the position where the cl.msgText messages are rendered */
	struct menuNode_s *firstNode, *initNode, *closeNode, *renderNode;
	struct menuNode_s *popupNode, *hoverNode, *eventNode, *leaveNode;
} menu_t;

menu_t* MN_PushMenu(const char *name);
void MN_PopMenu(qboolean all);
menu_t* MN_GetActiveMenu(void);
menu_t *MN_GetMenu(const char *name);

int MN_CompletePushMenu(const char *partial, const char **match);


#endif
