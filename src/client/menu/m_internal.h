/**
 * @file m_internal.h
 * @brief Internal data use by the menu package
 * @note It should not be include by a file outside the menu package
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

#ifndef CLIENT_MENU_M_INTERNAL_H
#define CLIENT_MENU_M_INTERNAL_H

#define MAX_MENUS			128
#define MAX_MENUNODES		8192
#define MAX_MENUSTACK		32
#define MAX_MENUACTIONS		8192

#include "node/m_node_window.h"
#include "m_actions.h"
#include "m_nodes.h"
#include "m_icon.h"
#include "m_condition.h"
#include "m_data.h"
#include "node/m_node_model.h"

/**
 * @todo Maybe merge cl_menuSysPool and mn.adata (same usage)
 * @todo Menu must manage itself cl_menuSysPool (initialisation), if not possible, the outside must set it with a menu function. Anywhere it need a private access (menu only).
 */
typedef struct menuGlobal_s {

	/**
	 * @brief Holds shared data
	 * @note The array id is given via num in the menuNode definitions
	 * @sa MN_MenuTextReset
	 * @sa MN_RegisterText
	 * @sa MN_GetText
	 * @sa MN_RegisterLinkedListText
	 */
	menuSharedData_t sharedData[MAX_MENUTEXTS];

	menuOption_t menuOptions[MAX_MENUOPTIONS];
	int numOptions;

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

	menuDepends_t menuConditions[MAX_MENUCONDITIONS];
	int numConditions;

	menuIcon_t menuIcons[MAX_MENUICONS];
	int numIcons;

} menuGlobal_t;

extern menuGlobal_t mn;
extern struct memPool_s *cl_menuSysPool;

#endif
