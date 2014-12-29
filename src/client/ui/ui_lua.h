/**
 * @file
 * @brief Basic lua initialization for the ui.
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#pragma once

#include "../../common/scripts_lua.h"

// prototype
struct uiNode_t;
struct linkedList_t;

/* ui lua startup/shutdown */
void UI_InitLua (void);
void UI_ShutdownLua (void);

/* register and call ufo module onload callback */
void UI_RegisterHandler_OnLoad (LUA_FUNCTION fcn);
void UI_CallHandler_OnLoad (lua_State* L, const char* key);

/* lua script functions */
bool UI_ParseAndLoadLuaScript (const char* name, const char** text);
bool UI_ExecuteLuaEventScript (uiNode_t* node, LUA_EVENT event);
bool UI_ExecuteLuaEventScript_ReturnBool (uiNode_t* node, LUA_EVENT event, bool &result);
bool UI_ExecuteLuaEventScript_XY (uiNode_t* node, LUA_EVENT event, int x, int y);
bool UI_ExecuteLuaEventScript_DxDy (uiNode_t* node, LUA_EVENT event, int dx, int dy);
bool UI_ExecuteLuaEventScript_Key (uiNode_t* node, LUA_EVENT event, unsigned int key, unsigned short unicode);

bool UI_ExecuteLuaEventScript_DragDrop (uiNode_t* node, LUA_EVENT event, bool &result);
bool UI_ExecuteLuaEventScript_DragDrop_XY (uiNode_t* node, LUA_EVENT event, int x, int y, bool &result);
bool UI_ExecuteLuaEventScript_DragDrop_IsDropped (uiNode_t* node, LUA_EVENT event, bool isDropped, bool &result);

bool UI_ExecuteLuaMethod (uiNode_t* node, LUA_FUNCTION fcn, linkedList_t* params, int nparams);
bool UI_ExecuteLuaConFunc (uiNode_t* node, LUA_FUNCTION fcn);
bool UI_ExecuteLuaMethod_ByName (uiNode_t* node, const char* name, linkedList_t* params, int nparams);

/* lua uiNode create functions */
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super);
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super);
uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super);

/* information */
void UI_PrintNodeTree (uiNode_t* node, int level = 0);
const char* UI_Node_TypeOf(uiNode_t* node);

