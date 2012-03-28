/**
 * @file ui_node.cpp
 * @brief C interface to allow to access to cpp node code.
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

#include <typeinfo>
#include "ui_main.h"
#include "ui_behaviour.h"
#include "ui_nodes.h"
#include "ui_node.h"
#include "node/ui_node_abstractnode.h"
#include "node/ui_node_abstractoption.h"
#include "node/ui_node_battlescape.h"
#include "node/ui_node_panel.h"
#include "ui_parse.h"
#include "ui_components.h"
#include "ui_internal.h"

qboolean UI_Node_IsVirtual(const struct uiNode_s *node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b == NULL;
}

qboolean UI_Node_IsDrawable(const struct uiNode_s *node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b != NULL;
}

qboolean UI_Node_IsOptionContainer(const struct uiNode_s *node)
{
	uiAbstractOptionNode* b = dynamic_cast<uiAbstractOptionNode*>(node->behaviour->manager);
	return b != NULL;
}

qboolean UI_Node_IsWindow(const struct uiNode_s *node)
{
	uiWindowNode* b = dynamic_cast<uiWindowNode*>(node->behaviour->manager);
	return b != NULL;
}

qboolean UI_Node_IsBattleScape(const struct uiNode_s *node)
{
	uiBattleScapeNode *b = dynamic_cast<uiBattleScapeNode*>(node->behaviour->manager);
	return b != NULL;
}

qboolean UI_Node_IsAbstract(const struct uiNode_s *node)
{
	return node->behaviour->isAbstract;
}

qboolean UI_Node_IsDrawItselfChild(const struct uiNode_s *node)
{
	return node->behaviour->drawItselfChild;
}

/**
 * @todo Use typeid when it is possible
 */
qboolean UI_Node_IsFunction(const struct uiNode_s *node)
{
	return node->behaviour->isFunction;
}

/**
 * @todo Use typeid when it is possible
 */
qboolean UI_Node_IsScrollableContainer(const struct uiNode_s *node)
{
	uiAbstractScrollableNode *b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager);
	return b != NULL;
}

const char* UI_Node_GetWidgetName(const struct uiNode_s *node)
{
	return node->behaviour->name;
}

intptr_t UI_Node_GetMemorySize(const struct uiNode_s *node)
{
	return sizeof(*node) + node->behaviour->extraDataSize;
}

void UI_Node_Draw(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->draw(node);
}

void UI_Node_DrawTooltip(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->drawTooltip(node, x, y);
}

void UI_Node_DrawOverWindow(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->drawOverWindow(node);
}

/* mouse events */
void UI_Node_LeftClick(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->leftClick(node, x, y);
}

void UI_Node_RightClick(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->rightClick(node, x, y);
}

void UI_Node_MiddleClick(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->middleClick(node, x, y);
}

qboolean UI_Node_Scroll(struct uiNode_s *node, int deltaX, int deltaY)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->scroll(node, deltaX, deltaY);
}

void UI_Node_MouseMove(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->mouseMove(node, x, y);
}

void UI_Node_MouseDown(struct uiNode_s *node, int x, int y, int button)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->mouseDown(node, x, y, button);
}

void UI_Node_MouseUp(struct uiNode_s *node, int x, int y, int button)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->mouseUp(node, x, y, button);
}

void UI_Node_CapturedMouseMove(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->capturedMouseMove(node, x, y);
}

void UI_Node_CapturedMouseLost(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->capturedMouseLost(node);
}

/* system allocation */

void UI_Node_Loading(struct uiNode_s *node)
{
	uiNode *b = node->behaviour->manager;
	b->loading(node);
}

void UI_Node_Loaded(struct uiNode_s *node)
{
	uiNode *b = node->behaviour->manager;
	b->loaded(node);
}

void UI_Node_Clone(const struct uiNode_s *source, struct uiNode_s *clone)
{
	uiNode *b = source->behaviour->manager;
	b->clone(source, clone);
}

void UI_Node_NewNode(struct uiNode_s *node)
{
	uiNode *b = node->behaviour->manager;
	b->newNode(node);
}

