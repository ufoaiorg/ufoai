/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "ui_node.h"
#include "ui_parse.h"
#include "ui_draw.h"
#include "ui_dragndrop.h"
#include "ui_timer.h"

#include "../input/cl_keys.h"
#include "../input/cl_input.h"
#include "../cl_shared.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_camera.h"
#include "../battlescape/cl_actor.h"
#include "../battlescape/cl_battlescape.h"

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
static uiNode_t* oldHoveredNode;

/**
 * @brief save old position of the mouse
 */
static int oldMousePosX, oldMousePosY;

/**
 * @brief save the captured node
 * @sa UI_SetMouseCapture
 * @sa UI_GetMouseCapture
 * @sa UI_MouseRelease
 * @todo think about replacing it by a boolean. When capturedNode != nullptr => hoveredNode == capturedNode
 * it create unneed case
 */
static uiNode_t* capturedNode;

/**
 * @brief Store node which receive a mouse down event
 */
static uiNode_t* pressedNode;

/**
 * @brief X position of the mouse when pressedNode != nullptr
 */
static int pressedNodeLocationX;

/**
 * @brief Y position of the mouse when pressedNode != nullptr
 */
static int pressedNodeLocationY;

/**
 * @brief Button pressed when pressedNode != nullptr
 */
static int pressedNodeButton;

/**
 * @brief Value of pressedNodeLocationX when event already sent
 */
#define UI_STARTDRAG_ALREADY_SENT -1

/**
 * @brief Value of the delay to wait before sending a long press event
 * This value is used for Qt 4.7. Maybe we can use a cvar
 */
#define LONGPRESS_DELAY 800

/**
 * @brief Timer used to manage long press event
 */
static uiTimer_t* longPressTimer;


/**
 * @brief Execute the current focused action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa UI_FocusNextActionNode
 */
static bool UI_FocusExecuteActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	if (IN_GetMouseSpace() != MS_UI)
		return false;

	if (UI_GetMouseCapture())
		return false;

	if (focusNode) {
		if (focusNode->onClick) {
			UI_ExecuteEventActions(focusNode, focusNode->onClick);
		}
		UI_ExecuteEventActions(focusNode, focusNode->onMouseLeave);
		focusNode = nullptr;
		return true;
	}
#endif
	return false;
}

#if 0	/**< @todo need a cleanup */
/**
 * @sa UI_FocusExecuteActionNode
 * @note node must be in a window
 */
static uiNode_t* UI_GetNextActionNode (uiNode_t* node)
{
	if (node)
		node = node->next;
	while (node) {
		if (UI_CheckVisibility(node) && !node->invis
		 && ((node->onClick && node->onMouseEnter) || node->onMouseEnter))
			return node;
		node = node->next;
	}
	return nullptr;
}
#endif

/**
 * @brief Set the focus to the next action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa UI_FocusExecuteActionNode
 */
static bool UI_FocusNextActionNode (void)
{
#if 0	/**< @todo need a cleanup */
	static int i = UI_MAX_WINDOWSTACK + 1;	/* to cycle between all windows */

	if (IN_GetMouseSpace() != MS_UI)
		return false;

	if (UI_GetMouseCapture())
		return false;

	if (i >= ui_global.windowStackPos)
		i = UI_GetLastFullScreenWindow();

	assert(i >= 0);

	if (focusNode) {
		uiNode_t* node = UI_GetNextActionNode(focusNode);
		if (node)
			return UI_FocusSetNode(node);
	}

	while (i < ui_global.windowStackPos) {
		uiNode_t* window;
		window = ui_global.windowStack[i++];
		if (UI_FocusSetNode(UI_GetNextActionNode(window->firstChild)))
			return true;
	}
	i = UI_GetLastFullScreenWindow();

	/* no node to focus */
	UI_RemoveFocus();
#endif
	return false;
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
	focusNode = nullptr;

	/* lost the focus */
	if (tmp) {
		UI_Node_FocusLost(tmp);
	}

	/* get the focus */
	focusNode = node;
	UI_Node_FocusGained(focusNode);
}

/**
 * @brief check if a node got the focus
 */
bool UI_HasFocus (const uiNode_t* node)
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
	focusNode = nullptr;

	/* callback the lost of the focus */
	UI_Node_FocusLost(tmp);
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
 * This command creates a relation between a key and a node.
 *
 * The relation is stored with the node (to display the shortcut in the tooltip)
 * and with the parent window of the node (for faster search of all available shortcuts).
 *
 * The storage on the node is not a list, so if there is more than one shortcut
 * to a node we can't display all shortcuts in the tooltip, but the binding will still
 * work.
 *
 * If the parent window is inherited, the binding is duped to other extended
 * windows and the relation is flagged as "inherited".
 *
 * @param path Path to a node, or a node method
 * @param key The key number to use (see for example the K_* names are matched up.)
 * @param description Textual description of what the key/cmd does (for tooltip)
 * @param inherited True if this binding is inherited from another binding.
 *
 * @todo check: only one binding per nodes
 * @todo check: key per window must be unique
 * @todo check: key used into UI_KeyPressed can't be used
 */
