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

/*
 * @brief Check the node inheritance
 * @param[in] node Requested node
 * @param[in] behaviourName Property name we search
 * @return True if the node inherits from the behaviour
 */
qboolean MN_NodeInstanceOf (const menuNode_t *node, const char* behaviourName)
{
	const nodeBehaviour_t *behaviour;
	for (behaviour = node->behaviour; behaviour; behaviour = behaviour->super) {
		if (Q_strcmp(behaviour->name, behaviourName) == 0)
			return qtrue;
	}
	return qfalse;
}

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
	{"ghost", V_BOOL, offsetof(menuNode_t, ghost), MEMBER_SIZEOF(menuNode_t, ghost)},
	{"texh", V_POS, offsetof(menuNode_t, texh), MEMBER_SIZEOF(menuNode_t, texh)},
	{"texl", V_POS, offsetof(menuNode_t, texl), MEMBER_SIZEOF(menuNode_t, texl)},
	{"border", V_INT, offsetof(menuNode_t, border), MEMBER_SIZEOF(menuNode_t, border)},
	{"padding", V_INT, offsetof(menuNode_t, padding), MEMBER_SIZEOF(menuNode_t, padding)},
	{"bgcolor", V_COLOR, offsetof(menuNode_t, bgcolor), MEMBER_SIZEOF(menuNode_t, bgcolor)},
	{"bordercolor", V_COLOR, offsetof(menuNode_t, bordercolor), MEMBER_SIZEOF(menuNode_t, bordercolor)},
	/** @todo use V_REF_OF_STRING when its possible ('image' is never a cvar) */
	{"key", V_CVAR_OR_STRING, offsetof(menuNode_t, key), 0},
	{"num", V_INT, offsetof(menuNode_t, num), MEMBER_SIZEOF(menuNode_t, num)},

	{"tooltip", V_CVAR_OR_LONGSTRING, offsetof(menuNode_t, tooltip), 0},	/* translated in MN_Tooltip */
	/** @todo use V_REF_OF_STRING when its possible ('image' is never a cvar) */
	{"image", V_CVAR_OR_STRING, offsetof(menuNode_t, image), 0},
	{"cvar", V_SPECIAL_CVAR, offsetof(menuNode_t, cvar), 0},
	{"string", V_CVAR_OR_LONGSTRING, offsetof(menuNode_t, text), 0},	/* no gettext here - this can be a cvar, too */
	/** @todo use V_REF_OF_STRING when its possible ('font' is never a cvar) */
	{"font", V_CVAR_OR_STRING, offsetof(menuNode_t, font), 0},

	{"color", V_COLOR, offsetof(menuNode_t, color), MEMBER_SIZEOF(menuNode_t, color)},
	{"selectcolor", V_COLOR, offsetof(menuNode_t, selectedColor), MEMBER_SIZEOF(menuNode_t, selectedColor)},
	{"textalign", V_ALIGN, offsetof(menuNode_t, textalign), MEMBER_SIZEOF(menuNode_t, textalign)},
	{"visiblewhen", V_SPECIAL_IF, offsetof(menuNode_t, visibilityCondition), 0},

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

	/* maybe only button */
	{"icon", V_SPECIAL_ICONREF, offsetof(menuNode_t, icon), MEMBER_SIZEOF(menuNode_t, icon)},

	/* very special attribute */
	{"excluderect", V_SPECIAL_EXCLUDERECT, 0, 0},

	{NULL, V_NULL, 0, 0}
};

/**
 * @brief return a relative position of a point into a node.
 * @param [in] node Requested node
 * @param [out] pos Result position
 * @param [in] pointDirection
 * @note For example we can request the right-bottom corner with ALIGN_LR (low, right)
 */
void MN_NodeGetPoint (const menuNode_t* node, vec2_t pos, byte pointDirection)
{
	switch (pointDirection % 3) {
	case 0:	/* left */
		pos[0] = 0;
		break;
	case 1:	/* middle */
		pos[0] = node->size[0] / 2;
		break;
	case 2:	/* right */
		pos[0] = node->size[0];
		break;
	default:
		assert(qfalse);
	}

	/* vertical (0 is upper) */
	switch ((pointDirection % 9) / 3) {
	case 0:	/* top */
		pos[1] = 0;
		break;
	case 1: /* middle */
		pos[1] = node->size[1] / 2;
		break;
	case 2: /* bottom */
		pos[1] = node->size[1];
		break;
	default:
		assert(qfalse);
	}
}

