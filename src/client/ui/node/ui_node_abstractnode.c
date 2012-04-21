/**
 * @file ui_node_abstractnode.c
 * @brief Every node extends this node
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"
#include "../ui_parse.h"
#include "../ui_main.h"
#include "../ui_components.h"
#include "../ui_internal.h"

/**
 * @brief Check the node inheritance
 * @param[in] node Requested node
 * @param[in] behaviourName Behaviour name we check
 * @return True if the node inherits from the behaviour
 */
qboolean UI_NodeInstanceOf (const uiNode_t *node, const char* behaviourName)
{
	const uiBehaviour_t *behaviour;
	for (behaviour = node->behaviour; behaviour; behaviour = behaviour->super) {
		if (Q_streq(behaviour->name, behaviourName))
			return qtrue;
	}
	return qfalse;
}

/**
 * @brief Check the node inheritance
 * @param[in] node Requested node
 * @param[in] behaviour Behaviour we check
 * @return True if the node inherits from the behaviour
 */
qboolean UI_NodeInstanceOfPointer (const uiNode_t *node, const uiBehaviour_t* behaviour)
{
	const uiBehaviour_t *b;
	for (b = node->behaviour; b; b = b->super) {
		if (b == behaviour)
			return qtrue;
	}
	return qfalse;
}

/**
 * @brief return a relative position of a point into a node.
 * @param [in] node Requested node
 * @param [out] pos Result position
 * @param [in] direction
 * @note For example we can request the right-bottom corner with LAYOUTALIGN_BOTTOMRIGHT
 */
void UI_NodeGetPoint (const uiNode_t* node, vec2_t pos, int direction)
{
	switch (UI_GET_HORIZONTAL_ALIGN(direction)) {
	case LAYOUTALIGN_H_LEFT:
		pos[0] = 0;
		break;
	case LAYOUTALIGN_H_MIDDLE:
		pos[0] = (int)(node->size[0] / 2);
		break;
	case LAYOUTALIGN_H_RIGHT:
		pos[0] = node->size[0];
		break;
	default:
		Com_Printf("UI_NodeGetPoint: Align '%d' (0x%X) is not a common alignment", direction, direction);
		pos[0] = 0;
		pos[1] = 0;
		return;
	}

	switch (UI_GET_VERTICAL_ALIGN(direction)) {
	case LAYOUTALIGN_V_TOP:
		pos[1] = 0;
		break;
	case LAYOUTALIGN_V_MIDDLE:
		pos[1] = (int)(node->size[1] / 2);
		break;
	case LAYOUTALIGN_V_BOTTOM:
		pos[1] = node->size[1];
		break;
	default:
		Com_Printf("UI_NodeGetPoint: Align '%d' (0x%X) is not a common alignment", direction, direction);
		pos[0] = 0;
		pos[1] = 0;
		return;
	}
}

/**
 * @brief Returns the absolute position of a node.
 * @param[in] node Context node
 * @param[out] pos Absolute position
 */
