/**
 * @file m_node_abstractnode.c
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
#include "../m_parse.h"
#include "../m_input.h"
#include "../m_main.h"
#include "m_node_abstractnode.h"

/** @brief valid properties for a node */
static const value_t properties[] = {
	{"pos", V_POS, offsetof(menuNode_t, pos), MEMBER_SIZEOF(menuNode_t, pos)},
	{"size", V_POS, offsetof(menuNode_t, size), MEMBER_SIZEOF(menuNode_t, size)},
	{"width", V_FLOAT, offsetof(menuNode_t, size[0]), MEMBER_SIZEOF(menuNode_t, size[0])},
	{"height", V_FLOAT, offsetof(menuNode_t, size[1]), MEMBER_SIZEOF(menuNode_t, size[1])},
	{"left", V_FLOAT, offsetof(menuNode_t, pos[0]), MEMBER_SIZEOF(menuNode_t, pos[0])},
	{"top", V_FLOAT, offsetof(menuNode_t, pos[1]), MEMBER_SIZEOF(menuNode_t, pos[1])},

	{"invis", V_BOOL, offsetof(menuNode_t, invis), MEMBER_SIZEOF(menuNode_t, invis)},
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
	{"blend", V_BOOL, offsetof(menuNode_t, blend), MEMBER_SIZEOF(menuNode_t, blend)},
	{"disabled", V_BOOL, offsetof(menuNode_t, disabled), MEMBER_SIZEOF(menuNode_t, disabled)},
	{"texh", V_POS, offsetof(menuNode_t, texh), MEMBER_SIZEOF(menuNode_t, texh)},
	{"texl", V_POS, offsetof(menuNode_t, texl), MEMBER_SIZEOF(menuNode_t, texl)},
	{"border", V_INT, offsetof(menuNode_t, border), MEMBER_SIZEOF(menuNode_t, border)},
	{"padding", V_INT, offsetof(menuNode_t, padding), MEMBER_SIZEOF(menuNode_t, padding)},
	{"scale", V_VECTOR, offsetof(menuNode_t, scale), MEMBER_SIZEOF(menuNode_t, scale)},
	{"baseid", V_BASEID, offsetof(menuNode_t, baseid), MEMBER_SIZEOF(menuNode_t, baseid)},
	{"timeout", V_INT, offsetof(menuNode_t, timeOut), MEMBER_SIZEOF(menuNode_t, timeOut)},
	{"timeout_once", V_BOOL, offsetof(menuNode_t, timeOutOnce), MEMBER_SIZEOF(menuNode_t, timeOutOnce)},
	{"bgcolor", V_COLOR, offsetof(menuNode_t, bgcolor), MEMBER_SIZEOF(menuNode_t, bgcolor)},
	{"bordercolor", V_COLOR, offsetof(menuNode_t, bordercolor), MEMBER_SIZEOF(menuNode_t, bordercolor)},
	{"key", V_STRING, offsetof(menuNode_t, key), 0},

	{"tooltip", V_LONGSTRING|V_MENU_COPY, offsetof(menuNode_t, tooltip), 0},	/* translated in MN_Tooltip */
	{"image", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},
	{"roq", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},
	{"model", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},	/** @todo Rename into model */
	{"cvar", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataModelSkinOrCVar), 0},	/* for selectbox */
	{"skin", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataModelSkinOrCVar), 0},
	{"string", V_LONGSTRING|V_MENU_COPY, offsetof(menuNode_t, text), 0},	/* no gettext here - this can be a cvar, too */
	{"font", V_STRING|V_MENU_COPY, offsetof(menuNode_t, font), 0},
#if 0 /* never use */
	{"weapon", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},
