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
#include "../../../shared/ufotypes.h"
#include "../../../common/cvar.h"
#include "../../../common/scripts.h"
#include "../../../common/scripts_lua.h"

/* import common client functions */
#include "../../cl_renderer.h"

/* import ui specific functions */
#include "../ui_behaviour.h"
#include "../ui_data.h"
#include "../ui_dataids.h"
#include "../ui_node.h"
#include "../ui_nodes.h"
#include "../ui_main.h"
#include "../ui_sprite.h"

#include "../node/ui_node_abstractnode.h"
#include "../node/ui_node_abstractoption.h"
#include "../node/ui_node_abstractscrollable.h"
#include "../node/ui_node_abstractscrollbar.h"
#include "../node/ui_node_abstractvalue.h"

#include "../node/ui_node_bar.h"
#include "../node/ui_node_button.h"
#include "../node/ui_node_checkbox.h"
#include "../node/ui_node_item.h"
#include "../node/ui_node_model.h"
#include "../node/ui_node_panel.h"
#include "../node/ui_node_string.h"
#include "../node/ui_node_text.h"
#include "../node/ui_node_textentry.h"
#include "../node/ui_node_window.h"

#include "../ui_lua.h"


/*
	typedefs only visible for SWIG, used for subclassing uiNode_t (see below for more details). Note
	that uiAbstractNode_t is missing from the list, since this is the uiNode_t type.
*/
typedef uiNode_t uiAbstractOptionNode_t;
typedef uiNode_t uiAbstractScrollableNode_t;
typedef uiNode_t uiAbstractScrollbarNode_t;
typedef uiNode_t uiAbstractValueNode_t;

typedef uiNode_t uiBarNode_t;
typedef uiNode_t uiButtonNode_t;
typedef uiNode_t uiCheckBoxNode_t;
typedef uiNode_t uiItemNode_t;
typedef uiNode_t uiModelNode_t;
typedef uiNode_t uiPanelNode_t;
typedef uiNode_t uiStringNode_t;
typedef uiNode_t uiTextNode_t;
typedef uiNode_t uiTextEntryNode_t;
typedef uiNode_t uiTextureNode_t;
typedef uiNode_t uiVScrollBarNode_t;
typedef uiNode_t uiWindowNode_t;

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

/** @brief linked into ui_global.sharedData - defined in UI scripts via dataId property */
%rename (DataIds) uiDataIDs_t;
enum uiDataIDs_t {
	TEXT_NULL,		/**< default value, should not be used */
	TEXT_STANDARD,
	TEXT_LIST,
	TEXT_LIST2,
	TEXT_UFOPEDIA,
	TEXT_UFOPEDIA_REQUIREMENT,
	TEXT_BUILDINGS,
	TEXT_BUILDING_INFO,
	TEXT_RESEARCH,
	TEXT_POPUP,
	TEXT_POPUP_INFO,
	TEXT_AIRCRAFT_LIST,
	TEXT_AIRCRAFT_INFO,
	TEXT_MULTISELECTION,
	TEXT_PRODUCTION_LIST,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_EMPLOYEE,
	TEXT_MOUSECURSOR_RIGHT,
	TEXT_PRODUCTION_QUEUED,
	TEXT_STATS_MISSION,
	TEXT_STATS_BASES,
	TEXT_STATS_NATIONS,
	TEXT_STATS_EMPLOYEES,
	TEXT_STATS_COSTS,
	TEXT_STATS_INSTALLATIONS,
	TEXT_STATS_7,
	TEXT_BASE_LIST,
	TEXT_BASE_INFO,
	TEXT_TRANSFER_LIST,
	TEXT_TRANSFER_LIST_AMOUNT,
	TEXT_TRANSFER_LIST_TRANSFERED,
	TEXT_MOUSECURSOR_PLAYERNAMES,
	TEXT_CARGO_LIST,
	TEXT_CARGO_LIST_AMOUNT,
	TEXT_UFOPEDIA_MAILHEADER,
	TEXT_UFOPEDIA_MAIL,
	TEXT_MARKET_NAMES,
	TEXT_MARKET_STORAGE,
	TEXT_MARKET_MARKET,
	TEXT_MARKET_PRICES,
	TEXT_CHAT_WINDOW,
	TEXT_AIREQUIP_1,
	TEXT_AIREQUIP_2,
	TEXT_BASEDEFENCE_LIST,
	TEXT_TIPOFTHEDAY,
	TEXT_GENERIC,
	TEXT_XVI,
	TEXT_MOUSECURSOR_TOP,
	TEXT_MOUSECURSOR_BOTTOM,
	TEXT_MOUSECURSOR_LEFT,
	TEXT_MESSAGEOPTIONS,
	TEXT_UFORECOVERY_NATIONS,
	TEXT_UFORECOVERY_UFOYARDS,
	TEXT_UFORECOVERY_CAPACITIES,
	TEXT_MATERIAL_STAGES,
	TEXT_IRCCONTENT,
	TEXT_IRCUSERS,
	TEXT_MULTIPLAYER_USERLIST,
	TEXT_MULTIPLAYER_USERTEAM,
	TEXT_ITEMDESCRIPTION,
	TEXT_MISSIONBRIEFING,
	TEXT_MISSIONBRIEFING_TITLE,
	TEXT_MISSIONBRIEFING_VICTORY_CONDITIONS,

