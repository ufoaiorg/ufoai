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
#include "../../../common/scripts_lua.h"

/* import ui specific functions */
#include "../ui_main.h"
#include "../ui_behaviour.h"
#include "../ui_nodes.h"
#include "../ui_node.h"

#include "../ui_lua.h"

/* typedefs only visible for SWIG, used for subclassing uiNode_t (see below for more details). */
typedef uiNode_t uiWindow_t;
typedef uiNode_t uiButton_t;
typedef uiNode_t uiCheckBox_t;

// todo: implement other ui node classes

%}

/* typemap for converting lua function to a reference to be used by C */
%typemap(in) LUA_FUNCTION {
	$1 = (LUA_FUNCTION)luaL_ref (L, LUA_REGISTRYINDEX);
}
%typemap(in) LUA_EVENT {
	$1 = (LUA_EVENT)luaL_ref (L, LUA_REGISTRYINDEX);
}

/* expose node structure */
%rename(uiNode) uiNode_t;
struct uiNode_t {
	/* these values are read-only from lua */
	%immutable;

	/* common properties */
	char* name;				/**< name from the script files */
	/* common navigation */
	%rename (first) firstChild;
	uiNode_t* firstChild; 	/**< first element of linked list of child */
	%rename (last) lastChild;
	uiNode_t* lastChild;	/**< last element of linked list of child */
	uiNode_t* next;			/**< Next element into linked list */
	uiNode_t* parent;		/**< Parent window, else nullptr */
	uiNode_t* root;			/**< Shortcut to the root node */

	/* these values are read/write from lua */
	%mutable;

	/* events */
	%rename (on_click) lua_onClick;
    %rename (on_rightclick) lua_onRightClick;
    %rename (on_middleclick) lua_onMiddleClick;
    LUA_EVENT lua_onMiddleClick; /**< references the lua on_middleclick method attached to this node */
    LUA_EVENT lua_onClick; /**< references the event in lua: on_click (node, x, y) */
    LUA_EVENT lua_onRightClick; /**< references the event in lua: on_rightclick (node, x, y) */
    LUA_EVENT lua_onMiddleClick; /**< references the event in lua: on_middleclick (node, x, y) */
	%rename (on_wheelup) lua_onWheelUp;
	%rename (on_wheeldown) lua_onWheelDown;
	%rename (on_wheel) lua_onWheel;
    LUA_EVENT lua_onWheelUp; /**< references the event in lua: on_wheelup (node, dx, dy) */
    LUA_EVENT lua_onWheelDown; /**< references the event in lua: on_wheeldown (node, dx, dy) */
    LUA_EVENT lua_onWheel; /**< references the event in lua: on_wheel (node, dx, dy) */
    %rename (on_focusgained) lua_onFocusGained;
    %rename (on_focuslost) lua_onFocusLost;
    %rename (on_keypressed) lua_onKeyPressed;
    %rename (on_keyreleased) lua_onKeyReleased;
    LUA_EVENT lua_onFocusGained; /**< references the event in lua: on_focusgained (node) */
    LUA_EVENT lua_onFocusLost; /**< references the event in lua: on_focuslost (node) */
    LUA_EVENT lua_onKeyPressed; /**< references the event in lua: on_keypressed (node, key, unicode) */
    LUA_EVENT lua_onKeyReleased; /**< references the event in lua: on_keyreleased (node, key, unicode) */
};
%extend uiNode_t {
	/* functions operating on a node */
	bool is_window () { return UI_Node_IsWindow ($self); };
	uiNode_t* create_control (const char* type, const char* name, const char* super) { return UI_CreateControl ($self, type, name, super); };
};

/*
	The following defines derived "classes" from uiNode. This solves the problem of all nodes being structures
	of type uiNode_t where the actual "class" is found in the behaviour value. In the interface file we actually
	implement the class hierarcy by typecasting the uiNode_t structure into various derived classes. Then we
	use SWIG's extend mechanism to provide direct support for the properties and methods in the derived classes
	and direct these back to a call into the behaviour class.
*/
%rename (uiWindow) uiWindow_t;
struct uiWindow_t: uiNode_t {
};
%extend uiWindow_t {
	void set_background (const char* name) { Com_Printf("calling uiWindow::set_background with arg = %s\n", name); };
};

%rename (uiButton) uiButton_t;
struct uiButton_t: uiNode_t {
};
%extend uiButton_t {
};

%rename (uiCheckBox) uiCheckBox_t;
struct uiCheckBox_t {
};
%extend uiCheckBox_t {
};


/* expose registration functions for callbacks */
%rename(register_onload) UI_RegisterHandler_OnLoad;
void UI_RegisterHandler_OnLoad (lua_State *L, LUA_FUNCTION fcn);

/* expose uiNode creation functions */
%rename (__create_control) UI_CreateControl;
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super);
%rename (__create_component) UI_CreateComponent;
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super);
%rename (__create_window) UI_CreateWindow;
uiWindow_t* UI_CreateWindow (const char* type, const char* name, const char* super);

/* expose lua helper functions */
%luacode {

function ufoui.create_window (name, super)
	ufo.print("entering support function: create_window\n")
	local node=ufoui.__create_window ("window", name, super)
	return node
end

function ufoui.create_component (type, name, super)
	return ufoui.__create_component (type, name, super)
end

function ufoui.create_control (parent, type, name, super)
	return ufoui.__create_control (parent, type, name, super)
end

}
