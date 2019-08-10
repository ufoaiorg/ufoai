/**
	@file Interface file for SWIG to generarte lua ui binding.
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
/* disable all casting warnings (enabled at the bottom line of this file) */
#pragma GCC diagnostic ignored "-Wcast-qual"

/* common client stuff */
#include "../../../shared/shared.h"
#include "../../../shared/vector.h"
#include "../../../shared/ufotypes.h"
#include "../../../common/cvar.h"
#include "../../../common/scripts.h"
#include "../../../common/scripts_lua.h"

#include "../../cl_renderer.h"
#include "../../cl_inventory.h"

/* ui specific stuff */
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
#include "../node/ui_node_base.h"
#include "../node/ui_node_baseinventory.h"
#include "../node/ui_node_battlescape.h"
#include "../node/ui_node_button.h"
#include "../node/ui_node_checkbox.h"
#include "../node/ui_node_container.h"
#include "../node/ui_node_controls.h"
#include "../node/ui_node_data.h"
#include "../node/ui_node_geoscape.h"
#include "../node/ui_node_image.h"
#include "../node/ui_node_item.h"
#include "../node/ui_node_linechart.h"
#include "../node/ui_node_messagelist.h"
#include "../node/ui_node_model.h"
#include "../node/ui_node_option.h"
#include "../node/ui_node_optionlist.h"
#include "../node/ui_node_optiontree.h"
#include "../node/ui_node_panel.h"
#include "../node/ui_node_radar.h"
#include "../node/ui_node_radiobutton.h"
#include "../node/ui_node_rows.h"
#include "../node/ui_node_selectbox.h"
#include "../node/ui_node_sequence.h"
#include "../node/ui_node_special.h"
#include "../node/ui_node_spinner.h"
#include "../node/ui_node_string.h"
#include "../node/ui_node_tab.h"
#include "../node/ui_node_tbar.h"
#include "../node/ui_node_text.h"
#include "../node/ui_node_text2.h"
#include "../node/ui_node_textentry.h"
#include "../node/ui_node_textlist.h"
#include "../node/ui_node_texture.h"
#include "../node/ui_node_timer.h"
#include "../node/ui_node_video.h"
#include "../node/ui_node_vscrollbar.h"
#include "../node/ui_node_window.h"
#include "../node/ui_node_zone.h"

#include "../ui_lua.h"

/* other game stuff */
#include "../../../game/inv_shared.h"

/**
 * typedefs only visible for SWIG, used for subclassing uiNode_t (see below for more details). Note
 * that uiAbstractNode_t is missing from the list, since this is the uiNode_t type.
 */
typedef uiNode_t uiAbstractOptionNode_t;
typedef uiNode_t uiAbstractScrollableNode_t;
typedef uiNode_t uiAbstractScrollbarNode_t;
typedef uiNode_t uiAbstractValueNode_t;

typedef uiNode_t uiBarNode_t;
typedef uiNode_t uiBaseLayoutNode_t;
typedef uiNode_t uiBaseInventoryNode_t;
typedef uiNode_t uiButtonNode_t;
typedef uiNode_t uiCheckBoxNode_t;
typedef uiNode_t uiConFuncNode_t;
typedef uiNode_t uiContainerNode_t;
typedef uiNode_t uiDataNode_t;
typedef uiNode_t uiGeoscapeNode_t;
typedef uiNode_t uiImageNode_t;
typedef uiNode_t uiItemNode_t;
typedef uiNode_t uiLineChartNode_t;
typedef uiNode_t uiMessageListNode_t;
typedef uiNode_t uiModelNode_t;
typedef uiNode_t uiOptionNode_t;
typedef uiNode_t uiOptionListNode_t;
typedef uiNode_t uiOptionTreeNode_t;
typedef uiNode_t uiPanelNode_t;
typedef uiNode_t uiRadarNode_t;
typedef uiNode_t uiRadioButtonNode_t;
typedef uiNode_t uiRowsNode_t;
typedef uiNode_t uiSelectBoxNode_t;
typedef uiNode_t uiSequenceNode_t;
typedef uiNode_t uiSpinnerNode_t;
typedef uiNode_t uiStringNode_t;
typedef uiNode_t uiTabNode_t;
typedef uiNode_t uiTBarNode_t;
typedef uiNode_t uiTextNode_t;
typedef uiNode_t uiText2Node_t;
typedef uiNode_t uiTextEntryNode_t;
typedef uiNode_t uiTextListNode_t;
typedef uiNode_t uiTextureNode_t;
typedef uiNode_t uiTimerNode_t;
typedef uiNode_t uiVideoNode_t;
typedef uiNode_t uiVScrollBarNode_t;
typedef uiNode_t uiWidgetNode_t; /* note: ufo class = "controls" */
typedef uiNode_t uiWindowNode_t;
typedef uiNode_t uiZoneNode_t;

// skipped: uiFuncNode, uiNullNode

/**
 * @brief This function queries the SWIG type table for a type information structure. It is used in combination
 * with the typemap for converting return values that specify uiNode_t*.
 */
void* UI_SWIG_TypeQuery (const char* name)
{
	swig_type_info* info = SWIG_TypeQuery(name);
	if (!info) {
		info = SWIG_TypeQuery("uiNode_t *");
	}
	return info;
}

/**
 * @brief This function returns the SWIG typename for the given node.
 */
