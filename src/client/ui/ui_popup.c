/**
 * @file ui_popup.c
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

#include "ui_main.h"
#include "ui_nodes.h"
#include "ui_popup.h"
#include "ui_actions.h"
#include "node/ui_node_abstractnode.h"

#define POPUPBUTTON_WINDOW_NAME "popup_button"
#define POPUPBUTTON_NODE_NAME "popup_button_"
#define POPUP_WINDOW_NAME "popup"

/** @brief strings to be used for popup when text is not static */
char popupText[UI_MAX_SMALLTEXTLEN];
static char popupAction1[UI_MAX_SMALLTEXTLEN];
static char popupAction2[UI_MAX_SMALLTEXTLEN];
static char popupAction3[UI_MAX_SMALLTEXTLEN];

/**
 * @brief Popup on geoscape
 * @note Only use static strings here - or use popupText if you really have to
 * build the string
 */
void UI_Popup (const char *title, const char *text)
{
	Cvar_Set("ui_sys_popup_title", title);
	UI_RegisterText(TEXT_POPUP_INFO, text);
	if (!UI_IsWindowOnStack(POPUP_WINDOW_NAME))
		UI_PushWindow(POPUP_WINDOW_NAME, NULL, NULL);
}

/**
 * @brief Generates a popup that contains a list of selectable choices.
 * @param[in] title Title of the popup.
 * @param[in] headline First line of text written in the popup.
 * @param[in] entries List of the selectables choices.
 * @param[in] clickAction Action to perform when one clicked on the popup.
 */
uiNode_t *UI_PopupList (const char *title, const char *headline, linkedList_t* entries, const char *clickAction)
{
	uiNode_t* window;
	uiNode_t* listNode;

	Cvar_Set("ui_sys_popup_title", title);
	UI_RegisterText(TEXT_POPUP_INFO, headline);

	/* make sure, that we are using the linked list */
	UI_ResetData(TEXT_LIST);
	UI_RegisterLinkedListText(TEXT_LIST, entries);

	window = UI_GetWindow(POPUPLIST_WINDOW_NAME);
	if (!window)
		Com_Error(ERR_FATAL, "Could not get "POPUPLIST_WINDOW_NAME" window");
	listNode = UI_GetNode(window, POPUPLIST_NODE_NAME);
	if (!listNode)
		Com_Error(ERR_FATAL, "Could not get "POPUPLIST_NODE_NAME" node in "POPUPLIST_WINDOW_NAME" window");

	/* free previous actions */
	if (listNode->onClick) {
		assert(listNode->onClick->d.terminal.d1.data);
		Mem_Free(listNode->onClick->d.terminal.d1.data);
		Mem_Free(listNode->onClick);
		listNode->onClick = NULL;
	}

	if (clickAction) {
		UI_PoolAllocAction(&listNode->onClick, EA_CMD, clickAction);
	} else {
		listNode->onClick = NULL;
	}

	if (!UI_IsWindowOnStack(window->name))
		UI_PushWindow(window->name, NULL, NULL);
	return listNode;
}

/**
 * @brief Set string and action button.
 * @param[in] window window where buttons are modified.
 * @param[in] button Name of the node of the button.
 * @param[in] clickAction Action to perform when button is clicked.
 * @note clickAction may be NULL if button is not needed.
 */
static void UI_SetOneButton (uiNode_t* window, const char *button, const char *clickAction)
{
	uiNode_t* buttonNode;

	buttonNode = UI_GetNode(window, button);
	if (!buttonNode)
		Com_Error(ERR_FATAL, "Could not get %s node in %s window", button, window->name);

	/* free previous actions */
	if (buttonNode->onClick) {
		assert(buttonNode->onClick->d.terminal.d1.data);
		Mem_Free(buttonNode->onClick->d.terminal.d1.data);
		Mem_Free(buttonNode->onClick);
		buttonNode->onClick = NULL;
	}

	if (clickAction) {
		UI_PoolAllocAction(&buttonNode->onClick, EA_CMD, clickAction);
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
 * @param[in] clickAction3 Action to perform when one clicked on the third button.
 * @param[in] clickText3 String that will be written in third button.
 * @param[in] tooltip3 Tooltip of third button.
 * @note clickAction AND clickText must be NULL if button should be invisible.
 */
void UI_PopupButton (const char *title, const char *text,
	const char *clickAction1, const char *clickText1, const char *tooltip1,
	const char *clickAction2, const char *clickText2, const char *tooltip2,
	const char *clickAction3, const char *clickText3, const char *tooltip3)
{
	uiNode_t* window;

	Cvar_Set("ui_sys_popup_title", title);
	if (text)
		UI_RegisterText(TEXT_POPUP_INFO, text);
	else
		UI_RegisterText(TEXT_POPUP_INFO, popupText);

	window = UI_GetWindow(POPUPBUTTON_WINDOW_NAME);
	if (!window)
		Com_Error(ERR_FATAL, "Could not get \""POPUPBUTTON_WINDOW_NAME"\" window");

	Cvar_Set("ui_sys_popup_button_text1", clickText1);
	Cvar_Set("ui_sys_popup_button_tooltip1", tooltip1);
	if (!clickAction1 && !clickText1) {
		UI_SetOneButton(window, va("%s1", POPUPBUTTON_NODE_NAME),
			NULL);
	} else {
		UI_SetOneButton(window, va("%s1", POPUPBUTTON_NODE_NAME),
			clickAction1 ? clickAction1 : popupAction1);
	}

	Cvar_Set("ui_sys_popup_button_text2", clickText2);
	Cvar_Set("ui_sys_popup_button_tooltip2", tooltip2);
	if (!clickAction2 && !clickText2) {
		UI_SetOneButton(window, va("%s2", POPUPBUTTON_NODE_NAME), NULL);
	} else {
		UI_SetOneButton(window, va("%s2", POPUPBUTTON_NODE_NAME),
			clickAction2 ? clickAction2 : popupAction2);
	}

	Cvar_Set("ui_sys_popup_button_text3", clickText3);
	Cvar_Set("ui_sys_popup_button_tooltip3", tooltip3);
	if (!clickAction3 && !clickText3) {
		UI_SetOneButton(window, va("%s3", POPUPBUTTON_NODE_NAME), NULL);
	} else {
		UI_SetOneButton(window, va("%s3", POPUPBUTTON_NODE_NAME),
			clickAction3 ? clickAction3 : popupAction3);
	}

	if (!UI_IsWindowOnStack(window->name))
		UI_PushWindow(window->name, NULL, NULL);
}
