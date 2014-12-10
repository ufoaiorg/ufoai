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
#include "../../common/cvar.h"
#include "../../common/hashtable.h"
#include "../../common/filesys.h"
#include "../../common/scripts_lua.h"
#include "../cl_lua.h"

//#include "../client.h"
#include "ui_main.h"
#include "ui_components.h"
#include "ui_behaviour.h"
#include "ui_node.h"
#include "ui_parse.h"
#include "ui_internal.h"

#include "../../common/swig_lua_runtime.h"

/* internal field for passing script name as a key to lua registration functions */
char ui_scriptname[256] = "";


/**
 * @brief Performs UI specific initialization. Call this after CL_InitLua.
 */
void UI_InitLua (void) {
}

/**
 * @brief Performs UI specific lua shutdown. Call this before CL_ShutdownLua.
 */
void UI_ShutdownLua (void) {
}

/**
 * @brief Executes a lua event handler.
 * @param[in] node The node the event handler is associated with.
 * @param[in] event The event to execute.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the event handler in lua is: onevent(sender)
 */
bool UI_ExecuteLuaEventScript (uiNode_t* node, LUA_EVENT event) {
	lua_rawgeti (CL_GetLuaState (), LUA_REGISTRYINDEX, event); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	if (lua_pcall (CL_GetLuaState(), 1, 0, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
	return true;
}

/**
 * @brief Executes a lua event handler and returns the result as a boolean.
 * @param[in] node The node the event handler is associated with.
 * @param[in] event The event to execute.
 * @param[in/out] result The boolean value returned by the lua script.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the event handler in lua is: onevent(sender) and must return a boolean value
 */
bool UI_ExecuteLuaEventScript_ReturnBool (uiNode_t* node, LUA_EVENT event, bool &result) {
	lua_rawgeti (CL_GetLuaState (), LUA_REGISTRYINDEX, event); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	if (lua_pcall (CL_GetLuaState(), 1, 1, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
    if (!lua_isboolean(CL_GetLuaState(), -1)) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: expecting a boolean as return value\n", node->name, node->behaviour->name);
		return false;
    }
    result = lua_toboolean(CL_GetLuaState(), -1);
	return true;
}

/**
 * @brief Executes a lua event handler with (x,y) argument.
 * @param[in] node The node the event handler is associated with.
 * @param[in] event The event to execute.
 * @param[in] x The x-coordinate of the (x,y) argument.
 * @param[in] y The y-coordinate of the (x,y) argument.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the event handler in lua is: onevent(sender, x, y)
 */
bool UI_ExecuteLuaEventScript_XY (uiNode_t* node, LUA_EVENT event, int x, int y) {
	lua_rawgeti (CL_GetLuaState(), LUA_REGISTRYINDEX, event); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	lua_pushinteger(CL_GetLuaState(), x); /* push x on lua stack */
	lua_pushinteger(CL_GetLuaState(), y); /* push y on lua stack */
	if (lua_pcall (CL_GetLuaState(), 3, 0, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
	return true;
}

/**
 * @brief Executes a lua event handler with (dx,dy) argument.
 * @param[in] node The node the event handler is associated with.
 * @param[in] event The event to execute.
 * @param[in] dx The dx-coordinate of the (dx,dy) argument.
 * @param[in] dy The dy-coordinate of the (dx,dy) argument.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the event handler in lua is: onevent(sender, dx, dy)
 */
bool UI_ExecuteLuaEventScript_DxDy (uiNode_t* node, LUA_EVENT event, int dx, int dy) {
	lua_rawgeti (CL_GetLuaState(), LUA_REGISTRYINDEX, event); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	lua_pushinteger(CL_GetLuaState(), dx); /* push dx on lua stack */
	lua_pushinteger(CL_GetLuaState(), dy); /* push dy on lua stack */
	if (lua_pcall (CL_GetLuaState(), 3, 0, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
	return true;
}

/**
 * @brief Executes a lua event handler with (keycode,unicode) argument.
 * @param[in] node The node the event handler is associated with.
 * @param[in] event The event to execute.
 * @param[in] key The key value of the (keycode,unicode) argument.
 * @param[in] unicode The unicode value of the (keycode,unicode) argument.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the event handler in lua is: onevent(sender, key, unicode)
 */
bool UI_ExecuteLuaEventScript_Key (uiNode_t* node, LUA_EVENT event, unsigned int key, unsigned short unicode) {
	lua_rawgeti (CL_GetLuaState(), LUA_REGISTRYINDEX, event); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	lua_pushinteger(CL_GetLuaState(), key); /* push key on lua stack */
	lua_pushinteger(CL_GetLuaState(), unicode); /* push unicode on lua stack */
	if (lua_pcall (CL_GetLuaState(), 3, 0, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
	return true;
}


/**
 * @brief Executes a lua based method defined on the behaviour class of a node.
 * @param[in] node The node the method is defined on.
 * @param[in] fcn The The method to execute.
 * @param[in] params A linked list of parameters for the lua based function call.
 * @param[in] nparams The number of parameters for the lua based function call.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the method in lua is: function(sender, p1..pn), so the actual number of parameters
 * for the function is actually nparams + 1.
 * @note All parameters are send to lua as strings.
*/
bool UI_ExecuteLuaMethod (uiNode_t* node, LUA_FUNCTION fcn, linkedList_t* params, int nparams) {
	lua_rawgeti (CL_GetLuaState (), LUA_REGISTRYINDEX, fcn); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	/* push parameters on stack -> note: all the parameters are pushed as strings, since this is the only
	   parameter type the old-style script can handle */
	for(int i=0; i<nparams; i++) {
		const char* value=const_cast<const char*> ((char*)LIST_GetByIdx(params, i));
		lua_pushstring(CL_GetLuaState(), value);
	}
	if (lua_pcall (CL_GetLuaState(), 1, 0, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
	return true;
}

/**
 * @brief Executes a lua based confunc node.
 * @param[in] node The node the method is defined on.
 * @param[in] fcn The The method to execute.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the method in lua is: function(sender, p1..pn), so the actual number of parameters
 * for the function is actually nparams + 1.
 * @note Parameters are read from cmd.
 * @note All parameters are send to lua as strings.
*/
bool UI_ExecuteLuaConFunc (uiNode_t* node, LUA_FUNCTION fcn) {
	lua_rawgeti (CL_GetLuaState (), LUA_REGISTRYINDEX, fcn); /* push event function on lua stack */
	SWIG_NewPointerObj (CL_GetLuaState(), node, static_cast<swig_type_info*>(node->behaviour->lua_SWIG_typeinfo), 0); /* push sender on lua stack */
	/* read parameters from cmd and push them on the stack; first parameter is skipped, since this is the
	   function name; all parameters are strings */
	for(int i=1; i < Cmd_Argc(); i++) {
		lua_pushstring(CL_GetLuaState(), Cmd_Argv(i));
	}
	/* execute the confunc */
	if (lua_pcall (CL_GetLuaState(), Cmd_Argc(), 0, 0) != 0) {
		Com_Printf ("lua error(0) [node=%s, behaviour=%s]: %s\n", node->name, node->behaviour->name, lua_tostring(CL_GetLuaState(), -1));
		return false;
	};
	return true;
}

/**
 * @brief Executes a lua based method defined on the behaviour class of a node.
 * @param[in] node The node the method is defined on.
 * @param[in] name The name of the node.
 * @return True if the operation succeeds, false otherwise.
 * @note The signature of the method in lua is: function(sender)
*/
bool UI_ExecuteLuaMethod_ByName (uiNode_t* node, const char* name, linkedList_t* params, int nparams) {
	LUA_FUNCTION fn;
	if (UI_GetNodeMethod(node, name, fn)) {
		return UI_ExecuteLuaMethod(node, fn, params, nparams);
	}
	else {
		Com_Printf("UI_ExecuteNodeMethod_ByName: calling undefined method %s on node %s\n", name, node->name);
	}
	return false;
}

/**
 * @brief Register global lua callback function called after loading the module.
 */
void UI_RegisterHandler_OnLoad (LUA_FUNCTION fcn) {
	CL_RegisterCallback(ui_scriptname, fcn);
}

/**
 * @brief Call the registered callback handler. This stub primarily exists should the signature of the
 * call change in the future. CL_ExecuteCallback only supports a callback with no arguments.
 */
void UI_CallHandler_OnLoad (lua_State* L, const char* key) {
	CL_ExecuteCallback(L, key);
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
	Com_Printf ("UI_ParseAndLoadLuaScript: %s\n", name);
	/* load the contents of the lua file */
	if (luaL_loadbuffer(CL_GetLuaState(), *text, ntext, name) == 0) {
		/* set the script name for calls to the registration functions */
		Q_strncpyz (ui_scriptname, name, sizeof(ui_scriptname));
		/* the script is loaded, now execute it; this will trigger any register_XXXX functions to be
		   called by the lua script */
		if (lua_pcall(CL_GetLuaState(), 0, LUA_MULTRET, 0) != 0) {
			Com_Printf ("lua error: %s\n", lua_tostring(CL_GetLuaState(), -1));
		}
		else {
			/* at this point the lua file is loaded and callbacks are registered (on the stack),
			   now call the onLoad if it exists */
			UI_CallHandler_OnLoad (CL_GetLuaState(), name);
		}
		ui_scriptname[0]='\0';
	}
	else {
        Com_Error(0, "lua load error: %s\n", lua_tostring(CL_GetLuaState(), -1));
	}
	/* reset the content pointer, this will force the cached content to be released
	   FS_NextScriptHeader. */
	*text = nullptr;
	/* execute the onLoad function */
	return true;
}

/**
 * @brief Create a new control inherited from a given node class or other node.
 * @param[in] parent The parent control or window/component owning this control.
 * @param[in] type The behaviour type of the control to create. This can be a previously defined control instance.
 * @param[in] name The name of the control to create.
 * @param[in] super The name this control inherits from. If specified overrides the behaviour type of
 * super must match type.
 * @note A control is a node inside a component or window. Unlike a component or window, a control is
 * always attached to a parent.
 * @note Code needs to be refactored.
 */
uiNode_t* UI_CreateControl (uiNode_t* parent, const char* type, const char* name, const char* super)
{
	uiNode_t* node = nullptr;
	uiNode_t* node_super = nullptr;
	uiNode_t* inherited_control = nullptr;
	uiBehaviour_t* behaviour = nullptr;

	/* get the behaviour */
	behaviour = UI_GetNodeBehaviour(type);
	if (!behaviour) {
		/* if behaviour class not found, try to get a component of this name*/
		inherited_control = UI_GetComponent(type);
		if (!inherited_control) {
			Com_Printf("UI_CreateControl: node behaviour/control '%s' doesn't exist (%s)\n", type, UI_GetPath(parent));
			return nullptr;
		}
		behaviour = inherited_control->behaviour;
	}
	/* get the super if it exists */
	if (super) {
		node_super = UI_GetComponent(super);
		/* validate the behaviour matches with the type requested */
		if (node_super) {
			if (node_super->behaviour != behaviour) {
				Com_Printf("UI_CreateControl: behaviour [%s] of super does not match requested type [%s]\n", behaviour->name, node_super->behaviour->name);
				return nullptr;
			}
		}
	}

	/* check the name */
	if (!UI_TokenIsName(name, false)) {
		Com_Printf("UI_CreateControl: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", name);
		return false;
	}
	if (UI_TokenIsReserved(name)) {
		Com_Printf("UI_CreateControl: \"%s\" is a reserved token, we can't call a node with it\n", name);
		return false;
	}

	/* test if node already exists (inside the parent subtree) */
	/* Already existing node should only come from inherited node, we should not have 2 definitions of the same node into the same window. */
	if (parent) {
		node = UI_GetNode(parent, name);
		if (node) {
			Com_Printf("UI_CreateControl: trying to create duplicate node [%s]\n", UI_GetPath(node));
			return nullptr;
		}
	}

	/* clone using super */
	if (node_super) {
		node = UI_CloneNode(node_super, nullptr, true, name, true);
	}
	/* else try creating a clone of the component */
	else if (inherited_control) {
		/* initialize from a component */
		node = UI_CloneNode(inherited_control, nullptr, true, name, true);
	}
	/* else initialize a new node */
	else {
		node = UI_AllocNode(name, behaviour->name, true);
	}

	if (parent) {
		UI_AppendNode(parent, node);
	}

	/* call onload (properties are set from lua) */
	UI_Node_Loaded(node);

	return node;
}

void UI_PrintNodeTree (uiNode_t* node, int level) {
	int i;
	char* indent = new char[level+1];
	for(i=0; i<level; i++) indent[i] = '\t';
	indent[i] = 0x00;

	Com_Printf("%s[%s]\n", indent, node->name);
	for(uiNode_t* child=node->firstChild; child; child=child->next) {
		UI_PrintNodeTree(child, level + 1);
	}
	delete indent;
}

const char* UI_Node_TypeOf(uiNode_t* node) {
	if (node) {
		return node->behaviour->name;
	}
	return nullptr;
}

/**
 * @brief Create a new component inherited from a given node class or other node.
 * @param[in] type The behaviour type of the component to create.
 * @param[in] name The name of the component to create.
 * @param[in] super The name this component inherits from. If NULL the component has no super defined.
 * @note A component is a reusable window or panel.
 */
uiNode_t* UI_CreateComponent (const char* type, const char* name, const char* super)
{
	uiNode_t* node_super = nullptr;
	uiNode_t* component = nullptr;
	uiNode_t* inherited = nullptr;
	uiBehaviour_t* behaviour = nullptr;

	/* check the name */
	if (!UI_TokenIsName(name, false)) {
		Com_Printf("UI_CreateComponent: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", name);
		return false;
	}
	if (UI_TokenIsReserved(name)) {
		Com_Printf("UI_CreateComponent: \"%s\" is a reserved token, we can't call a node with it\n", name);
		return false;
	}

	/* get the type of behaviour */
	behaviour = UI_GetNodeBehaviour(type);
	if (!behaviour) {
		/* if no default behaviour found, try to get a custom component */
		inherited = UI_GetComponent(type);
		if (!inherited) {
			Com_Printf("UI_CreateComponent: node behaviour/control '%s' doesn't exist\n", type);
			return nullptr;
		}
	}
	/* get the super if it exists */
	if (super) {
		node_super = UI_GetComponent(super);
		/* validate the behaviour matches with the type requested */
		if (node_super) {
			if (node_super->behaviour != behaviour) {
				Com_Printf("UI_CreateComponent: behaviour [%s] of super does not match requested type [%s]\n", behaviour->name, node_super->behaviour->name);
				return nullptr;
			}
		}
	}

	/* clone using super */
	if (node_super) {
		component = UI_CloneNode(node_super, nullptr, true, name, true);
	}
	/* else initialize a new node */
	else {
		/* use inherited if available */
		if (inherited) {
			component = UI_CloneNode(inherited, nullptr, true, name, true);
		}
		/* else use the behaviour type */
		else {
			component = UI_AllocNode(name, behaviour->name, true);
		}
	}

	/* call onload (properties are set and controls are created from lua) */
	UI_Node_Loaded(component);

	/* add to list of instantiated components */
	UI_InsertComponent(component);

	//Com_Printf("UI_CreateComponent: registered new component, name [%s], type [%s]\n", name, type);

	return component;
}

/**
 * @brief Create a window node with specified type and inheritance.
 * @param[in] type The behaviour type of the window to create.
 * @param[in] name The name of the window to create.
 * @param[in] super The name this window inherits from. If NULL the window has no super defined.
 * @note A window is a top level component in the ui.
 * @todo If the old style ui scripts are no longer used, the check UI_TokenIsReserved should be removed.
 */
uiNode_t* UI_CreateWindow (const char* type, const char* name, const char* super)
{
	/* make sure we create windows only here */
	if (!Q_streq(type, "window")) {
		Com_Error(ERR_FATAL, "UI_CreateWindow: '%s %s' is not a window node\n", type, name);
		return nullptr;	/* never reached */
	}
	/* make sure the name of the window is a correct identifier */
	/* note: since this is called from lua, name is never quoted */
	if (!UI_TokenIsName(name, false)) {
		Com_Printf("UI_CreateWindow: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", name);
		return nullptr;
	}
	/* make sure the name of the window is not a reserverd word */
	if (UI_TokenIsReserved(name)) {
		Com_Printf("UI_CreateWindow: \"%s\" is a reserved token, we can't call a node with it (node \"%s\")\n", name, name);
		return nullptr;
	}

	/* search for windows with same name */
	int i;
	for (i = 0; i < ui_global.numWindows; i++)
		if (!strncmp(name, ui_global.windows[i]->name, sizeof(ui_global.windows[i]->name)))
			break;

	if (i < ui_global.numWindows) {
		Com_Printf("UI_CreateWindow: %s \"%s\" with same name found, second ignored\n", type, name);
	}

	if (ui_global.numWindows >= UI_MAX_WINDOWS) {
		Com_Error(ERR_FATAL, "UI_CreateWindow: max windows exceeded (%i) - ignore '%s'\n", UI_MAX_WINDOWS, name);
		return nullptr;	/* never reached */
	}

	/* create the window */
	uiNode_t* window;

	/* does this window inherit data from another window? */
	if (super != nullptr) {
		uiNode_t* superWindow;
		superWindow = UI_GetWindow(super);
		if (superWindow == nullptr) {
			Sys_Error("Could not get the super window \"%s\"", super);
		}
		window = UI_CloneNode(superWindow, nullptr, true, name, true);
	} else {
		window = UI_AllocNode(name, type, true);
		window->root = window;
	}

	/* call onload (properties are set and controls are created from lua) */
	UI_Node_Loaded(window);

	/* add to list of instantiated windows */
	UI_InsertWindow(window);

	return window;
}