#endif

	/* specific for model
	 * @todo move it into the node behaviour
	 */
	{"anim", V_STRING|V_MENU_COPY, offsetof(menuNode_t, u.model.animation), 0},
	{"angles", V_VECTOR, offsetof(menuNode_t, u.model.angles), MEMBER_SIZEOF(menuNode_t, u.model.angles)},
	{"center", V_VECTOR, offsetof(menuNode_t, u.model.center), MEMBER_SIZEOF(menuNode_t, u.model.center)},
	{"origin", V_VECTOR, offsetof(menuNode_t, u.model.origin), MEMBER_SIZEOF(menuNode_t, u.model.origin)},
	{"tag", V_STRING|V_MENU_COPY, offsetof(menuNode_t, u.model.tag), 0},

	{"color", V_COLOR, offsetof(menuNode_t, color), MEMBER_SIZEOF(menuNode_t, color)},
	{"selectcolor", V_COLOR, offsetof(menuNode_t, selectedColor), MEMBER_SIZEOF(menuNode_t, selectedColor)},
	{"align", V_ALIGN, offsetof(menuNode_t, align), MEMBER_SIZEOF(menuNode_t, align)},
	{"textalign", V_ALIGN, offsetof(menuNode_t, textalign), MEMBER_SIZEOF(menuNode_t, textalign)},
	{"if", V_IF, offsetof(menuNode_t, depends), 0},
	{"point_width", V_FLOAT, offsetof(menuNode_t, pointWidth), MEMBER_SIZEOF(menuNode_t, pointWidth)},
	{"gap_width", V_INT, offsetof(menuNode_t, gapWidth), MEMBER_SIZEOF(menuNode_t, gapWidth)},

	/* action event */
	{"click", V_SPECIAL_ACTION, offsetof(menuNode_t, onClick), MEMBER_SIZEOF(menuNode_t, onClick)},
	{"rclick", V_SPECIAL_ACTION, offsetof(menuNode_t, onRightClick), MEMBER_SIZEOF(menuNode_t, onRightClick)},
	{"mclick", V_SPECIAL_ACTION, offsetof(menuNode_t, onMiddleClick), MEMBER_SIZEOF(menuNode_t, onMiddleClick)},
	{"wheel", V_SPECIAL_ACTION, offsetof(menuNode_t, onWheel), MEMBER_SIZEOF(menuNode_t, onWheel)},
	{"in", V_SPECIAL_ACTION, offsetof(menuNode_t, onMouseIn), MEMBER_SIZEOF(menuNode_t, onMouseIn)},
	{"out", V_SPECIAL_ACTION, offsetof(menuNode_t, onMouseOut), MEMBER_SIZEOF(menuNode_t, onMouseOut)},
	{"whup", V_SPECIAL_ACTION, offsetof(menuNode_t, onWheelUp), MEMBER_SIZEOF(menuNode_t, onWheelUp)},
	{"whdown", V_SPECIAL_ACTION, offsetof(menuNode_t, onWheelDown), MEMBER_SIZEOF(menuNode_t, onWheelDown)},
	{"change", V_SPECIAL_ACTION, offsetof(menuNode_t, onChange), MEMBER_SIZEOF(menuNode_t, onChange)},

	/* very special attribute */
	{"excluderect", V_SPECIAL_EXCLUDERECT, 0, 0},

	{NULL, V_NULL, 0, 0},
};

/**
 * @brief Returns the absolute position of a menunode
 * @param[in] menunode
 * @param[out] pos
 */
