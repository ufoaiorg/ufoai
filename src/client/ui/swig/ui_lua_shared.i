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
#include "../../../shared/vector.h"
#include "../../../common/scripts_lua.h"

/* import ui specific functions */
#include "../ui_main.h"
#include "../ui_behaviour.h"
#include "../ui_sprite.h"
#include "../ui_nodes.h"
#include "../ui_node.h"

#include "../node/ui_node_abstractnode.h"
#include "../node/ui_node_button.h"
#include "../node/ui_node_checkbox.h"
#include "../node/ui_node_string.h"
#include "../node/ui_node_window.h"

#include "../ui_lua.h"

/* typedefs only visible for SWIG, used for subclassing uiNode_t (see below for more details). */
typedef uiNode_t uiWindow_t;
typedef uiNode_t uiButton_t;
typedef uiNode_t uiString_t;
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
	%rename (on_loaded) lua_onLoaded;
	%rename (on_activate) lua_onActivate;
    LUA_EVENT lua_onLoaded; /**< references the event in lua: on_loaded (node) */
    LUA_EVENT lua_onActivate; /**< references the event in lua: on_activate (node) */
};
%extend uiNode_t {
	bool is_window () { return UI_Node_IsWindow($self); };
	bool is_disabled () { return UI_Node_IsDisabled($self); };

	float left () { return $self->box.pos[0]; };
	float top () { return $self->box.pos[1]; };
	float widht () { return $self->box.size[0]; };
	float height () { return $self->box.size[1]; };
	int borderthickness () { return $self->border; };

	void append_node (uiNode_t* node) { UI_AppendNode($self, node); };
	void insert_node (uiNode_t* node, uiNode_t* prev) { UI_InsertNode($self, prev, node); };

	void set_pos (float x, float y) { Vector2Set($self->box.pos, x, y); }
	void set_size (float w, float h) { Vector2Set($self->box.size, w, h); }
	void set_color (float r, float g, float b, float a) { Vector4Set($self->color, r, g, b, a); };
	void set_disabledcolor (float r, float g, float b, float a) { Vector4Set($self->disabledColor, r, g, b, a); };
	void set_flashcolor (float r, float g, float b, float a) { Vector4Set($self->flashColor, r, g, b, a); };
	void set_selectcolor (float r, float g, float b, float a) { Vector4Set($self->selectedColor, r, g, b, a); };
	void set_backgroundcolor (float r, float g, float b, float a) { Vector4Set($self->bgcolor, r, g, b, a); };
	void set_bordercolor (float r, float g, float b, float a) { Vector4Set($self->bordercolor, r, g, b, a); };
	void set_text (const char* text) { UI_Node_SetText($self, text); };
	void set_tooltip (const char* text) { UI_Node_SetTooltip($self, text); };
	void set_disabled (bool value) { UI_Node_SetDisabled($self, value); };
	void set_borderthickness (int value) { $self->border = value; };
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
	bool is_fullscreen () { return UI_EXTRADATA($self, windowExtraData_t).isFullScreen; };

	void set_background (const char* name) { UI_Window_SetBackgroundByName($self, name); };
	void set_fullscreen (bool value) { UI_EXTRADATA($self, windowExtraData_t).isFullScreen = value; };

	%rename (on_windowopened) lua_onWindowOpened;
	%rename (on_windowclosed) lua_onWindowClosed;
    LUA_EVENT lua_onWindowOpened; /**< references the event in lua: on_loaded (node) */
    LUA_EVENT lua_onWindowClosed; /**< references the event in lua: on_activate (node) */
};
/*
	SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used
	to bring the values from the EXTRADATA structures to the lua class.
*/
%{
static int uiWindow_t_lua_onWindowOpened_get(uiWindow_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened;
}
static void uiWindow_t_lua_onWindowOpened_set (uiWindow_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened = fn;
}
static int uiWindow_t_lua_onWindowClosed_get(uiWindow_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed;
}
static int uiWindow_t_lua_onWindowClosed_set (uiWindow_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed = fn;
}
%}

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

%rename (uiString) uiString_t;
struct uiString_t {
};
%extend uiString_t {
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
uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super);

/*
  The following is inline code that is only visible in the SWIG generated module. It allows us to
  expose control creation functions to lua that return a lua "class" derived from uiNode.
*/
/* define uiNode subtypes creation functions */
%inline %{
uiButton_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "button", name, super);
};
uiCheckBox_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "checkbox", name, super);
}
uiString_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "string", name, super);
}
uiWindow_t* UI_CreateWindow (const char* name, const char* super) {
	return UI_CreateWindow("window", name, super);
}
%}

/* expose uiNode subtypes creation functions to lua */
%rename (create_button) UI_CreateButton;
uiButton_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super);
%rename (create_checkbox) UI_CreateCheckBox;
uiCheckBox_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super);
%rename (create_string) UI_CreateString;
uiString_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super);

%rename (create_window) UI_CreateWindow;
uiWindow_t* UI_CreateWindow (const char* name, const char* super);

