/**
 * @file ui_input.c
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_actions.h"
#include "ui_input.h"
#include "ui_internal.h"
#include "ui_nodes.h"
#include "ui_parse.h"
#include "ui_draw.h"
#include "ui_dragndrop.h"
#include "ui_sound.h"

#include "../input/cl_keys.h"
#include "../input/cl_input.h"

/**
 * @brief save the node with the focus
 */
static uiNode_t* focusNode;

/**
 * @brief save the current hovered node (first node under the mouse)
 * @sa UI_GetHoveredNode
 * @sa UI_MouseMove
 * @sa UI_CheckMouseMove
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
 * @sa UI_SetMouseCapture
 * @sa UI_GetMouseCapture
 * @sa UI_MouseRelease
 * @todo think about replacing it by a boolean. When capturedNode != NULL => hoveredNode == capturedNode
 * it create unneed case
 */
static uiNode_t* capturedNode;

/**
 * @brief Execute the current focused action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa UI_FocusNextActionNode
 */
static qboolean UI_FocusExecuteActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	if (IN_GetMouseSpace() != MS_UI)
		return qfalse;

	if (UI_GetMouseCapture())
		return qfalse;

	if (focusNode) {
		if (focusNode->onClick) {
			UI_ExecuteEventActions(focusNode, focusNode->onClick);
		}
		UI_ExecuteEventActions(focusNode, focusNode->onMouseLeave);
		focusNode = NULL;
		return qtrue;
	}
#endif
	return qfalse;
}

#if 0	/**< @todo need a cleanup */
/**
 * @sa UI_FocusExecuteActionNode
 * @note node must be in a window
 */
