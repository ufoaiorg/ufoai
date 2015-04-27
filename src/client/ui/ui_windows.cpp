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
#include "ui_input.h"
#include "ui_node.h"
#include "ui_popup.h"
#include "node/ui_node_abstractnode.h"
#include "node/ui_node_window.h"
#include "node/ui_node_battlescape.h"

#include "../cl_video.h"
#include "../input/cl_input.h"
#include "../input/cl_keys.h"

#define WINDOWEXTRADATA(node) UI_EXTRADATA(node, windowExtraData_t)
#define WINDOWEXTRADATACONST(node)  UI_EXTRADATACONST(node, windowExtraData_t)

/**
 * @brief Window name use as alternative for option
 */
static cvar_t* ui_sys_main;

/**
 * @brief Main window of a stack
 */
static cvar_t* ui_sys_active;

/**
 * @brief Returns the ID of the last fullscreen ID. Before this, window should be hidden.
 * @return The last full screen window on the screen, else 0. If the stack is empty, return -1
 */
int UI_GetLastFullScreenWindow (void)
{
	/* stack pos */
	int pos = ui_global.windowStackPos - 1;
	while (pos > 0) {
		if (UI_WindowIsFullScreen(ui_global.windowStack[pos]))
			break;
		pos--;
	}
	/* if we find nothing we return 0 */
	return pos;
}

/**
 * @brief Move the window on top of compatible windows.
 * "Compatible" mean non full screen windows, and windows
 * with the same window parent.
 * @param window Window we want to move
 */
void UI_MoveWindowOnTop (uiNode_t* window)
{
	int i, j;

	if (UI_WindowIsFullScreen(window))
		return;

	/* get window index */
	for (i = 0; i < ui_global.windowStackPos; i++) {
		if (ui_global.windowStack[i] == window)
			break;
	}

	/* search the last compatible window */
	for (j = i; j < ui_global.windowStackPos; j++) {
		if (UI_WindowIsFullScreen(ui_global.windowStack[j]))
			break;
		if (WINDOWEXTRADATA(window).parent != WINDOWEXTRADATA(ui_global.windowStack[j]).parent)
			break;
	}
	if (i + 1 == j)
		return;

	/* translate windows */
	for (; i < j - 1; i++) {
		ui_global.windowStack[i] = ui_global.windowStack[i+1];
	}
	/* add the current window on top */
	ui_global.windowStack[i] = window;
}

/**
 * @brief Remove the window from the window stack
 * @param[in] window The window to remove from the stack
 * @todo Why dont we call onClose?
 */
static void UI_DeleteWindowFromStack (uiNode_t* window)
{
	int i;

	/* get window index */
	for (i = 0; i < ui_global.windowStackPos; i++) {
		if (ui_global.windowStack[i] == window)
			break;
	}

	/* update stack */
	if (i < ui_global.windowStackPos) {
		ui_global.windowStackPos--;
		for (; i < ui_global.windowStackPos; i++)
			ui_global.windowStack[i] = ui_global.windowStack[i + 1];
		UI_InvalidateMouse();
	}
}

/**
 * @brief Searches the position in the current window stack for a given window id
 * @return -1 if the window is not on the stack
 */
static inline int UI_GetWindowPositionFromStackByName (const char* name)
{
	for (int i = 0; i < ui_global.windowStackPos; i++)
		if (Q_streq(ui_global.windowStack[i]->name, name))
			return i;

	return -1;
}

/**
 * @brief Insert a window at a position of the stack
 * @param[in] window The window to insert
 * @param[in] position Where we want to add the window (0 is the deeper element of the stack)
 */
static inline void UI_InsertWindowIntoStack (uiNode_t* window, int position)
{
	assert(position <= ui_global.windowStackPos);
	assert(position > 0);
	assert(window != nullptr);

	/* create space for the new window */
	for (int i = ui_global.windowStackPos; i > position; i--) {
		ui_global.windowStack[i] = ui_global.windowStack[i - 1];
	}
	/* insert */
	ui_global.windowStack[position] = window;
	ui_global.windowStackPos++;
}

/**
 * @brief Push a window onto the window stack
 * @param[in] name Name of the window to push onto window stack
 * @param[in] parentName Window name to link as parent-child (else nullptr)
 * @param[in] params List of string parameters to send to the onWindowOpened method.
 * It can be nullptr when there is no parameters, else this object must be freed by the caller.
 * @return A pointer to @c uiNode_t
 */