const char* UI_SWIG_NodeTypeName (void* node)
{
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
	if ($1) {
		swig_type_info* info = (swig_type_info*)$1->behaviour->lua_SWIG_typeinfo;
		SWIG_NewPointerObj(L, $1, info, 0); SWIG_arg++;
	} else {
		SWIG_NewPointerObj(L, nullptr, nullptr, 0); SWIG_arg++;
	}
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
	TEXT_PRODUCTION_LIST,
	TEXT_PRODUCTION_AMOUNT,
	TEXT_PRODUCTION_INFO,
	TEXT_MOUSECURSOR_RIGHT,
	TEXT_PRODUCTION_QUEUED,
	TEXT_BASE_LIST,
	TEXT_MOUSECURSOR_PLAYERNAMES,
	TEXT_UFOPEDIA_MAILHEADER,
	TEXT_UFOPEDIA_MAIL,
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

enum spinnerMode_t {
	/**
	 * Normal mode. The upper side of the node increase the value
	 * and the lower side of the node decrease the value
	 */
	SPINNER_NORMAL,
	/**
	 * Only increase mode. The whole node increase the value.
	 */
	SPINNER_ONLY_INCREASE,
	/**
	 * Only decrease mode. The whole node decrease the value.
	 */
	SPINNER_ONLY_DECREASE
};

/**
 * @brief A list of filter types in the market and production view.
 * @note Run-time only, please do not use this in savegame/structures and the like.
 * Please also do not use hardcoded numbers to access this (e.g. in a .ufo script).
 * @sa inv_shared.c:INV_ItemMatchesFilter
 * @sa inv_shared.c:INV_GetFilterTypeID
 */
typedef enum {
	/* All types starting with "FILTER_S_" contain items that can be used on/with soldiers (i.e. personal equipment). */
	FILTER_S_PRIMARY,		/**< All 'Primary' weapons and their ammo for soldiers (Except for heavy weapons). */
	FILTER_S_SECONDARY,		/**< All 'Secondary' weapons and their ammo for soldiers. */
	FILTER_S_HEAVY,			/**< Heavy weapons for soldiers. */
	FILTER_S_MISC,			/**< Misc. soldier equipment (i.e. everything else that is not in the other soldier-item filters) */
	FILTER_S_ARMOUR,		/**< Armour for soldiers. */
	FILTER_S_IMPLANT,		/**< Implants */
	MAX_SOLDIER_FILTERTYPES,

	/* Non-soldier items */
	FILTER_CRAFTITEM,	/**< Aircraft equipment. */
	FILTER_UGVITEM,		/**< Heavy equipment like tanks (i.e. these are actually employees) and their parts.
						 * Some of the content are special normal items (like for soldiers).
						 * The UGVs themself are specially handled.*/
	FILTER_AIRCRAFT,	/**< Aircrafts. */
	FILTER_DUMMY,		/**< Everything that is not in _any_ of the other filter types.
						 * Mostly plot-relevant stuff, unproducible stuff and stuff. */
	FILTER_DISASSEMBLY,

	MAX_FILTERTYPES,

	FILTER_ENSURE_32BIT = 0x7FFFFFFF
} itemFilterTypes_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose scroll
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
%rename(uiScroll) uiScroll_t;
struct uiScroll_t {
};
%extend uiScroll_t {
	int viewpos () { return $self->viewPos; };
	int viewsize () { return $self->viewSize; };
	int fullsize () { return $self->fullSize; };

	bool set_values (int pos, int size, bool full) { return $self->set (pos, size, full); };
	bool set_viewpos (int pos) { return $self->set (pos, -1, -1); };
	bool set_viewsize (int size) { return $self->set (-1, size, -1); };
	bool set_fullsize (bool value) { return $self->set (-1, -1, value); };

	bool moveto (int pos) { return $self->move(pos); };
	bool movedelta (int delta) { return $self->moveDelta (delta); };
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose cvar
///////////////////////////////////////////////////////////////////////////////////////////////////

%rename (cvar) cvar_t;
struct cvar_t {
};
%extend cvar_t {
	char* name () { return $self->name; };

	char* as_string () { return $self->string; };
	float as_float () { return $self->value; };
	int as_integer () { return $self->integer; };

	void set_value (float number) { Cvar_SetValue($self->name, number);  };
	void set_value (const char* text) { Cvar_Set($self->name, "%s", text); };
};

%rename (findvar) Cvar_FindVar;
cvar_t* Cvar_FindVar (const char* varName);
%rename (getvar) Cvar_Get;
cvar_t* Cvar_Get (const char* var_name, const char* var_value = nullptr, int flags = 0, const char* desc = nullptr);
%rename (delvar) Cvar_Delete;
bool Cvar_Delete(const char* var_name);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose inventory item
///////////////////////////////////////////////////////////////////////////////////////////////////

%rename (invDef) invDef_t;
struct invDef_t {
};
%extend invDef_t {
	char* name() { return $self->name; };
};

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
	%rename (on_visiblewhen) lua_onVisibleWhen;
	LUA_EVENT lua_onVisibleWhen; /**< references the event in lua: on_visible (node) */
};
%extend uiNode_t {
	bool is_window () { return UI_Node_IsWindow($self); };
	bool is_disabled () { return UI_Node_IsDisabled($self); };
	bool is_invisible () { return UI_Node_IsInvisible($self); };
	bool is_ghost () { return UI_Node_IsGhost($self); };
	bool is_flashing () { return UI_Node_IsFlashing($self); };
	bool is_function () { return UI_Node_IsFunction($self); };
	bool is_virtual () { return UI_Node_IsVirtual($self); };
	bool is_abstract () { return UI_Node_IsAbstract($self); };

	float left () { return $self->box.pos[0]; };
	float top () { return $self->box.pos[1]; };
	float width () { return $self->box.size[0]; };
	float height () { return $self->box.size[1]; };
	int bordersize () { return $self->border; };
	const char* name () { return $self->name; };
	const char* type () { return $self->behaviour->name; };
	const char* text () { return UI_Node_GetText($self); };
	const char* font () { return $self->font; };
	const char* image () { return $self->image; };
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
	uiNode_t* child (const char* name) { return UI_GetNode($self, name); };
	uiNode_t* find (const char* name) { return UI_FindNode($self, name); };

	void append_node (uiNode_t* node) { UI_AppendNode($self, node); };
	void insert_node (uiNode_t* node, uiNode_t* prev) { UI_InsertNode($self, prev, node); };
	void move_node (uiNode_t* node, uiNode_t* prev) { UI_MoveNode($self, prev, node); };
	void delete_node () { UI_DeleteNode ($self); };
	void remove_children () { UI_DeleteAllChild ($self); };

	void set_left (float value) { UI_NodeSetBox($self, value, -1, -1, -1); };
	void set_top (float value) { UI_NodeSetBox($self, -1, value, -1, -1); };
	void set_width (float value) { UI_NodeSetBox($self, -1, -1, value, -1); };
	void set_height (float value) { UI_NodeSetBox($self, -1, -1, -1, value); };
	void set_box (float left, float top, float width, float height) { UI_NodeSetBox($self, left, top, width, height); };

	void set_flashing (bool value) { $self->flash = value; };
	void set_flashspeed (float value) { $self->flashSpeed = value; };
	void set_invisible (bool value) { $self->invis = value; };
	void set_ghost (bool value) { $self->ghost = value; };
	void set_pos (float x, float y) { UI_NodeSetPos($self, x, y); }
	void set_size (float w, float h) { UI_NodeSetSize($self, w, h); }
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
	 * By implementing a __getitem and __setitem method here, we can extend the lua representation of the ui node
	 * in lua with additional functions. References to these functions are stored inside the C application, so
	 * lua can safely forget them.
	*/
	void __setitem (const char* name, LUA_METHOD fcn) { UI_Node_SetItem($self, name, fcn); };
	LUA_METHOD __getitem(const char* name) { LUA_METHOD fcn = UI_Node_GetItem($self, name); return fcn; };

	void add_classmethod(const char* name, LUA_METHOD fcn) { UI_AddBehaviourMethod($self->behaviour, name, fcn); };
	void add_nodemethod (const char* name, LUA_METHOD fcn) { UI_AddNodeMethod($self, name, fcn); };
};
/*
 * The following defines derived "classes" from uiNode. This solves the problem of all nodes being structures
 * of type uiNode_t where the actual "class" is found in the behaviour value. In the interface file we actually
 * implement the class hierarcy by typecasting the uiNode_t structure into various derived classes. Then we
 * use SWIG's extend mechanism to provide direct support for the properties and methods in the derived classes
 * and direct these back to a call into the behaviour class.
 */

%rename (uiAbstractOptionNode) uiAbstractOptionNode_t;
struct uiAbstractOptionNode_t: uiNode_t {
};
%extend uiAbstractOptionNode_t {
	int dataid () { return UI_AbstractOption_GetDataId($self); };
	int count () { return UI_AbstractOption_GetCount($self); };
	int lineheight () { return UI_EXTRADATA($self, abstractOptionExtraData_t).lineHeight; };

	const char* cvar () { return UI_AbstractOption_GetCvar($self); };

	int current () { return UI_AbstractOption_Scroll_Current($self); };
	int viewsize () { return UI_AbstractOption_Scroll_ViewSize($self); };
	int fullsize () { return UI_AbstractOption_Scroll_FullSize($self); };

	void set_dataid (int id) { UI_AbstractOption_SetDataId($self, id); };
	void set_cvar (const char* name) { UI_AbstractOption_SetCvar ($self, name); };
	void set_lineheight (int value) { UI_EXTRADATA($self, abstractOptionExtraData_t).lineHeight = value; };
	void set_background (const char* name) { UI_AbstractOption_SetBackgroundByName($self, name); };

	void set_current (int pos) { UI_AbstractOption_Scroll_SetCurrent($self, pos); };
	void set_viewsize (int size) { UI_AbstractOption_Scroll_SetViewSize($self, size); };
	void set_fullsize (int size) { UI_AbstractOption_Scroll_SetFullSize($self, size);};

	%rename (on_viewchange) lua_onViewChange;
	LUA_EVENT lua_onViewChange; 		/**< references the event in lua: on_viewchange (node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiAbstractOptionNode_t_lua_onViewChange_get(uiAbstractOptionNode_t* node) {
	return UI_EXTRADATA(node, abstractOptionExtraData_t).lua_onViewChange;
}
static void uiAbstractOptionNode_t_lua_onViewChange_set(uiAbstractOptionNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, abstractOptionExtraData_t).lua_onViewChange = fn;
}
%}

%rename (uiAbstractScrollableNode) uiAbstractScrollableNode_t;
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

	void set_viewpos (int pos) { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->setScrollY($self, pos, -1, -1); };
	void set_viewsize (int size) { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->setScrollY($self, -1, size, -1); };
	void set_fullsize (int size) { dynamic_cast<uiAbstractScrollableNode*>($self->behaviour->manager.get())->setScrollY($self, -1, -1, size); };

	%rename (on_viewchange) lua_onViewChange;
	LUA_EVENT lua_onViewChange; 		/**< references the event in lua: on_viewchange (node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiAbstractScrollableNode_t_lua_onViewChange_get(uiAbstractScrollableNode_t* node) {
	return UI_EXTRADATA(node, abstractScrollableExtraData_t).lua_onViewChange;
}
static void uiAbstractScrollableNode_t_lua_onViewChange_set(uiAbstractScrollableNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, abstractScrollableExtraData_t).lua_onViewChange = fn;
}
%}

%rename (uiAbstractScrollbarNode) uiAbstractScrollbarNode_t;
struct uiAbstractScrollbarNode_t: uiNode_t {
};
%extend uiAbstractScrollbarNode_t {
	bool is_autoshowscroll () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).hideWhenUnused; };

	int current () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).pos; };
	int viewsize () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).viewsize; };
	int fullsize () { return UI_EXTRADATA($self, abstractScrollbarExtraData_t).fullsize; };

	void set_autoshowscroll (bool value) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).hideWhenUnused = value; };
	void set_current (int pos) { UI_AbstractScrollbarNodeSet($self, pos); };
	void set_viewsize (int size) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).viewsize = size; };
	void set_fullsize (int size) { UI_EXTRADATA($self, abstractScrollbarExtraData_t).fullsize = size; };
};

%rename (uiAbstractValueNode) uiAbstractValueNode_t;
struct uiAbstractValueNode_t: uiNode_t {
};
%extend uiAbstractValueNode_t {
	float min () { return UI_AbstractValue_GetMin($self); };
	float max () { return UI_AbstractValue_GetMax($self); };
	float value () { return UI_AbstractValue_GetValue($self); };
	float delta () { return UI_AbstractValue_GetDelta($self); };
	float lastdiff () { return UI_AbstractValue_GetLastDiff($self); };
	float shiftmultiplier () { return UI_AbstractValue_GetShiftIncreaseFactor($self); };

	void inc_value () { UI_AbstractValue_IncValue ($self); };
	void dec_value () { UI_AbstractValue_DecValue ($self); };

	void set_range (float min, float max) { UI_AbstractValue_SetRange ($self, min, max); };
	void set_range (const char* min, const char* max) { UI_AbstractValue_SetRangeCvar($self, min, max); };
	void set_min (float min) { UI_AbstractValue_SetMin($self, min);  }
	void set_max (float max) { UI_AbstractValue_SetMax($self, max); }
	void set_value (float value) { UI_AbstractValue_SetValue($self, value); };
	void set_min (const char* min) { UI_AbstractValue_SetMinCvar($self, min); };
	void set_max (const char* max) { UI_AbstractValue_SetMaxCvar($self, max); };
	void set_value (const char* name) { UI_AbstractValue_SetValueCvar($self, name); };
	void set_delta (float delta) { UI_AbstractValue_SetDelta($self, delta); };
	void set_shiftmultiplier(float value) { UI_AbstractValue_SetShiftIncreaseFactor($self, value); };
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

%rename (uiBaseLayout) uiBaseLayoutNode_t;
struct uiBaseLayoutNode_t: uiNode_t {
};
%extend uiBaseLayoutNode_t {
	int baseid() { return UI_EXTRADATA($self, baseExtraData_t).baseid; };
	void set_baseid(int value) { UI_EXTRADATA($self, baseExtraData_t).baseid = value; };
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
struct uiCheckBoxNode_t: uiAbstractValueNode_t {
};
%extend uiCheckBoxNode_t {
	bool as_boolean () { return UI_CheckBox_ValueAsBoolean($self); };
	int as_integer() { return UI_CheckBox_ValueAsInteger($self); };

	void set_background (const char* name) { UI_CheckBox_SetBackgroundByName($self, name); };
	void set_iconchecked (const char* name) { UI_CheckBox_SetIconCheckedByName($self, name); };
	void set_iconunchecked (const char* name) { UI_CheckBox_SetIconUncheckedByName($self, name); };
	void set_iconunknown (const char* name) { UI_CheckBox_SetIconUnknownByName($self, name); };
	void toggle () { UI_CheckBox_Toggle($self); };
};

%rename (uiConFunc) uiConFuncNode_t;
struct uiConFuncNode_t: uiNode_t {
};
%extend uiConFuncNode_t {
};

%rename (uiContainer) uiContainerNode_t;
struct uiContainerNode_t: uiNode_t {
};
%extend uiContainerNode_t {
	int selectedid () { return UI_EXTRADATA($self, containerExtraData_t).lastSelectedId; };

	%rename (on_select) lua_onSelect;
	LUA_EVENT lua_onSelect; /**< references the event in lua: on_select(node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiContainerNode_t_lua_onSelect_get(uiContainerNode_t* node) {
	return UI_EXTRADATA(node, containerExtraData_t).lua_onSelect;
}
static void uiContainerNode_t_lua_onSelect_set(uiContainerNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, containerExtraData_t).lua_onSelect = fn;
}
%}

%rename (uiBaseInventory) uiBaseInventoryNode_t;
struct uiBaseInventoryNode_t: uiContainerNode_t {
};
%extend uiBaseInventoryNode_t {
	int filtertype () { return UI_EXTRADATA($self, baseInventoryExtraData_t).filterEquipType; };
	int columns () { return UI_EXTRADATA($self, baseInventoryExtraData_t).columns; };

	bool is_displayweapon () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayWeapon; };
	bool is_displayweaponammo () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayAmmoOfWeapon; };
	bool is_displayammo () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayAmmo; };
	bool is_displayimplant () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayImplant; };
	bool is_displayunavailable () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayUnavailableItem; };
	bool is_displayunavailableammo () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayUnavailableAmmoOfWeapon; };
	bool is_displayavailableontop () { return UI_EXTRADATA($self, baseInventoryExtraData_t).displayAvailableOnTop; };

	void set_displayweapon (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayWeapon = value; };
	void set_displayweaponammo (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayAmmoOfWeapon = value; };
	void set_displayammo (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayAmmo = value; };
	void set_displayimplant (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayImplant = value; };
	void set_displayunavailable (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayUnavailableItem = value; };
	void set_displayunavailableammo (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayUnavailableAmmoOfWeapon = value; };
	void set_displayavailableontop (bool value) { UI_EXTRADATA($self, baseInventoryExtraData_t).displayAvailableOnTop = value; };

	int viewpos () { return UI_EXTRADATA($self, baseInventoryExtraData_t).scrollY.viewPos; };
	int viewsize () { return UI_EXTRADATA($self, baseInventoryExtraData_t).scrollY.viewSize; };
	int fullsize () { return UI_EXTRADATA($self, baseInventoryExtraData_t).scrollY.fullSize; };
	void set_viewpos (int pos) { UI_EXTRADATA($self, baseInventoryExtraData_t).scrollY.viewPos = pos; };
	void set_viewsize (int size) { UI_EXTRADATA($self, baseInventoryExtraData_t).scrollY.viewSize = size; };
	void set_fullsize (int size) { UI_EXTRADATA($self, baseInventoryExtraData_t).scrollY.fullSize = size; };

	%rename (on_viewchange) lua_onViewChange;
	LUA_EVENT lua_onViewChange; 		/**< references the event in lua: on_viewchange (node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiBaseInventoryNode_t_lua_onViewChange_get(uiBaseInventoryNode_t* node)
{
	return UI_EXTRADATA(node, baseInventoryExtraData_t).lua_onViewChange;
}

static void uiBaseInventoryNode_t_lua_onViewChange_set(uiBaseInventoryNode_t* node, LUA_EVENT fn)
{
	UI_EXTRADATA(node, baseInventoryExtraData_t).lua_onViewChange = fn;
}
%}

%rename (uiData) uiDataNode_t;
struct uiDataNode_t: uiNode_t {
};
%extend uiDataNode_t {
	char* as_string () { return const_cast<char*>(UI_Node_GetText($self)); };
	int as_integer () { return UI_EXTRADATA($self, dataExtraData_t).integer; };
	float as_float () { return UI_EXTRADATA($self, dataExtraData_t).number; };

	void set_value (const char* value ) { UI_Node_SetText($self, value); };
	void set_value (int value) { UI_EXTRADATA($self, dataExtraData_t).integer = value; };
	void set_valuef (float value) { UI_EXTRADATA($self, dataExtraData_t).number = value; };
};

%rename (uiGeoscape) uiGeoscapeNode_t;
struct uiGeoscapeNode_t: uiNode_t {
};
%extend uiGeoscapeNode_t {
	int paddingright () { return UI_EXTRADATA($self, mapExtraData_t).paddingRight; };

	void set_paddingright (int value) { UI_EXTRADATA($self, mapExtraData_t).paddingRight = value; };

	void zoomin () { dynamic_cast<uiGeoscapeNode*>($self->behaviour->manager.get())->zoom($self, false); }
	void zoomout () { dynamic_cast<uiGeoscapeNode*>($self->behaviour->manager.get())->zoom($self, true); }
};

%rename (uiImage) uiImageNode_t;
struct uiImageNode_t: uiNode_t {
};
%extend uiImageNode_t {
	bool is_keepratio () { return UI_EXTRADATA($self, imageExtraData_t).preventRatio; }
	bool is_mousefx () { return UI_EXTRADATA($self, imageExtraData_t).mousefx; }

	vec2_struct_t* texh () { return (vec2_struct_t*)UI_EXTRADATA($self, imageExtraData_t).texh; };
	vec2_struct_t* texl () { return (vec2_struct_t*)UI_EXTRADATA($self, imageExtraData_t).texl; };

	void set_keepratio (bool value) { UI_EXTRADATA($self, imageExtraData_t).preventRatio = value; }
	void set_mousefx (bool value) { UI_EXTRADATA($self, imageExtraData_t).mousefx = value; }
	void set_source (const char* name) { UI_Node_SetImage($self, name); };
	void set_texh (float v1, float v2) { Vector2Set(UI_EXTRADATA($self, imageExtraData_t).texh, v1, v2); };
	void set_texl (float v1, float v2) { Vector2Set(UI_EXTRADATA($self, imageExtraData_t).texl, v1, v2); };
};

%rename (uiLineChart) uiLineChartNode_t;
struct uiLineChartNode_t: uiNode_t {
};
%extend uiLineChartNode_t {
	bool is_showaxes () { return UI_EXTRADATA($self, lineChartExtraData_t).displayAxes; };

	void set_showaxes(bool value) { UI_EXTRADATA($self, lineChartExtraData_t).displayAxes = value; };
	void set_axescolor(float r, float g, float b, float a) { Vector4Set(UI_EXTRADATA($self, lineChartExtraData_s).axesColor, r, g, b, a); };

	void clear () { UI_ClearLineChart($self); };
	void add_line (const char* id, bool visible, float r, float g, float b, float a, bool dots, int num_points) {
		float color[] = {r, g, b, a};
		UI_AddLineChartLine($self, id, visible, color, dots, num_points);
	};
	void add_point (const char* id, int x, int y) { UI_AddLineChartCoord($self, id, x, y); };
};

%rename (uiMessageList) uiMessageListNode_t;
struct uiMessageListNode_t: uiAbstractScrollableNode_t {
};
%extend uiMessageListNode_t {
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
	char* tag () { return const_cast<char*>(UI_EXTRADATA($self, modelExtraData_t).tag); };

	void set_autoscale (bool value) { UI_EXTRADATA($self, modelExtraData_t).autoscale = value; };
	void set_mouserotate (bool value) { UI_EXTRADATA($self, modelExtraData_t).rotateWithMouse = value; };

	void set_angles (float a1, float a2, float a3) { VectorSet(UI_EXTRADATA($self, modelExtraData_t).angles, a1, a2, a3); };
	void set_origin (float a1, float a2, float a3) { VectorSet(UI_EXTRADATA($self, modelExtraData_t).origin, a1, a2, a3); };
	void set_omega (float a1, float a2, float a3) { VectorSet(UI_EXTRADATA($self, modelExtraData_t).omega, a1, a2, a3); };
	void set_scale (float a1, float a2, float a3) { VectorSet(UI_EXTRADATA($self, modelExtraData_t).scale, a1, a2, a3); };

	void set_model (const char* name) { UI_Model_SetModelSource($self, name); };
	void set_skin (const char* name) { UI_Model_SetSkinSource($self, name); };
	void set_animation (const char* name) { UI_Model_SetAnimationSource($self, name); };
	void set_tag (const char* name) { UI_Model_SetTagSource($self, name); };
};

%rename (uiItem) uiItemNode_t;
struct uiItemNode_t: uiModelNode_t {
};
%extend uiItemNode_t {
	bool is_containerlike () { return UI_EXTRADATA($self, modelExtraData_t).containerLike; };

	void set_containerlike (bool value) { UI_EXTRADATA($self, modelExtraData_t).containerLike = value; };
};

%rename (uiOption) uiOptionNode_t;
struct uiOptionNode_t: uiNode_t {
};
%extend uiOptionNode_t {
	bool is_collapsed () { return UI_EXTRADATA($self, optionExtraData_t).collapsed; };
	bool is_flipicion () { return UI_EXTRADATA($self, optionExtraData_t).flipIcon; };
	bool is_truncated () { return UI_EXTRADATA($self, optionExtraData_t).truncated; };

	char* label () { return UI_EXTRADATA($self, optionExtraData_t).label; };
	char* value () { return UI_EXTRADATA($self, optionExtraData_t).value; };
	int count () { return UI_EXTRADATA($self, optionExtraData_t).childCount; };

	void set_label (const char* text) { UI_Option_SetLabel ($self, text); };
	void set_value (const char* text) { UI_Option_SetValue ($self, text); };
	void set_collapsed (bool value) { UI_EXTRADATA($self, optionExtraData_t).collapsed = value; };
	void set_flipicion (bool value) { UI_EXTRADATA($self, optionExtraData_t).flipIcon = value; };
	void set_truncated (bool value) { UI_EXTRADATA($self, optionExtraData_t).truncated = value; };
	void set_icon (const char* name) { UI_Option_SetIconByName ($self, name); };
};

%rename (uiOptionList) uiOptionListNode_t;
struct uiOptionListNode_t: uiAbstractOptionNode_t {
};
%extend uiOptionListNode_t {
};

%rename (uiOptionTree) uiOptionTreeNode_t;
struct uiOptionTreeNode_t: uiAbstractOptionNode_t {
};
%extend uiOptionTreeNode_t {
	void set_selectedvalue (const char* value) { UI_OptionTree_SelectValue($self, value); };
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

%rename (uiRadar) uiRadarNode_t;
struct uiRadarNode_t: uiNode_t {
};
%extend uiRadarNode_t {
};

%rename (uiRadioButton) uiRadioButtonNode_t;
struct uiRadioButtonNode_t: uiNode_t {
};
%extend uiRadioButtonNode_t {
	bool is_flipicon () { return UI_EXTRADATA($self, radioButtonExtraData_t).flipIcon; };

	char* as_string () { return UI_EXTRADATA($self, radioButtonExtraData_t).string; };
	float as_float () { return UI_EXTRADATA($self, radioButtonExtraData_t).value; };

	void set_value (const char* value) { UI_RadioButton_SetValue($self, value); };
	void set_value (float value) { UI_RadioButton_SetValue($self, value); };

	void set_flipicon (bool value) { UI_EXTRADATA($self, radioButtonExtraData_t).flipIcon = value; };
	void set_background (const char* name) { UI_RadioButton_SetBackgroundByName($self, name); };
	void set_icon (const char* name) { UI_RadioButton_SetIconByName($self, name); };
};

%rename (uiRows) uiRowsNode_t;
struct uiRowsNode_t: uiNode_t {
};
%extend uiRowsNode_t {
	int current () { return UI_EXTRADATA($self, rowsExtraData_t).current; };
	int lineheight () { return UI_EXTRADATA($self, rowsExtraData_t).lineHeight; };

	void set_current (int value) { UI_EXTRADATA($self, rowsExtraData_t).current = value; };
	void set_lineheight (int value) { UI_EXTRADATA($self, rowsExtraData_t).lineHeight = value; };
};

%rename (uiSelectBox) uiSelectBoxNode_t;
struct uiSelectBoxNode_t: uiAbstractOptionNode_t {
};
%extend uiSelectBoxNode_t {
};

%rename (uiSequence) uiSequenceNode_t;
struct uiSequenceNode_t: uiNode_t {
};
%extend uiSequenceNode_t {
	bool is_playing () { return UI_EXTRADATA($self, sequenceExtraData_t).playing; };

	void set_source (const char* name) { UI_Sequence_SetSource($self, name); };

	LUA_EVENT lua_onEnd; 		/**< references the event in lua: on_end(node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiSequenceNode_t_lua_onEnd_get(uiSequenceNode_t* node)
{
	return UI_EXTRADATA(node, sequenceExtraData_t).lua_onEnd;
}

static void uiSequenceNode_t_lua_onEnd_set(uiSequenceNode_t* node, LUA_EVENT fn)
{
	UI_EXTRADATA(node, sequenceExtraData_t).lua_onEnd = fn;
}
%}

%rename (uiSpinner) uiSpinnerNode_t;
struct uiSpinnerNode_t: uiAbstractValueNode_t {
};
%extend uiSpinnerNode_t {
	bool is_horizontal () { return UI_EXTRADATA($self, spinnerExtraData_t).horizontal; };
	bool is_inverted () { return UI_EXTRADATA($self, spinnerExtraData_t).inverted; };
	int mode () { return UI_EXTRADATA($self, spinnerExtraData_t).mode; };

	void set_background (const char* name) { UI_Spinner_SetBackgroundByName($self, name); };
	void set_topicon (const char* name) { UI_Spinner_SetTopIconByName($self, name); };
	void set_bottomicon (const char* name) { UI_Spinner_SetBottomIconByName($self, name); };
	void set_mode (int mode) { UI_EXTRADATA($self, spinnerExtraData_t).mode = mode; };
	void set_horizontal (bool value) { UI_EXTRADATA($self, spinnerExtraData_t).horizontal = value; };
	void set_inverted (bool value) { UI_EXTRADATA($self, spinnerExtraData_t).inverted = value; };
}

%rename (uiString) uiStringNode_t;
struct uiStringNode_t: uiNode_t {
};
%extend uiStringNode_t {
	int longlines () { return UI_EXTRADATA($self, stringExtraData_t).longlines; };

	void set_longlines (int value) { UI_EXTRADATA($self, stringExtraData_t).longlines = value; };
};

%rename (uiTab) uiTabNode_t;
struct uiTabNode_t: uiAbstractOptionNode_t {
};
%extend uiTabNode_t {
};

%rename (uiTBar) uiTBarNode_t;
struct uiTBarNode_t: uiAbstractValueNode_t {
};
%extend uiTBarNode_t {
	vec2_struct_t* texh () { return (vec2_struct_t*)UI_EXTRADATA($self, tbarExtraData_t).texh; };
	vec2_struct_t* texl () { return (vec2_struct_t*)UI_EXTRADATA($self, tbarExtraData_t).texl; };

	void set_source (const char* name) { UI_TBar_SetImage($self, name); };
	void set_texh (float v1, float v2) { Vector2Set(UI_EXTRADATA($self, tbarExtraData_t).texh, v1, v2); };
	void set_texl (float v1, float v2) { Vector2Set(UI_EXTRADATA($self, tbarExtraData_t).texl, v1, v2); };
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

%rename (uiText2) uiText2Node_t;
struct uiText2Node_t: uiTextNode_t {
};
%extend uiText2Node_t {
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

	void focus () { UI_TextEntryNodeFocus($self, nullptr); };
	void unfocus () { UI_TextEntryNodeUnFocus($self, nullptr); };

	%rename (on_textabort) lua_onTextEntryAbort;
	LUA_EVENT lua_onTextEntryAbort; /**< references the event in lua: on_textabort(node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiTextEntryNode_t_lua_onTextEntryAbort_get(uiTextEntryNode_t* node)
{
	return UI_EXTRADATA(node, textEntryExtraData_s).lua_onTextEntryAbort;
}

static void uiTextEntryNode_t_lua_onTextEntryAbort_set(uiTextEntryNode_t* node, LUA_EVENT fn)
{
	UI_EXTRADATA(node, textEntryExtraData_s).lua_onTextEntryAbort = fn;
}
%}

%rename (uiTextList) uiTextListNode_t;
struct uiTextListNode_t: uiTextNode_t {
};
%extend uiTextListNode_t {
};

%rename (uiTexture) uiTextureNode_t;
struct uiTextureNode_t: uiNode_t {
};
%extend uiTextureNode_t {
	void set_source (const char* name) { UI_Node_SetImage($self, name); };
};

%rename (uiTimer) uiTimerNode_t;
struct uiTimerNode_t: uiNode_t {
};
%extend uiTimerNode_t {
	int timeout () { return UI_EXTRADATA($self, timerExtraData_t).timeOut; }

	void set_timeout (int value) { UI_EXTRADATA($self, timerExtraData_t).timeOut = value; };

	LUA_EVENT lua_onEvent; 			/**< references the event in lua: on_event (node, x, y) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiTimerNode_t_lua_onEvent_get(uiTimerNode_t* node)
{
	return UI_EXTRADATA(node, timerExtraData_t).lua_onEvent;
}

static void uiTimerNode_t_lua_onEvent_set(uiTimerNode_t* node, LUA_EVENT fn)
{
	UI_EXTRADATA(node, timerExtraData_t).lua_onEvent = fn;
}
%}

%rename (uiVideo) uiVideoNode_t;
struct uiVideoNode_t: uiNode_t {
};
%extend uiVideoNode_t {
	bool is_nosound () { return UI_EXTRADATA($self, videoExtraData_t).nosound; };

	void set_nosound (bool value) { UI_EXTRADATA($self, videoExtraData_t).nosound = value; };
	void set_source (const char* name) { UI_Video_SetSource($self, name); };

	LUA_EVENT lua_onEnd; 		/**< references the event in lua: on_end(node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiVideoNode_t_lua_onEnd_get(uiVideoNode_t* node)
{
	return UI_EXTRADATA(node, videoExtraData_t).lua_onEnd;
}

static void uiVideoNode_t_lua_onEnd_set(uiVideoNode_t* node, LUA_EVENT fn)
{
	UI_EXTRADATA(node, videoExtraData_t).lua_onEnd = fn;
}
%}

%rename (uiVScrollbar) uiVScrollBarNode_t;
struct uiVScrollBarNode_t: uiAbstractScrollbarNode_t {
};
%extend uiVScrollBarNode_t {
};

%rename (uiWidget) uiWidgetNode_t;
struct uiWidgetNode_t: uiImageNode_t {
};
%extend uiWidgetNode_t {
};

%rename (uiWindow) uiWindowNode_t;
struct uiWindowNode_t: uiNode_t {
};
%extend uiWindowNode_t {
	bool is_fullscreen () { return UI_EXTRADATA($self, windowExtraData_t).isFullScreen; };
	bool is_modal () { return UI_EXTRADATA($self, windowExtraData_t).modal; }
	bool is_starlayout () { return UI_EXTRADATA($self, windowExtraData_t).starLayout; };

	void close () { UI_PopWindow (false); };
	void open () { UI_PushWindow($self->name, nullptr, nullptr); };

	void set_background (const char* name) { UI_Window_SetBackgroundByName($self, name); };
	void set_fullscreen (bool value) { UI_EXTRADATA($self, windowExtraData_t).isFullScreen = value; };
	void set_modal (bool value) { UI_EXTRADATA($self, windowExtraData_t).modal = value; };
	void set_fill (bool value) { dynamic_cast<uiWindowNode*>($self->behaviour->manager.get())->setFill($self, value); };
	void set_dragbutton (bool value) { UI_Window_SetDragButton($self, value); };
	void set_closebutton (bool value) { UI_Window_SetCloseButton($self, value); };
	void set_starlayout (bool value) { UI_EXTRADATA($self, windowExtraData_t).starLayout = value; };
	void set_preventtypingescape (bool value) { UI_EXTRADATA($self, windowExtraData_t).preventTypingEscape = value; };
	void set_noticepos(float x, float y) { Vector2Set(UI_EXTRADATA($self, windowExtraData_t).noticePos, x, y); };
	void set_dropdown (bool value) { UI_EXTRADATA($self, windowExtraData_t).dropdown = value; };

	%rename (on_windowopened) lua_onWindowOpened;
	%rename (on_windowactivate) lua_onWindowActivate;
	%rename (on_windowclosed) lua_onWindowClosed;
	LUA_EVENT lua_onWindowOpened; /**< references the event in lua: on_loaded (node) */
	LUA_EVENT lua_onWindowActivate; /**< references the event in lua: on_windowactivated (node) */
	LUA_EVENT lua_onWindowClosed; /**< references the event in lua: on_activate (node) */
};
/*
 * SWIG allows us to extend a class with properties, provided we supply the get/set wrappers. This is used here
 * to bring the values from the EXTRADATA structures to the lua class.
 */
%{
static LUA_EVENT uiWindowNode_t_lua_onWindowOpened_get(uiWindowNode_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened;
}
static void uiWindowNode_t_lua_onWindowOpened_set (uiWindowNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowOpened = fn;
}
static LUA_EVENT uiWindowNode_t_lua_onWindowActivate_get(uiWindowNode_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowActivate;
}
static void uiWindowNode_t_lua_onWindowActivate_set (uiWindowNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowActivate = fn;
}
static LUA_EVENT uiWindowNode_t_lua_onWindowClosed_get(uiWindowNode_t* node) {
	return UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed;
}
static void uiWindowNode_t_lua_onWindowClosed_set (uiWindowNode_t* node, LUA_EVENT fn) {
	UI_EXTRADATA(node, windowExtraData_t).lua_onWindowClosed = fn;
}
%}

%rename (uiZone) uiZoneNode_t;
struct uiZoneNode_t: uiNode_t {
};
%extend uiZoneNode_t {
	bool is_repeat () { return UI_EXTRADATA($self, zoneExtraData_t).repeat; }
	int clickdelay () { return UI_EXTRADATA($self, zoneExtraData_t).clickDelay; };

	void set_repeat (bool value) { UI_EXTRADATA($self, zoneExtraData_t).repeat = value; };
	void set_clickdelay (int value) { UI_EXTRADATA($self, zoneExtraData_t).clickDelay = value; };
};

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

/* define uiNode subtypes creation functions */
%inline %{
static uiBarNode_t* UI_CreateBar (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "bar", name, super);
}
static uiBaseLayoutNode_t* UI_CreateBaseLayout (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "baselayout", name, super);
}
static uiBaseInventoryNode_t* UI_CreateBaseInventory (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "baseinventory", name, super);
}
static uiButtonNode_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "button", name, super);
}
static uiCheckBoxNode_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "checkbox", name, super);
}
static uiConFuncNode_t* UI_CreateConFunc (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "confunc", name, super);
}
static uiContainerNode_t* UI_CreateContainer (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "container", name, super);
}
static uiDataNode_t* UI_CreateData (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "data", name, super);
}
static uiGeoscapeNode_t* UI_CreateGeoscape (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "geoscape", name, super);
}
static uiImageNode_t* UI_CreateImage (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "image", name, super);
}
static uiItemNode_t* UI_CreateItem (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "item", name, super);
}
static uiLineChartNode_t* UI_CreateLineChart(uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "linechart", name, super);
}
static uiMessageListNode_t* UI_CreateMessageList (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "messagelist", name, super);
}
static uiModelNode_t* UI_CreateModel (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "model", name, super);
}
static uiOptionNode_t* UI_CreateOption (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "option", name, super);
}
static uiOptionListNode_t* UI_CreateOptionList (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "optionlist", name, super);
}
static uiOptionTreeNode_t* UI_CreateOptionTree (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "optiontree", name, super);
}
static uiPanelNode_t* UI_CreatePanel (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "panel", name, super);
}
static uiRadarNode_t* UI_CreateRadar (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "radar", name, super);
}
static uiRadioButtonNode_t* UI_CreateRadioButton (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "radiobutton", name, super);
}
static uiRowsNode_t* UI_CreateRows (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "rows", name, super);
}
static uiSelectBoxNode_t* UI_CreateSelectBox (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "selectbox", name, super);
}
static uiSequenceNode_t* UI_CreateSequence (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "sequence", name, super);
}
static uiSpinnerNode_t* UI_CreateSpinner (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "spinner", name, super);
}
static uiStringNode_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "string", name, super);
}
static uiTabNode_t* UI_CreateTab (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "tab", name, super);
}
static uiTBarNode_t* UI_CreateTBar (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "tbar", name, super);
}
static uiTextNode_t* UI_CreateText (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "text", name, super);
}
static uiText2Node_t* UI_CreateText2 (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "text2", name, super);
}
static uiTextEntryNode_t* UI_CreateTextEntry (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "textentry", name, super);
}
static uiTextListNode_t* UI_CreateTextList (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "textlist", name, super);
}
static uiTextureNode_t* UI_CreateTexture (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "texture", name, super);
}
static uiTimerNode_t* UI_CreateTimer (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "timer", name, super);
}
static uiVideoNode_t* UI_CreateVideo (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "video", name, super);
}
static uiVScrollBarNode_t* UI_CreateVScrollbar (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "vscrollbar", name, super);
}
static uiWidgetNode_t* UI_CreateWidget (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "controls", name, super);
}
static uiWindowNode_t* UI_CreateWindow (const char* name, const char* super) {
	return UI_CreateWindow("window", name, super);
}
static uiZoneNode_t* UI_CreateZone (uiNode_t* parent, const char* name, const char* super) {
	return UI_CreateControl (parent, "zone", name, super);
}
%}

