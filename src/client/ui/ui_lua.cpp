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
// STL
#include <map>
#include <string>

extern "C" {
	#include "../../libs/lua/lauxlib.h"
	#include "../../libs/lua/lualib.h"

	/* references to SWIG generated lua bindings */
	extern int luaopen_ufo (lua_State *L);
	extern int luaopen_ufoui (lua_State *L);
}

/* global lua state for ui-lua interfacing */
lua_State* ui_luastate = nullptr;
/* hash map for storing callback references */
std::map<std::string, int> ui_onload;

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
}

/**
 * @brief Shutdown the ui-lua interfacing environment.
 */
void UI_ShutdownLua (void) {
	if (ui_luastate) {
		ui_onload.clear();
        lua_close(ui_luastate);
		ui_luastate = nullptr;
	}
}

/**
 * @brief Registers a lua callback function, to be called when loading the lua script.
 * @param[in] L The lua state for calling lua.
 * @note The onload callback is file based.
*/
void UI_RegisterHandler_OnLoad (lua_State *L) {
	// TODO somehow test the number of arguments
	/* test if the top of the lua stack contains a function */
	if (!lua_isfunction (L, -1)) {
        Com_Error(ERR_FATAL, "lua callback registration error: %s\n", lua_tostring(ui_luastate, -1));
        lua_pop (L, -1);
	}
	/* ok, callback function found on the stack -> register this function in the registry; this will pop the value
	   from the stack */
	int regvalue = luaL_ref (L, LUA_REGISTRYINDEX);
	/* store regvalue into the list of handlers */
	/* note that since luaL_ref pops the function from the stack, the string pushed onto the stack is now
	   the top of the stack (see: parse & load) */
	if (lua_isstring(L, -1)) {
        /* get the name, use it as the key in the map */
        size_t len;
		const char* key=lua_tolstring(L, -1, &len);
		if (len > 0) {
			ui_onload.insert(std::pair<std::string,int>(key, regvalue));
		}
		else {
			Com_Error(ERR_FATAL, "lua callback registration error: script name has zero length!\n");
		}
	}
	else {
		/* error: we expect the top of the stack to contain the name of the lua script */
		Com_Error(ERR_FATAL, "lua callback registration error: script name expected on the stack!\n");
	}
}

/**
 * @brief Calls the registered lua onload callback function.
 * @param[in] L The lua state for calling lua.
 * @param[in] name The name of the .ufo file holding the lua script.
 * @note The signature of the lua function is: onload ().
 * @note If the signature changes, this function should change too.
 */
void UI_CallHandler_OnLoad (lua_State *L, const char* name) {
	/* look up the handler */
	int regvalue=ui_onload[name];
	lua_rawgeti (L, LUA_REGISTRYINDEX, regvalue);
	lua_pcall (L, 0, 0, 0);
}

/**
 * @brief Registers a lua callback function, to be called when processing a mouse click.
 * @param[in] L The lua state for calling lua.
 */
void UI_RegisterHandler_OnClick (lua_State *L) {
	/* test if the top of the lua stack contains a function */
	if (!lua_isfunction (L, -1)) {
        Com_Error(ERR_FATAL, "lua callback registration error: %s\n", lua_tostring(ui_luastate, -1));
		lua_pop (L, -1);
	}
	/* ok, callback function found on the stack -> register this function in the registry; this will pop the value
	   from the stack */
	int regvalue = luaL_ref (L, LUA_REGISTRYINDEX);
	/* store regvalue into the list of handlers */
}

/**
 * @brief Calls the registered lua onclick callback function.
 * @param[in] L The lua state for calling lua.
 * @note The signature of the lua function is: onclick(node).
 * @note If the signature changes, this function should change too.
 */
void UI_CallHandler_OnClick (lua_State *L) {
	// TODO get the registered index
	lua_rawgeti (L, LUA_REGISTRYINDEX, 0);
	// TODO push node argument to stack
	lua_pcall (L, 0, 0, 0);
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
	/* reset the content pointer, this will force the cached content to be released
	   FS_NextScriptHeader. */
	*text = nullptr;
	/* signal lua file found */
	Com_Printf ("found lua file: %s\n", name);
	/* load the lua file */
	if (luaL_loadfile(ui_luastate, name) == 0) {
		/* push the name of this script onto the lua stack; this value is used as a key when registering
		   callbacks */
		lua_pushstring(ui_luastate, name);
		/* the script is loaded, now execute it; this will trigger any register_XXXX functions to be
		   called by the lua script */
		lua_pcall(ui_luastate, 0, 0, 0);
		/* pop the script name from the stack */
		lua_pop(ui_luastate, -1);

		/* at this point the lua file is loaded and callbacks are registered (on the stack),
		   now call the onLoad if it exists */
		UI_CallHandler_OnLoad (ui_luastate, name);
	}
	else {
        Com_Error(0, "lua load error: %s\n", lua_tostring(ui_luastate, -1));
	}
	/* execute the onLoad function */
	return true;
}