static void UI_SetKeyBindingEx (const char* path, int key, const char* description, bool inherited)
{
	uiNode_t* node;
	const value_t* property = nullptr;

	UI_ReadNodePath(path, nullptr, nullptr, &node, &property);
	if (node == nullptr) {
		Com_Printf("UI_SetKeyBinding: node \"%s\" not found.\n", path);
		return;
	}

	if (property != nullptr && property->type != V_UI_NODEMETHOD)
		Com_Error(ERR_FATAL, "UI_SetKeyBinding: Only node and method are supported. Property @%s not found in path \"%s\".", property->string, path);

	/* init and link the keybinding */
	uiKeyBinding_t* binding = UI_AllocStaticKeyBinding();
	binding->node = node;
	binding->property = property;
	binding->key = key;
	binding->inherited = inherited;
	node->key = binding;

	if (Q_strnull(description))
		Com_Printf("Warning: Empty description for UI keybinding: %s (%s)\n", path, Key_KeynumToString(key));
	else
		binding->description = Mem_PoolStrDup(description, ui_dynPool, 0);

	UI_WindowNodeRegisterKeyBinding(node->root, binding);

	/* search and update windows that extend node->root */
	for (int windowId = 0; windowId < ui_global.numWindows; windowId++) {
		uiNode_t* window = ui_global.windows[windowId];

		/* skip windows which are not direct extends of the main window */
		if (window->super != node->root)
			continue;

		/* create a new patch from the new windows */
		char newPath[256];
		newPath[0] = '\0';
		Q_strcat(newPath, sizeof(newPath), "%s%s", window->name, path + strlen(node->root->name));
		UI_SetKeyBindingEx(newPath, key, description, true);
	}
}

/**
 * @brief Set a binding from a key to a node to active.
 *
 * @param path Path to a node, or a node method
 * @param key The key number to use (see for example the K_* names are matched up.)
 * @param description Textual description of what the key/cmd does (for tooltip)
 *
 * @todo check: only one binding per nodes?
 * @todo check: key per window must be unique
 * @todo check: key used into UI_KeyPressed can't be used
 */
void UI_SetKeyBinding (const char* path, int key, const char* description)
{
	UI_SetKeyBindingEx(path, key, description, false);
}

/**
 * @brief Check if a key binding exists for a window and execute it
 */
static bool UI_KeyPressedInWindow (unsigned int key, const uiNode_t* window)
{
	uiNode_t* node;
	const uiKeyBinding_t* binding;

	/* search requested key binding */
	binding = UI_WindowNodeGetKeyBinding(window, key);
	if (!binding)
		return false;

	/* check node visibility */
	node = binding->node;
	while (node) {
		if (node->disabled || node->invis)
			return false;
		node = node->parent;
	}

	/* execute event */
	node = binding->node;
	if (binding->property == nullptr)
		UI_Node_Activate(node);
	else if (binding->property->type == V_UI_NODEMETHOD) {
		uiCallContext_t newContext;
		uiNodeMethod_t func = (uiNodeMethod_t) binding->property->ofs;
		newContext.source = node;
		newContext.useCmdParam = false;
		func(node, &newContext);
	} else
		Com_Printf("UI_KeyPressedInWindow: @%s not supported.", binding->property->string);

	return true;
}

/**
 * @brief Called by the client when the user released a key
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 * @return true, if we used the event
 */
bool UI_KeyRelease (unsigned int key, unsigned short unicode)
{
	/* translate event into the node with focus */
	if (focusNode) {
		return UI_Node_KeyReleased(focusNode, key, unicode);
	}

	return false;
}

/**
 * @brief Called by the client when the user type a key
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 * @return true, if we used the event
 * @todo think about what we should do when the mouse is captured
 */
