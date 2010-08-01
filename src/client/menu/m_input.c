/**
 * @file m_input.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "m_main.h"
#include "m_internal.h"
#include "m_actions.h"
#include "m_input.h"
#include "m_internal.h"
#include "m_nodes.h"
#include "m_parse.h"
#include "m_draw.h"
#include "m_dragndrop.h"
#include "m_sound.h"

#include "../input/cl_keys.h"
#include "../input/cl_input.h"

/**
 * @brief save the node with the focus
 */
static uiNode_t* focusNode;

/**
 * @brief save the current hovered node (first node under the mouse)
 * @sa MN_GetHoveredNode
 * @sa MN_MouseMove
 * @sa MN_CheckMouseMove
 */
static uiNode_t* hoveredNode;

/**
 * @brief save the previous hovered node
 */
static uiNode_t *oldHoveredNode;

/**
 * @brief save old position of the mouse
 */
static int oldMousePosX, oldMousePosY;

/**
 * @brief save the captured node
 * @sa MN_SetMouseCapture
 * @sa MN_GetMouseCapture
 * @sa MN_MouseRelease
 * @todo think about replacing it by a boolean. When capturedNode != NULL => hoveredNode == capturedNode
 * it create unneed case
 */
static uiNode_t* capturedNode;

/**
 * @brief Execute the current focused action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusNextActionNode
 */
static qboolean MN_FocusExecuteActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	if (mouseSpace != MS_MENU)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (focusNode) {
		if (focusNode->onClick) {
			MN_ExecuteEventActions(focusNode, focusNode->onClick);
		}
		MN_ExecuteEventActions(focusNode, focusNode->onMouseLeave);
		focusNode = NULL;
		return qtrue;
	}
#endif
	return qfalse;
}

#if 0	/**< @todo need a cleanup */
/**
 * @sa MN_FocusExecuteActionNode
 * @note node must not be in menu
 */
static uiNode_t *MN_GetNextActionNode (uiNode_t* node)
{
	if (node)
		node = node->next;
	while (node) {
		if (MN_CheckVisibility(node) && !node->invis
		 && ((node->onClick && node->onMouseEnter) || node->onMouseEnter))
			return node;
		node = node->next;
	}
	return NULL;
}
#endif

/**
 * @brief Set the focus to the next action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusExecuteActionNode
 */
static qboolean MN_FocusNextActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	uiNode_t* menu;
	static int i = MAX_MENUSTACK + 1;	/* to cycle between all menus */

	if (mouseSpace != MS_MENU)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (i >= mn.menuStackPos)
		i = MN_GetLastFullScreenWindow();

	assert(i >= 0);

	if (focusNode) {
		uiNode_t *node = MN_GetNextActionNode(focusNode);
		if (node)
			return MN_FocusSetNode(node);
	}

	while (i < mn.menuStackPos) {
		menu = mn.menuStack[i++];
		if (MN_FocusSetNode(MN_GetNextActionNode(menu->firstChild)))
			return qtrue;
	}
	i = MN_GetLastFullScreenWindow();

	/* no node to focus */
	MN_RemoveFocus();
#endif
	return qfalse;
}

/**
 * @brief request the focus for a node
 */
void MN_RequestFocus (uiNode_t* node)
{
	uiNode_t* tmp;
	assert(node);
	if (node == focusNode)
		return;

	/* invalidate the data before calling the event */
	tmp = focusNode;
	focusNode = NULL;

	/* lost the focus */
	if (tmp && tmp->behaviour->focusLost) {
		tmp->behaviour->focusLost(tmp);
	}

	/* get the focus */
	focusNode = node;
	if (focusNode->behaviour->focusGained) {
		focusNode->behaviour->focusGained(focusNode);
	}
}

/**
 * @brief check if a node got the focus
 */
qboolean MN_HasFocus (const uiNode_t* node)
{
	return node == focusNode;
}

/**
 * @sa MN_FocusExecuteActionNode
 * @sa MN_FocusNextActionNode
 * @sa IN_Frame
 * @sa Key_Event
 */