/**
 * @brief Returns the absolute position of a menunode
 * @param[in] menunode
 * @param[out] pos
 */
void MN_GetNodeAbsPos (const menuNode_t* node, vec2_t pos)
{
	assert (node);
	assert (pos);

	/* if we request the position of an undrawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Sys_Error("MN_NodeAbsoluteToRelativePos: Node '%s' dont have position", node->name);

	Vector2Set(pos, 0, 0);
	while (node) {
		pos[0] += node->pos[0];
		pos[1] += node->pos[1];
		node = node->parent;
	}
}

/**
 * @brief Update a relative point to an absolute one
 * @param[in] node The requested node
 * @param[inout] pos A point to transform
 */
void MN_NodeRelativeToAbsolutePoint (const menuNode_t* node, vec2_t pos)
{
	assert(node);
	assert(pos);
	while (node) {
		pos[0] += node->pos[0];
		pos[1] += node->pos[1];
		node = node->parent;
	}
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
	/* if we request the position of an undrawable node, there is a problem */
	assert(node->behaviour->isVirtual == qfalse);
	assert(x != NULL);
	assert(y != NULL);

	/* if we request the position of an undrawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Sys_Error("MN_NodeAbsoluteToRelativePos: Node '%s' doesn't have a position", node->name);

	while (node) {
		*x -= node->pos[0];
		*y -= node->pos[1];
		node = node->parent;
	}
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
 * @brief Search a child node by given name
 * @node Only search with one depth
 * @sa MN_GetNodeFromCurrentMenu
 */
menuNode_t *MN_GetNode (const menuNode_t* const node, const char *name)
{
	menuNode_t *current = NULL;

	if (!node)
		return NULL;

	for (current = node->firstChild; current; current = current->next)
		if (!Q_strncmp(name, current->name, sizeof(current->name)))
			break;

	return current;
}

/**
 * @brief Insert a node next another one into a node. If prevNode is NULL add the node on the heap of the menu
 * @param[in] node Node where inserting a node
 * @param[in] prevNode previous node, will became before the newNode; else NULL if newNode will become the first child of the node
 * @param[in] newNode node we insert
 */
void MN_InsertNode (menuNode_t* const node, menuNode_t *prevNode, menuNode_t *newNode)
{
	assert(node);
	assert(newNode);
	/* insert only a single element */
	assert(!newNode->next);
	/* everything come from the same menu (force the dev to update himself this links) */
	assert(!prevNode || (prevNode->menu == newNode->menu));
	if (prevNode == NULL) {
		newNode->next = node->firstChild;
		node->firstChild = newNode;
		if (node->lastChild == NULL) {
			node->lastChild = newNode;
		}
		newNode->parent = node;
		return;
	}
	newNode->next = prevNode->next;
	prevNode->next = newNode;
	if (prevNode == node->lastChild) {
		node->lastChild = newNode;
	}
	newNode->parent = node;
}

/**
 * @brief add a node at the end of the node child
 */
void MN_AppendNode (menuNode_t* const node, menuNode_t *newNode)
{
	MN_InsertNode(node, node->lastChild, newNode);
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
 * @todo use function from script.c for common type
 */
qboolean MN_NodeSetProperty (menuNode_t* node, const value_t *property, const char* value)
{
	byte* b = (byte*)node + property->ofs;

	if (property->type == V_FLOAT) {
		*(float*) b = atof(value);
	} else if (property->type == V_CVAR_OR_FLOAT) {
		b = (byte*) (*(void**)b);
		if (!Q_strncmp((const char*)b, "*cvar", 5)) {
			MN_SetCvar(&((char*)b)[6], NULL, atof(value));
		} else {
			*(float*) b = atof(value);
		}
	} else if (property->type == V_INT) {
		*(int*) b = atoi(value);
	} else if (property->type == V_BOOL) {
		qboolean v;
		if (!Q_strcmp(value, "true"))
			v = qtrue;
		else if (!Q_strcmp(value, "false"))
			v = qfalse;
		else
			v = atoi(value) != 0;
		*(qboolean*) b = v;
	} else {
		Com_Printf("MN_NodeSetProperty: Unimplemented type for property '%s.%s@%s'\n", node->menu->name, node->name, property->string);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Return a float from a node property
 * @param[in] node Requested node
 * @param[in] property Requested property
 */
float MN_GetFloatFromNodeProperty (const menuNode_t* node, const value_t* property)
{
	const byte* b = (const byte*)node + property->ofs;

	if (property->type == V_FLOAT) {
		return *(const float*) b;
	} else if (property->type == V_CVAR_OR_FLOAT) {
		b = *(const byte* const*) b;
		if (!Q_strncmp((const char*)b, "*cvar", 5)) {
			const char* cvarName = b + 6;
			cvar_t *cvar = NULL;
			cvar = Cvar_Get(cvarName, "", 0, "Menu cvar");
			return cvar->value;
		} else {
			return *(const float*) b;
		}
	} else if (property->type == V_INT) {
		return *(const int*) b;
	} else if (property->type == V_BOOL) {
		return *(const qboolean *) b;
	} else {
#if debug
		Com_Printf("MN_GetFloatFromNodeProperty: Unimplemented float getter for property '%s@%s'. If it should return a float, request it.\n", MN_GetPath(node), property->string);
#else
		Com_Printf("MN_GetFloatFromNodeProperty: Property '%s@%s' can't return a float\n", MN_GetPath(node), property->string);
#endif
		return 0;
	}

	return 0;
}

#ifdef DEBUG
/**
 * @brief set a node property from the command line
 */
static void MN_NodeSetProperty_f (void)
{
	menuNode_t *node;
	const value_t *property;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <nodepath> <prop> <value>\n", Cmd_Argv(0));
		return;
	}

	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (!node) {
		Com_Printf("MN_NodeSetProperty_f: Node '%s' not found\n", Cmd_Argv(1));
		return;
	}

	property = MN_GetPropertyFromBehaviour(node->behaviour, Cmd_Argv(2));
	if (!property) {
		Com_Printf("Property '%s.%s@%s' doesn't exist\n", node->menu->name, node->name, Cmd_Argv(2));
		return;
	}

	MN_NodeSetProperty(node, property, Cmd_Argv(3));
}
#endif

/**
 * @brief Invalidate a node and all his parent to request a layout update
 */
void MN_Invalidate (menuNode_t *node)
{
	while (node) {
		if (node->invalidated)
			return;
		node->invalidated = qtrue;
		node = node->parent;
	}
}

static qboolean MN_AbstractNodeDNDEnter (menuNode_t *node)
{
	return qfalse;
}

static qboolean MN_AbstractNodeDNDMove (menuNode_t *node, int x, int y)
{
	return qtrue;
}

static void MN_AbstractNodeDNDLeave (menuNode_t *node)
{
}

static qboolean MN_AbstractNodeDNDDrop (menuNode_t *node, int x, int y)
{
	return qtrue;
}

static qboolean MN_AbstractNodeDNDFinished (menuNode_t *node, qboolean isDroped)
{
	return isDroped;
}

/**
 * @brief Call to update the node layout. This common code revalidates the node tree.
 */
static void MN_AbstractNodeDoLayout (menuNode_t *node)
{
	menuNode_t *child;
	if (!node->invalidated)
		return;

	for (child = node->firstChild; child; child = child->next) {
		child->behaviour->doLayout(child);
	}

	node->invalidated = qfalse;
}

void MN_RegisterAbstractNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractnode";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;

	/* drag and drop callback */
	behaviour->dndEnter = MN_AbstractNodeDNDEnter;
	behaviour->dndMove = MN_AbstractNodeDNDMove;
	behaviour->dndLeave = MN_AbstractNodeDNDLeave;
	behaviour->dndDrop = MN_AbstractNodeDNDDrop;
	behaviour->dndFinished = MN_AbstractNodeDNDFinished;
	behaviour->doLayout = MN_AbstractNodeDoLayout;

	/* some commands */
	Cmd_AddCommand("mn_hidenode", MN_HideNode_f, "Hides a given menu node");
	Cmd_AddCommand("mn_unhidenode", MN_UnHideNode_f, "Unhides a given menu node");
#ifdef DEBUG
	Cmd_AddCommand("debug_mnsetnodeproperty", MN_NodeSetProperty_f, "Set a node property");
#endif
}
