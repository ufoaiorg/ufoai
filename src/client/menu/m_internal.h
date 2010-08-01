/**
 * @file m_internal.h
 * @brief Internal data use by the menu package
 * @note It should not be include by a file outside the menu package
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

#ifndef CLIENT_MENU_M_INTERNAL_H
#define CLIENT_MENU_M_INTERNAL_H

#define MAX_WINDOWS			128
#define MAX_COMPONENTS		64
#define MAX_MENUSTACK		32
#define MAX_MENUACTIONS		2*8192

#include "node/m_node_window.h"
#include "node/m_node_model.h"
#include "m_actions.h"
#include "m_nodes.h"
#include "m_icon.h"
#include "m_input.h"
#include "m_expression.h"
#include "m_data.h"

/**
 * @brief Global data shared into all menu code
 */
typedef struct uiGlobal_s {

	/**
	 * @brief Holds shared data
	 * @note The array id is given via num in the menuNode definitions
	 * @sa UI_ResetData
	 * @sa UI_RegisterText
	 * @sa UI_GetText
	 * @sa UI_RegisterLinkedListText
	 */
	uiSharedData_t sharedData[MAX_MENUTEXTS];

	int numNodes;

	uiNode_t* windows[MAX_WINDOWS];
	int numWindows;

	uiNode_t* components[MAX_COMPONENTS];
	int numComponents;

	byte *adata, *curadata;
	int adataize;

	uiNode_t *windowStack[MAX_MENUSTACK];
	int windowStackPos;

	uiAction_t actions[MAX_MENUACTIONS];
	int numActions;

	menuModel_t menuModels[MAX_MENUMODELS];
	int numMenuModels;

	uiExcludeRect_t excludeRect[MAX_EXLUDERECTS];
	int numExcludeRect;

	uiIcon_t icons[MAX_MENUICONS];
	int numIcons;

	uiKeyBinding_t keyBindings[MAX_MENUKEYBINDING];
	int numKeyBindings;

} uiGlobal_t;

extern uiGlobal_t mn;
extern struct memPool_s *mn_sysPool;
extern struct memPool_s *mn_dynStringPool;
extern struct memPool_s *mn_dynPool;

#endif
