/**
 * @file m_main.h
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

#ifndef CLIENT_MENU_M_MAIN_H
#define CLIENT_MENU_M_MAIN_H

#define MAX_MENUS			128
#define MAX_MENUNODES		8192
#define MAX_MENUSTACK		32
#define MAX_MENUACTIONS		8192

#include "node/m_node_window.h"
#include "m_actions.h"
#include "m_messages.h"
#include "m_nodes.h"
#include "m_icon.h"
#include "node/m_node_model.h"

typedef struct mouseRepeat_s {
	menu_t *menu;				/**< where - the menu is it executed in (context) */
	menuAction_t *action;		/**< what - the action node to be executed (the clicknode e.g.) */
	unsigned nexttime;			/**< when (milliseconds) - calculated from cls.realtime + delay */
	int lastclicked;			/**< when was the last recorded click? */
	short clickDelay;			/**< milliseconds for the delay */
	int numClick;				/**< number of click made since the begining of this click */
	menuNode_t *node;			/**< node where action is repeated */
	int textLine;				/**< line clicked for a text node */
} mouseRepeat_t;

typedef struct menuGlobal_s {
	const char *menuText[MAX_MENUTEXTS];	/**< @brief Holds static array of characters to display
											* @note The array id is given via num in the menuNode definitions
											* @sa MN_MenuTextReset
											* @sa mn.menuTextLinkedList */
	linkedList_t *menuTextLinkedList[MAX_MENUTEXTS];	/**< @brief Holds a linked list for displaying in the menu
											* @note The array id is given via num in the menuNode definitions
											* @sa MN_MenuTextReset
											* @sa mn.menuText */

	mouseRepeat_t mouseRepeat;

	char messageBuffer[MAX_MESSAGE_TEXT];
	message_t *messageStack;
	chatMessage_t *chatMessageStack;

	selectBoxOptions_t menuSelectBoxes[MAX_SELECT_BOX_OPTIONS];
	int numSelectBoxes;

	menuNode_t menuNodes[MAX_MENUNODES];
	int numNodes;

	menu_t menus[MAX_MENUS];
	int numMenus;

	byte *adata, *curadata;
	int adataize;

	menu_t *menuStack[MAX_MENUSTACK];
	int menuStackPos;

	menuAction_t menuActions[MAX_MENUACTIONS];
	int numActions;

	menuModel_t menuModels[MAX_MENUMODELS];
	int numMenuModels;

	excludeRect_t excludeRect[MAX_EXLUDERECTS];
	int numExcludeRect;

	menuIcon_t menuIcons[MAX_MENUICONS];
	int numIcons;

} menuGlobal_t;

extern menuGlobal_t mn;
extern struct memPool_s *cl_menuSysPool;

void MN_Init(void);
void MN_Shutdown(void);
qboolean MN_CheckCondition(menuNode_t *node);
int MN_GetVisibleMenuCount(void);
void MN_SetNewMenuPos(menu_t* menu, int x, int y);

void MN_SetNewMenuPos_f (void);

void MN_DragMenu(void);

menu_t* MN_PushMenu(const char *name, const char *parentName);
void MN_PopMenu(qboolean all);
void MN_CloseMenu(const char* name);
menu_t* MN_GetActiveMenu(void);
menuNode_t* MN_GetNodeFromCurrentMenu(const char *name);
qboolean MN_CursorOnMenu(int x, int y);
menu_t *MN_GetMenu(const char *name);

int MN_CompletePushMenu(const char *partial, const char **match);

#endif
