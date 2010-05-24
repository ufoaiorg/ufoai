/**
 * @file m_popup.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "m_main.h"
#include "m_nodes.h"
#include "m_popup.h"
#include "m_actions.h"
#include "node/m_node_abstractnode.h"

#define POPUPBUTTON_MENU_NAME "popup_button"
#define POPUPBUTTON_NODE_NAME "popup_button_"
#define POPUP_MENU_NAME "popup"

/** @brief strings to be used for popup when text is not static */
char popupText[MAX_SMALLMENUTEXTLEN];
static char popupAction1[MAX_SMALLMENUTEXTLEN];
static char popupAction2[MAX_SMALLMENUTEXTLEN];
static char popupAction3[MAX_SMALLMENUTEXTLEN];

/**
 * @brief Popup on geoscape
 * @note Only use static strings here - or use popupText if you really have to
 * build the string
 */
void MN_Popup (const char *title, const char *text)
{
	Cvar_Set("mn_sys_popup_title", title);
	MN_RegisterText(TEXT_POPUP_INFO, text);
	if (!MN_IsWindowOnStack(POPUP_MENU_NAME))
		MN_PushWindow(POPUP_MENU_NAME, NULL);
}

/**
 * @brief Generates a popup that contains a list of selectable choices.
 * @param[in] title Title of the popup.
 * @param[in] headline First line of text written in the popup.
 * @param[in] entries List of the selectables choices.
 * @param[in] clickAction Action to perform when one clicked on the popup.
 */
menuNode_t *MN_PopupList (const char *title, const char *headline, linkedList_t* entries, const char *clickAction)
{
	menuNode_t* popupListMenu;
	menuNode_t* listNode;

	Cvar_Set("mn_sys_popup_title", title);
	MN_RegisterText(TEXT_POPUP_INFO, headline);

	/* make sure, that we are using the linked list */
	MN_ResetData(TEXT_LIST);
	MN_RegisterLinkedListText(TEXT_LIST, entries);

	popupListMenu = MN_GetWindow(POPUPLIST_MENU_NAME);
	if (!popupListMenu)
		Com_Error(ERR_FATAL, "Could not get "POPUPLIST_MENU_NAME" menu");
	listNode = MN_GetNode(popupListMenu, POPUPLIST_NODE_NAME);
	if (!listNode)
		Com_Error(ERR_FATAL, "Could not get "POPUPLIST_NODE_NAME" node in "POPUPLIST_MENU_NAME" menu");

	/* free previous actions */
	if (listNode->onClick) {
		assert(listNode->onClick->d.terminal.d1.data);
		Mem_Free(listNode->onClick->d.terminal.d1.data);
		Mem_Free(listNode->onClick);
		listNode->onClick = NULL;
	}

	if (clickAction) {
		MN_PoolAllocAction(&listNode->onClick, EA_CMD, clickAction);
	} else {
		listNode->onClick = NULL;
	}

	if (!MN_IsWindowOnStack(popupListMenu->name))
		MN_PushWindow(popupListMenu->name, NULL);
	return listNode;
}

/**
 * @brief Set string and action button.
 * @param[in] menu menu where buttons are modified.
 * @param[in] button Name of the node of the button.
 * @param[in] clickAction Action to perform when button is clicked.
 * @note clickAction may be NULL if button is not needed.
 */
static void MN_SetOneButton (menuNode_t* menu, const char *button, const char *clickAction)
{
	menuNode_t* buttonNode;

	buttonNode = MN_GetNode(menu, button);
	if (!buttonNode)
		Com_Error(ERR_FATAL, "Could not get %s node in %s menu", button, menu->name);

	/* free previous actions */
	if (buttonNode->onClick) {
		assert(buttonNode->onClick->d.terminal.d1.data);
		Mem_Free(buttonNode->onClick->d.terminal.d1.data);
		Mem_Free(buttonNode->onClick);
		buttonNode->onClick = NULL;
	}

	if (clickAction) {
		MN_PoolAllocAction(&buttonNode->onClick, EA_CMD, clickAction);
		buttonNode->invis = qfalse;
	} else {
		buttonNode->onClick = NULL;
		buttonNode->invis = qtrue;
	}
}

/**
 * @brief Generates a popup that contains up to 3 buttons.
 * @param[in] title Title of the popup.
 * @param[in] text Text to display in the popup (use popupText if text is NULL).
 * @param[in] clickAction1 Action to perform when one clicked on the first button.
 * @param[in] clickText1 String that will be written in first button.
 * @param[in] tooltip1 Tooltip of first button.
 * @param[in] clickAction2 Action to perform when one clicked on the second button.
 * @param[in] clickText2 String that will be written in second button.
 * @param[in] tooltip2 Tooltip of second button.
 * @param[in] clickAction3 Action to perform when one clicked on the second button.
 * @param[in] clickText3 String that will be written in second button.
 * @param[in] tooltip3 Tooltip of second button.
 * @note clickAction AND clickText must be NULL if button should be invisible.
 */
void MN_PopupButton (const char *title, const char *text,
	const char *clickAction1, const char *clickText1, const char *tooltip1,
	const char *clickAction2, const char *clickText2, const char *tooltip2,
	const char *clickAction3, const char *clickText3, const char *tooltip3)
{
	menuNode_t* popupButtonMenu;

	Cvar_Set("mn_sys_popup_title", title);
	if (text)
		MN_RegisterText(TEXT_POPUP_INFO, text);
	else
		MN_RegisterText(TEXT_POPUP_INFO, popupText);

	popupButtonMenu = MN_GetWindow(POPUPBUTTON_MENU_NAME);
	if (!popupButtonMenu)
		Com_Error(ERR_FATAL, "Could not get "POPUPBUTTON_MENU_NAME" menu");

	Cvar_Set("mn_sys_popup_button_text1", clickText1);
	Cvar_Set("mn_sys_popup_button_tooltip1", tooltip1);
	if (!clickAction1 && !clickText1) {
		MN_SetOneButton(popupButtonMenu, va("%s1", POPUPBUTTON_NODE_NAME),
			NULL);
	} else {
		MN_SetOneButton(popupButtonMenu, va("%s1", POPUPBUTTON_NODE_NAME),
			clickAction1 ? clickAction1 : popupAction1);
	}

	Cvar_Set("mn_sys_popup_button_text2", clickText2);
	Cvar_Set("mn_sys_popup_button_tooltip2", tooltip2);
	if (!clickAction2 && !clickText2) {
		MN_SetOneButton(popupButtonMenu, va("%s2", POPUPBUTTON_NODE_NAME), NULL);
	} else {
		MN_SetOneButton(popupButtonMenu, va("%s2", POPUPBUTTON_NODE_NAME),
			clickAction2 ? clickAction2 : popupAction2);
	}

	Cvar_Set("mn_sys_popup_button_text3", clickText3);
	Cvar_Set("mn_sys_popup_button_tooltip3", tooltip3);
	if (!clickAction3 && !clickText3) {
		MN_SetOneButton(popupButtonMenu, va("%s3", POPUPBUTTON_NODE_NAME), NULL);
	} else {
		MN_SetOneButton(popupButtonMenu, va("%s3", POPUPBUTTON_NODE_NAME),
			clickAction3 ? clickAction3 : popupAction3);
	}

	if (!MN_IsWindowOnStack(popupButtonMenu->name))
		MN_PushWindow(popupButtonMenu->name, NULL);
}
