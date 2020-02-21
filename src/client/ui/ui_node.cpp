/**
 * @file
 * @brief C interface to allow to access to cpp node code.
 */

/*
Copyright (C) 2002-2020 UFO: Alien Invasion.

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

#include "../../common/hashtable.h"


bool UI_Node_IsVirtual (uiNode_t const* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b == nullptr;
}

bool UI_Node_IsDrawable (uiNode_t const* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b != nullptr;
}

bool UI_Node_IsOptionContainer (uiNode_t const* node)
{
	uiAbstractOptionNode* b = dynamic_cast<uiAbstractOptionNode*>(node->behaviour->manager.get());
	return b != nullptr;
}

bool UI_Node_IsWindow (uiNode_t const* node)
{
	uiWindowNode* b = dynamic_cast<uiWindowNode*>(node->behaviour->manager.get());
	return b != nullptr;
}

bool UI_Node_IsBattleScape (uiNode_t const* node)
{
	uiBattleScapeNode* b = dynamic_cast<uiBattleScapeNode*>(node->behaviour->manager.get());
	return b != nullptr;
}

bool UI_Node_IsAbstract (uiNode_t const* node)
{
	return node->behaviour->isAbstract;
}

bool UI_Node_IsDrawItselfChild (uiNode_t const* node)
{
	return node->behaviour->drawItselfChild;
}

/**
 * @todo Use typeid when it is possible
 */
bool UI_Node_IsFunction (uiNode_t const* node)
{
	return node->behaviour->isFunction;
}

/**
 * @todo Use typeid when it is possible
 */
bool UI_Node_IsScrollableContainer (uiNode_t const* node)
{
	uiAbstractScrollableNode* b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager.get());
	return b != nullptr;
}

const char* UI_Node_GetWidgetName (uiNode_t const* node)
{
	return node->behaviour->name;
}

intptr_t UI_Node_GetMemorySize (uiNode_t const* node)
{
	return sizeof(*node) + node->behaviour->extraDataSize;
}

void UI_Node_Draw (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->draw(node);
}

void UI_Node_DrawTooltip (const uiNode_t* node, int x, int y)
{
	const uiLocatedNode* b = dynamic_cast<const uiLocatedNode*>(node->behaviour->manager.get());
	b->drawTooltip(node, x, y);
}

void UI_Node_DrawOverWindow (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->drawOverWindow(node);
}

/* mouse events */
void UI_Node_LeftClick (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onLeftClick(node, x, y);
}

void UI_Node_RightClick (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onRightClick(node, x, y);
}

void UI_Node_MiddleClick (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onMiddleClick(node, x, y);
}

bool UI_Node_Scroll (uiNode_t* node, int deltaX, int deltaY)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onScroll(node, deltaX, deltaY);
}

void UI_Node_MouseMove (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onMouseMove(node, x, y);
}

void UI_Node_MouseDown (uiNode_t* node, int x, int y, int button)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onMouseDown(node, x, y, button);
}

void UI_Node_MouseUp (uiNode_t* node, int x, int y, int button)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onMouseUp(node, x, y, button);
}

void UI_Node_MouseEnter (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onMouseEnter(node);
}

void UI_Node_MouseLeave (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onMouseLeave(node);
}

bool UI_Node_MouseLongPress (uiNode_t* node, int x, int y, int button)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onMouseLongPress(node, x, y, button);
}

bool UI_Node_StartDragging (uiNode_t* node, int startX, int startY, int currentX, int currentY, int button)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onStartDragging(node, startX, startY, currentX, currentY, button);
}

void UI_Node_CapturedMouseMove (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onCapturedMouseMove(node, x, y);
}

void UI_Node_CapturedMouseLost (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onCapturedMouseLost(node);
}

/* system allocation */

void UI_Node_Loading (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->onLoading(node);
}

void UI_Node_Loaded (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->onLoaded(node);
}

void UI_Node_Clone (uiNode_t const* source, uiNode_t* clone)
{
	uiNode* b = source->behaviour->manager.get();
	b->clone(source, clone);
}

