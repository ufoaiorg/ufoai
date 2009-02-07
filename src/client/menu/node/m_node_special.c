/**
 * @file m_node_special.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../m_nodes.h"
#include "../m_actions.h"
#include "m_node_window.h"
#include "m_node_special.h"
#include "m_node_abstractnode.h"

/**
 * @brief Call before the script initializes the node
 */
static void MN_FuncNodeLoading (menuNode_t *node)
{
	if (!Q_strncmp(node->name, "event", 5)) {
		node->timeOut = 2000; /* default value */
	}
}

/**
 * @brief Call after the script initialized the node
 */
static void MN_FuncNodeLoaded (menuNode_t *node)
{
	menuNode_t * menu = node->menu;
	if (!Q_strncmp(node->name, "init", 4)) {
		if (!menu->u.window.onInit)
			menu->u.window.onInit = node->onClick;
		else
			Com_Printf("MN_FuncNodeLoaded: second init function ignored (menu \"%s\")\n", menu->name);
	} else if (!Q_strncmp(node->name, "close", 5)) {
		if (!menu->u.window.onClose)
			menu->u.window.onClose = node->onClick;
		else
			Com_Printf("MN_FuncNodeLoaded: second close function ignored (menu \"%s\")\n", menu->name);
	} else if (!Q_strncmp(node->name, "event", 5)) {
		if (!menu->u.window.onTimeOut) {
			menu->u.window.eventNode = node;
			menu->u.window.onTimeOut = node->onClick;
		} else
			Com_Printf("MN_FuncNodeLoaded: second event function ignored (menu \"%s\")\n", menu->name);
	} else if (!Q_strncmp(node->name, "leave", 5)) {
		if (!menu->u.window.onLeave) {
			menu->u.window.onLeave = node->onClick;
		} else
			Com_Printf("MN_FuncNodeLoaded: second leave function ignored (menu \"%s\")\n", menu->name);
	}

}

void MN_RegisterFuncNode (nodeBehaviour_t *behaviour)
{
	memset(behaviour, 0, sizeof(behaviour));
	behaviour->name = "func";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->loading = MN_FuncNodeLoading;
	behaviour->loaded = MN_FuncNodeLoaded;
}

void MN_RegisterNullNode (nodeBehaviour_t *behaviour)
{
	memset(behaviour, 0, sizeof(behaviour));
	behaviour->name = "";
	behaviour->isVirtual = qtrue;
}

/**
 * @brief Callback to execute a confunc
 */
static void MN_ConfuncCommand_f (void)
{
	menuNode_t *node = (menuNode_t *) Cmd_Userdata();
	assert(node);
	assert(MN_NodeInstanceOf(node, "confunc"));
	MN_ExecuteConFuncActions(node, node->onClick);
}

/**
 * @brief Call after the script initialized the node
 */
static void MN_ConFuncNodeLoaded (menuNode_t *node)
{
	/* register confunc non inherited */
	if (node->super == NULL) {
		/* don't add a callback twice */
		if (!Cmd_Exists(node->name)) {
			Cmd_AddCommand(node->name, MN_ConfuncCommand_f, "Confunc callback");
			Cmd_AddUserdata(node->name, node);
		} else {
			Com_Printf("MN_ParseNodeBody: Command name for confunc '%s.%s' already registered\n", node->menu->name, node->name);
		}
	}
}

/**
 * @brief Callback every time the parent menu is open (pushed into the active menu stack)
 */
static void MN_ConFuncNodeInit (menuNode_t *node)
{
	/* override confunc only for inherited confunc node */
	if (node->super) {
		assert(Cmd_Exists(node->name));
		Cmd_AddUserdata(node->name, node);
	}
}

void MN_RegisterConFuncNode (nodeBehaviour_t *behaviour)
{
	memset(behaviour, 0, sizeof(behaviour));
	behaviour->name = "confunc";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->loaded = MN_ConFuncNodeLoaded;
	behaviour->init = MN_ConFuncNodeInit;
}

void MN_RegisterCvarFuncNode (nodeBehaviour_t *behaviour)
{
	memset(behaviour, 0, sizeof(behaviour));
	behaviour->name = "cvarfunc";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
}
