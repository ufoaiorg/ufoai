/**
 * @file m_popup.c
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

#include "../client.h"
#include "m_main.h"
#include "m_popup.h"

#define POPUPLIST_MENU_NAME "popup_list"
#define POPUPLIST_NODE_NAME "popup_list"
#define POPUP_MENU_NAME "popup"

char popupText[MAX_SMALLMENUTEXTLEN];

/**
 * @brief Popup in geoscape
 * @note Only use static strings here - or use popupText if you really have to
 * build the string
 */
void MN_Popup (const char *title, const char *text)
{
	mn.menuText[TEXT_POPUP] = title;
	mn.menuText[TEXT_POPUP_INFO] = text;
	if (ccs.singleplayer)
		CL_GameTimeStop();
	MN_PushMenu(POPUP_MENU_NAME);
}

menuNode_t *MN_PopupList (const char *title, const char *headline, linkedList_t* entries, const char *clickAction)
{
	menu_t* popupListMenu;
	menuNode_t* listNode;

	mn.menuText[TEXT_POPUP] = title;
	mn.menuText[TEXT_POPUP_INFO] = headline;
	
	/* make sure, that we are using the linked list */
	MN_MenuTextReset(TEXT_LIST);
	mn.menuTextLinkedList[TEXT_LIST] = entries;
	if (ccs.singleplayer)
		CL_GameTimeStop();

	popupListMenu = MN_GetMenu(POPUPLIST_MENU_NAME);
	if (!popupListMenu)
		Sys_Error("Could not get "POPUPLIST_MENU_NAME" menu");
	listNode = MN_GetNode(popupListMenu, POPUPLIST_NODE_NAME);
	if (!listNode)
		Sys_Error("Could not get "POPUPLIST_NODE_NAME" node in "POPUPLIST_MENU_NAME" menu");

	/* free previous actions */
	if (listNode->click) {
		assert(listNode->click->data);
		Mem_Free(listNode->click->data);
		Mem_Free(listNode->click);
		listNode->click = NULL;
	}

	if (clickAction) {
		listNode->mousefx = qtrue;
		listNode->click = MN_SetMenuAction(&listNode->click, EA_CMD, clickAction);
	} else {
		listNode->mousefx = qfalse;
		listNode->click = NULL;
	}

	MN_PushMenu(popupListMenu->name);
	return listNode;	
}
