/**
 * @file ui_windows.c
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
#include "ui_input.h"
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
static cvar_t *ui_sys_main;

/**
 * @brief Main window of a stack
 */
static cvar_t *ui_sys_active;

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
void UI_MoveWindowOnTop (uiNode_t * window)
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
static void UI_DeleteWindowFromStack (uiNode_t *window)
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
static inline int UI_GetWindowPositionFromStackByName (const char *name)
{
	int i;
	for (i = 0; i < ui_global.windowStackPos; i++)
		if (Q_streq(ui_global.windowStack[i]->name, name))
			return i;

	return -1;
}

/**
 * @brief Insert a window at a position of the stack
 * @param[in] window The window to insert
 * @param[in] position Where we want to add the window (0 is the deeper element of the stack)
 */
static inline void UI_InsertWindowIntoStack (uiNode_t *window, int position)
{
	int i;
	assert(position <= ui_global.windowStackPos);
	assert(position > 0);
	assert(window != NULL);

	/* create space for the new window */
	for (i = ui_global.windowStackPos; i > position; i--) {
		ui_global.windowStack[i] = ui_global.windowStack[i - 1];
	}
	/* insert */
	ui_global.windowStack[position] = window;
	ui_global.windowStackPos++;
}

/**
 * @brief Push a window onto the window stack
 * @param[in] name Name of the window to push onto window stack
 * @param[in] parentName Window name to link as parent-child (else NULL)
 * @param[in] params List of string parametters we want to send to the onWindowOpened method
 * @return A pointer to @c uiNode_t
 */