void UI_GetNodeAbsPos (const uiNode_t* node, vec2_t pos)
{
	assert(node);
	assert(pos);

	/* if we request the position of a non drawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Com_Error(ERR_FATAL, "UI_GetNodeAbsPos: Node '%s' doesn't have a position", node->name);

	Vector2Set(pos, 0, 0);
	while (node) {
#ifdef DEBUG
		if (node->pos[0] != (int)node->pos[0] || node->pos[1] != (int)node->pos[1])
			Com_Error(ERR_FATAL, "UI_GetNodeAbsPos: Node '%s' position %f,%f is not integer", UI_GetPath(node), node->pos[0], node->pos[1]);
#endif
		pos[0] += node->pos[0];
		pos[1] += node->pos[1];
		node = node->parent;
	}
}

/**
 * @brief Returns the absolute position of a node in the screen.
 * Screen position is not used for the node rendering cause we use OpenGL
 * translations. But this function is need for some R_functions methodes.
 * @param[in] node Context node
 * @param[out] pos Absolute position into the screen
 */
void UI_GetNodeScreenPos (const uiNode_t* node, vec2_t pos)
{
	assert(node);
	assert(pos);

	/* if we request the position of a non drawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Com_Error(ERR_FATAL, "UI_GetNodeAbsPos: Node '%s' doesn't have a position", node->name);

	Vector2Set(pos, 0, 0);
	while (node) {
#ifdef DEBUG
		if (node->pos[0] != (int)node->pos[0] || node->pos[1] != (int)node->pos[1])
			Com_Error(ERR_FATAL, "UI_GetNodeAbsPos: Node '%s' position %f,%f is not integer", UI_GetPath(node), node->pos[0], node->pos[1]);
#endif
		pos[0] += node->pos[0];
		pos[1] += node->pos[1];
		node = node->parent;

		if (node && node->behaviour->getClientPosition) {
			vec2_t clientPosition = {0, 0};
			node->behaviour->getClientPosition(node, clientPosition);
			pos[0] += clientPosition[0];
			pos[1] += clientPosition[1];
		}
	}
}

/**
 * @brief Update a relative point to an absolute one
 * @param[in] node The requested node
 * @param[in,out] pos A point to transform
 */
void UI_NodeRelativeToAbsolutePoint (const uiNode_t* node, vec2_t pos)
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
void UI_NodeAbsoluteToRelativePos (const uiNode_t* node, int *x, int *y)
{
	assert(node != NULL);
	/* if we request the position of an undrawable node, there is a problem */
	assert(node->behaviour->isVirtual == qfalse);
	assert(x != NULL);
	assert(y != NULL);

	/* if we request the position of an undrawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Com_Error(ERR_DROP, "UI_NodeAbsoluteToRelativePos: Node '%s' doesn't have a position", node->name);

	while (node) {
		*x -= node->pos[0];
		*y -= node->pos[1];

		if (node->behaviour->getClientPosition) {
			vec2_t clientPosition = {0, 0};
			node->behaviour->getClientPosition(node, clientPosition);
			*x -= clientPosition[0];
			*y -= clientPosition[1];
		}

		node = node->parent;
	}
}

/**
 * @brief Hides a given node
 * @note Sanity check whether node is null included
 */
void UI_HideNode (uiNode_t* node)
{
	if (node)
		node->invis = qtrue;
	else
		Com_Printf("UI_HideNode: No node given\n");
}

/**
 * @brief Unhides a given node
 * @note Sanity check whether node is null included
 */
void UI_UnHideNode (uiNode_t* node)
{
	if (node)
		node->invis = qfalse;
	else
		Com_Printf("UI_UnHideNode: No node given\n");
}

/**
 * @brief Update the node size and fire the size callback
 */
void UI_NodeSetSize (uiNode_t* node, vec2_t size)
{
	if (Vector2Equal(node->size, size))
		return;
	node->size[0] = size[0];
	node->size[1] = size[1];
	node->behaviour->sizeChanged(node);
}

/**
 * @brief Search a child node by given name
 * @note Only search with one depth
 */
uiNode_t *UI_GetNode (const uiNode_t* const node, const char *name)
{
	uiNode_t *current = NULL;

	if (!node)
		return NULL;

	for (current = node->firstChild; current; current = current->next)
		if (Q_streq(name, current->name))
			break;

	return current;
}

/**
 * @brief Insert a node next another one into a node. If prevNode is NULL add the node on the head of the window
 * @param[in] node Node where inserting a node
 * @param[in] prevNode previous node, will became before the newNode; else NULL if newNode will become the first child of the node
 * @param[in] newNode node we insert
 */
void UI_InsertNode (uiNode_t* const node, uiNode_t *prevNode, uiNode_t *newNode)
{
	if (newNode->root == NULL)
		newNode->root = node->root;

	assert(node);
	assert(newNode);
	/* insert only a single element */
	assert(!newNode->next);
	/* everything come from the same window (force the dev to update himself this links) */
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

	if (newNode->root && newNode->indexed)
		UI_WindowNodeAddIndexedNode(newNode->root, newNode);

	UI_Invalidate(node);
}

/**
 * @brief Remove a node from a parent node
 * @return The removed node, else NULL
 * @param[in] node Node where is the child
 * @param[in] child Node we want to remove
 */
uiNode_t* UI_RemoveNode (uiNode_t* const node, uiNode_t *child)
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
		uiNode_t *previous = node->firstChild;
		while (previous != NULL) {
			if (previous->next == child)
				break;
			previous = previous->next;
		}
		previous->next = child->next;
		if (previous == NULL)
			return NULL;
	}
	UI_Invalidate(node);

	/** update cache */
	if (node->lastChild == child) {
		node->lastChild = node->firstChild;
		while (node->lastChild && node->lastChild->next)
			node->lastChild = node->lastChild->next;
	}
	if (child->root && child->indexed)
		UI_WindowNodeRemoveIndexedNode(child->root, child);

	child->next = NULL;
	return child;
}

void UI_UpdateRoot (uiNode_t *node, uiNode_t *newRoot)
{
	node->root = newRoot;
	node = node->firstChild;
	while (node) {
		UI_UpdateRoot(node, newRoot);
		node = node->next;
	}
}

/**
 * @brief add a node at the end of the node child
 */
void UI_AppendNode (uiNode_t* const node, uiNode_t *newNode)
{
	UI_InsertNode(node, node->lastChild, newNode);
}

void UI_NodeSetPropertyFromRAW (uiNode_t* node, const value_t *property, const void* rawValue, int rawType)
{
	void *mem = ((byte *) node + property->ofs);

	if (property->type != rawType) {
		Com_Printf("UI_NodeSetPropertyFromRAW: type %i expected, but @%s type %i found. Property setter to '%s@%s' skipped\n", rawType, property->string, property->type, UI_GetPath(node), property->string);
		return;
	}

	if ((property->type & V_UI_MASK) == V_NOT_UI)
		Com_SetValue(node, rawValue, property->type, property->ofs, property->size);
	else if ((property->type & V_UI_MASK) == V_UI_CVAR) {
		UI_FreeStringProperty(*(void**)mem);
		switch (property->type & V_BASETYPEMASK) {
		case V_FLOAT:
			**(float **) mem = *(const float*) rawValue;
			break;
		case V_INT:
			**(int **) mem = *(const int*) rawValue;
			break;
		default:
			*(const byte **) mem = (const byte*) rawValue;
			break;
		}
	} else if (property->type == V_UI_ACTION) {
		*(const uiAction_t**) mem = (const uiAction_t*) rawValue;
	} else if (property->type == V_UI_SPRITEREF) {
		*(const uiSprite_t**) mem = (const uiSprite_t*) rawValue;
	} else {
		Com_Error(ERR_FATAL, "UI_NodeSetPropertyFromRAW: Property type '%d' unsupported", property->type);
	}
	node->behaviour->propertyChanged(node, property);
}

/**
 * @brief Set node property
 */
qboolean UI_NodeSetProperty (uiNode_t* node, const value_t *property, const char* value)
{
	byte* b = (byte*)node + property->ofs;
	const int specialType = property->type & V_UI_MASK;
	int result;
	size_t bytes;

	switch (specialType) {
	case V_NOT_UI:	/* common type */
		result = Com_ParseValue(node, value, property->type, property->ofs, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("UI_NodeSetProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return qfalse;
		}
		node->behaviour->propertyChanged(node, property);
		return qtrue;

	case V_UI_CVAR:	/* cvar */
		switch ((int)property->type) {
		case V_UI_CVAR:
			if (Q_strstart(value, "*cvar:")) {
				UI_FreeStringProperty(*(void**)b);
				*(char**) b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
				node->behaviour->propertyChanged(node, property);
				return qtrue;
			}
			break;
		case V_CVAR_OR_FLOAT:
			{
				float f;

				if (Q_strstart(value, "*cvar:")) {
					UI_FreeStringProperty(*(void**)b);
					*(char**) b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
					node->behaviour->propertyChanged(node, property);
					return qtrue;
				}

				result = Com_ParseValue(&f, value, V_FLOAT, 0, sizeof(f), &bytes);
				if (result != RESULT_OK) {
					Com_Printf("UI_NodeSetProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
					return qfalse;
				}

				b = (byte*) (*(void**)b);
				if (Q_strstart((const char*)b, "*cvar"))
					Cvar_SetValue(&((char*)b)[6], f);
				else
					*(float*) b = f;
				node->behaviour->propertyChanged(node, property);
				return qtrue;
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
			{
				UI_FreeStringProperty(*(void**)b);
				*(char**) b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
				node->behaviour->propertyChanged(node, property);
				return qtrue;
			}
		}
		break;

	case V_UI:
		switch ((int)property->type) {
		case V_UI_SPRITEREF:
			{
				uiSprite_t* sprite = UI_GetSpriteByName(value);
				*(const uiSprite_t**) b = sprite;
				return qtrue;
			}
		}
		break;
	}

	Com_Printf("UI_NodeSetProperty: Unimplemented type for property '%s@%s'\n", UI_GetPath(node), property->string);
	return qfalse;
}

/**
 * @brief Return a string from a node property
 * @param[in] node Requested node
 * @param[in] property Requested property
 * @return Return a string value of a property, else NULL, if the type is not supported
 */
const char* UI_GetStringFromNodeProperty (const uiNode_t* node, const value_t* property)
{
	const int baseType = property->type & V_UI_MASK;
	const byte* b = (const byte*)node + property->ofs;
	assert(node);
	assert(property);

	switch (baseType) {
	case V_NOT_UI: /* common type */
		return Com_ValueToStr(node, property->type, property->ofs);
	case V_UI_CVAR:
		switch ((int)property->type) {
		case V_CVAR_OR_FLOAT:
			{
				const float f = UI_GetReferenceFloat(node, *(const void*const*)b);
				const int i = f;
				if (f == i)
					return va("%i", i);
				else
					return va("%f", f);
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
		case V_UI_CVAR:
			return UI_GetReferenceString(node, *(const char*const*)b);
		}
		break;
	default:
		break;
	}

	Com_Printf("UI_GetStringFromNodeProperty: Unsupported string getter for property type 0x%X (%s@%s)\n", property->type, UI_GetPath(node), property->string);
	return NULL;
}

/**
 * @brief Return a float from a node property
 * @param[in] node Requested node
 * @param[in] property Requested property
 * @return Return the float value of a property, else 0, if the type is not supported
 * @note If the type is not supported, a waring is reported to the console
 */
float UI_GetFloatFromNodeProperty (const uiNode_t* node, const value_t* property)
{
	const byte* b = (const byte*)node + property->ofs;
	assert(node);

	if (property->type == V_FLOAT) {
		return *(const float*) b;
	} else if ((property->type & V_UI_MASK) == V_UI_CVAR) {
		b = *(const byte* const*) b;
		if (Q_strstart((const char*)b, "*cvar")) {
			const char* cvarName = (const char*)b + 6;
			const cvar_t *cvar = Cvar_Get(cvarName, "", 0, "UI script cvar property");
			return cvar->value;
		} else if (property->type == V_CVAR_OR_FLOAT) {
			return *(const float*) b;
		} else if (property->type == V_CVAR_OR_STRING) {
			return atof((const char*)b);
		}
	} else if (property->type == V_INT) {
		return *(const int*) b;
	} else if (property->type == V_BOOL) {
		return *(const qboolean *) b;
	} else {
#ifdef DEBUG
		Com_Printf("UI_GetFloatFromNodeProperty: Unimplemented float getter for property '%s@%s'. If it should return a float, request it.\n", UI_GetPath(node), property->string);
#else
		Com_Printf("UI_GetFloatFromNodeProperty: Property '%s@%s' can't return a float\n", UI_GetPath(node), property->string);
#endif
	}

	return 0;
}

#ifdef DEBUG
/**
 * @brief display value of a node property from the command line
 */
static void UI_NodeGetProperty_f (void)
{
	uiNode_t *node;
	const value_t *property;
	const char *sValue;
	float fValue;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nodepath@prop>\n", Cmd_Argv(0));
		return;
	}

	UI_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);

	if (node == NULL) {
		Com_Printf("UI_NodeGetProperty_f: Node from path '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}

	if (property == NULL) {
		Com_Printf("UI_NodeGetProperty_f: Property from path '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}

	/* check string value */
	sValue = UI_GetStringFromNodeProperty(node, property);
	if (sValue) {
		Com_Printf("\"%s\" is \"%s\"\n", Cmd_Argv(1), sValue);
		return;
	}

	/* check float value */
	fValue = UI_GetFloatFromNodeProperty(node, property);
	Com_Printf("\"%s\" is \"%f\"\n", Cmd_Argv(1), fValue);
}

/**
 * @brief set a node property from the command line
 * @todo Unify path syntaxe to allow to create a common autocompletion
 */
static void UI_NodeSetProperty_f (void)
{
	uiNode_t *node;
	const value_t *property;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <nodepath> <prop> <value>\n", Cmd_Argv(0));
		return;
	}

	node = UI_GetNodeByPath(Cmd_Argv(1));
	if (!node) {
		Com_Printf("UI_NodeSetProperty_f: Node '%s' not found\n", Cmd_Argv(1));
		return;
	}

	property = UI_GetPropertyFromBehaviour(node->behaviour, Cmd_Argv(2));
	if (!property) {
		Com_Printf("Property '%s@%s' doesn't exist\n", UI_GetPath(node), Cmd_Argv(2));
		return;
	}

	UI_NodeSetProperty(node, property, Cmd_Argv(3));
}
#endif

/**
 * @brief Invalidate a node and all his parent to request a layout update
 */
void UI_Invalidate (uiNode_t *node)
{
	while (node) {
		if (node->invalidated)
			return;
		node->invalidated = qtrue;
		node = node->parent;
	}
}

/**
 * @brief Validate a node tree
 */
void UI_Validate (uiNode_t *node)
{
	if (node->invalidated)
		node->behaviour->doLayout(node);
}

static qboolean UI_AbstractNodeDNDEnter (uiNode_t *node)
{
	return qfalse;
}

static qboolean UI_AbstractNodeDNDMove (uiNode_t *node, int x, int y)
{
	return qtrue;
}

static void UI_AbstractNodeDNDLeave (uiNode_t *node)
{
}

static qboolean UI_AbstractNodeDNDDrop (uiNode_t *node, int x, int y)
{
	return qtrue;
}

static qboolean UI_AbstractNodeDNDFinished (uiNode_t *node, qboolean isDroped)
{
	return isDroped;
}

/**
 * @brief Call to update a cloned node
 */
static void UI_AbstractNodeClone (const uiNode_t *source, uiNode_t *clone)
{
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void UI_AbstractNodeActivate (uiNode_t *node)
{
	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);
}

/**
 * @brief Call to update the node layout. This common code revalidates the node tree.
 */
static void UI_AbstractNodeDoLayout (uiNode_t *node)
{
	uiNode_t *child;
	if (!node->invalidated)
		return;

	for (child = node->firstChild; child; child = child->next) {
		child->behaviour->doLayout(child);
	}

	node->invalidated = qfalse;
}

static void UI_AbstractNodeInit (uiNode_t *node, linkedList_t *params)
{
	uiNode_t* child;
	for (child = node->firstChild; child; child = child->next) {
		if (child->behaviour->windowOpened) {
			child->behaviour->windowOpened(child, NULL);
		}
	}
}

static void UI_AbstractNodeClose (uiNode_t *node)
{
	uiNode_t* child;
	for (child = node->firstChild; child; child = child->next) {
		if (child->behaviour->windowClosed) {
			child->behaviour->windowClosed(child);
		}
	}
}

/**
 * @brief Callback stub
 */
static void UI_AbstractNodeSizeChanged (uiNode_t *node)
{
	if (node->firstChild != NULL)
		UI_Invalidate(node);
}

static void UI_AbstractNodeVisibilityChange (uiNode_t *node)
{
	if (node->parent != NULL)
		UI_Invalidate(node->parent);
}

static const value_t *propertyWidth;
static const value_t *propertyHeight;
static const value_t *propertySize;
static const value_t *propertyInvis;

static void UI_AbstractNodePropertyChanged (uiNode_t *node, const value_t *property)
{
	if (property == propertyWidth || property == propertyHeight || property == propertySize) {
		node->behaviour->sizeChanged(node);
	} else if (property == propertyInvis) {
		UI_AbstractNodeVisibilityChange(node);
	}
}

static void UI_AbstractNodeCallRemovaAllChild (uiNode_t *node, const uiCallContext_t *context)
{
	if (UI_GetParamNumber(context) != 0) {
		Com_Printf("UI_AbstractNodeCallRemovaAllChild: Invalid number of parameters\n");
		return;
	}
	UI_DeleteAllChild(node);
}

static void UI_AbstractNodeCallCreateChild (uiNode_t *node, const uiCallContext_t *context)
{
	uiNode_t *child;
	uiNode_t *component;
	const char *name;
	const char *type;

	if (UI_GetParamNumber(context) != 2) {
		Com_Printf("UI_AbstractNodeCallCreateChild: Invalid number of parameters\n");
		return;
	}

	name = UI_GetParam(context, 1);
	type = UI_GetParam(context, 2);

	component = UI_GetComponent(type);
	if (component) {
		child = UI_CloneNode(component, node->root, qtrue, name, qtrue);
	} else {
		child = UI_AllocNode(name, type, qtrue);
	}

	if (child == NULL) {
		Com_Printf("UI_AbstractNodeCallCreateChild: Impossible to create the node\n");
		return;
	}

	UI_AppendNode(node, child);
}

static void UI_AbstractNodeCallDelete (uiNode_t *node, const uiCallContext_t *context)
{
	if (UI_GetParamNumber(context) != 0) {
		Com_Printf("UI_AbstractNodeCallDelete: Invalid number of parameters\n");
		return;
	}
	UI_DeleteNode(node);
}

static qboolean UI_AbstractNodeScroll (uiNode_t *node, int deltaX, int deltaY)
{
	if (node->onWheelUp && deltaY < 0) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		return qtrue;
	}
	if (node->onWheelDown && deltaY > 0) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		return qtrue;
	}
	if (node->onWheel && deltaY != 0) {
		UI_ExecuteEventActions(node, node->onWheel);
		return qtrue;
	}
	return qfalse;
}

void UI_RegisterAbstractNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "abstractnode";
	behaviour->isAbstract = qtrue;

	/* callbacks */
	behaviour->dndEnter = UI_AbstractNodeDNDEnter;
	behaviour->dndMove = UI_AbstractNodeDNDMove;
	behaviour->dndLeave = UI_AbstractNodeDNDLeave;
	behaviour->dndDrop = UI_AbstractNodeDNDDrop;
	behaviour->dndFinished = UI_AbstractNodeDNDFinished;
	behaviour->doLayout = UI_AbstractNodeDoLayout;
	behaviour->clone = UI_AbstractNodeClone;
	behaviour->activate = UI_AbstractNodeActivate;
	behaviour->propertyChanged = UI_AbstractNodePropertyChanged;
	behaviour->sizeChanged = UI_AbstractNodeSizeChanged;
	behaviour->windowOpened = UI_AbstractNodeInit;
	behaviour->windowClosed = UI_AbstractNodeClose;
	behaviour->scroll = UI_AbstractNodeScroll;

	/* Top-left position of the node */
	UI_RegisterNodeProperty(behaviour, "pos", V_POS, uiNode_t, pos);
	/* Size of the node */
	propertySize = UI_RegisterNodeProperty(behaviour, "size", V_POS, uiNode_t, size);
	/* Width of the node (see also <code>size</code>) */
	propertyWidth = UI_RegisterNodeProperty(behaviour, "width", V_FLOAT, uiNode_t, size[0]);
	/* Height of the node (see also <code>size</code>) */
	propertyHeight = UI_RegisterNodeProperty(behaviour, "height", V_FLOAT, uiNode_t, size[1]);
	/* Left position of the node (see also <code>pos</code>) */
	UI_RegisterNodeProperty(behaviour, "left", V_FLOAT, uiNode_t, pos[0]);
	/* Top position of the node (see also <code>pos</code>) */
	UI_RegisterNodeProperty(behaviour, "top", V_FLOAT, uiNode_t, pos[1]);

	/* If true, the node name is indexed into the window. We can access to the node with
	 * the path "windowName#nodeName"
	 */
	UI_RegisterNodeProperty(behaviour, "indexed", V_BOOL, uiNode_t, indexed);
	/* If true, the node is not displayed nor or activatable. */
	propertyInvis = UI_RegisterNodeProperty(behaviour, "invis", V_BOOL, uiNode_t, invis);
	/* If true, the node is disabled. Few nodes support it, fell free to request an update. */
	UI_RegisterNodeProperty(behaviour, "disabled", V_BOOL, uiNode_t, disabled);
	/* If true, the node is not ''tangible''. We click through it, then it will not receive mouse event. */
	UI_RegisterNodeProperty(behaviour, "ghost", V_BOOL, uiNode_t, ghost);
	/* Border size we want to display. */
	UI_RegisterNodeProperty(behaviour, "border", V_INT, uiNode_t, border);
	/* Padding size we want to use. Few node support it. */
	UI_RegisterNodeProperty(behaviour, "padding", V_INT, uiNode_t, padding);
	/* Background color we want to display. */
	UI_RegisterNodeProperty(behaviour, "bgcolor", V_COLOR, uiNode_t, bgcolor);
	/* Border color we want to display. */
	UI_RegisterNodeProperty(behaviour, "bordercolor", V_COLOR, uiNode_t, bordercolor);

	/*
	 * Used to set the position of the node when the parent use a layout manager.
	 * Else it do nothing.
	 * Available values are: LAYOUTALIGN_TOPLEFT, LAYOUTALIGN_TOP, LAYOUTALIGN_TOPRIGHT,
	 * LAYOUTALIGN_LEFT, LAYOUTALIGN_MIDDLE, LAYOUTALIGN_RIGHT, LAYOUTALIGN_BOTTOMLEFT,
	 * LAYOUTALIGN_BOTTOM, LAYOUTALIGN_BOTTOMRIGHT, LAYOUTALIGN_FILL.
	 * Allowed value depend the layout manager used. The update to date list is into
	 * ui_node_panel.c
	 * @image html http://ufoai.org/wiki/images/Layout.png
	 */
	UI_RegisterNodeProperty(behaviour, "align", V_INT, uiNode_t, align);

	/*
	 * Used share an int, only used by 1 behavour
	 * @todo move it to the right behaviour, delete it
	 */
	UI_RegisterNodeProperty(behaviour, "num", V_INT, uiNode_t, num);

	/* Tooltip we want to use. */
	UI_RegisterNodeProperty(behaviour, "tooltip", V_CVAR_OR_LONGSTRING, uiNode_t, tooltip);
	/* Image to use. Each behaviour use it like they want.
	 * @todo Move it into behaviour need it.
	 * @todo use V_REF_OF_STRING when its possible ('image' is never a cvar)
	 */
	UI_RegisterNodeProperty(behaviour, "image", V_CVAR_OR_STRING, uiNode_t, image);
	/* Text the node will display.
	 */
	UI_RegisterNodeProperty(behaviour, "string", V_CVAR_OR_LONGSTRING, uiNode_t, text);
	/* Text font the node will use.
	 * @todo use V_REF_OF_STRING when its possible ('font' is never a cvar).
	 */
	UI_RegisterNodeProperty(behaviour, "font", V_CVAR_OR_STRING, uiNode_t, font);

	/* Text color the node will use. */
	UI_RegisterNodeProperty(behaviour, "color", V_COLOR, uiNode_t, color);
	/* Text color the node will use when something is selected. */
	UI_RegisterNodeProperty(behaviour, "selectcolor", V_COLOR, uiNode_t, selectedColor);
	/* Alignement of the text into the node, or elements into blocks. */
	UI_RegisterNodeProperty(behaviour, "contentalign", V_UI_ALIGN, uiNode_t, contentAlign);
	/* When <code>invis</code> property is false (default value);
	 * this condition say if the node is visible or not. It use a script expression.
	 */
	UI_RegisterNodeProperty(behaviour, "visiblewhen", V_UI_IF, uiNode_t, visibilityCondition);

	/* Called when the user click with left button into the node. */
	UI_RegisterNodeProperty(behaviour, "onclick", V_UI_ACTION, uiNode_t, onClick);
	/* Called when the user click with right button into the node. */
	UI_RegisterNodeProperty(behaviour, "onrclick", V_UI_ACTION, uiNode_t, onRightClick);
	/* Called when the user click with middle button into the node. */
	UI_RegisterNodeProperty(behaviour, "onmclick", V_UI_ACTION, uiNode_t, onMiddleClick);
	/* Called when the user use the mouse wheel over the node. */
	UI_RegisterNodeProperty(behaviour, "onwheel", V_UI_ACTION, uiNode_t, onWheel);
	/* Called when the user use the mouse wheel up over the node. */
	UI_RegisterNodeProperty(behaviour, "onwheelup", V_UI_ACTION, uiNode_t, onWheelUp);
	/* Called when the user use the mouse wheel down over the node. */
	UI_RegisterNodeProperty(behaviour, "onwheeldown", V_UI_ACTION, uiNode_t, onWheelDown);
	/* Called when the mouse enter over the node. */
	UI_RegisterNodeProperty(behaviour, "onmouseenter", V_UI_ACTION, uiNode_t, onMouseEnter);
	/* Called when the mouse go out of the node. */
	UI_RegisterNodeProperty(behaviour, "onmouseleave", V_UI_ACTION, uiNode_t, onMouseLeave);
	/* Called when the internal content of the nde change. Each behaviour use it how they need it.
	 * @todo Move it where it is need.
	 */
	UI_RegisterNodeProperty(behaviour, "onchange", V_UI_ACTION, uiNode_t, onChange);

	/* Special attribute only use into the node description to exclude part of the node
	 * (see also <code>ghost</code>). Rectangle position is relative to the node. */
	UI_RegisterNodeProperty(behaviour, "excluderect", V_UI_EXCLUDERECT, uiNode_t, firstExcludeRect);

	/* Remove all child from the node (only dynamic allocated nodes). */
	UI_RegisterNodeMethod(behaviour, "removeallchild", UI_AbstractNodeCallRemovaAllChild);

	/* Create a new child with name and type. */
	UI_RegisterNodeMethod(behaviour, "createchild", UI_AbstractNodeCallCreateChild);

	/* Delete the node and remove it from his parent. */
	UI_RegisterNodeMethod(behaviour, "delete", UI_AbstractNodeCallDelete);

	/** @todo move it into common? */
	Com_RegisterConstInt("ALIGN_UL", ALIGN_UL);
	Com_RegisterConstInt("ALIGN_UC", ALIGN_UC);
	Com_RegisterConstInt("ALIGN_UR", ALIGN_UR);
	Com_RegisterConstInt("ALIGN_CL", ALIGN_CL);
	Com_RegisterConstInt("ALIGN_CC", ALIGN_CC);
	Com_RegisterConstInt("ALIGN_CR", ALIGN_CR);
	Com_RegisterConstInt("ALIGN_LL", ALIGN_LL);
	Com_RegisterConstInt("ALIGN_LC", ALIGN_LC);
	Com_RegisterConstInt("ALIGN_LR", ALIGN_LR);

	/* some commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_mnsetnodeproperty", UI_NodeSetProperty_f, "Set a node property");
	Cmd_AddCommand("debug_mngetnodeproperty", UI_NodeGetProperty_f, "Get a node property");
#endif
}
