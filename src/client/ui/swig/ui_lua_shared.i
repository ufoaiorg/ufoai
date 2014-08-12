/**
	@file Interface file for SWIG to generarte lua ui binding.
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

/* expose the ui code using a lua table ufoui */
%module ufoui

%{
/* import headers into the interface so they can be used */
#include <typeinfo>

/* import common functions */
#include "../../../shared/shared.h"

/* import ui specific functions */
#include "../ui_main.h"
#include "../ui_behaviour.h"
#include "../ui_nodes.h"
#include "../ui_node.h"

#include "../ui_lua.h"
%}

/* typemap for converting lua function to C */
%typemap(in) LUA_ONLOAD_CALLBACK {
	$1 = (LUA_ONLOAD_CALLBACK)luaL_ref (L, LUA_REGISTRYINDEX);
}

/* expose node structure */
%rename(uiNode) uiNode_t;
struct uiNode_t {
	/* values that are read only accessible from lua */
	%immutable;
	char name[MAX_VAR];			/**< name from the script files */

	/* values that are read/write accessible from lua */
	%mutable;
};

/* expose registration functions for callbacks */
%rename(register_onload) UI_RegisterHandler_OnLoad;
void UI_RegisterHandler_OnLoad (lua_State *L, LUA_ONLOAD_CALLBACK fcn);

/* expose uiNode creation functions */
%rename (create_control) UI_CreateControl;
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super);
%rename (create_component) UI_CreateComponent;
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super);
%rename (create_window) UI_CreateWindow;
uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super);
