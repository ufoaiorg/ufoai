/**
 * @file
 * @brief Basic lua initialization for the ui.
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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


#include "ui_lua.h"

#include "../../shared/cxx.h"
#include "../../shared/defines.h"
#include "../../shared/shared.h"
#include "../../common/hashtable.h"

extern "C" {
	#include "../../libs/lua/lauxlib.h"
	#include "../../libs/lua/lualib.h"

	/* references to SWIG generated lua bindings */
	extern int luaopen_ufo (lua_State *L);
	extern int luaopen_ufoui (lua_State *L);
}

/* global lua state for ui-lua interfacing */
lua_State* ui_luastate = nullptr;
/* internal field for passing script name as a key to lua registration functions */
char ui_scriptname[256] = "";
/* hash map for storing callback references */
hashTable_s* ui_onload = nullptr;

/**
 * @brief Initializes the ui-lua interfacing environment.
 */
void UI_InitLua (void) {
	/* clean up old state */
	if (ui_luastate) {
		UI_ShutdownLua();
	}

	/* initialize new lua environment dedicated to the ui */
	ui_luastate = luaL_newstate();

    /* add basic lua libraries to the lua environment */
    luaL_openlibs(ui_luastate);
    /* add the ufo module -> exposes common functions */
    luaopen_ufo (ui_luastate);
    /* add the ufoui module -> exposes ui specific functions */
    luaopen_ufoui (ui_luastate);

    /* initialize hash table for onload callback mechanism */
    ui_onload = HASH_NewTable (true, true, true);
}

/**
 * @brief Shutdown the ui-lua interfacing environment.
 */
void UI_ShutdownLua (void) {
	if (ui_luastate) {
        HASH_DeleteTable (&ui_onload);
        lua_close(ui_luastate);
		ui_luastate = nullptr;
	}
}

/**
 * @brief Registers a lua callback function, to be called when loading the lua script.
 * @param[in] L The lua state for calling lua.
 * @note The onload callback is file based.
*/
void UI_RegisterHandler_OnLoad (lua_State *L, LUA_ONLOAD_CALLBACK fnc) {
	int regvalue = (int) fnc;
	/* store regvalue into the list of handlers */
	int len = strlen (ui_scriptname);
	if (len > 0) {
		Com_Printf ("UI_RegisterHandler_OnLoad: file = %s, callback idx = %d\n", ui_scriptname, regvalue);
		HASH_Insert (ui_onload, ui_scriptname, len, &regvalue, sizeof(regvalue));
	}
	else {
		Com_Printf("lua callback registration error: script name has zero length!\n");
	}
}

/**
 * @brief Calls the registered lua onload callback function.
 * @param[in] L The lua state for calling lua.
 * @param[in] script The name of the .ufo file holding the lua script.
 * @note The signature of the lua function is: onload ().
 * @note If the signature changes, this function should change too.
 */
static void UI_CallHandler_OnLoad (lua_State *L, const char* script) {
	/* look up the handler */
	void *value = HASH_Get(ui_onload, script, strlen (script));
	if (value) {
		int regvalue = * ((int*)value);
		Com_Printf ("UI_CallHandler_OnLoad: file = %s, callback idx = %d\n", script, regvalue);
		lua_rawgeti (L, LUA_REGISTRYINDEX, regvalue);
		lua_pcall (L, 0, 0, 0);
	}
	else {
	}
}

/**
 * @brief Parses and loads a .ufo file holding a lua script.
 * @param[in] name The name of the .ufo file (holding the lua script).
 * @param[in] text The contents of the .ufo file, only provided to be in line with code handling other
 * .ufo content types.
 * @sa CL_ParseClientData
 * @return True if parsing was succesful, false otherwise.
 * @note All parsed lua files will be added to the lua state.
 */
bool UI_ParseAndLoadLuaScript (const char* name, const char** text) {
	/* signal lua file found */
	Com_Printf ("UI_ParseAndLoadLuaScript: found lua file: %s\n", name);
	/* load the contents of the lua file */
	if (luaL_loadstring(ui_luastate, *text) == 0) {
		/* set the script name for calls to the registration functions */
		Q_strncpyz (ui_scriptname, name, sizeof(ui_scriptname));
		/* the script is loaded, now execute it; this will trigger any register_XXXX functions to be
		   called by the lua script */
		if (lua_pcall(ui_luastate, 0, LUA_MULTRET, 0)) {
			Com_Printf ("lua error: %s\n", lua_tostring(ui_luastate, -1));
		};
		ui_scriptname[0]='\0';
		/* at this point the lua file is loaded and callbacks are registered (on the stack),
		   now call the onLoad if it exists */
		UI_CallHandler_OnLoad (ui_luastate, name);
	}
	else {
        Com_Error(0, "lua load error: %s\n", lua_tostring(ui_luastate, -1));
	}
	/* reset the content pointer, this will force the cached content to be released
	   FS_NextScriptHeader. */
	*text = nullptr;
	/* execute the onLoad function */
	return true;
}