bool UI_KeyPressed (unsigned int key, unsigned short unicode)
{
	if (UI_DNDIsDragging()) {
		if (key == K_ESCAPE) {
			UI_DNDAbort();
			return true;
		}
		return false;
	}

	if (key == K_ESCAPE && CL_BattlescapeRunning()
	 && selActor && CL_ActorFireModeActivated(selActor->actorMode)) {
		/* Cancel firing with Escape, needed for Android, where right mouse click is bound to multitouch, which is non-obvious */
		CL_ActorSetMode(selActor, M_MOVE);
		return true;
	}

	/* translate event into the node with focus */
	if (focusNode && UI_Node_KeyPressed(focusNode, key, unicode)) {
		return true;
	}

	/* else use common behaviour */
	switch (key) {
	case K_TAB:
		if (UI_FocusNextActionNode())
			return true;
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if (UI_FocusExecuteActionNode())
			return true;
		break;
	case K_ESCAPE:
		if (UI_GetMouseCapture() != nullptr) {
			UI_MouseRelease();
			return true;
		}
		UI_PopWindowWithEscKey();
		return true;
	}

	int lastWindowId = UI_GetLastFullScreenWindow();
	if (lastWindowId < 0)
		return false;

	/* check "active" window from top to down */
	for (int windowId = ui_global.windowStackPos - 1; windowId >= lastWindowId; windowId--) {
		const uiNode_t* window = ui_global.windowStack[windowId];
		if (!window)
			return false;
		if (UI_KeyPressedInWindow(key, window))
			return true;
		if (UI_WindowIsModal(window))
			break;
	}

	return false;
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
 * @return Return a node, else nullptr
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
	assert(capturedNode == nullptr);
	assert(node != nullptr);
	capturedNode = node;
}

/**
 * @brief Release the captured node
 */
void UI_MouseRelease (void)
{
	uiNode_t* tmp = capturedNode;

	if (capturedNode == nullptr)
		return;

	capturedNode = nullptr;
	UI_Node_CapturedMouseLost(tmp);
	UI_InvalidateMouse();
}

void UI_ResetInput (void)
{
	hoveredNode = nullptr;
	oldHoveredNode = nullptr;
	focusNode = nullptr;
	capturedNode = nullptr;
	pressedNode = nullptr;
	longPressTimer = nullptr;
}

/**
 * @brief Get the current hovered node
 * @return A node, else nullptr if the mouse hover nothing
 */
uiNode_t* UI_GetHoveredNode (void)
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
bool UI_CheckMouseMove (void)
{
	/* is hovered node no more draw */
	if (hoveredNode && (hoveredNode->invis || !UI_CheckVisibility(hoveredNode)))
		UI_InvalidateMouse();

	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		UI_MouseMove(mousePosX, mousePosY);
		return true;
	}

	return false;
}

/**
 * @brief Is called every time the mouse position change
 */
void UI_MouseMove (int x, int y)
{
	if (UI_DNDIsDragging())
		return;

	if (pressedNode && !capturedNode) {
		if (pressedNodeLocationX != UI_STARTDRAG_ALREADY_SENT) {
			UI_TimerStop(longPressTimer);
			int dist = abs(pressedNodeLocationX - x) + abs(pressedNodeLocationY - y);
			if (dist > 4) {
				uiNode_t* node = pressedNode;
				while (node) {
					if (UI_Node_StartDragging(node, pressedNodeLocationX, pressedNodeLocationY, x, y, pressedNodeButton))
						break;
					node = node->parent;
				}
				pressedNodeLocationX = UI_STARTDRAG_ALREADY_SENT;
			}
		}
	}

	/* send the captured move mouse event */
	if (capturedNode) {
		UI_Node_CapturedMouseMove(capturedNode, x, y);
		return;
	}

	hoveredNode = UI_GetNodeAtPosition(x, y);

	/* update houvered node by sending messages */
	if (oldHoveredNode != hoveredNode) {
		uiNode_t* commonNode = hoveredNode;
		uiNode_t* node;

		/* search the common node */
		while (commonNode) {
			node = oldHoveredNode;
			while (node) {
				if (node == commonNode)
					break;
				node = node->parent;
			}
			if (node != nullptr)
				break;
			commonNode = commonNode->parent;
		}

		/* send 'leave' event from old node to common node */
		node = oldHoveredNode;
		while (node != commonNode) {
			UI_Node_MouseLeave(node);
			node = node->parent;
		}
		if (oldHoveredNode)
			oldHoveredNode->state = false;

		/* send 'enter' event from common node to new node */
		while (commonNode != hoveredNode) {
			/** @todo we can remove that loop with an array allocation */
			node = hoveredNode;
			while (node->parent != commonNode)
				node = node->parent;
			commonNode = node;
			UI_Node_MouseEnter(node);
		}
		if (hoveredNode) {
			hoveredNode->state = true;
			UI_Node_MouseEnter(node);
		}
	}

	oldHoveredNode = hoveredNode;

	/* send the move event */
	if (hoveredNode)
		UI_Node_MouseMove(hoveredNode, x, y);
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
	if (UI_IsMouseInvalidate())
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		UI_Node_LeftClick(capturedNode, x, y);
		return;
	}

	/* if we click outside a dropdown window, we close it */
	/** @todo need to refactoring it with the focus code (cleaner) */
	/** @todo at least should be moved on the mouse down event (when the focus should change) */
	/** @todo this code must be on mouse down */
	if (!pressedNode && ui_global.windowStackPos != 0) {
		uiNode_t* window = ui_global.windowStack[ui_global.windowStackPos - 1];
		if (UI_WindowIsDropDown(window)) {
			UI_PopWindow();
		}
	}

	const bool disabled = (pressedNode == nullptr) || (pressedNode->disabled) || (pressedNode->parent && pressedNode->parent->disabled);
	if (!disabled) {
		UI_Node_LeftClick(pressedNode, x, y);
	}
}