void MN_RemoveFocus (void)
{
	uiNode_t* tmp;

	if (MN_GetMouseCapture())
		return;

	if (!focusNode)
		return;

	/* invalidate the data before calling the event */
	tmp = focusNode;
	focusNode = NULL;

	/* callback the lost of the focus */
	if (tmp->behaviour->focusLost) {
		tmp->behaviour->focusLost(tmp);
	}
}

static uiKeyBinding_t* MN_AllocStaticKeyBinding (void)
{
	uiKeyBinding_t* result;
	if (mn.numKeyBindings >= MAX_MENUKEYBINDING)
		Com_Error(ERR_FATAL, "MN_AllocStaticKeyBinding: MAX_MENUKEYBINDING hit");

	result = &mn.keyBindings[mn.numKeyBindings];
	mn.numKeyBindings++;

	memset(result, 0, sizeof(*result));
	return result;
}

int MN_GetKeyBindingCount (void)
{
	return mn.numKeyBindings;
}

uiKeyBinding_t* MN_GetKeyBindingByIndex (int index)
{
	return &mn.keyBindings[index];
}

/**
 * @todo check: only one binding per nodes
 * @todo check: key per window must be unique
 * @todo check: key used into MN_KeyPressed can't be used
 */
void MN_SetKeyBinding (const char* path, int key)
{
	uiNode_t *node;
	uiKeyBinding_t *binding;
	const value_t *property = NULL;

	MN_ReadNodePath(path, NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("MN_SetKeyBinding: node \"%s\" not found.\n", path);
		return;
	}

	if (property != NULL && property->type != V_UI_NODEMETHOD)
		Com_Error(ERR_FATAL, "MN_SetKeyBinding: Only node and method are supported. Property @%s not found in path \"%s\".", property->string, path);

	/* init and link the keybinding */
	binding = MN_AllocStaticKeyBinding();
	binding->node = node;
	binding->property = property;
	binding->key = key;
	node->key = binding;
	MN_WindowNodeRegisterKeyBinding(node->root, binding);
}

/**
 * @brief Check if a key binding exists for a window and execute it
 */
static qboolean MN_KeyPressedInWindow (unsigned int key, const uiNode_t *window)
{
	uiNode_t *node;
	const uiKeyBinding_t *binding;

	/* search requested key binding */
	binding = MN_WindowNodeGetKeyBinding(window, key);
	if (!binding)
		return qfalse;

	/* check node visibility */
	node = binding->node;
	while (node) {
		if (node->disabled || node->invis)
			return qfalse;
		node = node->parent;
	}

	/* execute event */
	node = binding->node;
	if (binding->property == NULL)
		node->behaviour->activate(node);
	else if (binding->property->type == V_UI_NODEMETHOD) {
		uiCallContext_t newContext;
		uiNodeMethod_t func = (uiNodeMethod_t) binding->property->ofs;
		newContext.source = node;
		newContext.useCmdParam = qfalse;
		func(node, &newContext);
	} else
		Com_Printf("MN_KeyPressedInWindow: @%s not supported.", binding->property->string);

	return qtrue;
}

/**
 * @brief Called by the client when the user type a key
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 * @return qtrue, if we used the event
 * @todo think about what we should do when the mouse is captured
 */
qboolean MN_KeyPressed (unsigned int key, unsigned short unicode)
{
	int windowId;
	int lastWindowId;

	if (MN_DNDIsDragging()) {
		if (key == K_ESCAPE)
			MN_DNDAbort();
		return qtrue;
	}

	/* translate event into the node with focus */
	if (focusNode && focusNode->behaviour->keyPressed) {
		if (focusNode->behaviour->keyPressed(focusNode, key, unicode))
			return qtrue;
	}

	/* else use common behaviour */
	switch (key) {
	case K_TAB:
		if (MN_FocusNextActionNode())
			return qtrue;
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if (MN_FocusExecuteActionNode())
			return qtrue;
		break;
	case K_ESCAPE:
		if (MN_GetMouseCapture() != NULL) {
			MN_MouseRelease();
			return qtrue;
		}
		MN_PopWindowWithEscKey();
		return qtrue;
	}

	lastWindowId = MN_GetLastFullScreenWindow();
	if (lastWindowId < 0)
		return qfalse;

	/* check "active" window from top to down */
	for (windowId = mn.windowStackPos - 1; windowId >= lastWindowId; windowId--) {
		const uiNode_t *window = mn.windowStack[windowId];
		if (!window)
			return qfalse;
		if (MN_KeyPressedInWindow(key, window))
			return qtrue;
		if (MN_WindowIsModal(window))
			break;
	}

	return qfalse;
}

