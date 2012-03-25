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

extern "C" {

#include "ui_main.h"
#include "ui_behaviour.h"
#include "ui_nodes.h"
#include "ui_node.h"

/**
 * @todo Use typeid when it is possible
 */
qboolean UI_Node_IsVirtual(const struct uiNode_s *node)
{
	return node->behaviour->isVirtual;
}

/**
 * @todo Use typeid when it is possible
 */
qboolean UI_Node_IsDrawable(const struct uiNode_s *node)
{
	return (node->behaviour->draw != NULL) ? qtrue : qfalse;
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
	return (node->behaviour->getClientPosition != NULL) ? qtrue : qfalse;
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
	node->behaviour->draw(node);
}

void UI_Node_DrawTooltip(struct uiNode_s *node, int x, int y)
{
	node->behaviour->drawTooltip(node, x, y);
}

void UI_Node_DrawOverWindow(struct uiNode_s *node)
{
	node->behaviour->drawOverWindow(node);
}

/* mouse events */
void UI_Node_LeftClick(struct uiNode_s *node, int x, int y)
{
	node->behaviour->leftClick(node, x, y);
}

void UI_Node_RightClick(struct uiNode_s *node, int x, int y)
{
	node->behaviour->rightClick(node, x, y);
}

void UI_Node_MiddleClick(struct uiNode_s *node, int x, int y)
{
	node->behaviour->middleClick(node, x, y);
}

qboolean UI_Node_Scroll(struct uiNode_s *node, int deltaX, int deltaY)
{
	return node->behaviour->scroll(node, deltaX, deltaY);
}

void UI_Node_MouseMove(struct uiNode_s *node, int x, int y)
{
	node->behaviour->mouseMove(node, x, y);
}

void UI_Node_MouseDown(struct uiNode_s *node, int x, int y, int button)
{
	node->behaviour->mouseDown(node, x, y, button);
}

void UI_Node_MouseUp(struct uiNode_s *node, int x, int y, int button)
{
	node->behaviour->mouseUp(node, x, y, button);
}

void UI_Node_CapturedMouseMove(struct uiNode_s *node, int x, int y)
{
	node->behaviour->capturedMouseMove(node, x, y);
}

void UI_Node_CapturedMouseLost(struct uiNode_s *node)
{
	node->behaviour->capturedMouseLost(node);
}

/* system allocation */

void UI_Node_Loading(struct uiNode_s *node)
{
	node->behaviour->loading(node);
}

void UI_Node_Loaded(struct uiNode_s *node)
{
	node->behaviour->loaded(node);
}

void UI_Node_Clone(const struct uiNode_s *source, struct uiNode_s *clone)
{
	source->behaviour->clone(source, clone);
}

void UI_Node_NewNode(struct uiNode_s *node)
{
	node->behaviour->newNode(node);
}

void UI_Node_DeleteNode(struct uiNode_s *node)
{
	node->behaviour->deleteNode(node);
}

/* system callback */

void UI_Node_WindowOpened(struct uiNode_s *node, linkedList_t *params)
{
	node->behaviour->windowOpened(node, params);
}

void UI_Node_WindowClosed(struct uiNode_s *node)
{
	node->behaviour->windowClosed(node);
}

void UI_Node_DoLayout(struct uiNode_s *node)
{
	node->behaviour->doLayout(node);
}

void UI_Node_Activate(struct uiNode_s *node)
{
	node->behaviour->activate(node);
}

void UI_Node_PropertyChanged(struct uiNode_s *node, const value_t *property)
{
	node->behaviour->propertyChanged(node, property);
}

void UI_Node_SizeChanged(struct uiNode_s *node)
{
	node->behaviour->sizeChanged(node);
}

void UI_Node_GetClientPosition(const struct uiNode_s *node, vec2_t position)
{
	node->behaviour->getClientPosition(node, position);
}

/* drag and drop callback */

qboolean UI_Node_DndEnter(struct uiNode_s *node)
{
	return node->behaviour->dndEnter(node);
}

qboolean UI_Node_DndMove(struct uiNode_s *node, int x, int y)
{
	return node->behaviour->dndMove(node, x, y);
}

void UI_Node_DndLeave(struct uiNode_s *node)
{
	node->behaviour->dndLeave(node);
}

qboolean UI_Node_DndDrop(struct uiNode_s *node, int x, int y)
{
	return node->behaviour->dndDrop(node, x, y);
}

qboolean UI_Node_DndFinished(struct uiNode_s *node, qboolean isDroped)
{
	return node->behaviour->dndFinished(node, isDroped);
}

/* focus and keyboard events */

void UI_Node_FocusGained(struct uiNode_s *node)
{
	node->behaviour->focusGained(node);
}

void UI_Node_FocusLost(struct uiNode_s *node)
{
	node->behaviour->focusLost(node);
}

qboolean UI_Node_KeyPressed(struct uiNode_s *node, unsigned int key, unsigned short unicode)
{
	return node->behaviour->keyPressed(node, key, unicode);
}

qboolean UI_Node_KeyReleased(struct uiNode_s *node, unsigned int key, unsigned short unicode)
{
	return node->behaviour->keyReleased(node, key, unicode);
}

/* cell size */

int UI_Node_GetCellWidth(struct uiNode_s *node)
{
	return node->behaviour->getCellWidth(node);
}

int UI_Node_GetCellHeight(struct uiNode_s *node)
{
	return node->behaviour->getCellHeight(node);
}

#ifdef DEBUG

void UI_Node_DebugCountWidget(struct uiNode_s *node, int count)
{
	node->behaviour->count += count;
}

#endif

}