	OPTION_LANGUAGES,
	OPTION_JOYSTICKS,
	OPTION_VIDEO_RESOLUTIONS,
	OPTION_SINGLEPLAYER_SKINS,
	OPTION_MULTIPLAYER_SKINS,
	OPTION_UFOPEDIA,
	OPTION_UFOS,
	OPTION_DROPSHIPS,
	OPTION_BASELIST,
	OPTION_TEAMDEFS,
	OPTION_PRODUCTION_REQUIREMENTS,
	OPTION_CAMPAIGN_LIST,

	LINESTRIP_FUNDING,
	LINESTRIP_COLOR,

	UI_MAX_DATAID
};

%rename (LongLines) longlines_t;
typedef enum {
	LONGLINES_WRAP,
	LONGLINES_CHOP,
	LONGLINES_PRETTYCHOP,

	LONGLINES_LAST
} longlines_t;


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
//	expose scroll
///////////////////////////////////////////////////////////////////////////////////////////////////

struct uiScroll_t {
};
%extend uiScroll_t {
	int viewpos () { return $self->viewPos; };
	int viewsize () { return $self->viewSize; };
	bool fullsize () { return $self->fullSize; };

	void set_fullsize (bool value) { $self->fullSize = (value ? 1 : 0); };
	bool set_values (int pos, int size, bool full) { return $self->set (pos, size, (full ? 1 : 0)); };

	bool moveto (int pos) { return $self->move(pos); };
	bool movedelta (int delta) { return $self->moveDelta (delta); };
};

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
    %rename (on_change) lua_onChange;
    LUA_EVENT lua_onChange; /**< references the event in lua: on_change (node) */
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
	int bordersize () { return $self->border; };
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
	uiNode_t* operator[] (const char* name) { return UI_GetNode(node, name); };

	void append_node (uiNode_t* node) { UI_AppendNode($self, node); };
	void insert_node (uiNode_t* node, uiNode_t* prev) { UI_InsertNode($self, prev, node); };

	void set_left (float value) { $self->box.pos[0] = value; };
	void set_top (float value) { $self->box.pos[1] = value; };
	void set_widht (float value) { $self->box.size[0] = value; };
	void set_height (float value) { $self->box.size[1] = value; };

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
	void set_bordersize (int size) { $self->border = size; };
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

	/*
		By implementing a __getitem and __setitem method here, we can extend the lua representation of the ui node
		in lua with additional functions. References to these functions are stored inside the C application, so
		lua can safely forget them.
	*/
	void __setitem (const char* name, LUA_METHOD fcn) { UI_Node_SetItem($self, name, fcn); };
	LUA_METHOD __getitem(const char* name) { LUA_METHOD fcn = UI_Node_GetItem($self, name); return fcn; };

	void add_classmethod(const char* name, LUA_METHOD fcn) { UI_AddBehaviourMethod($self->behaviour, name, fcn); };
	void add_nodemethod (const char* name, LUA_METHOD fcn) { UI_AddNodeMethod($self, name, fcn); };
};
/*
	The following defines derived "classes" from uiNode. This solves the problem of all nodes being structures
	of type uiNode_t where the actual "class" is found in the behaviour value. In the interface file we actually
	implement the class hierarcy by typecasting the uiNode_t structure into various derived classes. Then we
	use SWIG's extend mechanism to provide direct support for the properties and methods in the derived classes
	and direct these back to a call into the behaviour class.
*/

