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

/*
	This code exposes the node structures from the ui to lua. In lua, the structures are accessible
	as a set of classes with inheritance following the inheritance defined by the manager classes.

	As a rule of thumb, all fields are exposed by the following convenction.
	- a field node->F will get a lua method node:F() for reading
	- a field node->F will get a lua method node:set_F(value) for writing

	All fields are turned to lowercase with.

	Exception to these are the events. Events are exposed as direct accessible fields in lua so they
	follow the following convenction:
	- an event node->E will get a lua event node.E for reading and writing.

    Depending on the event, the signature can vary, however all events will have a first argument named
    'sender' wich is set to the instance the event belongs to.
*/

/* expose everything in lua using a single table 'ufo' */
%module ufo

%{
/* import headers into the interface so they can be used */
#include <typeinfo>

/* import common functions */
#include "../../../shared/shared.h"
#include "../../../shared/vector.h"
#include "../../../common/cvar.h"
#include "../../../common/scripts.h"
#include "../../../common/scripts_lua.h"

/* import ui specific functions */
#include "../ui_main.h"
#include "../ui_behaviour.h"
#include "../ui_sprite.h"
#include "../ui_nodes.h"
#include "../ui_node.h"

#include "../node/ui_node_abstractnode.h"
#include "../node/ui_node_abstractoption.h"
#include "../node/ui_node_abstractscrollable.h"
#include "../node/ui_node_abstractscrollbar.h"
#include "../node/ui_node_abstractvalue.h"

#include "../node/ui_node_bar.h"
#include "../node/ui_node_button.h"
#include "../node/ui_node_checkbox.h"
#include "../node/ui_node_panel.h"
#include "../node/ui_node_string.h"
#include "../node/ui_node_window.h"

#include "../ui_lua.h"

/*
	typedefs only visible for SWIG, used for subclassing uiNode_t (see below for more details). Note
	that uiAbstractNode_t is missing from the list, since this is the uiNode_t type.
*/
typedef uiNode_t uiAbstractValue_t;
typedef uiNode_t uiAbstractOption_t;
typedef uiNode_t uiAbstractScrollable_t;
typedef uiNode_t uiAbstractScrollbar_t;

typedef uiNode_t uiBar_t;
typedef uiNode_t uiButton_t;
typedef uiNode_t uiCheckBox_t;
typedef uiAbstractScrollable_t uiPanel_t;
typedef uiNode_t uiString_t;
typedef uiNode_t uiTexture_t;
typedef uiNode_t uiWindow_t;

typedef uiNode_t uiFunc_t;

// todo: implement other ui node classes

/*
   This function queries the SWIG type table for a type information structure. It is used in combination
   with the typemap for converting return values that specify uiNode_t*.
*/
void* UI_SWIG_TypeQuery (const char* name) {
	swig_type_info* info=SWIG_TypeQuery(name);
	if (!info) {
		info = SWIG_TypeQuery("uiNode_t *");
	}
	return info;
}

/*
	This function returns the SWIG typename for the given node.
*/
const char* UI_SWIG_NodeTypeName (void* node) {
	swig_type_info* info = (swig_type_info*)((uiNode_t*)node)->behaviour->lua_SWIG_typeinfo;
	return info->str;
}

%}

/* typemap for converting lua function to a reference to be used by C */
%typemap(in) LUA_FUNCTION {
	$1 = (LUA_FUNCTION)luaL_ref (L, LUA_REGISTRYINDEX);
}
%typemap(in) LUA_EVENT {
	$1 = (LUA_EVENT)luaL_ref (L, LUA_REGISTRYINDEX);
}
%typemap(in) LUA_METHOD {
	$1 = (LUA_METHOD)luaL_ref (L, LUA_REGISTRYINDEX);
}
/* typemap for converting a reference to a lua function */
%typemap(out) LUA_FUNCTION {
	lua_rawgeti(L, LUA_REGISTRYINDEX, $1); SWIG_arg++;
}
%typemap(out) LUA_METHOD {
	lua_rawgeti(L, LUA_REGISTRYINDEX, $1); SWIG_arg++;
}

/* typemap for dynamic casting uiNode_t* into lua table corresponding to the actual behaviour class
   (so a uiNode_t* representing a button will become a uiButton table in lua). The value of lua_SWIG_typeinfo
   is set in UI_RegisterXXXX functions. */