/* expose uiNode subtypes creation functions to lua */
%rename (create_bar) UI_CreateBar;
uiBarNode_t* UI_CreateBar (uiNode_t* parent, const char* name, const char* super);
%rename (create_button) UI_CreateButton;
uiButtonNode_t* UI_CreateButton (uiNode_t* parent, const char* name, const char* super);
%rename (create_baselayout) UI_CreateBaseLayout;
uiBaseLayoutNode_t* UI_CreateBaseLayout (uiNode_t* parent, const char* name, const char* super);
%rename (create_baseinventory) UI_CreateBaseInventory;
uiBaseInventoryNode_t* UI_CreateBaseInventory (uiNode_t* parent, const char* name, const char* super);
%rename (create_checkbox) UI_CreateCheckBox;
uiCheckBoxNode_t* UI_CreateCheckBox (uiNode_t* parent, const char* name, const char* super);
%rename (create_confunc) UI_CreateConFunc;
uiConFuncNode_t* UI_CreateConFunc (uiNode_t* parent, const char* name, const char* super);
%rename (create_container) UI_CreateContainer;
uiContainerNode_t* UI_CreateContainer (uiNode_t* parent, const char* name, const char* super);
%rename (create_data) UI_CreateData;
uiDataNode_t* UI_CreateData (uiNode_t* parent, const char* name, const char* super);
%rename (create_geoscape) UI_CreateGeoscape;
uiGeoscapeNode_t* UI_CreateGeoscape (uiNode_t* parent, const char* name, const char* super);
%rename (create_image) UI_CreateImage;
uiImageNode_t* UI_CreateImage (uiNode_t* parent, const char* name, const char* super);
%rename (create_item) UI_CreateItem;
uiItemNode_t* UI_CreateItem (uiNode_t* parent, const char* name, const char* super);
%rename (create_linechart) UI_CreateLineChart;
uiLineChartNode_t* UI_CreateLineChart (uiNode_t* parent, const char* name, const char* super);
%rename (create_messagelist) UI_CreateMessageList;
uiMessageListNode_t* UI_CreateMessageList (uiNode_t* parent, const char* name, const char* super);
%rename (create_model) UI_CreateModel;
uiModelNode_t* UI_CreateModel (uiNode_t* parent, const char* name, const char* super);
%rename (create_option) UI_CreateOption;
uiOptionNode_t* UI_CreateOption (uiNode_t* parent, const char* name, const char* super);
%rename (create_optionlist) UI_CreateOptionList;
uiOptionListNode_t* UI_CreateOptionList (uiNode_t* parent, const char* name, const char* super);
%rename (create_optiontree) UI_CreateOptionTree;
uiOptionTreeNode_t* UI_CreateOptionTree (uiNode_t* parent, const char* name, const char* super);
%rename (create_panel) UI_CreatePanel;
uiPanelNode_t* UI_CreatePanel (uiNode_t* parent, const char* name, const char* super);
%rename (create_radar) UI_CreateRadar;
uiRadarNode_t* UI_CreateRadar (uiNode_t* parent, const char* name, const char* super);
%rename (create_radiobutton) UI_CreateRadioButton;
uiRadioButtonNode_t* UI_CreateRadioButton (uiNode_t* parent, const char* name, const char* super);
%rename (create_rows) UI_CreateRows;
uiRowsNode_t* UI_CreateRows (uiNode_t* parent, const char* name, const char* super);
%rename (create_selectbox) UI_CreateSelectBox;
uiSelectBoxNode_t* UI_CreateSelectBox (uiNode_t* parent, const char* name, const char* super);
%rename (create_sequence) UI_CreateSequence;
uiSequenceNode_t* UI_CreateSequence (uiNode_t* parent, const char* name, const char* super);
%rename (create_spinner) UI_CreateSpinner;
uiSpinnerNode_t* UI_CreateSpinner (uiNode_t* parent, const char* name, const char* super);
%rename (create_string) UI_CreateString;
uiStringNode_t* UI_CreateString (uiNode_t* parent, const char* name, const char* super);
%rename (create_tab) UI_CreateTab;
uiTabNode_t* UI_CreateTab (uiNode_t* parent, const char* name, const char* super);
%rename (create_tbar) UI_CreateTBar;
uiTBarNode_t* UI_CreateTBar (uiNode_t* parent, const char* name, const char* super);
%rename (create_text) UI_CreateText;
uiTextNode_t* UI_CreateText (uiNode_t* parent, const char* name, const char* super);
%rename (create_text2) UI_CreateText2;
uiText2Node_t* UI_CreateText2 (uiNode_t* parent, const char* name, const char* super);
%rename (create_textentry) UI_CreateTextEntry;
uiTextEntryNode_t* UI_CreateTextEntry (uiNode_t* parent, const char* name, const char* super);
%rename (create_textlist) UI_CreateTextList;
uiTextListNode_t* UI_CreateTextList (uiNode_t* parent, const char* name, const char* super);
%rename (create_texture) UI_CreateTexture;
uiTextureNode_t* UI_CreateTexture (uiNode_t* parent, const char* name, const char* super);
%rename (create_timer) UI_CreateTimer;
uiTimerNode_t* UI_CreateTimer (uiNode_t* parent, const char* name, const char* super);
%rename (create_video) UI_CreateVideo;
uiVideoNode_t* UI_CreateVideo (uiNode_t* parent, const char* name, const char* super);
%rename (create_vscrollbar) UI_CreateVScrollbar;
uiVScrollBarNode_t* UI_CreateVScrollbar (uiNode_t* parent, const char* name, const char* super);
%rename (create_widget) UI_CreateWidget;
uiWidgetNode_t* UI_CreateWidget (uiNode_t* parent, const char* name, const char* super);
%rename (create_window) UI_CreateWindow;
uiWindowNode_t* UI_CreateWindow (const char* name, const char* super);
%rename (create_zone) UI_CreateZone;
uiZoneNode_t* UI_CreateZone (uiNode_t* parent, const char* name, const char* super);
/* expose component creation fuction */
%rename (create_component) UI_CreateComponent;
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super);

