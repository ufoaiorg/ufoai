/**
 * @file ui_internal.h
 * @brief Internal data use by the UI package
 * @note It should not be include by a file outside the UI package
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

#ifndef CLIENT_UI_UI_INTERNAL_H
#define CLIENT_UI_UI_INTERNAL_H

#define UI_MAX_WINDOWS			128
#define UI_MAX_COMPONENTS		128
#define UI_MAX_WINDOWSTACK		32
#define UI_MAX_ACTIONS			2*8192
#define UI_MAX_VARIABLESTACK	64

#include "node/ui_node_window.h"
#include "node/ui_node_model.h"
#include "ui_main.h"
#include "ui_actions.h"
#include "ui_behaviour.h"
#include "ui_nodes.h"
#include "ui_sprite.h"
#include "ui_input.h"
#include "ui_expression.h"
#include "ui_data.h"

/**
 * @brief Global data shared into all UI code
 */
typedef struct uiGlobal_s {

	/**
	 * @brief Holds shared data
	 * @note The array id is given via dataID in the node definitions
	 * @sa UI_ResetData
	 * @sa UI_RegisterText
	 * @sa UI_GetText
	 * @sa UI_RegisterLinkedListText
	 */
	uiSharedData_t sharedData[UI_MAX_DATAID];

	/**
	 * @brief Local var for script function
	 */
	uiValue_t variableStack[UI_MAX_VARIABLESTACK];

	int numNodes;

	uiNode_t* windows[UI_MAX_WINDOWS];
	int numWindows;

	uiNode_t* components[UI_MAX_COMPONENTS];
	int numComponents;

	byte *adata, *curadata;
	int adataize;

	uiNode_t *windowStack[UI_MAX_WINDOWSTACK];
	int windowStackPos;

	uiAction_t actions[UI_MAX_ACTIONS];
	int numActions;

	uiModel_t models[UI_MAX_MODELS];
	int numModels;

	uiSprite_t sprites[UI_MAX_SPRITES];
	int numSprites;

	uiKeyBinding_t keyBindings[UI_MAX_KEYBINDING];
	int numKeyBindings;

} uiGlobal_t;

extern uiGlobal_t ui_global;

extern memPool_t *ui_sysPool;
extern memPool_t *ui_dynStringPool;
extern memPool_t *ui_dynPool;

/**
 * Alignment memory for structures
 * @todo Remove it and use something from compiler
 */
#define STRUCT_MEMORY_ALIGN	8

void* UI_AllocHunkMemory(size_t size, int align, qboolean reset);

void UI_FinishInit(void);
void UI_FinishWindowsInit(void);

#endif