uiNode_t* UI_PushWindow (const char* name, const char* parentName, linkedList_t* params)
{
	UI_ReleaseInput();

	uiNode_t* window = UI_GetWindow(name);
	if (window == nullptr) {
		Com_Printf("Window \"%s\" not found.\n", name);
		return nullptr;
	}

	UI_DeleteWindowFromStack(window);

	if (ui_global.windowStackPos < UI_MAX_WINDOWSTACK)
		if (parentName) {
			const int parentPos = UI_GetWindowPositionFromStackByName(parentName);
			if (parentPos == -1) {
				Com_Printf("Didn't find parent window \"%s\" for window push of \"%s\"\n", parentName, name);
				return nullptr;
			}
			UI_InsertWindowIntoStack(window, parentPos + 1);
			WINDOWEXTRADATA(window).parent = ui_global.windowStack[parentPos];
		} else
			ui_global.windowStack[ui_global.windowStackPos++] = window;
	else
		Com_Printf("Window stack overflow\n");

	UI_Node_WindowOpened(window, params);

	/* change from e.g. console mode to game input mode (fetch input) */
	Key_SetDest(key_game);

	UI_InvalidateMouse();
	return window;
}

/**
 * @brief Complete function for ui_push
 * @sa Cmd_AddParamCompleteFunction
 * @sa UI_PushWindow
 * @note Does not really complete the input - but shows at least all parsed windows
 */
int UI_CompleteWithWindow (const char* partial, const char** match)
{
	int n = 0;
	for (uiNode_t** i = ui_global.windows, ** const end = i + ui_global.numWindows; i != end; ++i) {
		char const* const name = (*i)->name;
		if (Cmd_GenericCompleteFunction(name, partial, match)) {
			Com_Printf("%s\n", name);
			++n;
		}
	}
	return n;
}

/**
 * @brief Console function to push a child window onto the window stack
 * @sa UI_PushWindow
 */