/**
 * @sa GEO_ResetAction
 * @sa UI_ExecuteEventActions
 * @sa UI_LeftClick
 * @sa UI_MiddleClick
 * @sa UI_MouseWheel
 */
static void UI_RightClick (int x, int y)
{
	if (UI_IsMouseInvalidate())
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		UI_Node_RightClick(capturedNode, x, y);
		return;
	}

	const bool disabled = (pressedNode == nullptr) || (pressedNode->disabled) || (pressedNode->parent && pressedNode->parent->disabled);
	if (!disabled) {
		UI_Node_RightClick(pressedNode, x, y);
	}
}

/**
 * @sa UI_LeftClick
 * @sa UI_RightClick
 * @sa UI_MouseWheel
 */
static void UI_MiddleClick (int x, int y)
{
	if (UI_IsMouseInvalidate())
		return;

	/* send it to the captured mouse node */
	if (capturedNode) {
		UI_Node_MiddleClick(capturedNode, x, y);
		return;
	}

	const bool disabled = (pressedNode == nullptr) || (pressedNode->disabled) || (pressedNode->parent && pressedNode->parent->disabled);
	if (!disabled) {
		UI_Node_MiddleClick(pressedNode, x, y);
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
	uiNode_t* node;

	/* send it to the captured mouse node */
	if (capturedNode) {
		UI_Node_Scroll(capturedNode, deltaX, deltaY);
		return;
	}

	node = hoveredNode;

	while (node) {
		if (UI_Node_Scroll(node, deltaX, deltaY)) {
			break;
		}
		node = node->parent;
	}
}

/**
 * Callback function waked up to send long click event
 */
static void UI_LongPressCallback (uiNode_t* , uiTimer_t* timer)
{
	UI_TimerStop(timer);

	/* make sure the event still make sense */
	if (pressedNode == nullptr)
		return;

	uiNode_t* node = pressedNode;
	while (node) {
		if (UI_Node_MouseLongPress(node, pressedNodeLocationX, pressedNodeLocationY, pressedNodeButton))
			break;
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
	/* disable old long click event */
	if (longPressTimer)
		UI_TimerStop(longPressTimer);

	uiNode_t* node;

	/* captured or hover node */
	node = capturedNode ? capturedNode : hoveredNode;

	if (node != nullptr) {
		UI_MoveWindowOnTop(node->root);
		UI_Node_MouseDown(node, x, y, button);
	}

	/* select clickableNode on button up, and detect multipress button */
	if (pressedNode == nullptr) {
		pressedNode = node;
		pressedNodeLocationX = x;
		pressedNodeLocationY = y;
		pressedNodeButton = button;
		/** @todo find a way to create the timer when UI loading and remove "if (longPressTimer)" */
		if (longPressTimer == nullptr) {
			longPressTimer = UI_AllocTimer(nullptr, LONGPRESS_DELAY, UI_LongPressCallback);
		}
		UI_TimerStart(longPressTimer);
	} else {
		pressedNode = nullptr;
	}
}

/**
 * @brief Called when we are in UI mode and up a mouse button
 * @sa UI_LeftClick
 * @sa UI_RightClick
 * @sa UI_MiddleClick
 */
void UI_MouseUp (int x, int y, int button)
{
	/* disable long click event */
	if (longPressTimer)
		UI_TimerStop(longPressTimer);

	/* send click event */
	/** @todo it is really need to clean up this subfunctions */
	if (pressedNode || capturedNode) {
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
	}

	/* captured or hovered node */
	uiNode_t* node = nullptr;
	if (capturedNode) {
		node = capturedNode;
	} else if (pressedNode == hoveredNode) {
		node = hoveredNode;
	}

	pressedNode = nullptr;
	if (node == nullptr)
		return;

	UI_Node_MouseUp(node, x, y, button);
}