/* typemaps for UI_PushWindow's params argument  */
%typemap (in, checkfn="Com_LuaIsNilOrTable") linkedList_t* params {
	$1 = Com_LuaTableToStringList(L, $input);
}
%typemap (freearg) linkedList_t* params {
	LIST_Delete(&$1);
}
/* expose window functions */
%rename (pop_window) UI_PopWindow;
void UI_PopWindow (bool all);
%rename (push_window) UI_PushWindow;
uiNode_t* UI_PushWindow (const char* name, const char* parentName, linkedList_t* params);
%rename (get_window) UI_GetWindow;
uiNode_t* UI_GetWindow(const char* name);
/* Clear UI_PushWindow's params argument typemaps */
%typemap (in) linkedList_t* params;
%typemap (freearg) linkedList_t* params;

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose generic node functions
///////////////////////////////////////////////////////////////////////////////////////////////////

%rename (delete_node) UI_DeleteNode;
void UI_DeleteNode(uiNode_t* node);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose command functions
///////////////////////////////////////////////////////////////////////////////////////////////////

%rename (cmd) Cbuf_AddText;
void Cbuf_AddText(const char* command);

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

/* expose information functions */
%rename (nodetree) UI_PrintNodeTree;
void UI_PrintNodeTree (uiNode_t* node);
%rename (nodepath) UI_GetPath;
const char* UI_GetPath(const uiNode_t* node);

///////////////////////////////////////////////////////////////////////////////////////////////////
//	expose .ufo as lua module
///////////////////////////////////////////////////////////////////////////////////////////////////

/* expose registration functions for callbacks */
%rename(register_onload) UI_RegisterHandler_OnLoad;
void UI_RegisterHandler_OnLoad (LUA_FUNCTION fcn);