struct uiAbstractOptionNode_t: uiNode_t {
};
%extend uiAbstractOptionNode_t {
	int dataid () { return UI_EXTRADATA($self, abstractOptionExtraData_t).dataId; };
    int count () { return UI_EXTRADATA($self, abstractOptionExtraData_t).count; };

	void set_dataid (const char* name) { UI_EXTRADATA($self, abstractOptionExtraData_t).dataId = UI_GetDataIDByName(name); };
	void set_background (const char* name) { UI_AbstractOption_SetBackgroundByName($self, name); };

	%rename (on_viewchange) lua_onViewChange;
	LUA_EVENT lua_onViewChange; 		/**< references the event in lua: on_viewchange (node) */
};
/*
	SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
	to bring the values from the EXTRADATA structures to the lua class.
*/
%{
static LUA_EVENT uiAbstractOptionNode_t_lua_onViewChange_get(uiAbstractOptionNode_t* node) {
	return UI_EXTRADATA(node, abstractOptionExtraData_t).lua_onViewChange;
}
static LUA_EVENT uiAbstractOptionNode_t_lua_onViewChange_set(uiAbstractOptionNode_t* node, LUA_EVENT fn) {
	return UI_EXTRADATA(node, abstractOptionExtraData_t).lua_onViewChange;
}
%}

struct uiAbstractScrollableNode_t: uiNode_t {
};
%extend uiAbstractScrollableNode_t {
	int viewpos () { return UI_EXTRADATA($self, abstractScrollableExtraData_t).scrollY.viewPos; };
	int viewsize () { return UI_EXTRADATA($self, abstractScrollableExtraData_t).scrollY.viewSize; };
	int fullsize () { return UI_EXTRADATA($self, abstractScrollableExtraData_t).scrollY.fullSize; };

	void pageup () { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->pageUp($self); };
	void pagedown () { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->pageDown($self); };
	void moveup () { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->moveUp($self); };
	void movedown () { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->moveDown($self); };
	void movehome () { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->moveHome($self); };
	void moveend () { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->moveEnd($self); };

	void set_viewpos (int pos) { UI_EXTRADATA($self, abstractScrollableExtraData_t).scrollY.viewPos = pos; };
	void set_viewsize (int size) { UI_EXTRADATA($self, abstractScrollableExtraData_t).scrollY.viewSize = size; };
	void set_fullsize (int size) { UI_EXTRADATA($self, abstractScrollableExtraData_t).scrollY.fullSize = size; };

	%rename (on_viewchange) lua_onViewChange;
	LUA_EVENT lua_onViewChange; 		/**< references the event in lua: on_viewchange (node) */
};
/*
	SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
	to bring the values from the EXTRADATA structures to the lua class.
*/
%{
static LUA_EVENT uiAbstractScrollableNode_t_lua_onViewChange_get(uiAbstractScrollableNode_t* node) {
	return UI_EXTRADATA(node, abstractScrollableExtraData_t).lua_onViewChange;
}
static LUA_EVENT uiAbstractScrollableNode_t_lua_onViewChange_set(uiAbstractScrollableNode_t* node, LUA_EVENT fn) {
	return UI_EXTRADATA(node, abstractScrollableExtraData_t).lua_onViewChange;
}
%}

struct uiAbstractScrollbarNode_t: uiNode_t {
};
%extend uiAbstractScrollbarNode_t {
	bool is_autoshowscroll () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).hideWhenUnused; };
	int current () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).pos; };
	int viewsize () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).viewsize; };
	int fullsize () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).fullsize; };

	void set_autoshowscroll (bool value) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).hideWhenUnused = value; };
	void set_current (int pos) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).pos; };
	void set_viewsize (int size) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).viewsize = size; };
	void set_fullsize (int size) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).fullsize = size; };
};