void UI_Node_InitNodeDynamic (uiNode_t* node) {
	uiNode* b = node->behaviour->manager.get();
	b->initNodeDynamic(node);
}

void UI_Node_InitNode (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->initNode(node);
}

void UI_Node_DeleteNode (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->deleteNode(node);
}

/* system callback */

void UI_Node_WindowOpened (uiNode_t* node, linkedList_t* params)
{
	uiNode* b = node->behaviour->manager.get();
	b->onWindowOpened(node, params);
}

void UI_Node_WindowClosed (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->onWindowClosed(node);
}

void UI_Node_WindowActivate (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->onWindowActivate(node);
}

void UI_Node_DoLayout (uiNode_t* node)
{
	if (UI_Node_IsDrawable(node)) {
		uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
		b->doLayout(node);
	}
}

void UI_Node_Activate (uiNode_t* node)
{
	uiNode* b = node->behaviour->manager.get();
	b->onActivate(node);
}

void UI_Node_PropertyChanged (uiNode_t* node, const value_t* property)
{
	uiNode* b = node->behaviour->manager.get();
	b->onPropertyChanged(node, property);
}

void UI_Node_PosChanged (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onSizeChanged(node);
}

void UI_Node_SizeChanged (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onSizeChanged(node);
}

void UI_Node_GetClientPosition (uiNode_t const* node, vec2_t position)
{
	uiAbstractScrollableNode* b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager.get());
	b->getClientPosition(node, position);
}

/* drag and drop callback */

bool UI_Node_DndEnter (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onDndEnter(node);
}

bool UI_Node_DndMove (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onDndMove(node, x, y);
}

void UI_Node_DndLeave (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onDndLeave(node);
}

bool UI_Node_DndDrop (uiNode_t* node, int x, int y)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onDndDrop(node, x, y);
}

bool UI_Node_DndFinished (uiNode_t* node, bool isDroped)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onDndFinished(node, isDroped);
}

/* focus and keyboard events */

void UI_Node_FocusGained (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onFocusGained(node);
}

void UI_Node_FocusLost (uiNode_t* node)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	b->onFocusLost(node);
}

bool UI_Node_KeyPressed (uiNode_t* node, unsigned int key, unsigned short unicode)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onKeyPressed(node, key, unicode);
}

bool UI_Node_KeyReleased (uiNode_t* node, unsigned int key, unsigned short unicode)
{
	uiLocatedNode* b = dynamic_cast<uiLocatedNode*>(node->behaviour->manager.get());
	return b->onKeyReleased(node, key, unicode);
}

/* cell size */

int UI_Node_GetCellWidth (uiNode_t* node)
{
	uiAbstractScrollableNode* b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager.get());
	return b->getCellWidth(node);
}

int UI_Node_GetCellHeight (uiNode_t* node)
{
	uiAbstractScrollableNode* b = dynamic_cast<uiAbstractScrollableNode*>(node->behaviour->manager.get());
	return b->getCellHeight(node);
}

const char* UI_Node_GetText (uiNode_t* node) {
	#ifdef DEBUG
	if (node->text == nullptr) {
		Com_Printf("warning: requesting uninitialized node->text from [%s]\n", UI_GetPath(node));
	}
	#endif // DEBUG
	return (node->text? node->text : "");
}

void UI_Node_SetText (uiNode_t* node, const char* text) {
	UI_FreeStringProperty(node->text);
	node->text = Mem_PoolStrDup(text, ui_dynStringPool, 0);
}

void UI_Node_SetFont (uiNode_t* node, const char* name) {
	UI_FreeStringProperty(node->font);
	node->font = Mem_PoolStrDup(name, ui_dynStringPool, 0);
}

void UI_Node_SetImage (uiNode_t* node, const char* name) {
	UI_FreeStringProperty(node->image);
	node->image = Mem_PoolStrDup(name, ui_dynStringPool, 0);
}

const char* UI_Node_GetTooltip (uiNode_t* node) {
	return node->tooltip;
}