void UI_Node_DeleteNode(struct uiNode_s *node)
{
	uiNode *b = node->behaviour->manager;
	b->deleteNode(node);
}

/* system callback */

void UI_Node_WindowOpened(struct uiNode_s *node, linkedList_t *params)
{
	uiNode *b = node->behaviour->manager;
	b->windowOpened(node, params);
}

void UI_Node_WindowClosed(struct uiNode_s *node)
{
	uiNode *b = node->behaviour->manager;
	b->windowClosed(node);
}

void UI_Node_DoLayout(struct uiNode_s *node)
{
	if (UI_Node_IsDrawable(node)) {
		uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
		b->doLayout(node);
	}
}

void UI_Node_Activate(struct uiNode_s *node)
{
	uiNode *b = node->behaviour->manager;
	b->activate(node);
}

void UI_Node_PropertyChanged(struct uiNode_s *node, const value_t *property)
{
	uiNode *b = node->behaviour->manager;
	b->propertyChanged(node, property);
}

void UI_Node_SizeChanged(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->sizeChanged(node);
}

void UI_Node_GetClientPosition(const struct uiNode_s *node, vec2_t position)
{
	uiAbstractScrollableNode *b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager);
	b->getClientPosition(node, position);
}

/* drag and drop callback */

qboolean UI_Node_DndEnter(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->dndEnter(node);
}

qboolean UI_Node_DndMove(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->dndMove(node, x, y);
}

void UI_Node_DndLeave(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->dndLeave(node);
}

qboolean UI_Node_DndDrop(struct uiNode_s *node, int x, int y)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->dndDrop(node, x, y);
}

qboolean UI_Node_DndFinished(struct uiNode_s *node, qboolean isDroped)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->dndFinished(node, isDroped);
}

/* focus and keyboard events */

void UI_Node_FocusGained(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->focusGained(node);
}

void UI_Node_FocusLost(struct uiNode_s *node)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	b->focusLost(node);
}

qboolean UI_Node_KeyPressed(struct uiNode_s *node, unsigned int key, unsigned short unicode)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->keyPressed(node, key, unicode);
}

qboolean UI_Node_KeyReleased(struct uiNode_s *node, unsigned int key, unsigned short unicode)
{
	uiLocatedNode *b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager);
	return b->keyPressed(node, key, unicode);
}

/* cell size */

int UI_Node_GetCellWidth(struct uiNode_s *node)
{
	uiAbstractScrollableNode *b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager);
	return b->getCellWidth(node);
}

int UI_Node_GetCellHeight(struct uiNode_s *node)
{
	uiAbstractScrollableNode *b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager);
	return b->getCellHeight(node);
}

#ifdef DEBUG

void UI_Node_DebugCountWidget(struct uiNode_s *node, int count)
{
	node->behaviour->count += count;
}

#endif

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

		if (node && UI_Node_IsScrollableContainer(node)) {
			vec2_t clientPosition = {0, 0};
			UI_Node_GetClientPosition(node, clientPosition);
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

		if (UI_Node_IsScrollableContainer(node)) {
			vec2_t clientPosition = {0, 0};
			UI_Node_GetClientPosition(node, clientPosition);
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
	UI_Node_SizeChanged(node);
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
	UI_Node_PropertyChanged(node, property);
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
		UI_Node_PropertyChanged(node, property);
		return qtrue;

	case V_UI_CVAR:	/* cvar */
		switch ((int)property->type) {
		case V_UI_CVAR:
			if (Q_strstart(value, "*cvar:")) {
				UI_FreeStringProperty(*(void**)b);
				*(char**) b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
				UI_Node_PropertyChanged(node, property);
				return qtrue;
			}
			break;
		case V_CVAR_OR_FLOAT:
			{
				float f;

				if (Q_strstart(value, "*cvar:")) {
					UI_FreeStringProperty(*(void**)b);
					*(char**) b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
					UI_Node_PropertyChanged(node, property);
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
				UI_Node_PropertyChanged(node, property);
				return qtrue;
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
			{
				UI_FreeStringProperty(*(void**)b);
				*(char**) b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
				UI_Node_PropertyChanged(node, property);
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
		UI_Node_DoLayout(node);
}
