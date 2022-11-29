/**
 * @file
 * @brief Basic lua initialization for the client.
 */

/*
Copyright (C) 2002-2022 UFO: Alien Invasion.

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

#include "cl_lua.h"

#include "../../shared/cxx.h"
#include "../../shared/defines.h"
#include "../../shared/shared.h"
#include "../../common/hashtable.h"
#include "../../common/filesys.h"

/* global lua state for ui-lua interfacing */
lua_State* cl_luastate = nullptr;
/* hash map for storing callback references */
hashTable_s* cl_callback = nullptr;

/* references to SWIG generated lua bindings */
extern "C" int luaopen_ufo (lua_State *L);

/**
 * @brief Loader that enables the lua files to access .ufo files through the ufo filesystem.
 * @note This function is called from inside the lua environment. If lua encounters a 'require' statement,
 * it uses an internal table of loading functions to load the module.
 */
static int CL_UfoModuleLoader (lua_State* L) {
	/* this function is called by lua with the module name on the stack */
	char module[256];
	char errmsg[512];
	const char* name = lua_tostring(L, -1);
	byte* buffer;

	/* initialize */
	memset(module, 0, sizeof(module));
	Com_sprintf(module, sizeof(module), "ufos/ui/%s", name);
	memset(errmsg, 0, sizeof(errmsg));

	/* find the module using ufo's filesystem */
	int len = FS_LoadFile(module, &buffer);
	if (len != -1) {
		/* found, the contents of the file is now present in buffer */
		/* load the contents into the lua state */
		if (luaL_loadbuffer(L, (const char*) buffer, len, module) == 0) {
			return 1;
		} else {
			/* push error string onto the stack */
			sprintf(errmsg, "custom loader error - cannot load module named [%s]\n", module);
			lua_pushstring(L, errmsg);
		}
	} else {
		/* push error string onto the stack */
		sprintf(errmsg, "custom loader error - cannot find module named [%s]\n", module);
		lua_pushstring(L, errmsg);
	}
	/* an error occured, return 0*/
	return 0;
}


/**
 * @brief This function adds loader to the lua table of module loaders that enables lua to access the
 * ufo filesystem.
 */
static void CL_InsertModuleLoader (lua_State* L) {
	/* save stack position */
	int pos = lua_gettop(L);

	/* push the global table 'package' on the stack */
	lua_getfield(L, LUA_GLOBALSINDEX, "package");
	/* using the table 'package', push the 'loaders' field on the stack */
	lua_getfield(L, -1, "loaders");
	/* remote the 'package' entry from the stack */
	lua_remove(L, -2);

	/* the lua stack now only holds the field 'loaders' on top */
	/* next, determine the number of loaders by counting (lua doesn't have a function for this) */
	int nloaders = 0;
	/* lua_next pushes a (key, value) pair using the first entry following the key already on the stack;
	   in this case we start with a nil key so lua_next will return the first (key,value) pair in the table
	   of loaders */
	lua_pushnil(L);
	/* if lua_next reaches the end, it returns 0 */
	while (lua_next(L, -2) != 0) {
		/* lua_next has pushed a (key,value) pair on the stack; remove the value, keep the key
		   for the next iteration */
		lua_pop(L, 1);
		nloaders++;
	}

	/* now that we have the number of entries in the 'loaders' table, we can add or own loader */
	lua_pushinteger(L, nloaders + 1);
	lua_pushcfunction(L, CL_UfoModuleLoader);
	lua_rawset(L, -3);

	/* restore stack position */
	lua_settop(L, pos);
}


/**
 * @brief Initializes the ui-lua interfacing environment.
 */
void CL_InitLua (void) {
	/* clean up old state */
	if (cl_luastate) {
		CL_ShutdownLua();
	}

	/* initialize new lua environment dedicated to the ui */
	cl_luastate = luaL_newstate();

	/* add basic lua libraries to the lua environment */
	luaL_openlibs(cl_luastate);

	/* insert custom module loader */
	CL_InsertModuleLoader(cl_luastate);

	/* add the ufo module -> exposes common functions */
	luaopen_ufo(cl_luastate);

	/* initialize hash table for onload callback mechanism */
	cl_callback = HASH_NewTable(true, true, true);
}

/**
 * @brief Shutdown the ui-lua interfacing environment.
 */
void CL_ShutdownLua (void) {
	if (cl_luastate) {
		HASH_DeleteTable(&cl_callback);
		lua_close(cl_luastate);
		cl_luastate = nullptr;
	}
}

/**
 * @brief Returns the lua state for the client side.
 */
lua_State* CL_GetLuaState (void) {
	return cl_luastate;
}

/**
 * @brief Registers a lua callback function with a key.
 * @param key A key for finding the callback function by name, usually the script name.
 * @param fnc A lua function registered in the lua registry index.
*/
void CL_RegisterCallback (const char* key, LUA_FUNCTION fnc) {
	int regvalue = (int) fnc;
	/* store regvalue into the list of handlers */
	int len = strlen(key);
	if (len > 0) {
		HASH_Insert(cl_callback, key, len, &regvalue, sizeof(regvalue));
	}
	else {
		Com_Printf("CL_RegisterCallback: lua callback registration error: script name has zero length!\n");
	}
}

/**
 * @brief Calls the registered lua onload callback function.
 * @param[in] L The lua state for calling lua.
 * @param[in] key script The name of the .ufo file holding the lua script.
 * @note The signature of the lua function is without any paramters: function ().
 * @note If the signature changes, this function should change too.
 */
void CL_ExecuteCallback (lua_State *L, const char* key) {
	/* look up the handler */
	void *value = HASH_Get(cl_callback, key, strlen (key));
	if (value) {
		int regvalue = * ((int*)value);
		lua_rawgeti(L, LUA_REGISTRYINDEX, regvalue);
		if (lua_pcall(L, 0, 0, 0) != 0) {
			Com_Printf("lua error: %s\n", lua_tostring(cl_luastate, -1));
		};
	}
}
