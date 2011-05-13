/**
 * @file ui_popup.h
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

#ifndef CLIENT_UI_UI_POPUP_H
#define CLIENT_UI_UI_POPUP_H

#include "node/ui_node_text.h"

#define POPUPLIST_WINDOW_NAME "popup_list"
#define POPUPLIST_NODE_NAME "popup_list"

/** @todo we should delete it, UI script support random text size */
/* used to speed up buffer safe string copies */
#define UI_MAX_SMALLTEXTLEN	1024

extern char popupText[UI_MAX_SMALLTEXTLEN];

void UI_Popup(const char *title, const char *text);
struct uiNode_s *UI_PopupList(const char *title, const char *headline, linkedList_t* entries, const char *clickAction);
void UI_PopupButton(const char *title, const char *text,
	const char *clickAction1, const char *clickText1, const char *tooltip1,
	const char *clickAction2, const char *clickText2, const char *tooltip2,
	const char *clickAction3, const char *clickText3, const char *tooltip3);

#endif