%typemap(out) uiNode_t* {
	swig_type_info* info=(swig_type_info*)$1->behaviour->lua_SWIG_typeinfo;
	SWIG_NewPointerObj(L, $1, info, 0); SWIG_arg++;
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose constants and typedefs
///////////////////////////////////////////////////////////////////////////////////////////////////

%rename (ContentAlign) align_t;
typedef enum {
	ALIGN_UL,
	ALIGN_UC,
	ALIGN_UR,
	ALIGN_CL,
	ALIGN_CC,
	ALIGN_CR,
	ALIGN_LL,
	ALIGN_LC,
	ALIGN_LR,
	ALIGN_UL_RSL,
	ALIGN_UC_RSL,
	ALIGN_UR_RSL,
	ALIGN_CL_RSL,
	ALIGN_CC_RSL,
	ALIGN_CR_RSL,
	ALIGN_LL_RSL,
	ALIGN_LC_RSL,
	ALIGN_LR_RSL,
} align_t;

%rename (LayoutAlign) layoutAlign_t;
typedef enum {
	LAYOUTALIGN_NONE = 0,

	/* vertical and horizontal flag bits */
	LAYOUTALIGN_H_MASK    = 0x03,
	LAYOUTALIGN_H_LEFT    = 0x01,
	LAYOUTALIGN_H_MIDDLE  = 0x02,
	LAYOUTALIGN_H_RIGHT   = 0x03,
	LAYOUTALIGN_V_MASK    = 0x0C,
	LAYOUTALIGN_V_TOP     = 0x04,
	LAYOUTALIGN_V_MIDDLE  = 0x08,
	LAYOUTALIGN_V_BOTTOM  = 0x0C,

	/* common alignment */
	LAYOUTALIGN_TOPLEFT     = (LAYOUTALIGN_V_TOP | LAYOUTALIGN_H_LEFT),
	LAYOUTALIGN_TOP         = (LAYOUTALIGN_V_TOP | LAYOUTALIGN_H_MIDDLE),
	LAYOUTALIGN_TOPRIGHT    = (LAYOUTALIGN_V_TOP | LAYOUTALIGN_H_RIGHT),
	LAYOUTALIGN_LEFT        = (LAYOUTALIGN_V_MIDDLE | LAYOUTALIGN_H_LEFT),
	LAYOUTALIGN_MIDDLE      = (LAYOUTALIGN_V_MIDDLE | LAYOUTALIGN_H_MIDDLE),
	LAYOUTALIGN_RIGHT       = (LAYOUTALIGN_V_MIDDLE | LAYOUTALIGN_H_RIGHT),
	LAYOUTALIGN_BOTTOMLEFT  = (LAYOUTALIGN_V_BOTTOM | LAYOUTALIGN_H_LEFT),
	LAYOUTALIGN_BOTTOM      = (LAYOUTALIGN_V_BOTTOM | LAYOUTALIGN_H_MIDDLE),
	LAYOUTALIGN_BOTTOMRIGHT = (LAYOUTALIGN_V_BOTTOM | LAYOUTALIGN_H_RIGHT),

	/* special align, everything bigger 0x10 */
	LAYOUTALIGN_SPECIAL     = 0x10,

	/* pack and star layout manager only */
	LAYOUTALIGN_FILL,

	LAYOUTALIGN_MAX,
	LAYOUTALIGN_ENSURE_32BIT = 0x7FFFFFFF
} layoutAlign_t;

%rename (LayoutPanel) panelLayout_t;
typedef enum {
	LAYOUT_NONE,
	LAYOUT_TOP_DOWN_FLOW,
	LAYOUT_LEFT_RIGHT_FLOW,
	LAYOUT_BORDER,
	LAYOUT_PACK,
	LAYOUT_STAR,
	LAYOUT_CLIENT,
	LAYOUT_COLUMN,

	LAYOUT_MAX,
	LAYOUT_ENSURE_32BIT = 0x7FFFFFFF
} panelLayout_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose cvar
///////////////////////////////////////////////////////////////////////////////////////////////////

struct cvar_t {
};
%extend cvar_t {
	char* name () { return $self->name; };

	char* as_string () { return $self->string; };
	float as_float () { return $self->value; };
	int as_integer () { return $self->integer; };
};

%rename (findvar) Cvar_FindVar;
cvar_t* Cvar_FindVar (const char* varName);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose ui nodes
///////////////////////////////////////////////////////////////////////////////////////////////////

/* expose node structure */
%rename(uiNode) uiNode_t;
struct uiNode_t {
	/* events */
	%rename (on_click) lua_onClick;
    %rename (on_rightclick) lua_onRightClick;
    %rename (on_middleclick) lua_onMiddleClick;
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
    %rename (on_mouseenter) lua_onMouseEnter;
    %rename (on_mouseleave) lua_onMouseLeave;
    LUA_EVENT lua_onMouseEnter; /**< references the event in lua: on_mouseenter (node) */
    LUA_EVENT lua_onMouseLeave; /**< references the event in lua: on_mouseleave (node) */
};
%extend uiNode_t {
	bool is_window () { return UI_Node_IsWindow($self); };
	bool is_disabled () { return UI_Node_IsDisabled($self); };
	bool is_invisible () { return $self->invis; };
	bool is_ghost () { return $self->ghost; };
	bool is_flashing () { return $self->flash; };

	float left () { return $self->box.pos[0]; };
	float top () { return $self->box.pos[1]; };
	float widht () { return $self->box.size[0]; };
	float height () { return $self->box.size[1]; };
	int borderthickness () { return $self->border; };
	char* name () { return const_cast<char*>($self->name); };
	char* type () { return const_cast<char*>($self->behaviour->name); };
	char* text () { return const_cast<char*>(UI_Node_GetText($self)); };
	char* font () { return const_cast<char*>($self->font); };
	char* image () { return const_cast<char*>($self->image); };
	int contentalign () { return $self->contentAlign; };
	int layoutalign () { return $self->align; };
	float flashspeed () { return $self->flashSpeed; };
	int padding () { return $self->padding; };

	/* common navigation */
	uiNode_t* first () { return $self->firstChild; };
	uiNode_t* last () { return $self->lastChild; };
	uiNode_t* next () { return $self->next; };
	uiNode_t* parent () { return $self->parent; };
	uiNode_t* root () { return $self->root; };

	void append_node (uiNode_t* node) { UI_AppendNode($self, node); };
	void insert_node (uiNode_t* node, uiNode_t* prev) { UI_InsertNode($self, prev, node); };

	void set_flashing (bool value) { $self->flash = value; };
	void set_flashspeed (float value) { $self->flashSpeed = value; };
	void set_invisible (bool value) { $self->invis = value; };
	void set_ghost (bool value) { $self->ghost = value; };
	void set_pos (float x, float y) { Vector2Set($self->box.pos, x, y); }
	void set_size (float w, float h) { Vector2Set($self->box.size, w, h); }
	void set_color (float r, float g, float b, float a) { Vector4Set($self->color, r, g, b, a); };
	void set_disabledcolor (float r, float g, float b, float a) { Vector4Set($self->disabledColor, r, g, b, a); };
	void set_flashcolor (float r, float g, float b, float a) { Vector4Set($self->flashColor, r, g, b, a); };
	void set_selectcolor (float r, float g, float b, float a) { Vector4Set($self->selectedColor, r, g, b, a); };
	void set_backgroundcolor (float r, float g, float b, float a) { Vector4Set($self->bgcolor, r, g, b, a); };
	void set_bordercolor (float r, float g, float b, float a) { Vector4Set($self->bordercolor, r, g, b, a); };
	void set_text (const char* text) { UI_Node_SetText($self, text); };
	void set_font (const char* name) { UI_Node_SetFont($self, name); };
	void set_image (const char* name) { UI_Node_SetImage ($self, name); };
	void set_contentalign (int value) { $self->contentAlign = value; };
	void set_layoutalign (int value) { $self->align = value; };
	void set_tooltip (const char* text) { UI_Node_SetTooltip($self, text); };
	void set_disabled (bool value) { UI_Node_SetDisabled($self, value); };
	void set_borderthickness (int value) { $self->border = value; };
	void set_padding (int value) { $self->padding = value; };

	void __setitem (const char* name, LUA_METHOD fcn) { Com_Printf("__setitem: %s = %d\n", name, fcn); UI_Node_SetItem($self, name, fcn); };
	LUA_METHOD __getitem(const char* name) { LUA_METHOD fcn = UI_Node_GetItem($self, name); Com_Printf("__getitem: %s = %d\n", name, fcn); return fcn; };

	void add_classmethod(const char* name, LUA_METHOD fcn) { UI_AddBehaviourMethod($self->behaviour, name, fcn); };
};
/*
	The following defines derived "classes" from uiNode. This solves the problem of all nodes being structures
	of type uiNode_t where the actual "class" is found in the behaviour value. In the interface file we actually
	implement the class hierarcy by typecasting the uiNode_t structure into various derived classes. Then we
	use SWIG's extend mechanism to provide direct support for the properties and methods in the derived classes
	and direct these back to a call into the behaviour class.
*/

struct uiAbstractScrollable_t: uiNode_t {
};
%extend uiAbstractScrollable_t {
};

struct uiAbstractValue_t: uiNode_t {
};
%extend uiAbstractValue_t {
};

%rename (uiButton) uiButton_t;
struct uiButton_t: uiNode_t {
};
%extend uiButton_t {
	bool flipicon() { return UI_EXTRADATA($self, buttonExtraData_t).flipIcon; };

	void set_flipicon (bool value) { UI_EXTRADATA($self, buttonExtraData_t).flipIcon = value; };
	void set_background (const char* name) { UI_Button_SetBackgroundByName($self, name); };
	void set_icon (const char* name) { UI_Button_SetIconByName($self, name); };
};

%rename (uiCheckBox) uiCheckBox_t;
struct uiCheckBox_t {
};
%extend uiCheckBox_t {
	void set_background (const char* name) { UI_CheckBox_SetBackgroundByName($self, name); };
	void set_iconchecked (const char* name) { UI_CheckBox_SetIconCheckedByName($self, name); };
	void set_iconunchecked (const char* name) { UI_CheckBox_SetIconUncheckedByName($self, name); };
	void set_iconunknown (const char* name) { UI_CheckBox_SetIconUnknownByName($self, name); };
};

%rename (uiPanel) uiPanel_t;
struct uiPanel_t: uiAbstractScrollable_t {
};
%extend uiPanel_t {
	bool is_wheelscrollable () { return UI_EXTRADATA($self, panelExtraData_t).wheelScrollable; };

	int layout () { return UI_EXTRADATA($self, panelExtraData_t).layout; };
	int layoutmargin () { return UI_EXTRADATA($self, panelExtraData_t).layoutMargin; };
	int layoutcolumns () { return UI_EXTRADATA($self, panelExtraData_t).layoutColumns; };

	void set_layout (int value) { UI_EXTRADATA($self, panelExtraData_t).layout = (panelLayout_t)value; };
	void set_layoutmargin (int value) { UI_EXTRADATA($self, panelExtraData_t).layoutMargin = value; };
	void set_layoutcolumns (int value) { UI_EXTRADATA($self, panelExtraData_t).layoutColumns = value; };
	void set_wheelscrollable (bool value) { UI_EXTRADATA($self, panelExtraData_t).wheelScrollable = value; };
	void set_background (const char* name) { UI_Panel_SetBackgroundByName($self, name); };

};

%rename (uiString) uiString_t;
struct uiString_t: uiNode_t {
};
%extend uiString_t {
};

struct uiTexture_t: uiNode_t {
};
%extend uiTexture_t {
	void set_source (const char* name) { UI_Node_SetImage($self, name); };
};

%rename (uiWindow) uiWindow_t;
struct uiWindow_t: uiNode_t {
};
%extend uiWindow_t {
	bool is_fullscreen () { return UI_EXTRADATA($self, windowExtraData_t).isFullScreen; };
	bool is_modal () { return UI_EXTRADATA($self, windowExtraData_t).modal; }

	void add_dragbutton () { UI_EXTRADATA($self, windowExtraData_t).dragButton = true; };
	void add_closebutton () { UI_EXTRADATA($self, windowExtraData_t).closeButton = true; };

	void close () { UI_PopWindow (false); };
	void open () { UI_PushWindow($self->name, nullptr, nullptr); };

	void set_background (const char* name) { UI_Window_SetBackgroundByName($self, name); };
	void set_fullscreen (bool value) { UI_EXTRADATA($self, windowExtraData_t).isFullScreen = value; };
	void set_modal (bool value) { UI_EXTRADATA($self, windowExtraData_t).modal = value; };
	void set_fill (bool value) { UI_EXTRADATA($self, windowExtraData_t).fill = value; };

	%rename (on_windowopened) lua_onWindowOpened;
	%rename (on_windowclosed) lua_onWindowClosed;
    LUA_EVENT lua_onWindowOpened; /**< references the event in lua: on_loaded (node) */
    LUA_EVENT lua_onWindowClosed; /**< references the event in lua: on_activate (node) */
};
/*
	SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
	to bring the values from the EXTRADATA structures to the lua class.
*/
%{
static LUA_EVENT uiWindow_t_lua_onWindowOpened_get(uiWindow_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened;
}
static void uiWindow_t_lua_onWindowOpened_set (uiWindow_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened = fn;
}
static LUA_EVENT uiWindow_t_lua_onWindowClosed_get(uiWindow_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed;
}
static void uiWindow_t_lua_onWindowClosed_set (uiWindow_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed = fn;
}
%}

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose special ui nodes
///////////////////////////////////////////////////////////////////////////////////////////////////

struct uiFunc_t: uiNode_t {
};
%extend uiFunc_t {
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose window management
///////////////////////////////////////////////////////////////////////////////////////////////////

/* expose uiNode creation functions */
%rename (__create_control) UI_CreateControl;
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super);
%rename (__create_window) UI_CreateWindow;
uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super);

