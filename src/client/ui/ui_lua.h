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

extern "C" {
	#include "../../libs/lua/lua.h"
}

// prototype
struct uiNode_t;

/**
 * @brief callback signatures for functions defined in Lua
 */
typedef void (*LUA_ONLOAD_CALLBACK)(void);
typedef void (*LUA_ONCLICK_CALLBACK) (void);

/**
 * @brief global lua state used in ui-lua interfacing
 */
extern lua_State* ui_luastate;

/* lua initialization functions */
void UI_InitLua (void);
void UI_ShutdownLua (void);

/* lua callback registration functions */
void UI_RegisterHandler_OnLoad (lua_State *L, LUA_ONLOAD_CALLBACK fnc);

/* lua script functions */
bool UI_ParseAndLoadLuaScript (const char* name, const char** text);

/* lua uiNode create functions */
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super);
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super);
uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super);