struct uiAbstractValueNode_t: uiNode_t {
};
%extend uiAbstractValueNode_t {
	float min () { return static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->getMin($self); };
	float max () { return static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->getMax($self); };
	float value () { return static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->getValue($self); };
	float delta () { return static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->getDelta($self); };
	float lastdiff () { return UI_EXTRADATA($self, abstractValueExtraData_t).lastdiff; };
	float shiftmultiplier () { return UI_EXTRADATA($self, abstractValueExtraData_t).shiftIncreaseFactor; };

	void inc_value () { static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->incValue ($self); };
	void dec_value () { static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->decValue ($self);};

	void set_range (float min, float max) { static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->setRange ($self, min, max);  };
	void set_value (float value) { static_cast<uiAbstractValueNode*>($self->behaviour->manager.get())->setValue ($self, value); };
};

%rename (uiBar) uiBarNode_t;
struct uiBarNode_t: uiAbstractValueNode_t {
};
%extend uiBarNode_t {
	bool is_readonly () { return UI_EXTRADATA($self, barExtraData_t).readOnly; };
	bool is_nohover () { return UI_EXTRADATA($self, barExtraData_t).noHover; };

	int direction () { return UI_EXTRADATA($self, barExtraData_t).orientation; };

	void set_direction (int value) { UI_EXTRADATA($self, barExtraData_t).orientation = (align_t)value; };
	void set_readonly (bool value) { UI_EXTRADATA($self, barExtraData_t).readOnly = value; };
	void set_nohover (bool value) { UI_EXTRADATA($self, barExtraData_t).noHover = value; };
};

%rename (uiButton) uiButtonNode_t;
struct uiButtonNode_t: uiNode_t {
};
%extend uiButtonNode_t {
	bool flipicon() { return UI_EXTRADATA($self, buttonExtraData_t).flipIcon; };

	void set_flipicon (bool value) { UI_EXTRADATA($self, buttonExtraData_t).flipIcon = value; };
	void set_background (const char* name) { UI_Button_SetBackgroundByName($self, name); };
	void set_icon (const char* name) { UI_Button_SetIconByName($self, name); };
};

%rename (uiCheckBox) uiCheckBoxNode_t;
struct uiCheckBoxNode_t: uiNode_t {
};
%extend uiCheckBoxNode_t {
	void set_background (const char* name) { UI_CheckBox_SetBackgroundByName($self, name); };
	void set_iconchecked (const char* name) { UI_CheckBox_SetIconCheckedByName($self, name); };
	void set_iconunchecked (const char* name) { UI_CheckBox_SetIconUncheckedByName($self, name); };
	void set_iconunknown (const char* name) { UI_CheckBox_SetIconUnknownByName($self, name); };
};

%rename (uiModel) uiModelNode_t;
struct uiModelNode_t: uiNode_t {
};
%extend uiModelNode_t {
	bool is_autoscale () { return UI_EXTRADATA($self, modelExtraData_t).autoscale; };
	bool is_mouserotate () { return UI_EXTRADATA($self, modelExtraData_t).rotateWithMouse; }

	vec3_struct_t* angles () { return (vec3_struct_t*)(UI_EXTRADATA($self, modelExtraData_t).angles); };
	vec3_struct_t* origin () { return (vec3_struct_t*)(UI_EXTRADATA($self, modelExtraData_t).origin); };
	vec3_struct_t* omega () { return (vec3_struct_t*)(UI_EXTRADATA($self, modelExtraData_t).omega); };
	vec3_struct_t* scale () { return (vec3_struct_t*)(UI_EXTRADATA($self, modelExtraData_t).scale); };

	char* model () { return const_cast<char*>(UI_EXTRADATA($self, modelExtraData_t).model); };
	char* skin () { return const_cast<char*>(UI_EXTRADATA($self, modelExtraData_t).skin); };
	char* animation () { return const_cast<char*>(UI_EXTRADATA($self, modelExtraData_t).animation); };

	void set_autoscale (bool value) { UI_EXTRADATA($self, modelExtraData_t).autoscale = value; };
	void set_mouserotate (bool value) { UI_EXTRADATA($self, modelExtraData_t).rotateWithMouse = value; };
	void set_angles (float a1, float a2, float a3) { VectorSet(UI_EXTRADATA($self, modelExtraData_t).angles, a1, a2, a3); };

	void set_model (const char* name) { UI_Model_SetModelSource($self, name); };
	void set_skin (const char* name) { UI_Model_SetSkinSource($self, name); };
	void set_animation (const char* name) { UI_Model_SetAnimationSource($self, name); };
};

