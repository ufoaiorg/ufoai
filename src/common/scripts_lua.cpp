/**
 * @file
 * @brief Module for lua script functions
 */

/*
Copyright (C) 2002-2019 UFO: Alien Invasion.

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

#include "scripts_lua.h"
#include "../shared/cxx.h"

int Com_LuaIsNilOrTable (lua_State* L, int index) {
	return (lua_isnil(L, index) || lua_istable(L, index));
}

/**
 * @brief Convert a lua table to a linkedList of character strings.
 */
linkedList_t* Com_LuaTableToStringList (lua_State* L, int index) {
	if (lua_isnil(L, index))
		return nullptr;

	linkedList_t* result = nullptr;
	/* table is in the stack at 'index' */
	lua_pushnil(L);  /* first key */
	while (lua_next(L, index) != 0) {
		/* 'key' at index -2 and 'value' at index -1 */
		const char* v = lua_tostring(L, -1);
		if (v)
			LIST_AddString(&result, v);
		/* removes 'value', keeps 'key' for next iteration */
		lua_pop(L, 1);
	}
	return result;
}
