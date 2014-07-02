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

#include "ui_lua.h"
extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

/* global lua state for ui-lua interfacing */
lua_State* ui_luastate = 0;

/**
 * @brief Initializes the ui-lua interfacing environment.
 */
void UI_InitLua (void) {
	/* clean up old state */
	if (ui_luastate) {
		UI_ShutdownLua();
	}

	/* initialize new lua environment dedicated to the ui */
	ui_luastate = lua_open ();

    /* add basic lua libraries to the lua environment */
    luaopen_base (ui_luastate);
	luaopen_string (ui_luastate);
	luaopen_math (ui_luastate);
	luaopen_table (ui_luastate);
	luaopen_os (ui_luastate);
	luaopen_package (ui_luastate);
}

/**
 * @brief Shutdown the ui-lua interfacing environment.
 */
void UI_ShutdownLua (void) {
	if (ui_luastate) {
        lua_close(ui_luastate);
		ui_luastate = 0;
	}
}