void UI_Node_SetTooltip (uiNode_t* node, const char* tooltip) {
	UI_FreeStringProperty((void*)node->tooltip);
	node->tooltip = Mem_PoolStrDup(tooltip, ui_dynStringPool, 0);
}

bool UI_Node_IsDisabled (uiNode_t const* node) {
	return node->disabled;
}

bool UI_Node_IsInvisible (uiNode_t const* node) {
	return node->invis;
}

bool UI_Node_IsGhost (uiNode_t const* node) {
	return node->ghost;
}

bool UI_Node_IsFlashing (uiNode_t const* node) {
	return node->flash;
}

void UI_Node_SetDisabled (uiNode_t* node, const bool value) {
	node->disabled = value;
}

#ifdef DEBUG

void UI_Node_DebugCountWidget (uiNode_t* node, int count)
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
bool UI_NodeInstanceOf (const uiNode_t* node, const char* behaviourName)
{
	for (const uiBehaviour_t* behaviour = node->behaviour; behaviour; behaviour = behaviour->super) {
		if (Q_streq(behaviour->name, behaviourName))
			return true;
	}
	return false;
}

/**
 * @brief Check the node inheritance
 * @param[in] node Requested node
 * @param[in] behaviour Behaviour we check
 * @return True if the node inherits from the behaviour
 */
bool UI_NodeInstanceOfPointer (const uiNode_t* node, const uiBehaviour_t* behaviour)
{
	for (const uiBehaviour_t* b = node->behaviour; b; b = b->super) {
		if (b == behaviour)
			return true;
	}
	return false;
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
		pos[0] = (int)(node->box.size[0] / 2);
		break;
	case LAYOUTALIGN_H_RIGHT:
		pos[0] = node->box.size[0];
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
		pos[1] = (int)(node->box.size[1] / 2);
		break;
	case LAYOUTALIGN_V_BOTTOM:
		pos[1] = node->box.size[1];
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
		if (node->box.pos[0] != (int)node->box.pos[0] || node->box.pos[1] != (int)node->box.pos[1])
			Com_Error(ERR_FATAL, "UI_GetNodeAbsPos: Node '%s' position %f,%f is not integer", UI_GetPath(node), node->box.pos[0], node->box.pos[1]);
#endif
		pos[0] += node->box.pos[0];
		pos[1] += node->box.pos[1];
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
		if (node->box.pos[0] != (int)node->box.pos[0] || node->box.pos[1] != (int)node->box.pos[1])
			Com_Error(ERR_FATAL, "UI_GetNodeAbsPos: Node '%s' position %f,%f is not integer", UI_GetPath(node), node->box.pos[0], node->box.pos[1]);
#endif
		pos[0] += node->box.pos[0];
		pos[1] += node->box.pos[1];
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
		pos[0] += node->box.pos[0];
		pos[1] += node->box.pos[1];
		node = node->parent;
	}
}

/**
 * @brief Update an absolute position to a relative one
 * @param[in] node Context node
 * @param[in,out] x an absolute x position
 * @param[in,out] y an absolute y position
 */