static uiNode_t *UI_GetNextActionNode (uiNode_t* node)
{
	if (node)
		node = node->next;
	while (node) {
		if (UI_CheckVisibility(node) && !node->invis
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
 * @sa UI_FocusExecuteActionNode
 */
static qboolean UI_FocusNextActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	static int i = UI_MAX_WINDOWSTACK + 1;	/* to cycle between all windows */

	if (IN_GetMouseSpace() != MS_UI)
		return qfalse;

	if (UI_GetMouseCapture())
		return qfalse;

	if (i >= ui_global.windowStackPos)
		i = UI_GetLastFullScreenWindow();

	assert(i >= 0);

	if (focusNode) {
		uiNode_t *node = UI_GetNextActionNode(focusNode);
		if (node)
			return UI_FocusSetNode(node);
	}

	while (i < ui_global.windowStackPos) {
		uiNode_t* window;
		window = ui_global.windowStack[i++];
		if (UI_FocusSetNode(UI_GetNextActionNode(window->firstChild)))
			return qtrue;
	}
	i = UI_GetLastFullScreenWindow();

	/* no node to focus */
	UI_RemoveFocus();
#endif
	return qfalse;
}

/**
 * @brief request the focus for a node
 */
void UI_RequestFocus (uiNode_t* node)
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
qboolean UI_HasFocus (const uiNode_t* node)
{
	return node == focusNode;
}

/**
 * @sa UI_FocusExecuteActionNode
 * @sa UI_FocusNextActionNode
 * @sa IN_Frame
 * @sa Key_Event
 */
void UI_RemoveFocus (void)
{
	uiNode_t* tmp;

	if (UI_GetMouseCapture())
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

static uiKeyBinding_t* UI_AllocStaticKeyBinding (void)
{
	uiKeyBinding_t* result;
	if (ui_global.numKeyBindings >= UI_MAX_KEYBINDING)
		Com_Error(ERR_FATAL, "UI_AllocStaticKeyBinding: UI_MAX_KEYBINDING hit");

	result = &ui_global.keyBindings[ui_global.numKeyBindings];
	ui_global.numKeyBindings++;

	OBJZERO(*result);
	return result;
}

int UI_GetKeyBindingCount (void)
{
	return ui_global.numKeyBindings;
}

uiKeyBinding_t* UI_GetKeyBindingByIndex (int index)
{
	return &ui_global.keyBindings[index];
}

/**
 * @brief Set a binding from a key to a node to active.
 *
 * This command create a relation between a key and a node.
 *
 * The relation is stored to the node (to display the shortcut into tooltip)
 * and to parent window of the node (to faster search of all available shortcuts).
 *
 * The storage on the node is not a list, then if there is more than one shortcut
 * to a node we can't display all shortcut to tooltip, but the binding will still
 * work.
 *
 * If the parent window is inherited, the binding is dup to others extend
 * windows and the relation is flagged as "inherited".
 *
 * @param path Path to a node, or a node mathod
 * @param key The key number to use (see for example the K_* names are matched up.)
 * @param inherited True if this binding is inherited from another binding.
 *
 * @todo check: only one binding per nodes
 * @todo check: key per window must be unique
 * @todo check: key used into UI_KeyPressed can't be used
 */
static void UI_SetKeyBindingEx (const char* path, int key, const char* description, qboolean inherited)
{
	uiNode_t *node;
	uiKeyBinding_t *binding;
	const value_t *property = NULL;
	int windowId;
	char newPath[256];

	UI_ReadNodePath(path, NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("UI_SetKeyBinding: node \"%s\" not found.\n", path);
		return;
	}

	if (property != NULL && property->type != V_UI_NODEMETHOD)
		Com_Error(ERR_FATAL, "UI_SetKeyBinding: Only node and method are supported. Property @%s not found in path \"%s\".", property->string, path);

	/* init and link the keybinding */
	binding = UI_AllocStaticKeyBinding();
	binding->node = node;
	binding->property = property;
	binding->key = key;
	binding->inherited = inherited;
	node->key = binding;

	if (Q_strnull(description))
		Com_Printf("Warning: Empty description for UI keybinding: %s (%s)\n", path, Key_KeynumToString(key));
	else
		binding->description = Mem_PoolStrDup(description, ui_dynPool, 0);;

	UI_WindowNodeRegisterKeyBinding(node->root, binding);

	/* search and update windows extend node->root */
	for (windowId = 0; windowId < ui_global.numWindows; windowId++) {
		uiNode_t *window = ui_global.windows[windowId];

		/* skip window which are not direct extends of the main window */
		if (window->super != node->root)
			continue;

		/* create a new patch from the new windows */
		newPath[0] = '\0';
		Q_strcat(newPath, window->name, sizeof(newPath));
		Q_strcat(newPath, path + strlen(node->root->name), sizeof(newPath));
		UI_SetKeyBindingEx(newPath, key, description, qtrue);
	}
}

/**
 * @brief Set a binding from a key to a node to active.
 *
 * @param path Path to a node, or a node mathod
 * @param key The key number to use (see for example the K_* names are matched up.)
 *
 * @todo check: only one binding per nodes?
 * @todo check: key per window must be unique
 * @todo check: key used into UI_KeyPressed can't be used
 */
void UI_SetKeyBinding (const char* path, int key, const char* description)
{
	UI_SetKeyBindingEx(path, key, description, qfalse);
}

/**
 * @brief Check if a key binding exists for a window and execute it
 */
static qboolean UI_KeyPressedInWindow (unsigned int key, const uiNode_t *window)
{
	uiNode_t *node;
	const uiKeyBinding_t *binding;

	/* search requested key binding */
	binding = UI_WindowNodeGetKeyBinding(window, key);
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
		Com_Printf("UI_KeyPressedInWindow: @%s not supported.", binding->property->string);

	return qtrue;
}

/**
 * @brief Called by the client when the user released a key
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 * @return qtrue, if we used the event
 */
qboolean UI_KeyRelease (unsigned int key, unsigned short unicode)
{
	/* translate event into the node with focus */
	if (focusNode && focusNode->behaviour->keyReleased) {
		if (focusNode->behaviour->keyReleased(focusNode, key, unicode))
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Called by the client when the user type a key
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 * @return qtrue, if we used the event
 * @todo think about what we should do when the mouse is captured
 */
qboolean UI_KeyPressed (unsigned int key, unsigned short unicode)
{
	int windowId;
	int lastWindowId;

	if (UI_DNDIsDragging()) {
		if (key == K_ESCAPE)
			UI_DNDAbort();
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
		if (UI_FocusNextActionNode())
			return qtrue;
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if (UI_FocusExecuteActionNode())
			return qtrue;
		break;
	case K_ESCAPE:
		if (UI_GetMouseCapture() != NULL) {
			UI_MouseRelease();
			return qtrue;
		}
		UI_PopWindowWithEscKey();
		return qtrue;
	}

	lastWindowId = UI_GetLastFullScreenWindow();
	if (lastWindowId < 0)
		return qfalse;

	/* check "active" window from top to down */
	for (windowId = ui_global.windowStackPos - 1; windowId >= lastWindowId; windowId--) {
		const uiNode_t *window = ui_global.windowStack[windowId];
		if (!window)
			return qfalse;
		if (UI_KeyPressedInWindow(key, window))
			return qtrue;
		if (UI_WindowIsModal(window))
			break;
	}

	return qfalse;
}

/**
 * @brief Release all captured input (keyboard or mouse)
 */
void UI_ReleaseInput (void)
{
	UI_RemoveFocus();
	UI_MouseRelease();
	if (UI_DNDIsDragging())
		UI_DNDAbort();
}

/**
 * @brief Return the captured node
 * @return Return a node, else NULL
 */
uiNode_t* UI_GetMouseCapture (void)
{
	return capturedNode;
}

/**
 * @brief Captured the mouse into a node
 */
void UI_SetMouseCapture (uiNode_t* node)
{
	assert(capturedNode == NULL);
	assert(node != NULL);
	capturedNode = node;
}

/**
 * @brief Release the captured node
 */
void UI_MouseRelease (void)
{
	uiNode_t *tmp = capturedNode;

	if (capturedNode == NULL)
		return;

	capturedNode = NULL;
	if (tmp->behaviour->capturedMouseLost)
		tmp->behaviour->capturedMouseLost(tmp);

	UI_InvalidateMouse();
}

/**
 * @brief Get the current hovered node
 * @return A node, else NULL if the mouse hover nothing
 */
uiNode_t *UI_GetHoveredNode (void)
{
	return hoveredNode;
}

/**
 * @brief Force to invalidate the mouse position and then to update hover nodes...
 */
void UI_InvalidateMouse (void)
{
	oldMousePosX = -1;
	oldMousePosY = -1;
}

/**
 * @brief Call mouse move only if the mouse position change
 */
qboolean UI_CheckMouseMove (void)
{
	/* is hovered node no more draw */
	if (hoveredNode && (hoveredNode->invis || !UI_CheckVisibility(hoveredNode)))
		UI_InvalidateMouse();

	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		UI_MouseMove(mousePosX, mousePosY);
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Is called every time the mouse position change
 */
void UI_MouseMove (int x, int y)
{
	if (UI_DNDIsDragging())
		return;

	/* send the captured move mouse event */
	if (capturedNode) {
		if (capturedNode->behaviour->capturedMouseMove)
			capturedNode->behaviour->capturedMouseMove(capturedNode, x, y);
		return;
	}

	hoveredNode = UI_GetNodeAtPosition(x, y);

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
			UI_ExecuteEventActions(node, node->onMouseLeave);
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
			UI_ExecuteEventActions(node, node->onMouseEnter);
		}
		if (hoveredNode) {
			hoveredNode->state = qtrue;
			UI_ExecuteEventActions(hoveredNode, hoveredNode->onMouseEnter);
		}
	}
	oldHoveredNode = hoveredNode;

	/* send the move event */
	if (hoveredNode && hoveredNode->behaviour->mouseMove) {
		hoveredNode->behaviour->mouseMove(hoveredNode, x, y);
	}
}

#define UI_IsMouseInvalidate() (oldMousePosX == -1)

/**
 * @brief Is called every time one clicks on a window/screen. Then checks if anything needs to be executed in the area of the click
 * (e.g. button-commands, inventory-handling, geoscape-stuff, etc...)
 * @sa UI_ExecuteEventActions
 * @sa UI_RightClick
 * @sa Key_Message
 */
static void UI_LeftClick (int x, int y)
{
	qboolean disabled;
	if (UI_IsMouseInvalidate())
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		if (capturedNode->behaviour->leftClick)
			capturedNode->behaviour->leftClick(capturedNode, x, y);
		return;
	}

	/* if we click outside a dropdown window, we close it */
	/** @todo need to refactoring it with the focus code (cleaner) */
	/** @todo at least should be moved on the mouse down event (when the focus should change) */
	if (!hoveredNode && ui_global.windowStackPos != 0) {
		uiNode_t *window = ui_global.windowStack[ui_global.windowStackPos - 1];
		if (UI_WindowIsDropDown(window)) {
			UI_PopWindow(qfalse);
		}
	}

	disabled = (hoveredNode == NULL) || (hoveredNode->disabled) || (hoveredNode->parent && hoveredNode->parent->disabled);
	if (!disabled) {
		if (hoveredNode->behaviour->leftClick) {
			hoveredNode->behaviour->leftClick(hoveredNode, x, y);
		} else {
			UI_ExecuteEventActions(hoveredNode, hoveredNode->onClick);
		}
	}
}

/**
 * @sa MAP_ResetAction
 * @sa UI_ExecuteEventActions
 * @sa UI_LeftClick
 * @sa UI_MiddleClick
 * @sa UI_MouseWheel
 */
static void UI_RightClick (int x, int y)
{
	qboolean disabled;
	if (UI_IsMouseInvalidate())
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
			UI_ExecuteEventActions(hoveredNode, hoveredNode->onRightClick);
		}
	}
}

/**
 * @sa UI_LeftClick
 * @sa UI_RightClick
 * @sa UI_MouseWheel
 */
static void UI_MiddleClick (int x, int y)
{
	qboolean disabled;
	if (UI_IsMouseInvalidate())
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
			UI_ExecuteEventActions(hoveredNode, hoveredNode->onMiddleClick);
		}
		return;
	}
}

