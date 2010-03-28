/**
 * @file m_node_abstractnode.c
 * @brief Every node extends this node
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "m_node_abstractnode.h"
#include "../m_parse.h"
#include "../m_main.h"
#include "../m_components.h"
#include "../m_internal.h"

/**
 * @brief Check the node inheritance
 * @param[in] node Requested node
 * @param[in] behaviourName Behaviour name we check
 * @return True if the node inherits from the behaviour
 */
qboolean MN_NodeInstanceOf (const menuNode_t *node, const char* behaviourName)
{
	const nodeBehaviour_t *behaviour;
	for (behaviour = node->behaviour; behaviour; behaviour = behaviour->super) {
		if (strcmp(behaviour->name, behaviourName) == 0)
			return qtrue;
	}
	return qfalse;
}

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
		pos[0] = (int)(node->size[0] / 2);
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
		pos[1] = (int)(node->size[1] / 2);
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
 * @param[in] node Context node
 * @param[out] pos Absolute position
 */
void MN_GetNodeAbsPos (const menuNode_t* node, vec2_t pos)
{
	assert(node);
	assert(pos);

	/* if we request the position of a non drawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Com_Error(ERR_FATAL, "MN_GetNodeAbsPos: Node '%s' doesn't have a position", node->name);

	Vector2Set(pos, 0, 0);
	while (node) {
#ifdef DEBUG
		if (node->pos[0] != (int)node->pos[0] || node->pos[1] != (int)node->pos[1])
			Com_Error(ERR_FATAL, "MN_GetNodeAbsPos: Node '%s' position %f,%f is not integer", MN_GetPath(node), node->pos[0], node->pos[1]);
#endif
		pos[0] += node->pos[0];
		pos[1] += node->pos[1];
		node = node->parent;
	}
}

/**
 * @brief Update a relative point to an absolute one
 * @param[in] node The requested node
 * @param[in,out] pos A point to transform
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
 * @param[in] node Context node
 * @param[in,out] x an absolute x position
 * @param[in,out] y an absolute y position
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
		Com_Error(ERR_DROP, "MN_NodeAbsoluteToRelativePos: Node '%s' doesn't have a position", node->name);

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
 * @brief Update the node size and fire the size callback
 */
void MN_NodeSetSize (menuNode_t* node, vec2_t size)
{
	if (node->size[0] == size[0] && node->size[1] == size[1])
		return;
	node->size[0] = size[0];
	node->size[1] = size[1];
	node->behaviour->sizeChanged(node);
}

/**
 * @brief Search a child node by given name
 * @note Only search with one depth
 */
menuNode_t *MN_GetNode (const menuNode_t* const node, const char *name)
{
	menuNode_t *current = NULL;

	if (!node)
		return NULL;

	for (current = node->firstChild; current; current = current->next)
		if (!strcmp(name, current->name))
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
	if (newNode->root == NULL)
		newNode->root = node->root;

	assert(node);
	assert(newNode);
	/* insert only a single element */
	assert(!newNode->next);
	/* everything come from the same menu (force the dev to update himself this links) */
	assert(!prevNode || (prevNode->root == newNode->root));
	if (prevNode == NULL) {
		newNode->next = node->firstChild;
		node->firstChild = newNode;
		if (node->lastChild == NULL) {
			node->lastChild = newNode;
		}
		newNode->parent = node;
	} else {
		newNode->next = prevNode->next;
		prevNode->next = newNode;
		if (prevNode == node->lastChild) {
			node->lastChild = newNode;
		}
		newNode->parent = node;
	}

	MN_Invalidate(node);
}

/**
 * @brief Remove a node from a parent node
 * @return The removed node, else NULL
 * @param[in] node Node where is the child
 * @param[in] child Node we want to remove
 */
menuNode_t* MN_RemoveNode (menuNode_t* const node, menuNode_t *child)
{
	assert(node);
	assert(child);
	assert(child->parent == node);
	assert(node->firstChild);

	/** remove the 'child' node */
	if (child == node->firstChild) {
		node->firstChild = child->next;
	} else {
		/** find node before 'child' node */
		menuNode_t *previous = node->firstChild;
		while (previous != NULL) {
			if (previous->next == child)
				break;
			previous = previous->next;
		}
		previous->next = child->next;
		if (previous == NULL)
			return NULL;
	}
	MN_Invalidate(node);

	/** update cache */
	if (node->lastChild == child) {
		node->lastChild = node->firstChild;
		while (node->lastChild && node->lastChild->next)
			node->lastChild = node->lastChild->next;
	}
	child->next = NULL;
	return child;
}

void MN_UpdateRoot (menuNode_t *node, menuNode_t *newRoot)
{
	node->root = newRoot;
	node = node->firstChild;
	while (node) {
		MN_UpdateRoot(node, newRoot);
		node = node->next;
	}
}

/**
 * @brief add a node at the end of the node child
 */
void MN_AppendNode (menuNode_t* const node, menuNode_t *newNode)
{
	MN_InsertNode(node, node->lastChild, newNode);
}

void MN_NodeSetPropertyFromRAW (menuNode_t* node, const value_t *property, void* rawValue, const value_t *rawType)
{
	void *mem = ((byte *) node + property->ofs);

	/** @todo should we only check the type, not the property? IMO yes */
	if (property != rawType)
		Com_Error(ERR_FATAL, "MN_ExecuteSetAction: @%s type %i expected, but @%s type %i found. Property setter to '%s@%s' skipped", rawType->string, rawType->type, property->string, property->type, MN_GetPath(node), property->string);

	if ((property->type & V_UI_MASK) == V_NOT_UI)
		Com_SetValue(node, rawValue, property->type, property->ofs, property->size);
	else if ((property->type & V_UI_MASK) == V_UI_CVAR) {
		MN_FreeStringProperty(*(void**)mem);
		switch (property->type & V_BASETYPEMASK) {
		case V_FLOAT:
			**(float **) mem = *(float*) rawValue;
			break;
		case V_INT:
			**(int **) mem = *(int*) rawValue;
			break;
		default:
			*(byte **) mem = rawValue;
		}
	} else if (property->type == V_UI_ACTION) {
		*(menuAction_t**) mem = (menuAction_t*) rawValue;
	} else if (property->type == V_UI_ICONREF) {
		*(menuIcon_t**) mem = (menuIcon_t*) rawValue;
	} else {
		Com_Error(ERR_FATAL, "MN_ExecuteSetAction: Property type '%d' unsupported", property->type);
	}
	node->behaviour->propertyChanged(node, property);
}

/**
 * @brief Set node property
 */
qboolean MN_NodeSetProperty (menuNode_t* node, const value_t *property, const char* value)
{
	byte* b = (byte*)node + property->ofs;
	const int specialType = property->type & V_UI_MASK;
	int result;
	size_t bytes;

	switch (specialType) {
	case V_NOT_UI:	/* common type */
		/* not possible to set a string */
		if (property->type == V_STRING || property->type == V_LONGSTRING)
			break;
		result = Com_ParseValue(node, value, property->type, property->ofs, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("MN_NodeSetProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return qfalse;
		}
		node->behaviour->propertyChanged(node, property);
		return qtrue;

	case V_UI_CVAR:	/* cvar */
		switch (property->type) {
		case V_CVAR_OR_FLOAT:
			{
				float f;

				if (!strncmp(value, "*cvar", 5)) {
					MN_FreeStringProperty(*(void**)b);
					*(char**) b = Mem_PoolStrDup(value, mn_dynStringPool, 0);
					node->behaviour->propertyChanged(node, property);
					return qtrue;
				}

				result = Com_ParseValue(&f, value, V_FLOAT, 0, sizeof(f), &bytes);
				if (result != RESULT_OK) {
					Com_Printf("MN_NodeSetProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
					return qfalse;
				}

				b = (byte*) (*(void**)b);
				if (!strncmp((const char*)b, "*cvar", 5))
					MN_SetCvar(&((char*)b)[6], NULL, f);
				else
					*(float*) b = f;
				node->behaviour->propertyChanged(node, property);
				return qtrue;
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
			{
				MN_FreeStringProperty(*(void**)b);
				*(char**) b = Mem_PoolStrDup(value, mn_dynStringPool, 0);
				node->behaviour->propertyChanged(node, property);
				return qtrue;
			}
		}
	}

	Com_Printf("MN_NodeSetProperty: Unimplemented type for property '%s@%s'\n", MN_GetPath(node), property->string);
	return qfalse;
}

/**
 * @brief Return a string from a node property
 * @param[in] node Requested node
 * @param[in] property Requested property
 * @return Return a string value of a property, else NULL, if the type is not supported
 */
const char* MN_GetStringFromNodeProperty (const menuNode_t* node, const value_t* property)
{
	const int baseType = property->type & V_UI_MASK;
	const byte* b = (const byte*)node + property->ofs;
	assert(node);
	assert(property);

	switch (baseType) {
	case V_NOT_UI: /* common type */
		return Com_ValueToStr(node, property->type, property->ofs);
	case V_UI_CVAR:
		switch (property->type) {
		case V_CVAR_OR_FLOAT:
			{
				const float f = MN_GetReferenceFloat(node, *(const void*const*)b);
				const int i = f;
				if (f == i)
					return va("%i", i);
				else
					return va("%f", f);
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
			return MN_GetReferenceString(node, *(const void*const*)b);
		}
		break;
	default:
		break;
	}

	Com_Printf("MN_GetStringFromNodeProperty: Unsupported string getter for property type 0x%X (%s@%s)\n", property->type, MN_GetPath(node), property->string);
	return NULL;
}

/**
 * @brief Return a float from a node property
 * @param[in] node Requested node
 * @param[in] property Requested property
 * @return Return the float value of a property, else 0, if the type is not supported
 * @note If the type is not supported, a waring is reported to the console
 */
float MN_GetFloatFromNodeProperty (const menuNode_t* node, const value_t* property)
{
	const byte* b = (const byte*)node + property->ofs;
	assert(node);

	if (property->type == V_FLOAT) {
		return *(const float*) b;
	} else if (property->type == V_CVAR_OR_FLOAT) {
		b = *(const byte* const*) b;
		if (!strncmp((const char*)b, "*cvar", 5)) {
			const char* cvarName = (const char*)b + 6;
			const cvar_t *cvar = Cvar_Get(cvarName, "", 0, "Menu cvar");
			return cvar->value;
		} else {
			return *(const float*) b;
		}
	} else if (property->type == V_INT) {
		return *(const int*) b;
	} else if (property->type == V_BOOL) {
		return *(const qboolean *) b;
	} else {
#ifdef DEBUG
		Com_Printf("MN_GetFloatFromNodeProperty: Unimplemented float getter for property '%s@%s'. If it should return a float, request it.\n", MN_GetPath(node), property->string);
#else
		Com_Printf("MN_GetFloatFromNodeProperty: Property '%s@%s' can't return a float\n", MN_GetPath(node), property->string);
#endif
	}

	return 0;
}

#ifdef DEBUG
/**
 * @brief display value of a node property from the command line
 */
static void MN_NodeGetProperty_f (void)
{
	menuNode_t *node;
	const value_t *property;
	const char *sValue;
	float fValue;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nodepath@prop>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);

	if (node == NULL) {
		Com_Printf("MN_NodeGetProperty_f: Node from path '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}

	if (property == NULL) {
		Com_Printf("MN_NodeGetProperty_f: Property from path '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}

	/* check string value */
	sValue = MN_GetStringFromNodeProperty(node, property);
	if (sValue) {
		Com_Printf("\"%s\" is \"%s\"\n", Cmd_Argv(1), sValue);
		return;
	}

	/* check float value */
	fValue = MN_GetFloatFromNodeProperty(node, property);
	Com_Printf("\"%s\" is \"%f\"\n", Cmd_Argv(1), fValue);
}

/**
 * @brief set a node property from the command line
 * @todo Unify path syntaxe to allow to create a common autocompletion
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
		Com_Printf("Property '%s@%s' doesn't exist\n", MN_GetPath(node), Cmd_Argv(2));
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
 * @brief Call to update a cloned node
 */
static void MN_AbstractNodeClone (const menuNode_t *source, menuNode_t *clone)
{
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void MN_AbstractNodeActivate (menuNode_t *node)
{
	if (node->onClick)
		MN_ExecuteEventActions(node, node->onClick);
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

static void MN_AbstractNodeInit (menuNode_t *node)
{
	menuNode_t* child;
	for (child = node->firstChild; child; child = child->next) {
		if (child->behaviour->init) {
			child->behaviour->init(child);
		}
	}
}

static void MN_AbstractNodeClose (menuNode_t *node)
{
	menuNode_t* child;
	for (child = node->firstChild; child; child = child->next) {
		if (child->behaviour->close) {
			child->behaviour->close(child);
		}
	}
}

/**
 * @brief Callback stub
 */
static void MN_AbstractNodeSizeChanged (menuNode_t *node)
{
	if (node->firstChild != NULL)
		MN_Invalidate(node);
}

static void MN_AbstractNodeVisibilityChange (menuNode_t *node)
{
	if (node->parent != NULL)
		MN_Invalidate(node->parent);
}

static const value_t *propertyWidth;
static const value_t *propertyHeight;
static const value_t *propertySize;
static const value_t *propertyInvis;

static void MN_AbstractNodePropertyChanged (menuNode_t *node, const value_t *property)
{
	if (property == propertyWidth || property == propertyHeight || property == propertySize) {
		node->behaviour->sizeChanged(node);
	} else if (property == propertyInvis) {
		MN_AbstractNodeVisibilityChange(node);
	}
}

static void MN_AbstractNodeCallRemovaAllChild (menuNode_t *node, const menuCallContext_t *context)
{
	if (MN_GetParamNumber(context) != 0) {
		Com_Printf("MN_AbstractNodeCallRemovaAllChild: Invalide number of param\n");
		return;
	}
	MN_DeleteAllChild(node);
}

static void MN_AbstractNodeCallCreateChild (menuNode_t *node, const menuCallContext_t *context)
{
	menuNode_t *child;
	menuNode_t *component;
	const char *name;
	const char *type;

	if (MN_GetParamNumber(context) != 2) {
		Com_Printf("MN_AbstractNodeCallCreateChild: Invalide number of param\n");
		return;
	}

	name = MN_GetParam(context, 1);
	type = MN_GetParam(context, 2);

	component = MN_GetComponent(type);
	if (component) {
		child = MN_CloneNode(component, node->root, qtrue, name, qtrue);
	} else {
		child = MN_AllocNode(name, type, qtrue);
	}

	if (child == NULL) {
		Com_Printf("MN_AbstractNodeCallCreateChild: Impossible de create the node\n");
		return;
	}

	MN_AppendNode(node, child);
}

static void MN_AbstractNodeCallDelete (menuNode_t *node, const menuCallContext_t *context)
{
	if (MN_GetParamNumber(context) != 0) {
		Com_Printf("MN_AbstractNodeCallDelete: Invalide number of param\n");
		return;
	}
	MN_DeleteNode(node);
}

/** @brief valid properties for a node */
static const value_t properties[] = {
	/* Top-left position of the node */
	{"pos", V_POS, offsetof(menuNode_t, pos), MEMBER_SIZEOF(menuNode_t, pos)},
	/* Size of the node */
	{"size", V_POS, offsetof(menuNode_t, size), MEMBER_SIZEOF(menuNode_t, size)},
	/* Width of the node (see also <code>size</code>) */
	{"width", V_FLOAT, offsetof(menuNode_t, size[0]), MEMBER_SIZEOF(menuNode_t, size[0])},
	/* Height of the node (see also <code>size</code>) */
	{"height", V_FLOAT, offsetof(menuNode_t, size[1]), MEMBER_SIZEOF(menuNode_t, size[1])},
	/* Left position of the node (see also <code>pos</code>) */
	{"left", V_FLOAT, offsetof(menuNode_t, pos[0]), MEMBER_SIZEOF(menuNode_t, pos[0])},
	/* Top position of the node (see also <code>pos</code>) */
	{"top", V_FLOAT, offsetof(menuNode_t, pos[1]), MEMBER_SIZEOF(menuNode_t, pos[1])},

	/* If true, the node is not displayed nor or activatable. */
	{"invis", V_BOOL, offsetof(menuNode_t, invis), MEMBER_SIZEOF(menuNode_t, invis)},
	/* If true, the node is disabled. Few nodes support it, fell free to request an update. */
	{"disabled", V_BOOL, offsetof(menuNode_t, disabled), MEMBER_SIZEOF(menuNode_t, disabled)},
	/* If true, the node is not ''tangible''. We click through it, then it will not receive mouse event. */
	{"ghost", V_BOOL, offsetof(menuNode_t, ghost), MEMBER_SIZEOF(menuNode_t, ghost)},
	/* Border size we want to display. */
	{"border", V_INT, offsetof(menuNode_t, border), MEMBER_SIZEOF(menuNode_t, border)},
	/* Padding size we want to use. Few node support it. */
	{"padding", V_INT, offsetof(menuNode_t, padding), MEMBER_SIZEOF(menuNode_t, padding)},
	/* Background color we want to display. */
	{"bgcolor", V_COLOR, offsetof(menuNode_t, bgcolor), MEMBER_SIZEOF(menuNode_t, bgcolor)},
	/* Border color we want to display. */
	{"bordercolor", V_COLOR, offsetof(menuNode_t, bordercolor), MEMBER_SIZEOF(menuNode_t, bordercolor)},

	/*
	 * Used to set the position of the node when the parent use a layout manager.
	 * Else it do nothing.
	 * Available values are: LAYOUTALIGN_TOPLEFT, LAYOUTALIGN_TOP, LAYOUTALIGN_TOPRIGHT,
	 * LAYOUTALIGN_LEFT, LAYOUTALIGN_MIDDLE, LAYOUTALIGN_RIGHT, LAYOUTALIGN_BOTTOMLEFT,
	 * LAYOUTALIGN_BOTTOM, LAYOUTALIGN_BOTTOMRIGHT, LAYOUTALIGN_FILL.
	 * Allowed value depend the layout manager used. The update to date list is into
	 * m_node_panel.c
	 * @image html http://ufoai.ninex.info/wiki/images/Layout.png
	 */
	{"align", V_INT, offsetof(menuNode_t, align), MEMBER_SIZEOF(menuNode_t, align)},

	/*
	 * Used share an int, only used by 1 behavour
	 * @todo move it to the right behaviour, delete it
	 */
	{"num", V_INT, offsetof(menuNode_t, num), MEMBER_SIZEOF(menuNode_t, num)},

	/* Tooltip we want to use. */
	{"tooltip", V_CVAR_OR_LONGSTRING, offsetof(menuNode_t, tooltip), 0},	/* translated in MN_Tooltip */
	/* Image to use. Each behaviour use it like they want.
	 * @todo Move it into behaviour need it.
	 * @todo use V_REF_OF_STRING when its possible ('image' is never a cvar)
	 */
	{"image", V_CVAR_OR_STRING, offsetof(menuNode_t, image), 0},
	/* Text the node will display. */
	{"string", V_CVAR_OR_LONGSTRING, offsetof(menuNode_t, text), 0},	/* no gettext here - this can be a cvar, too */
	/* Text font the node will use.
	 * @todo use V_REF_OF_STRING when its possible ('font' is never a cvar).
	 */
	{"font", V_CVAR_OR_STRING, offsetof(menuNode_t, font), 0},

	/* Text color the node will use. */
	{"color", V_COLOR, offsetof(menuNode_t, color), MEMBER_SIZEOF(menuNode_t, color)},
	/* Text color the node will use when something is selected. */
	{"selectcolor", V_COLOR, offsetof(menuNode_t, selectedColor), MEMBER_SIZEOF(menuNode_t, selectedColor)},
	/* Alignement of the text into the node, or elements into blocks. */
	{"textalign", V_ALIGN, offsetof(menuNode_t, textalign), MEMBER_SIZEOF(menuNode_t, textalign)},
	/* When <code>invis</code> property is false (default value), this condition say if the node is visible or not. It use a script expression. */
	{"visiblewhen", V_UI_IF, offsetof(menuNode_t, visibilityCondition), 0},

	/* Called when the user click with left button into the node. */
	{"onclick", V_UI_ACTION, offsetof(menuNode_t, onClick), MEMBER_SIZEOF(menuNode_t, onClick)},
	/* Called when the user click with right button into the node. */
	{"onrclick", V_UI_ACTION, offsetof(menuNode_t, onRightClick), MEMBER_SIZEOF(menuNode_t, onRightClick)},
	/* Called when the user click with middle button into the node. */
	{"onmclick", V_UI_ACTION, offsetof(menuNode_t, onMiddleClick), MEMBER_SIZEOF(menuNode_t, onMiddleClick)},
	/* Called when the user use the mouse wheel over the node. */
	{"onwheel", V_UI_ACTION, offsetof(menuNode_t, onWheel), MEMBER_SIZEOF(menuNode_t, onWheel)},
	/* Called when the user use the mouse wheel up over the node. */
	{"onwheelup", V_UI_ACTION, offsetof(menuNode_t, onWheelUp), MEMBER_SIZEOF(menuNode_t, onWheelUp)},
	/* Called when the user use the mouse wheel down over the node. */
	{"onwheeldown", V_UI_ACTION, offsetof(menuNode_t, onWheelDown), MEMBER_SIZEOF(menuNode_t, onWheelDown)},
	/* Called when the mouse enter over the node. */
	{"onmouseenter", V_UI_ACTION, offsetof(menuNode_t, onMouseEnter), MEMBER_SIZEOF(menuNode_t, onMouseEnter)},
	/* Called when the mouse go out of the node. */
	{"onmouseleave", V_UI_ACTION, offsetof(menuNode_t, onMouseLeave), MEMBER_SIZEOF(menuNode_t, onMouseLeave)},
	/* Called when the internal content of the nde change. Each behaviour use it how they need it.
	 * @todo Move it where it is need.
	 */
	{"onchange", V_UI_ACTION, offsetof(menuNode_t, onChange), MEMBER_SIZEOF(menuNode_t, onChange)},

	/* Identify an icon the node will use.
	 * @todo Move it where it is need.
	 */
	{"icon", V_UI_ICONREF, offsetof(menuNode_t, icon), MEMBER_SIZEOF(menuNode_t, icon)},

	/* Special attribute only use into the node description to exclude part of the node (see also <code>ghost</code>). Rectangle position is relative to the node. */
	{"excluderect", V_UI_EXCLUDERECT, 0, 0},

	/* Remove all child from the node (only dynamic allocated nodes). */
	{"removeallchild", V_UI_NODEMETHOD, ((size_t) MN_AbstractNodeCallRemovaAllChild), 0},

	/* Create a new child with name and type. */
	{"createchild", V_UI_NODEMETHOD, ((size_t) MN_AbstractNodeCallCreateChild), 0},

	/* Delete the node and remove it from his parent. */
	{"delete", V_UI_NODEMETHOD, ((size_t) MN_AbstractNodeCallDelete), 0},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractnode";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;

	propertyWidth = MN_GetPropertyFromBehaviour(behaviour, "width");
	propertyHeight = MN_GetPropertyFromBehaviour(behaviour, "height");
	propertySize = MN_GetPropertyFromBehaviour(behaviour, "size");
	propertyInvis = MN_GetPropertyFromBehaviour(behaviour, "invis");

	/* callbacks */
	behaviour->dndEnter = MN_AbstractNodeDNDEnter;
	behaviour->dndMove = MN_AbstractNodeDNDMove;
	behaviour->dndLeave = MN_AbstractNodeDNDLeave;
	behaviour->dndDrop = MN_AbstractNodeDNDDrop;
	behaviour->dndFinished = MN_AbstractNodeDNDFinished;
	behaviour->doLayout = MN_AbstractNodeDoLayout;
	behaviour->clone = MN_AbstractNodeClone;
	behaviour->activate = MN_AbstractNodeActivate;
	behaviour->propertyChanged = MN_AbstractNodePropertyChanged;
	behaviour->sizeChanged = MN_AbstractNodeSizeChanged;
	behaviour->init = MN_AbstractNodeInit;
	behaviour->close = MN_AbstractNodeClose;

	/* some commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_mnsetnodeproperty", MN_NodeSetProperty_f, "Set a node property");
	Cmd_AddCommand("debug_mngetnodeproperty", MN_NodeGetProperty_f, "Get a node property");
#endif
}
