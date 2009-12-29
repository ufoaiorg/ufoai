/**
 * @file m_internal.h
 * @brief Internal data use by the menu package
 * @note It should not be include by a file outside the menu package
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

#ifndef CLIENT_MENU_M_INTERNAL_H
#define CLIENT_MENU_M_INTERNAL_H

#define MAX_WINDOWS			128
#define MAX_COMPONENTS		24
#define MAX_MENUNODES		8192
#define MAX_MENUSTACK		32
#define MAX_MENUACTIONS		8192

#include "node/m_node_window.h"
#include "m_actions.h"
#include "m_nodes.h"
#include "m_icon.h"
#include "m_input.h"
#include "m_expression.h"
#include "m_data.h"
#include "node/m_node_model.h"

/**
 * @brief Global data shared into all menu code
 */
typedef struct menuGlobal_s {

	/**
	 * @brief Holds shared data
	 * @note The array id is given via num in the menuNode definitions
	 * @sa MN_ResetData
	 * @sa MN_RegisterText
	 * @sa MN_GetText
	 * @sa MN_RegisterLinkedListText
	 */
	menuSharedData_t sharedData[MAX_MENUTEXTS];

	menuOption_t options[MAX_MENUOPTIONS];
	int numOptions;

	menuNode_t nodes[MAX_MENUNODES];
	int numNodes;

	menuNode_t* windows[MAX_WINDOWS];
	int numWindows;

	menuNode_t* components[MAX_COMPONENTS];
	int numComponents;

	byte *adata, *curadata;
	int adataize;

	menuNode_t *windowStack[MAX_MENUSTACK];
	int windowStackPos;

	menuAction_t actions[MAX_MENUACTIONS];
	int numActions;

	menuModel_t menuModels[MAX_MENUMODELS];
	int numMenuModels;

	excludeRect_t excludeRect[MAX_EXLUDERECTS];
	int numExcludeRect;

	menuIcon_t icons[MAX_MENUICONS];
	int numIcons;

	menuKeyBinding_t keyBindings[MAX_MENUKEYBINDING];
	int numKeyBindings;

} menuGlobal_t;

extern menuGlobal_t mn;
extern struct memPool_s *mn_sysPool;
extern struct memPool_s *mn_dynStringPool;

#endif
