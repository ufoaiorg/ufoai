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
#include "../../common/filesys.h"

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
 * @brief Loader that enables the lua files to access .ufo files through the ufo filesystem.
 * @note This function is called from inside the lua environment. If lua encounters a 'require' statement,
 * it uses an internal table of loading functions to load the module.
 */
int UI_UfoModuleLoader (lua_State* L) {
	/* this function is called by lua with the module name on the stack */
	char module[256];
	char errmsg[256];
	const char* name = lua_tostring (L, -1);
	byte* buffer;

	/* initialize */
	memset(module, 0, sizeof(module));
	Q_strncpyz (module, name, sizeof(module));
	memset(errmsg, 0, sizeof(errmsg));

	/* find the module using ufo's filesystem */
	int len = FS_LoadFile (module, &buffer);
	if (len != -1) {
		/* found, the contents of the file is now present in buffer */
		/* load the contents into the lua state */
		if (luaL_loadbuffer (L, (const char*) buffer, len, module) == 0) {
			return 1;
		}
		else {
			/* push error string onto the stack */
            sprintf(errmsg, "custom loader error - cannot load module named [%s]\n", module);
			lua_pushstring (L, errmsg);
		}
	}
	else {
		/* push error string onto the stack */
		sprintf(errmsg, "custom loader error - cannot find module named [%s]\n", module);
		lua_pushstring (L, errmsg);
	}
	/* an error occured, return 0*/
	return 0;
}

/**
 * @brief This function adds loader to the lua table of module loaders that enables lua to access the
 * ufo filesystem.
 */
void UI_InsertModuleLoader () {
	/* save stack position */
	int pos = lua_gettop (ui_luastate);

	/* push the global table 'package' on the stack */
	lua_getfield (ui_luastate, LUA_GLOBALSINDEX, "package");
	/* using the table 'package', push the 'loaders' field on the stack */
	lua_getfield (ui_luastate, -1, "loaders");
	/* remote the 'package' entry from the stack */
	lua_remove (ui_luastate, -2);

	/* the lua stack now only holds the field 'loaders' on top */
	/* next, determine the number of loaders by counting (lua doesn't have a function for this) */
	int nloaders = 0;
	/* lua_next pushes a (key, value) pair using the first entry following the key already on the stack;
	   in this case we start with a nil key so lua_next will return the first (key,value) pair in the table
	   of loaders */
	lua_pushnil (ui_luastate);
	/* if lua_next reaches the end, it returns 0 */
	while (lua_next (ui_luastate, -2) != 0) {
		/* lua_next has pushed a (key,value) pair on the stack; remove the value, keep the key
		   for the next iteration */
		lua_pop (ui_luastate, 1);
		nloaders++;
	}

	/* now that we have the number of entries in the 'loaders' table, we can add or own loader */
	lua_pushinteger (ui_luastate, nloaders + 1);
	lua_pushcfunction (ui_luastate, UI_UfoModuleLoader);
	lua_rawset (ui_luastate, -3);

	/* restore stack position */
	lua_settop (ui_luastate, pos);
}

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

    /* insert custom module loader */
    UI_InsertModuleLoader();

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
	/* determine the length of the string buffer */
	int ntext = strlen (*text);
	/* signal lua file found */
	Com_Printf ("UI_ParseAndLoadLuaScript: found lua file: %s\n", name);
	/* load the contents of the lua file */
	if (luaL_loadbuffer(ui_luastate, *text, ntext, name) == 0) {
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