static void UI_PushChildWindow_f (void)
{
	if (Cmd_Argc() > 1)
		UI_PushWindow(Cmd_Argv(1), Cmd_Argv(2));
	else
		Com_Printf("Usage: %s <name> <parentname>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to push a window onto the window stack
 * @sa UI_PushWindow
 */
static void UI_PushWindow_f (void)
{
	if (Cmd_Argc() == 0) {
		Com_Printf("Usage: %s <name> <params>\n", Cmd_Argv(0));
		return;
	}

	linkedList_t* params = nullptr;
	for (int i = 2; i < Cmd_Argc(); i++) {
		LIST_AddString(&params, Cmd_Argv(i));
	}
	UI_PushWindow(Cmd_Argv(1), nullptr, params);
	LIST_Delete(&params);
}

/**
 * @brief Console function to push a dropdown window at a position. It work like UI_PushWindow but move the window at the right position
 * @sa UI_PushWindow
 * @note The usage is "ui_push_dropdown sourcenode pointposition destinationnode pointposition"
 * sourcenode must be a node into the window we want to push,
 * we will move the window to have sourcenode over destinationnode
 * pointposition select for each node a position (like a corner).
 */
static void UI_PushDropDownWindow_f (void)
{
	if (Cmd_Argc() != 4 && Cmd_Argc() != 5) {
		Com_Printf("Usage: %s <source-anchor> <point-in-source-anchor> <dest-anchor> <point-in-dest-anchor>\n", Cmd_Argv(0));
		return;
	}

	/* get the source anchor */
	uiNode_t* node = UI_GetNodeByPath(Cmd_Argv(1));
	if (node == nullptr) {
		Com_Printf("UI_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	size_t writtenBytes;
	int direction;
	int result = Com_ParseValue(&direction, Cmd_Argv(2), V_INT, 0, sizeof(direction), &writtenBytes);
	if (result != RESULT_OK) {
		Com_Printf("UI_PushDropDownWindow_f: '%s' in not a V_INT\n", Cmd_Argv(2));
		return;
	}
	vec2_t source;
	vec2_t destination;
	UI_NodeGetPoint(node, source, direction);
	UI_NodeRelativeToAbsolutePoint(node, source);

	/* get the destination anchor */
	if (Q_streq(Cmd_Argv(4), "mouse")) {
		destination[0] = mousePosX;
		destination[1] = mousePosY;
	} else {
		/* get the source anchor */
		node = UI_GetNodeByPath(Cmd_Argv(3));
		if (node == nullptr) {
			Com_Printf("UI_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(3));
			return;
		}
		result = Com_ParseValue(&direction, Cmd_Argv(4), V_INT, 0, sizeof(direction), &writtenBytes);
		if (result != RESULT_OK) {
			Com_Printf("UI_PushDropDownWindow_f: '%s' in not a V_INT\n", Cmd_Argv(4));
			return;
		}
		UI_NodeGetPoint(node, destination, direction);
		UI_NodeRelativeToAbsolutePoint(node, destination);
	}

	/* update the window and push it */
	node = UI_GetNodeByPath(Cmd_Argv(1));
	if (node == nullptr) {
		Com_Printf("UI_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	node = node->root;
	node->box.pos[0] += destination[0] - source[0];
	node->box.pos[1] += destination[1] - source[1];
	UI_PushWindow(node->name);
}

static void UI_RemoveWindowAtPositionFromStack (int position)
{
	assert(position < ui_global.windowStackPos);
	assert(position >= 0);

	/* create space for the new window */
	for (int i = position; i < ui_global.windowStackPos; i++) {
		ui_global.windowStack[i] = ui_global.windowStack[i + 1];
	}
	ui_global.windowStack[ui_global.windowStackPos--] = nullptr;
}

static void UI_CloseAllWindow (void)
{
	for (int i = ui_global.windowStackPos - 1; i >= 0; i--) {
		uiNode_t* window = ui_global.windowStack[i];

		UI_Node_WindowClosed(window);

		/* safe: unlink window */
		WINDOWEXTRADATA(window).parent = nullptr;
		ui_global.windowStackPos--;
		ui_global.windowStack[ui_global.windowStackPos] = nullptr;
	}
}

/**
 * @brief Init the stack to start with a window, and have an alternative window with ESC
 * @note Illustration about when/how we should use this http://ufoai.org/wiki/index.php/Image:UI_InitStack.jpg
 * @param[in] activeWindow The first active window of the stack, else nullptr
 * @param[in] mainWindow The alternative window, else nullptr if nothing
 * @todo remove Cvar_Set we have direct access to the cvar
 * @todo We should only call it a very few time. When we switch from/to this different par of the game: main-option-interface / geoscape-and-base / battlescape
 */
void UI_InitStack (const char* activeWindow, const char* mainWindow)
{
	UI_FinishInit();
	UI_PopWindow(true);

	assert(activeWindow != nullptr);
	Cvar_Set("ui_sys_active", "%s", activeWindow);
	/* prevent calls before UI script initialization */
	if (ui_global.numWindows != 0) {
		UI_PushWindow(activeWindow);
	}

	if (mainWindow)
		Cvar_Set("ui_sys_main", "%s", mainWindow);
}

/**
 * @brief Check if a named window is on the stack if active windows
 */
bool UI_IsWindowOnStack (const char* name)
{
	return UI_GetWindowPositionFromStackByName(name) != -1;
}

/**
 * @todo Find  better name
 */
static void UI_CloseWindowByRef (uiNode_t* window)
{
	uiNode_t* oldfirst = ui_global.windowStack[0];

	/** @todo If the focus is not on the window we close, we don't need to remove it */
	UI_ReleaseInput();

	assert(ui_global.windowStackPos);
	int i = UI_GetWindowPositionFromStackByName(window->name);
	if (i == -1) {
		Com_Printf("Window '%s' is not on the active stack\n", window->name);
		return;
	}

	/* close child */
	while (i + 1 < ui_global.windowStackPos) {
		uiNode_t* m = ui_global.windowStack[i + 1];
		if (WINDOWEXTRADATA(m).parent != window) {
			break;
		}

		UI_Node_WindowClosed(window);
		WINDOWEXTRADATA(m).parent = nullptr;
		UI_RemoveWindowAtPositionFromStack(i + 1);
	}

	/* close the window */
	UI_Node_WindowClosed(window);
	WINDOWEXTRADATA(window).parent = nullptr;
	UI_RemoveWindowAtPositionFromStack(i);

	UI_InvalidateMouse();

	if (ui_global.windowStackPos == 0) {
		/* ui_sys_main contains the window that is the very first window and should be
		 * pushed back onto the stack (otherwise there would be no window at all
		 * right now) */
		if (Q_streq(oldfirst->name, ui_sys_main->string)) {
			if (ui_sys_active->string[0] != '\0')
				UI_PushWindow(ui_sys_active->string);
			if (!ui_global.windowStackPos)
				UI_PushWindow(ui_sys_main->string);
		} else {
			if (ui_sys_main->string[0] != '\0')
				UI_PushWindow(ui_sys_main->string);
			if (!ui_global.windowStackPos)
				UI_PushWindow(ui_sys_active->string);
		}
	}

	uiNode_t* activeWindow = UI_GetActiveWindow();
	UI_Node_WindowActivate(activeWindow);
}

void UI_CloseWindow (const char* name)
{
	uiNode_t* window = UI_GetWindow(name);
	if (window == nullptr) {
		Com_Printf("Window '%s' not found\n", name);
		return;
	}

	/* found the correct add it to stack or bring it on top */
	UI_CloseWindowByRef(window);
}

/**
 * @brief Pops a window from the window stack
 * @param[in] all If true pop all windows from stack
 * @sa UI_PopWindow_f
 */
void UI_PopWindow (bool all)
{
	if (all) {
		UI_CloseAllWindow();
	} else {
		uiNode_t* mainWindow = ui_global.windowStack[ui_global.windowStackPos - 1];
		if (!ui_global.windowStackPos)
			return;
		if (WINDOWEXTRADATA(mainWindow).parent)
			mainWindow = WINDOWEXTRADATA(mainWindow).parent;
		UI_CloseWindowByRef(mainWindow);
	}

	/* change from e.g. console mode to game input mode (fetch input) */
	Key_SetDest(key_game);
}

/**
 * @brief Console function to close a named window
 * @sa UI_PushWindow
 */
static void UI_CloseWindow_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	UI_CloseWindow(Cmd_Argv(1));
}

void UI_PopWindowWithEscKey (void)
{
	/* nothing if stack is empty */
	if (ui_global.windowStackPos == 0)
		return;

	/* some window can prevent escape */
	const uiNode_t* window = ui_global.windowStack[ui_global.windowStackPos - 1];
	if (WINDOWEXTRADATACONST(window).preventTypingEscape)
		return;

	UI_PopWindow();
}

/**
 * @brief Console function to pop a window from the window stack
 * @sa UI_PopWindow
 */
static void UI_PopWindow_f (void)
{
	if (Cmd_Argc() > 1) {
		Com_Printf("Usage: %s\n", Cmd_Argv(0));
		return;
	}

	UI_PopWindow();
}

/**
 * @brief Returns the current active window from the window stack or nullptr if there is none
 * @return uiNode_t pointer from window stack
 * @sa UI_GetWindow
 */
uiNode_t* UI_GetActiveWindow (void)
{
	return (ui_global.windowStackPos > 0 ? ui_global.windowStack[ui_global.windowStackPos - 1] : nullptr);
}

/**
 * @brief Returns the name of the current window
 * @return Active window name, else empty string
 * @sa UI_GetActiveWIndow
 */
const char* UI_GetActiveWindowName (void)
{
	const uiNode_t* window = UI_GetActiveWindow();
	if (window == nullptr)
		return "";
	return window->name;
}

/**
 * @brief Check if a point is over a window from the stack
 * @sa IN_Parse
 */
bool UI_IsMouseOnWindow (void)
{
	if (UI_GetMouseCapture() != nullptr)
		return true;

	if (ui_global.windowStackPos != 0) {
		if (WINDOWEXTRADATA(ui_global.windowStack[ui_global.windowStackPos - 1]).dropdown)
			return true;
	}

	const uiNode_t* hovered = UI_GetHoveredNode();
	if (hovered) {
		/* else if it is a render node */
		if (UI_Node_IsBattleScape(hovered)) {
			return false;
		}
		return true;
	}

	return true;
}

/**
 * @brief Searches all windows for the specified one
 * @param[in] name Name of the window we search
 * @return The window found, else nullptr
 * @note Use dichotomic search
 * @sa UI_GetActiveWindow
 */
uiNode_t* UI_GetWindow (const char* name)
{
	unsigned char min = 0;
	unsigned char max = ui_global.numWindows;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const int diff = strcmp(ui_global.windows[mid]->name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return ui_global.windows[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	return nullptr;
}

/**
 * @brief Invalidate all windows of the current stack.
 */
void UI_InvalidateStack (void)
{
	for (int pos = 0; pos < ui_global.windowStackPos; pos++) {
		UI_Invalidate(ui_global.windowStack[pos]);
	}
	Cvar_SetValue("ui_sys_screenwidth", viddef.virtualWidth);
	Cvar_SetValue("ui_sys_screenheight", viddef.virtualHeight);
}

/**
 * @brief Sets new x and y coordinates for a given window
 * @todo remove that
 */
void UI_SetNewWindowPos (uiNode_t* window, int x, int y)
{
	if (window)
		Vector2Set(window->box.pos, x, y);
}

/**
 * @brief Add a new window to the list of all windows
 * @note Sort windows by alphabet
 */
void UI_InsertWindow (uiNode_t* window)
{

	if (ui_global.numWindows >= UI_MAX_WINDOWS)
		Com_Error(ERR_FATAL, "UI_InsertWindow: hit UI_MAX_WINDOWS");

	/* search the insertion position */
	int pos;
	for (pos = 0; pos < ui_global.numWindows; pos++) {
		const uiNode_t* node = ui_global.windows[pos];
		if (strcmp(window->name, node->name) < 0)
			break;
	}

	/* create the space */
	for (int i = ui_global.numWindows - 1; i >= pos; i--)
		ui_global.windows[i + 1] = ui_global.windows[i];

	/* insert */
	ui_global.windows[pos] = window;
	ui_global.numWindows++;
}

/**
 * @brief Finish windows initialization
 * @note private function
 */
void UI_FinishWindowsInit (void)
{
	for (int i = 0; i < ui_global.numWindows; i++) {
		uiNode_t* window = ui_global.windows[i];
		if (WINDOWEXTRADATA(window).onScriptLoaded)
			UI_ExecuteEventActions(window, WINDOWEXTRADATA(window).onScriptLoaded);
	}
}

static void UI_InitStack_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <mainwindow> [<optionwindow>]\n", Cmd_Argv(0));
		return;
	}

	const char* mainWindow = Cmd_Argv(1);
	const char* optionWindow = nullptr;
	if (Cmd_Argc() == 3) {
		optionWindow = Cmd_Argv(2);
	}

	UI_InitStack(mainWindow, optionWindow);
}

/**
 * @brief Display in the conde the tree of nodes
 */
static void UI_DebugTree (const uiNode_t* node, int depth)
{
	for (int i = 0; i < depth; i++) {
		Com_Printf("    ");
	}
	Com_Printf("+ %s %s\n", UI_Node_GetWidgetName(node), node->name);

	const uiNode_t* child = node->firstChild;
	while (child) {
		UI_DebugTree(child, depth + 1);
		child = child->next;
	}
}

static void UI_DebugTree_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mainwindow>\n", Cmd_Argv(0));
		return;
	}

	const char* window = Cmd_Argv(1);
	const uiNode_t* node = UI_GetWindow(window);
	UI_DebugTree(node, 0);
}

static void UI_Popup_f (void)
{
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <header> <body>\n", Cmd_Argv(0));
		return;
	}

	const char* header = Cmd_Argv(1);
	const char* body = Cmd_Argv(2);
	UI_Popup(header, body);
}

void UI_InitWindows (void)
{
	ui_sys_main = Cvar_Get("ui_sys_main", "", 0, "This is the main window id that is at the very first window stack - also see ui_sys_active");
	ui_sys_active = Cvar_Get("ui_sys_active", "", 0, "The active window we will return to when hitting esc once - also see ui_sys_main");

	/* add command */
	Cmd_AddCommand("ui_push", UI_PushWindow_f, "Push a window to the window stack");
	Cmd_AddParamCompleteFunction("ui_push", UI_CompleteWithWindow);
	Cmd_AddCommand("ui_push_dropdown", UI_PushDropDownWindow_f, "Push a dropdown window at a position");
	Cmd_AddCommand("ui_push_child", UI_PushChildWindow_f, "Push a window to the windowstack with a big dependency to a parent window");
	Cmd_AddCommand("ui_pop", UI_PopWindow_f, "Pops the current window from the stack");
	Cmd_AddCommand("ui_close", UI_CloseWindow_f, "Close a window");
	Cmd_AddCommand("ui_initstack", UI_InitStack_f, "Initialize the window stack with a main and an option window.");
	Cmd_AddCommand("ui_tree", UI_DebugTree_f, "Display a tree of nodes from a window into the console.");
	Cmd_AddCommand("ui_popup", UI_Popup_f, "Shows a popup window");
	Cmd_AddParamCompleteFunction("ui_tree", UI_CompleteWithWindow);
}