/**
 * @brief Called when we are in UI mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in @see CL_ParseInput)
 * @sa UI_LeftClick
 * @sa UI_RightClick
 * @sa UI_MiddleClick
 */
void UI_MouseScroll (int deltaX, int deltaY)
{
	uiNode_t *node;

	/* send it to the captured mouse node */
	if (capturedNode) {
		capturedNode->behaviour->scroll(capturedNode, deltaX, deltaY);
		return;
	}

	node = hoveredNode;

	while (node) {
		if (node->behaviour->scroll(node, deltaX, deltaY)) {
			break;
		}
		node = node->parent;
	}
}

/**
 * @brief Called when we are in UI mode and down a mouse button
 * @sa UI_LeftClick
 * @sa UI_RightClick
 * @sa UI_MiddleClick
 */
void UI_MouseDown (int x, int y, int button)
{
	uiNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node != NULL) {
		UI_MoveWindowOnTop(node->root);
		if (node->behaviour->mouseDown)
			node->behaviour->mouseDown(node, x, y, button);
	}

	/* click event */
	/** @todo should be send this event when the mouse up (after a down on the same node) */
	switch (button) {
	case K_MOUSE1:
		UI_LeftClick(x, y);
		break;
	case K_MOUSE2:
		UI_RightClick(x, y);
		break;
	case K_MOUSE3:
		UI_MiddleClick(x, y);
		break;
	}
	UI_PlaySound("click1");
}

/**
 * @brief Called when we are in UI mode and up a mouse button
 * @sa UI_LeftClick
 * @sa UI_RightClick
 * @sa UI_MiddleClick
 */
void UI_MouseUp (int x, int y, int button)
{
	uiNode_t *node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node == NULL)
		return;

	if (node->behaviour->mouseUp)
		node->behaviour->mouseUp(node, x, y, button);
}