/**
 * @brief Release all captured input (keyboard or mouse)
 */
void MN_ReleaseInput (void)
{
	MN_RemoveFocus();
	MN_MouseRelease();
	if (MN_DNDIsDragging())
		MN_DNDAbort();
}

/**
 * @brief Return the captured node
 * @return Return a node, else NULL
 */
uiNode_t* MN_GetMouseCapture (void)
{
	return capturedNode;
}

/**
 * @brief Captured the mouse into a node
 */
void MN_SetMouseCapture (uiNode_t* node)
{
	assert(capturedNode == NULL);
	assert(node != NULL);
	capturedNode = node;
}

/**
 * @brief Release the captured node
 */
void MN_MouseRelease (void)
{
	uiNode_t *tmp = capturedNode;

	if (capturedNode == NULL)
		return;

	capturedNode = NULL;
	if (tmp->behaviour->capturedMouseLost)
		tmp->behaviour->capturedMouseLost(tmp);

	MN_InvalidateMouse();
}

/**
 * @brief Get the current hovered node
 * @return A node, else NULL if the mouse hover nothing
 */
uiNode_t *MN_GetHoveredNode (void)
{
	return hoveredNode;
}

/**
 * @brief Force to invalidate the mouse position and then to update hover nodes...
 */
void MN_InvalidateMouse (void)
{
	oldMousePosX = -1;
	oldMousePosY = -1;
}

/**
 * @brief Call mouse move only if the mouse position change
 */
qboolean MN_CheckMouseMove (void)
{
	/* is hovered node no more draw */
	if (hoveredNode && (hoveredNode->invis || !MN_CheckVisibility(hoveredNode)))
		MN_InvalidateMouse();

	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		MN_MouseMove(mousePosX, mousePosY);
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Is called every time the mouse position change
 */
void MN_MouseMove (int x, int y)
{
	if (MN_DNDIsDragging())
		return;

	/* send the captured move mouse event */
	if (capturedNode) {
		if (capturedNode->behaviour->capturedMouseMove)
			capturedNode->behaviour->capturedMouseMove(capturedNode, x, y);
		return;
	}

	hoveredNode = MN_GetNodeAtPosition(x, y);

	/* update nodes: send 'in' and 'out' event */
	if (oldHoveredNode != hoveredNode) {
		uiNode_t *commonNode = hoveredNode;
		uiNode_t *node;

		/* search the common node */
		while (commonNode) {
			node = oldHoveredNode;
			while (node) {
				if (node == commonNode)
					break;
				node = node->parent;
			}
			if (node != NULL)
				break;
			commonNode = commonNode->parent;
		}

		/* send 'leave' event from old node to common node */
		node = oldHoveredNode;
		while (node != commonNode) {
			MN_ExecuteEventActions(node, node->onMouseLeave);
			node = node->parent;
		}
		if (oldHoveredNode)
			oldHoveredNode->state = qfalse;

		/* send 'enter' event from common node to new node */
		while (commonNode != hoveredNode) {
			/** @todo we can remove that loop with an array allocation */
			node = hoveredNode;
			while (node->parent != commonNode)
				node = node->parent;
			commonNode = node;
			MN_ExecuteEventActions(node, node->onMouseEnter);
		}
		if (hoveredNode) {
			hoveredNode->state = qtrue;
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onMouseEnter);
		}
	}
	oldHoveredNode = hoveredNode;

	/* send the move event */
	if (hoveredNode && hoveredNode->behaviour->mouseMove) {
		hoveredNode->behaviour->mouseMove(hoveredNode, x, y);
	}
}

#define MN_IsMouseInvalidate (oldMousePosX == -1)

/**
 * @brief Is called every time one clicks on a menu/screen. Then checks if anything needs to be executed in the area of the click
 * (e.g. button-commands, inventory-handling, geoscape-stuff, etc...)
 * @sa MN_ExecuteEventActions
 * @sa MN_RightClick
 * @sa Key_Message
 */