/* define uiNode subtypes creation functions */
%inline %{
static uiButton_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "button", name, super);
}
static uiCheckBox_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "checkbox", name, super);
}
static uiPanel_t* UI_CreatePanel (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "panel", name, super);
}
static uiString_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "string", name, super);
}
static uiWindow_t* UI_CreateWindow (const char* name, const char* super) {
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
%rename (create_panel) UI_CreatePanel;
uiPanel_t* UI_CreatePanel (uiNode_t* parent, const char* name, const char* super);
%rename (create_window) UI_CreateWindow;
uiWindow_t* UI_CreateWindow (const char* name, const char* super);

/* expose component creation fuction */
%rename (create_component) UI_CreateComponent;
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super);

/* expose window functions */
%rename (pop_window) UI_PopWindow;
void UI_PopWindow (bool all);
%rename (push_window) UI_PushWindow;
uiNode_t* UI_PushWindow (const char* name, const char* parentName, linkedList_t* params);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose command functions
///////////////////////////////////////////////////////////////////////////////////////////////////

%rename (cmd) Cbuf_AddText;
void Cbuf_AddText (const char* command);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose common functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/* expose common functions to lua */
%rename (print) Com_Printf;
void Com_Printf (const char* fmt);
%rename (dprint) Com_DPrintf;
void Com_DPrintf(int level, const char* fmt);
%rename (error) Com_Error;
void Com_Error(int code, const char* fmt);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose .ufo as lua module
///////////////////////////////////////////////////////////////////////////////////////////////////

/* expose registration functions for callbacks */
%rename(register_onload) UI_RegisterHandler_OnLoad;
void UI_RegisterHandler_OnLoad (LUA_FUNCTION fcn);

%luacode {
--[[
	@brief Creates an extendible wrapper around the userdata table supplied by SWIG.
	@param[in] cppObject The SWIG wrapper that holds the C++ object, typically a uiNode
	@note This code is used to create a proxy in lua around the userdata object SWIG provides. The SWIG
	userdata object cannot be extended with new lua based functions and properties. Using a proxy however
	this becomes possible.
--]]
local function create_proxy(cppObject)
	local proxy = {}
    local wrapper_metatable = {}

	function wrapper_metatable.__index(self, key)
		local ret = rawget(self, key)
		if(not ret) then
			ret = cppObject[key]
			if(type(ret) == "function") then
				return function(self, ...)
					return ret(cppObject, ...)
				end
			else
				return ret
			end
		else
			return ret
		end
	end


    setmetatable(proxy, wrapper_metatable)
    return proxy
end

%}