void UI_NodeAbsoluteToRelativePos (const uiNode_t* node, int* x, int* y)
{
	assert(node != nullptr);
	/* if we request the position of an undrawable node, there is a problem */
	assert(node->behaviour->isVirtual == false);
	assert(x != nullptr);
	assert(y != nullptr);

	/* if we request the position of an undrawable node, there is a problem */
	if (node->behaviour->isVirtual)
		Com_Error(ERR_DROP, "UI_NodeAbsoluteToRelativePos: Node '%s' doesn't have a position", node->name);

	while (node) {
		*x -= node->box.pos[0];
		*y -= node->box.pos[1];

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
		node->invis = true;
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
		node->invis = false;
	else
		Com_Printf("UI_UnHideNode: No node given\n");
}

/**
 * @brief Update the node size and fire the pos callback
 */
void UI_NodeSetPos(uiNode_t* node, vec2_t pos) {
	if (Vector2Equal(node->box.pos, pos))
		return;
	node->box.pos[0] = pos[0];
	node->box.pos[1] = pos[1];
	UI_Node_PosChanged(node);
}

/**
 * @brief Update the node size and fire the pos callback
 */
void UI_NodeSetPos(uiNode_t* node, float x, float y) {
	vec2_t p;
	p[0] = x; p[1] = y;
	UI_NodeSetPos(node, p);
}

/**
 * @brief Update the node size and fire the size callback
 */
void UI_NodeSetSize (uiNode_t* node, vec2_t size)
{
	if (Vector2Equal(node->box.size, size))
		return;
	node->box.size[0] = size[0];
	node->box.size[1] = size[1];
	UI_Node_SizeChanged(node);
}
/**
 * @brief Update the node size and fire the size callback
 */
void UI_NodeSetSize (uiNode_t* node, float w, float h) {
	vec2_t s;
	s[0] = w; s[1] = h;
	UI_NodeSetSize(node, s);
}

/**
 * @brief Update the node size and height and fire callbacks.
 * @note Use value -1 for x, y, w, h to specify no change in value.
 */
void UI_NodeSetBox (uiNode_t* node, float x, float y, float w, float h) {
	vec2_t p;
	vec2_t s;
	if (x != -1) p[0] = x; else p[0] = node->box.pos[0];
	if (y != -1) p[1] = y; else p[1] = node->box.pos[1];
	if (w != -1) s[0] = w; else s[0] = node->box.size[0];
	if (h != -1) s[1] = h; else s[1] = node->box.size[1];
	UI_NodeSetPos(node, p);
	UI_NodeSetSize(node, s);
}

/**
 * @brief Search a child node by given name
 * @note Only search with one depth
 */
uiNode_t* UI_GetNode(const uiNode_t* node, const char* name) {
	uiNode_t* current = nullptr;

	if (!node)
		return nullptr;

	for (current = node->firstChild; current; current = current->next)
		if (Q_streq(name, current->name))
			break;

	return current;
}

/**
 * @brief Returns the previous node based on the order of nodes in the parent.
 * @return A uiNode_t* or nullptr if no previous node is found.
 * @note A nullptr is returned if the node is either the last node in the chain or in case the node is the
 * only node.
 */
uiNode_t* UI_GetPrevNode(const uiNode_t* node) {
	uiNode_t* current = nullptr;
	for (current = node->parent->firstChild; current; current = current->next) {
		if (node == current->next) break;
	}
	return current;
}

/**
 * @brief Recursive searches for a child node by name in the entire subtree.
 * @return A uiNode_t* or nullptr if not found.
 */
uiNode_t* UI_FindNode(const uiNode_t* node, const char* name) {
	/* search current level */
	uiNode_t* result = UI_GetNode(node, name);
	if (!result) {
		/* iterate child nodes and search next level */
		for(uiNode_t* current = node->firstChild; current; current = current->next) {
			result = UI_FindNode(current, name);
			if (result) break;
		}
	}
	return result;
}

/**
 * @brief Insert a node next another one into a node. If prevNode is nullptr add the node on the head of the window
 * @param[in] parent Node where the newNode is inserted in
 * @param[in] prevNode previous node, will became before the newNode; else nullptr if newNode will become the first child of the node
 * @param[in] newNode node we insert
 */
void UI_InsertNode (uiNode_t* const parent, uiNode_t* prevNode, uiNode_t* newNode)
{
	/* parent and newNode should be valid, or else insertion doesn't make sense */
	assert(parent);
	assert(newNode);
	/* insert only a single element */
	assert(!newNode->next);

	uiNode_t** const anchor = prevNode ? &prevNode->next : &parent->firstChild;
	newNode->next   = *anchor;
	*anchor         = newNode;
	newNode->parent = parent;

	UI_UpdateRoot (newNode, parent->root);

	if (!parent->firstChild) {
		parent->firstChild = newNode;
	}
	if (!parent->lastChild) {
		parent->lastChild = newNode;
	}

	if (!newNode->next) {
		parent->lastChild = newNode;
	}

	if (newNode->root && newNode->indexed)
		UI_WindowNodeAddIndexedNode(newNode->root, newNode);

	UI_Invalidate(parent);
}

/**
 * @brief Remove a node from a parent node
 * @return The removed node, else nullptr
 * @param[in] node Node where is the child
 * @param[in] child Node we want to remove
 */
uiNode_t* UI_RemoveNode (uiNode_t* const node, uiNode_t* child)
{
	assert(node);
	assert(child);
	assert(child->parent == node);
	assert(node->firstChild);

	/** remove the 'child' node */
	for (uiNode_t** anchor = &node->firstChild;; anchor = &(*anchor)->next) {
		if (!*anchor)
			return 0;

		if (*anchor == child) {
			*anchor = child->next;
			break;
		}
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

	child->next = nullptr;
	return child;
}

/**
 * @brief Moves a node in the tree
 * @param[in] parent Node where the moved node is inserted in
 * @param[in] prevNode Previous node to move this one after
 * @param[in] node Node to move
 */
void UI_MoveNode (uiNode_t* const parent, uiNode_t* prevNode, uiNode_t* node)
{
	/* parent and newNode should be valid, or else insertion doesn't make sense */
	assert(parent);
	assert(node);

	if (!UI_RemoveNode(parent, node))
		return;
	UI_InsertNode(parent, prevNode, node);

	UI_Invalidate(parent);
}

void UI_UpdateRoot (uiNode_t* node, uiNode_t* newRoot)
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
 * @param parent The node where newNode is inserted as a child.
 * @param newNode The node to insert.
 */
void UI_AppendNode (uiNode_t* const parent, uiNode_t* newNode)
{
	UI_InsertNode(parent, parent->lastChild, newNode);
}

void UI_NodeSetPropertyFromRAW (uiNode_t* node, const value_t* property, const void* rawValue, int rawType)
{
	if (property->type != rawType) {
		Com_Printf("UI_NodeSetPropertyFromRAW: type %i expected, but @%s type %i found. Property setter to '%s@%s' skipped\n", rawType, property->string, property->type, UI_GetPath(node), property->string);
		return;
	}

	if ((property->type & V_UI_MASK) == V_NOT_UI)
		Com_SetValue(node, rawValue, property->type, property->ofs, property->size);
	else if ((property->type & V_UI_MASK) == V_UI_CVAR) {
		UI_FreeStringProperty(Com_GetValue<void*>(node, property));
		switch (property->type & V_BASETYPEMASK) {
		case V_FLOAT: *Com_GetValue<float*     >(node, property) = *static_cast<float const*>(rawValue); break;
		case V_INT:   *Com_GetValue<int*       >(node, property) = *static_cast<int   const*>(rawValue); break;
		default:       Com_GetValue<byte const*>(node, property) =  static_cast<byte  const*>(rawValue); break;
		}
	} else if (property->type == V_UI_ACTION) {
		Com_GetValue<uiAction_t const*>(node, property) = static_cast<uiAction_t const*>(rawValue);
	} else if (property->type == V_UI_SPRITEREF) {
		Com_GetValue<uiSprite_t const*>(node, property) = static_cast<uiSprite_t const*>(rawValue);
	} else {
		Com_Error(ERR_FATAL, "UI_NodeSetPropertyFromRAW: Property type '%d' unsupported", property->type);
	}
	UI_Node_PropertyChanged(node, property);
}

/**
 * @brief Set node property
 */
bool UI_NodeSetProperty (uiNode_t* node, const value_t* property, const char* value)
{
	const int specialType = property->type & V_UI_MASK;
	int result;
	size_t bytes;

	switch (specialType) {
	case V_NOT_UI:	/* common type */
		result = Com_ParseValue(node, value, property->type, property->ofs, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("UI_NodeSetProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return false;
		}
		UI_Node_PropertyChanged(node, property);
		return true;

	case V_UI_CVAR:	/* cvar */
		switch ((int)property->type) {
		case V_UI_CVAR:
			if (Q_strstart(value, "*cvar:")) {
				char*& b = Com_GetValue<char*>(node, property);
				UI_FreeStringProperty(b);
				b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
				UI_Node_PropertyChanged(node, property);
				return true;
			}
			break;
		case V_CVAR_OR_FLOAT:
			{
				float f;

				if (Q_strstart(value, "*cvar:")) {
					char*& b = Com_GetValue<char*>(node, property);
					UI_FreeStringProperty(b);
					b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
					UI_Node_PropertyChanged(node, property);
					return true;
				}

				result = Com_ParseValue(&f, value, V_FLOAT, 0, sizeof(f), &bytes);
				if (result != RESULT_OK) {
					Com_Printf("UI_NodeSetProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
					return false;
				}

				void* const b = Com_GetValue<void*>(node, property);
				if (char const* const cvar = Q_strstart((char const*)b, "*cvar:"))
					Cvar_SetValue(cvar, f);
				else
					*(float*) b = f;
				UI_Node_PropertyChanged(node, property);
				return true;
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
			{
				char*& b = Com_GetValue<char*>(node, property);
				UI_FreeStringProperty(b);
				b = Mem_PoolStrDup(value, ui_dynStringPool, 0);
				UI_Node_PropertyChanged(node, property);
				return true;
			}
		}
		break;

	case V_UI:
		switch ((int)property->type) {
		case V_UI_SPRITEREF:
			{
				uiSprite_t* sprite = UI_GetSpriteByName(value);
				Com_GetValue<uiSprite_t const*>(node, property) = sprite;
				UI_Node_PropertyChanged(node, property);
				return true;
			}
		}
		break;
	}

	Com_Printf("UI_NodeSetProperty: Unimplemented type for property '%s@%s'\n", UI_GetPath(node), property->string);
	return false;
}

/**
 * @brief Return a string from a node property
 * @param[in] node Requested node
 * @param[in] property Requested property
 * @return Return a string value of a property, else nullptr, if the type is not supported
 */
const char* UI_GetStringFromNodeProperty (const uiNode_t* node, const value_t* property)
{
	const int baseType = property->type & V_UI_MASK;
	assert(node);
	assert(property);

	switch (baseType) {
	case V_NOT_UI: /* common type */
		return Com_ValueToStr(node, property->type, property->ofs);
	case V_UI_CVAR:
		switch ((int)property->type) {
		case V_CVAR_OR_FLOAT:
			{
				const float f = UI_GetReferenceFloat(node, Com_GetValue<void*>(node, property));
				const int i = f;
				if (f == i)
					return va("%i", i);
				else
					return va("%f", f);
			}
		case V_CVAR_OR_LONGSTRING:
		case V_CVAR_OR_STRING:
		case V_UI_CVAR:
			return UI_GetReferenceString(node, Com_GetValue<char*>(node, property));
		}
		break;
	default:
		break;
	}

	Com_Printf("UI_GetStringFromNodeProperty: Unsupported string getter for property type 0x%X (%s@%s)\n", property->type, UI_GetPath(node), property->string);
	return nullptr;
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
	assert(node);

	if (property->type == V_FLOAT) {
		return Com_GetValue<float>(node, property);
	} else if ((property->type & V_UI_MASK) == V_UI_CVAR) {
		void* const b = Com_GetValue<void*>(node, property);
		if (char const* const cvarName = Q_strstart((char const*)b, "*cvar:")) {
			const cvar_t* cvar = Cvar_Get(cvarName, "", 0, "UI script cvar property");
			return cvar->value;
		} else if (property->type == V_CVAR_OR_FLOAT) {
			return *(const float*) b;
		} else if (property->type == V_CVAR_OR_STRING) {
			return atof((const char*)b);
		}
	} else if (property->type == V_INT) {
		return Com_GetValue<int>(node, property);
	} else if (property->type == V_BOOL) {
		return Com_GetValue<bool>(node, property);
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
void UI_Invalidate (uiNode_t* node)
{
	while (node) {
		if (node->invalidated)
			return;
		node->invalidated = true;
		node = node->parent;
	}
}

/**
 * @brief Validate a node tree
 */
void UI_Validate (uiNode_t* node)
{
	if (node->invalidated)
		UI_Node_DoLayout(node);
}

/**
 * @brief Adds a lua based method to the list of available node methods for calling.
 * @param[in] node The node to extend.
 * @param[in] name The name of the new method to add
 * @param[in] fcn The lua based function reference.
 * @note If the method name is already defined, the new method is not added and a warning will be issued
 * in the log.
 * @note The method will be instance based, meaning that if a new node is created of the same behaviour,
 * it will not contain this method unless this node was specified as super.
 */
void UI_AddNodeMethod (uiNode_t* node, const char* name, LUA_METHOD fcn) {
	//Com_Printf ("UI_AddNodeMethod: registering node method [%s] on node [%s]\n", name, UI_GetPath(node));

	/* the first method, so create a method table on this node */
	if (!node->nodeMethods) {
		node->nodeMethods = HASH_NewTable(true, true, false);
	}
	/* add the method */
    if (!HASH_Insert(node->nodeMethods, name, strlen(name), &fcn, sizeof(fcn))) {
		Com_Printf("UI_AddNodeMethod: method [%s] already defined on this behaviour [%s]\n", name, node->name);
    }
}

/**
 * @brief Finds the lua based method on this node or its super.
 * @param[in] node The node to examine.
 * @param[in] name The name of the method to find
 * @param[out] fcn A reference to a LUA_METHOD value to the corresponding lua based function or to LUA_NOREF if
 * the method is not found
 * @return True if the method is found, false otherwise.
 * @note This method will first search for instance methods, then it will check for behaviour methods.
 */
bool UI_GetNodeMethod (const uiNode_t* node, const char* name, LUA_METHOD &fcn) {
	fcn = LUA_NOREF;
	// search the instance methods
	for(const uiNode_t* ref=node;ref;ref = ref->super) {
		if (ref->nodeMethods) {
            void* val=HASH_Get(ref->nodeMethods, name, strlen(name));
			if (val != nullptr) {
				fcn = *((LUA_METHOD *)val);
				return true;
			}
		}
	}
	// no instance method found, now scan for behaviour method
	return UI_GetBehaviourMethod(node->behaviour, name, fcn);
}

/**
 * @brief Returns true if a node method of given name is available on this node or its super.
 * @param[in] node The node to examine.
 * @param[in] name The name of the method to find
 * @return True if the method is found, false otherwise.
 * @note This method will first search for instance methods, then it will check for behaviour methods.
 */
bool UI_HasNodeMethod (uiNode_t* node, const char* name) {
	LUA_METHOD fn;
	// search the instance methods
	if (UI_GetNodeMethod(node, name, fn)) {
		return true;
	}
	// nothing found, now check behaviour methods
	return UI_GetBehaviourMethod(node->behaviour, name, fn);
}



/**
 * @brief This functions adds a lua based method to the internal uiNode behaviour.
 * @param[in] node The node getting new behaviour.
 * @param[in] name The name of the new behaviour entry (this is the function name).
 * @param[in] fcn A reference to a lua based method.
 * @note This is a placeholder for extending behaviour in lua and store it in the ui internal structure.
 * Currently, only lua based functions are supported.
 */
void UI_Node_SetItem (uiNode_t* node, const char* name, LUA_METHOD fcn) {
	UI_AddNodeMethod(node, name, fcn);
}

/**
 * @brief This functions queries a lua based method in the internal uiNode behaviour.
 * @param[in] node The node with behaviour being queried.
 * @param[in] name The name of the behaviour entry to find.
 * @return A LUA_METHOD value pointing to a lua defined function or LUA_NOREF if no function is found.
 * @note This is a placeholder for extending behaviour in lua and store it in the ui internal structure.
 * Currently, only lua based functions are supported.
 */
LUA_METHOD UI_Node_GetItem (uiNode_t* node, const char* name) {
	LUA_METHOD fcn;
	if (UI_GetNodeMethod(node, name, fcn)) {
		return fcn;
	}
	return LUA_NOREF;
}