uiNode_t* UI_PushWindow (const char *name, const char *parentName, linkedList_t *params)
{
	uiNode_t *window;

	UI_ReleaseInput();

	window = UI_GetWindow(name);
	if (window == NULL) {
		Com_Printf("Window \"%s\" not found.\n", name);
		return NULL;
	}

	UI_DeleteWindowFromStack(window);

	if (ui_global.windowStackPos < UI_MAX_WINDOWSTACK)
		if (parentName) {
			const int parentPos = UI_GetWindowPositionFromStackByName(parentName);
			if (parentPos == -1) {
				Com_Printf("Didn't find parent window \"%s\" for window push of \"%s\"\n", parentName, name);
				return NULL;
			}
			UI_InsertWindowIntoStack(window, parentPos + 1);
			WINDOWEXTRADATA(window).parent = ui_global.windowStack[parentPos];
		} else
			ui_global.windowStack[ui_global.windowStackPos++] = window;
	else
		Com_Printf("Window stack overflow\n");

	if (window->behaviour->windowOpened) {
		window->behaviour->windowOpened(window, params);
	}

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
int UI_CompleteWithWindow (const char *partial, const char **match)
{
	int i;
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	const size_t len = strlen(partial);

	if (len == 0) {
		for (i = 0; i < ui_global.numWindows; i++)
			Com_Printf("%s\n", ui_global.windows[i]->name);
		return 0;
	}

	/* check for partial matches */
	for (i = 0; i < ui_global.numWindows; i++) {
		char const* const name = ui_global.windows[i]->name;
		if (!strncmp(partial, name, len)) {
			Com_Printf("%s\n", name);
			localMatch[matches++] = name;
			if (matches >= MAX_COMPLETE) {
				Com_Printf("UI_CompleteWithWindow: hit MAX_COMPLETE\n");
				break;
			}
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @brief Console function to push a child window onto the window stack
 * @sa UI_PushWindow
 */
static void UI_PushChildWindow_f (void)
{
	if (Cmd_Argc() > 1)
		UI_PushWindow(Cmd_Argv(1), Cmd_Argv(2), NULL);
	else
		Com_Printf("Usage: %s <name> <parentname>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to push a window onto the window stack
 * @sa UI_PushWindow
 */
static void UI_PushWindow_f (void)
{
	linkedList_t *params = NULL;
	int i;

	if (Cmd_Argc() == 0) {
		Com_Printf("Usage: %s <name> <params>\n", Cmd_Argv(0));
		return;
	}

	for (i = 2; i < Cmd_Argc(); i++) {
		LIST_AddString(&params, Cmd_Argv(i));
	}
	UI_PushWindow(Cmd_Argv(1), NULL, params);
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
	vec2_t source;
	vec2_t destination;
	uiNode_t *node;
	int direction;
	size_t writtenBytes;
	int result;

	if (Cmd_Argc() != 4 && Cmd_Argc() != 5) {
		Com_Printf("Usage: %s <source-anchor> <point-in-source-anchor> <dest-anchor> <point-in-dest-anchor>\n", Cmd_Argv(0));
		return;
	}

	/* get the source anchor */
	node = UI_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("UI_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	result = Com_ParseValue(&direction, Cmd_Argv(2), V_INT, 0, sizeof(direction), &writtenBytes);
	if (result != RESULT_OK) {
		Com_Printf("UI_PushDropDownWindow_f: '%s' in not a V_INT\n", Cmd_Argv(2));
		return;
	}
	UI_NodeGetPoint(node, source, direction);
	UI_NodeRelativeToAbsolutePoint(node, source);

	/* get the destination anchor */
	if (Q_streq(Cmd_Argv(4), "mouse")) {
		destination[0] = mousePosX;
		destination[1] = mousePosY;
	} else {
		/* get the source anchor */
		node = UI_GetNodeByPath(Cmd_Argv(3));
		if (node == NULL) {
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
	if (node == NULL) {
		Com_Printf("UI_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	node = node->root;
	node->pos[0] += destination[0] - source[0];
	node->pos[1] += destination[1] - source[1];
	UI_PushWindow(node->name, NULL, NULL);
}

static void UI_RemoveWindowAtPositionFromStack (int position)
{
	int i;
	assert(position < ui_global.windowStackPos);
	assert(position >= 0);

	/* create space for the new window */
	for (i = position; i < ui_global.windowStackPos; i++) {
		ui_global.windowStack[i] = ui_global.windowStack[i + 1];
	}
	ui_global.windowStack[ui_global.windowStackPos--] = NULL;
}

static void UI_CloseAllWindow (void)
{
	int i;
	for (i = ui_global.windowStackPos - 1; i >= 0; i--) {
		uiNode_t *window = ui_global.windowStack[i];

		if (window->behaviour->windowClosed)
			window->behaviour->windowClosed(window);

		/* safe: unlink window */
		WINDOWEXTRADATA(window).parent = NULL;
		ui_global.windowStackPos--;
		ui_global.windowStack[ui_global.windowStackPos] = NULL;
	}
}

/**
 * @brief Init the stack to start with a window, and have an alternative window with ESC
 * @param[in] activeWindow The first active window of the stack, else NULL
 * @param[in] mainWindow The alternative window, else NULL if nothing
 * @param[in] popAll If true, clean up the stack first
 * @param[in] pushActive If true, push the active window into the stack
 * @todo remove Cvar_Set we have direct access to the cvar
 * @todo check why activeWindow can be NULL. It should never be NULL: a stack must not be empty
 * @todo We should only call it a very few time. When we switch from/to this different par of the game: main-option-interface / geoscape-and-base / battlescape
 * @todo Update the code: popAll should be every time true
 * @todo Update the code: pushActive should be every time true
 * @todo Illustration about when/how we should use UI_InitStack http://ufoai.org/wiki/index.php/Image:UI_InitStack.jpg
 */
void UI_InitStack (const char* activeWindow, const char* mainWindow, qboolean popAll, qboolean pushActive)
{
	UI_FinishInit();

	if (popAll)
		UI_PopWindow(qtrue);
	if (activeWindow) {
		Cvar_Set("ui_sys_active", activeWindow);
		/* prevent calls before UI script initialization */
		if (ui_global.numWindows != 0) {
			if (pushActive)
				UI_PushWindow(activeWindow, NULL, NULL);
		}
	}

	if (mainWindow)
		Cvar_Set("ui_sys_main", mainWindow);
}

/**
 * @brief Check if a named window is on the stack if active windows
 */
qboolean UI_IsWindowOnStack (const char* name)
{
	return UI_GetWindowPositionFromStackByName(name) != -1;
}

/**
 * @todo Find  better name
 */
static void UI_CloseWindowByRef (uiNode_t *window)
{
	int i;

	/** @todo If the focus is not on the window we close, we don't need to remove it */
	UI_ReleaseInput();

	assert(ui_global.windowStackPos);
	i = UI_GetWindowPositionFromStackByName(window->name);
	if (i == -1) {
		Com_Printf("Window '%s' is not on the active stack\n", window->name);
		return;
	}

	/* close child */
	while (i + 1 < ui_global.windowStackPos) {
		uiNode_t *m = ui_global.windowStack[i + 1];
		if (WINDOWEXTRADATA(m).parent != window) {
			break;
		}
		if (window->behaviour->windowClosed)
			window->behaviour->windowClosed(window);
		WINDOWEXTRADATA(m).parent = NULL;
		UI_RemoveWindowAtPositionFromStack(i + 1);
	}

	/* close the window */
	if (window->behaviour->windowClosed)
		window->behaviour->windowClosed(window);
	WINDOWEXTRADATA(window).parent = NULL;
	UI_RemoveWindowAtPositionFromStack(i);

	UI_InvalidateMouse();
}

void UI_CloseWindow (const char* name)
{
	uiNode_t *window = UI_GetWindow(name);
	if (window == NULL) {
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
void UI_PopWindow (qboolean all)
{
	uiNode_t *oldfirst = ui_global.windowStack[0];

	if (all) {
		UI_CloseAllWindow();
	} else {
		uiNode_t *mainWindow = ui_global.windowStack[ui_global.windowStackPos - 1];
		if (!ui_global.windowStackPos)
			return;
		if (WINDOWEXTRADATA(mainWindow).parent)
			mainWindow = WINDOWEXTRADATA(mainWindow).parent;
		UI_CloseWindowByRef(mainWindow);

		if (ui_global.windowStackPos == 0) {
			/* ui_sys_main contains the window that is the very first window and should be
			 * pushed back onto the stack (otherwise there would be no window at all
			 * right now) */
			if (Q_streq(oldfirst->name, ui_sys_main->string)) {
				if (ui_sys_active->string[0] != '\0')
					UI_PushWindow(ui_sys_active->string, NULL, NULL);
				if (!ui_global.windowStackPos)
					UI_PushWindow(ui_sys_main->string, NULL, NULL);
			} else {
				if (ui_sys_main->string[0] != '\0')
					UI_PushWindow(ui_sys_main->string, NULL, NULL);
				if (!ui_global.windowStackPos)
					UI_PushWindow(ui_sys_active->string, NULL, NULL);
			}
		}
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
	const uiNode_t *window = ui_global.windowStack[ui_global.windowStackPos - 1];

	/* nothing if stack is empty */
	if (ui_global.windowStackPos == 0)
		return;

	/* some window can prevent escape */
	if (WINDOWEXTRADATACONST(window).preventTypingEscape)
		return;

	UI_PopWindow(qfalse);
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

	UI_PopWindow(qfalse);
}

/**
 * @brief Returns the current active window from the window stack or NULL if there is none
 * @return uiNode_t pointer from window stack
 * @sa UI_GetWindow
 */
uiNode_t* UI_GetActiveWindow (void)
{
	return (ui_global.windowStackPos > 0 ? ui_global.windowStack[ui_global.windowStackPos - 1] : NULL);
}

/**
 * @brief Returns the name of the current window
 * @return Active window name, else empty string
 * @sa UI_GetActiveWIndow
 */
const char* UI_GetActiveWindowName (void)
{
	const uiNode_t* window = UI_GetActiveWindow();
	if (window == NULL)
		return "";
	return window->name;
}

/**
 * @brief Check if a point is over a window from the stack
 * @sa IN_Parse
 */
qboolean UI_IsMouseOnWindow (void)
{
	const uiNode_t *hovered;

	if (UI_GetMouseCapture() != NULL)
		return qtrue;

	if (ui_global.windowStackPos != 0) {
		if (WINDOWEXTRADATA(ui_global.windowStack[ui_global.windowStackPos - 1]).dropdown)
			return qtrue;
	}

	hovered = UI_GetHoveredNode();
	if (hovered) {
		/* else if it is a render node */
		if (hovered->behaviour == ui_battleScapeBehaviour) {
			return qfalse;
		}
		return qtrue;
	}

	return qtrue;
}

/**
 * @brief Searches all windows for the specified one
 * @param[in] name Name of the window we search
 * @return The window found, else NULL
 * @note Use dichotomic search
 * @sa UI_GetActiveWindow
 */
uiNode_t *UI_GetWindow (const char *name)
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

	return NULL;
}

/**
 * @brief Invalidate all windows of the current stack.
 */
void UI_InvalidateStack (void)
{
	int pos;
	for (pos = 0; pos < ui_global.windowStackPos; pos++) {
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
		Vector2Set(window->pos, x, y);
}

/**
 * @brief Add a new window to the list of all windows
 * @note Sort windows by alphabet
 */
void UI_InsertWindow (uiNode_t* window)
{
	int pos = 0;
	int i;

	if (ui_global.numWindows >= UI_MAX_WINDOWS)
		Com_Error(ERR_FATAL, "UI_InsertWindow: hit UI_MAX_WINDOWS");

	/* search the insertion position */
	for (pos = 0; pos < ui_global.numWindows; pos++) {
		const uiNode_t* node = ui_global.windows[pos];
		if (strcmp(window->name, node->name) < 0)
			break;
	}

	/* create the space */
	for (i = ui_global.numWindows - 1; i >= pos; i--)
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
	int i;
	for (i = 0; i < ui_global.numWindows; i++) {
		uiNode_t *window = ui_global.windows[i];
		if (WINDOWEXTRADATA(window).onScriptLoaded)
			UI_ExecuteEventActions(window, WINDOWEXTRADATA(window).onScriptLoaded);
	}
}

static void UI_InitStack_f (void) {
	const char *mainWindow;
	const char *optionWindow = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <mainwindow> [<optionwindow>]\n", Cmd_Argv(0));
		return;
	}

	mainWindow = Cmd_Argv(1);
	if (Cmd_Argc() == 3) {
		optionWindow = Cmd_Argv(2);
	}

	UI_InitStack(mainWindow, optionWindow, qtrue, qtrue);
}

/**
 * @brief Display in the conde the tree of nodes
 */
static void UI_DebugTree (const uiNode_t *node, int depth)
{
	const uiNode_t *child = node->firstChild;
	int i;

	for (i = 0; i < depth; i++) {
		Com_Printf("    ");
	}
	Com_Printf("+ %s %s\n", node->behaviour->name, node->name);

	while (child) {
		UI_DebugTree(child, depth + 1);
		child = child->next;
	}
}

static void UI_DebugTree_f (void)
{
	const char *window;
	const uiNode_t *node;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mainwindow>\n", Cmd_Argv(0));
		return;
	}

	window = Cmd_Argv(1);
	node = UI_GetWindow(window);
	UI_DebugTree(node, 0);
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
	Cmd_AddParamCompleteFunction("ui_tree", UI_CompleteWithWindow);
}