%rename (uiItem) uiItemNode_t;
struct uiItemNode_t: uiModelNode_t {
};
%extend uiItemNode_t {
	bool is_containerlike () { return UI_EXTRADATA($self, modelExtraData_t).containerLike; };

	void set_containerlike (bool value) { UI_EXTRADATA($self, modelExtraData_t).containerLike = value; };
};

%rename (uiPanel) uiPanelNode_t;
struct uiPanelNode_t: uiAbstractScrollableNode_t {
};
%extend uiPanelNode_t {
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

%rename (uiString) uiStringNode_t;
struct uiStringNode_t: uiNode_t {
};
%extend uiStringNode_t {
	int longlines () { return UI_EXTRADATA($self, stringExtraData_t).longlines; };

	void set_longlines (int value) { UI_EXTRADATA($self, stringExtraData_t).longlines = value; };
};

%rename (uiText) uiTextNode_t;
struct uiTextNode_t: uiAbstractScrollableNode_t {
};
%extend uiTextNode_t {
	int dataid () { return UI_EXTRADATA($self, textExtraData_t).dataID; };
	int lineheight () { return UI_EXTRADATA($self, textExtraData_t).lineHeight; };
	int lineselected () { return UI_EXTRADATA($self, textExtraData_t).textLineSelected; };
	int longlines () { return UI_EXTRADATA($self, textExtraData_t).longlines; };
	char* textselected () { return const_cast<char*>(UI_EXTRADATA($self, textExtraData_t).textSelected); };
	int tabwidth () { return UI_EXTRADATA($self, textExtraData_t).tabWidth; }

	void set_dataid (int id) { UI_EXTRADATA($self, textExtraData_t).dataID = id; };
	void set_longlines (int value) { UI_EXTRADATA($self, textExtraData_t).longlines =  value; };
	void set_lineheight (int value) { UI_EXTRADATA($self, textExtraData_t).lineHeight = value; };
	void set_lineselected (int line) { UI_TextNodeSelectLine($self, line); };
	void set_tabwidth (int value) { UI_EXTRADATA($self, textExtraData_t).tabWidth = value; };
};

%rename (uiTextEntry) uiTextEntryNode_t;
struct uiTextEntryNode_t: uiNode_t {
};
%extend uiTextEntryNode_t {
	bool is_password () { return UI_EXTRADATA($self, textEntryExtraData_t).isPassword; };
	bool is_clickoutabort () {return UI_EXTRADATA($self, textEntryExtraData_s).clickOutAbort; };

	int cursorposition () { return UI_EXTRADATA($self, textEntryExtraData_s).cursorPosition; };

	void set_password (bool value) { UI_EXTRADATA($self, textEntryExtraData_s).isPassword = value; };
	void set_clickoutabort (bool value) { UI_EXTRADATA($self, textEntryExtraData_s).clickOutAbort = value; };
	void set_background (const char* name) { UI_TextEntry_SetBackgroundByName($self, name); };

	%rename (on_textabort) lua_onTextEntryAbort;
    LUA_EVENT lua_onTextEntryAbort; /**< references the event in lua: on_textabort(node) */
};
/*
	SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
	to bring the values from the EXTRADATA structures to the lua class.
*/
%{
static LUA_EVENT uiTextEntryNode_t_lua_onTextEntryAbort_get(uiTextEntryNode_t* node) {
	return UI_EXTRADATA(node, textEntryExtraData_s).lua_onTextEntryAbort;
}
static LUA_EVENT uiTextEntryNode_t_lua_onTextEntryAbort_set(uiTextEntryNode_t* node, LUA_EVENT fn) {
	return UI_EXTRADATA(node, textEntryExtraData_s).lua_onTextEntryAbort;
}
%}

%rename (uiTexture) uiTextureNode_t;
struct uiTextureNode_t: uiNode_t {
};
%extend uiTextureNode_t {
	void set_source (const char* name) { UI_Node_SetImage($self, name); };
};

%rename (uiVScrollbar) uiVScrollbarNode_t;
struct uiVScrollBarNode_t: uiAbstractScrollbarNode_t {
};
%extend uiVScrollbarNode_t {
};

%rename (uiWindow) uiWindowNode_t;
struct uiWindowNode_t: uiNode_t {
};
%extend uiWindowNode_t {
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
static LUA_EVENT uiWindowNode_t_lua_onWindowOpened_get(uiWindowNode_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened;
}
static void uiWindowNode_t_lua_onWindowOpened_set (uiWindowNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened = fn;
}
static LUA_EVENT uiWindowNode_t_lua_onWindowClosed_get(uiWindowNode_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed;
}
static void uiWindowNode_t_lua_onWindowClosed_set (uiWindowNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed = fn;
}
%}

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose special ui nodes
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
struct uiFunc_t: uiNode_t {
};
%extend uiFunc_t {
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose window management
///////////////////////////////////////////////////////////////////////////////////////////////////

/* expose uiNode creation functions */
%rename (create_control) UI_CreateControl;
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super);
//%rename (__create_window) UI_CreateWindow;
//uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super);

/* define uiNode subtypes creation functions */
%inline %{
static uiBarNode_t* UI_CreateBar (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "bar", name, super);
}
static uiButtonNode_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "button", name, super);
}
static uiCheckBoxNode_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "checkbox", name, super);
}
static uiItemNode_t* UI_CreateItem (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "item", name, super);
}
static uiModelNode_t* UI_CreateModel (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "model", name, super);
}
static uiPanelNode_t* UI_CreatePanel (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "panel", name, super);
}
static uiStringNode_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "string", name, super);
}
static uiTextNode_t* UI_CreateText (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "text", name, super);
}
static uiTextEntryNode_t* UI_CreateTextEntry (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "textentry", name, super);
}
static uiTextureNode_t* UI_CreateTexture (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "texture", name, super);
}
static uiVScrollBarNode_t* UI_CreateVScrollbar (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "vscrollbar", name, super);
}
static uiWindowNode_t* UI_CreateWindow (const char* name, const char* super) {
	return UI_CreateWindow("window", name, super);
}
%}