static void MN_LeftClick (int x, int y)
{
	qboolean disabled;
	if (MN_IsMouseInvalidate)
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->leftClick)
			capturedNode->behaviour->leftClick(capturedNode, x, y);
		return;
	}

	/* if we click out side a dropdown menu, we close it */
	/** @todo need to refactoring it with the focus code (cleaner) */
	/** @todo at least should be moved on the mouse down event (when the focus should change) */
	if (!hoveredNode && mn.windowStackPos != 0) {
		uiNode_t *menu = mn.windowStack[mn.windowStackPos - 1];
		if (MN_WindowIsDropDown(menu)) {
			MN_PopWindow(qfalse);
		}
	}

	disabled = (hoveredNode == NULL) || (hoveredNode->disabled) || (hoveredNode->parent && hoveredNode->parent->disabled);
	if (!disabled) {
		if (hoveredNode->behaviour->leftClick) {
			hoveredNode->behaviour->leftClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onClick);
		}
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_ExecuteEventActions
 * @sa MN_LeftClick
 * @sa MN_MiddleClick
 * @sa MN_MouseWheel
 */
static void MN_RightClick (int x, int y)
{
	qboolean disabled;
	if (MN_IsMouseInvalidate)
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->rightClick)
			capturedNode->behaviour->rightClick(capturedNode, x, y);
		return;
	}

	disabled = (hoveredNode == NULL) || (hoveredNode->disabled) || (hoveredNode->parent && hoveredNode->parent->disabled);
	if (!disabled) {
		if (hoveredNode->behaviour->rightClick) {
			hoveredNode->behaviour->rightClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onRightClick);
		}
	}
}

/**
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
static void MN_MiddleClick (int x, int y)
{
	qboolean disabled;
	if (MN_IsMouseInvalidate)
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->middleClick)
			capturedNode->behaviour->middleClick(capturedNode, x, y);
		return;
	}

	disabled = (hoveredNode == NULL) || (hoveredNode->disabled) || (hoveredNode->parent && hoveredNode->parent->disabled);
	if (!disabled) {
		if (hoveredNode->behaviour->middleClick) {
			hoveredNode->behaviour->middleClick(hoveredNode, x, y);
		} else {
			MN_ExecuteEventActions(hoveredNode, hoveredNode->onMiddleClick);
		}
		return;
	}
}

/**
 * @brief Called when we are in menu mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in @see CL_ParseInput)
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseWheel (qboolean down, int x, int y)
{
	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->mouseWheel)
			capturedNode->behaviour->mouseWheel(capturedNode, down, x, y);
		return;
	}

	if (hoveredNode) {
		if (hoveredNode->behaviour->mouseWheel) {
			hoveredNode->behaviour->mouseWheel(hoveredNode, down, x, y);
		} else {
			if (hoveredNode->onWheelUp && !down)
				MN_ExecuteEventActions(hoveredNode, hoveredNode->onWheelUp);
			if (hoveredNode->onWheelDown && down)
				MN_ExecuteEventActions(hoveredNode, hoveredNode->onWheelDown);
			else
				MN_ExecuteEventActions(hoveredNode, hoveredNode->onWheel);
		}
	}
}

/**
 * @brief Called when we are in menu mode and down a mouse button
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseDown (int x, int y, int button)
{
	uiNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node != NULL) {
		MN_MoveWindowOnTop(node->root);
		if (node->behaviour->mouseDown)
			node->behaviour->mouseDown(node, x, y, button);
	}

	/* click event */
	/** @todo should be send this event when the mouse up (after a down on the same node) */
	switch (button) {
	case K_MOUSE1:
		MN_LeftClick(x, y);
		break;
	case K_MOUSE2:
		MN_RightClick(x, y);
		break;
	case K_MOUSE3:
		MN_MiddleClick(x, y);
		break;
	}
	MN_PlaySound("click1");
}

/**
 * @brief Called when we are in menu mode and up a mouse button
 * @sa MN_LeftClick
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseUp (int x, int y, int button)
{
	uiNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node == NULL)
		return;

	if (node->behaviour->mouseUp)
		node->behaviour->mouseUp(node, x, y, button);
}