void MN_GetNodeAbsPos (const menuNode_t* node, vec2_t pos)
{
	if (!node)
		Sys_Error("MN_GetNodeAbsPos: No node given");
	if (!node->menu)
		Sys_Error("MN_GetNodeAbsPos: Node '%s' has no menu", node->name);

	/* if we request the position of an undrawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Sys_Error("MN_GetNodeAbsPos: Node '%s' dont have position", node->name);

	Vector2Set(pos, node->menu->pos[0] + node->pos[0], node->menu->pos[1] + node->pos[1]);
}

/**
 * @brief Update an absolute position to a relative one
 * @param[in] menunode
 * @param[inout] x an absolute x position
 * @param[inout] y an absolute y position
 */
void MN_NodeAbsoluteToRelativePos (const menuNode_t* node, int *x, int *y)
{
	assert(node != NULL);
	assert(x != NULL);
	assert(y != NULL);

	/* if we request the position of an undrawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Sys_Error("MN_NodeAbsoluteToRelativePos: Node '%s' dont have position", node->name);

	*x -= node->menu->pos[0] + node->pos[0];
	*y -= node->menu->pos[1] + node->pos[1];
}

/**
 * @brief Hides a given menu node
 * @note Sanity check whether node is null included
 */
void MN_HideNode (menuNode_t* node)
{
	if (node)
		node->invis = qtrue;
	else
		Com_Printf("MN_HideNode: No node given\n");
}

/**
 * @brief Script command to hide a given menu node
 */
static void MN_HideNode_f (void)
{
	if (Cmd_Argc() == 2)
		MN_HideNode(MN_GetNodeFromCurrentMenu(Cmd_Argv(1)));
	else
		Com_Printf("Usage: %s <node>\n", Cmd_Argv(0));
}

/**
 * @brief Unhides a given menu node
 * @note Sanity check whether node is null included
 */
void MN_UnHideNode (menuNode_t* node)
{
	if (node)
		node->invis = qfalse;
	else
		Com_Printf("MN_UnHideNode: No node given\n");
}

/**
 * @brief Script command to unhide a given menu node
 */
static void MN_UnHideNode_f (void)
{
	if (Cmd_Argc() == 2)
		MN_UnHideNode(MN_GetNodeFromCurrentMenu(Cmd_Argv(1)));
	else
		Com_Printf("Usage: %s <node>\n", Cmd_Argv(0));
}

/**
 * @brief Sets new x and y coordinates for a given node
 */
void MN_SetNewNodePos (menuNode_t* node, int x, int y)
{
	if (node) {
		node->pos[0] = x;
		node->pos[1] = y;
	}
}

/**
 * @brief Set node property
 * @note More hard to set string like that at the run time
 * @todo remove atof
 * @todo add support of more fixed size value (everything else string)
 */
qboolean MN_NodeSetProperty (menuNode_t* node, const value_t *property, const char* value)
{
	byte* b = (byte*)node + property->ofs;

	if (property->type == V_FLOAT) {
		*(float*) b = atof(value);
	} else if (property->type == (V_FLOAT|V_MENU_COPY)) {
		b = (byte*) (*(void**)b);
		if (!Q_strncmp((const char*)b, "*cvar", 5)) {
			MN_SetCvar(&((char*)b)[6], NULL, atof(value));
		} else {
			*(float*) b = atof(value);
		}
	} else if (property->type == V_INT) {
		*(int*) b = atoi(value);
	} else {
		Com_Printf("MN_NodeSetProperty: Unimplemented type for property '%s.%s@%s'\n", node->menu->name, node->name, property->string);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief set a node property from the command line
 */
static void MN_NodeSetProperty_f (void)
{
	menuNode_t *node;
	const value_t *property;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <node> <prop> <value>\n", Cmd_Argv(0));
		return;
	}

	node = MN_GetNodeFromCurrentMenu(Cmd_Argv(1));
	if (!node) {
		Com_Printf("MN_NodeSetProperty_f: Node '%s' not found\n", Cmd_Argv(1));
		return;
	}

	property = MN_NodeGetPropertyDefinition(node, Cmd_Argv(2));
	if (!property) {
		Com_Printf("Property '%s.%s@%s' dont exists\n", node->menu->name, node->name, Cmd_Argv(2));
		return;
	}

	MN_NodeSetProperty(node, property, Cmd_Argv(3));
}

static qboolean MN_ABstractNodeDNDEnter (menuNode_t *node)
{
	return qfalse;
}

static qboolean MN_ABstractNodeDNDMove (menuNode_t *node, int x, int y)
{
	return qtrue;
}

static void MN_ABstractNodeDNDLeave (menuNode_t *node)
{
}

static qboolean MN_ABstractNodeDNDDrop (menuNode_t *node, int x, int y)
{
	return qtrue;
}

static qboolean MN_ABstractNodeDNDFinished (menuNode_t *node, qboolean isDroped)
{
	return isDroped;
}

void MN_RegisterAbstractNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractnode";
	behaviour->isAbstract = qtrue;
	behaviour->id = MN_ABSTRACTNODE;
	behaviour->properties = properties;

	/* drag and drop callback */
	behaviour->dndEnter = MN_ABstractNodeDNDEnter;
	behaviour->dndMove = MN_ABstractNodeDNDMove;
	behaviour->dndLeave = MN_ABstractNodeDNDLeave;
	behaviour->dndDrop = MN_ABstractNodeDNDDrop;
	behaviour->dndFinished = MN_ABstractNodeDNDFinished;

	/* some commands */
	Cmd_AddCommand("mn_hidenode", MN_HideNode_f, "Hides a given menu node");
	Cmd_AddCommand("mn_unhidenode", MN_UnHideNode_f, "Unhides a given menu node");
	Cmd_AddCommand("mn_setnodeproperty", MN_NodeSetProperty_f, "Set a node property");
}