/* expose uiNode subtypes creation functions to lua */
%rename (create_bar) UI_CreateBar;
uiBarNode_t* UI_CreateBar (uiNode_t* parent, const char* name, const char* super);
%rename (create_button) UI_CreateButton;
uiButtonNode_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super);
%rename (create_checkbox) UI_CreateCheckBox;
uiCheckBoxNode_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super);
%rename (create_item) UI_CreateItem;
uiItemNode_t* UI_CreateItem (uiNode_t* parent, const char* name, const char* super);
%rename (create_model) UI_CreateModel;
uiModelNode_t* UI_CreateModel (uiNode_t* parent, const char* name, const char* super);
%rename (create_panel) UI_CreatePanel;
uiPanelNode_t* UI_CreatePanel (uiNode_t* parent, const char* name, const char* super);
%rename (create_string) UI_CreateString;
uiStringNode_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super);
%rename (create_text) UI_CreateText;
uiTextNode_t* UI_CreateText (uiNode_t* parent, const char* name, const char* super);
%rename (create_textentry) UI_CreateTextEntry;
uiStringNode_t* UI_CreateTextEntry (uiNode_t* parent, const char* name, const char* super);
%rename (create_texture) UI_CreateTexture;
uiTextureNode_t* UI_CreateTexture (uiNode_t* parent, const char* name, const char* super);
%rename (create_vscrollbar) UI_CreateVScrollbar;
uiVScrollBarNode_t* UI_CreateVScrollbar (uiNode_t* parent, const char* name, const char* super);
%rename (create_window) UI_CreateWindow;
uiWindowNode_t* UI_CreateWindow (const char* name, const char* super);
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

