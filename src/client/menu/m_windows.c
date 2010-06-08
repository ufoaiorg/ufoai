/**
 * @file m_windows.c
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
#include "m_input.h"
#include "node/m_node_abstractnode.h"
#include "node/m_node_window.h"

#include "../client.h"

#define WINDOWEXTRADATA(node) MN_EXTRADATA(node, windowExtraData_t)
#define WINDOWEXTRADATACONST(node)  MN_EXTRADATACONST(node, windowExtraData_t)

/**
 * @brief Window name use as alternative for option
 */
static cvar_t *mn_sys_main;

/**
 * @brief Main window of a stack
 */
static cvar_t *mn_sys_active;

/**
 * @brief Returns the ID of the last fullscreen ID. Before this, window should be hidden.
 * @return The last full screen window on the screen, else 0. If the stack is empty, return -1
 */
int MN_GetLastFullScreenWindow (void)
{
	/* stack pos */
	int pos = mn.windowStackPos - 1;
	while (pos > 0) {
		if (MN_WindowIsFullScreen(mn.windowStack[pos]))
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
void MN_MoveWindowOnTop (menuNode_t * window)
{
	int i, j;

	if (MN_WindowIsFullScreen(window))
		return;

	/* get window index */
	for (i = 0; i < mn.windowStackPos; i++) {
		if (mn.windowStack[i] == window)
			break;
	}

	/* search the last compatible window */
	for (j = i; j < mn.windowStackPos; j++) {
		if (MN_WindowIsFullScreen(mn.windowStack[j]))
			break;
		if (WINDOWEXTRADATA(window).parent != WINDOWEXTRADATA(mn.windowStack[j]).parent)
			break;
	}
	if (i + 1 == j)
		return;

	/* translate windows */
	for (; i < j - 1; i++) {
		mn.windowStack[i] = mn.windowStack[i+1];
	}
	/* add the current window on top */
	mn.windowStack[i] = window;
}

/**
 * @brief Remove the window from the window stack
 * @param[in] window The window to remove from the stack
 * @sa MN_PushWindowDelete
 * @todo Why dont we call onClose?
 */
static void MN_DeleteWindowFromStack (menuNode_t *window)
{
	int i;

	/* get window index */
	for (i = 0; i < mn.windowStackPos; i++) {
		if (mn.windowStack[i] == window)
			break;
	}

	/* update stack */
	if (i < mn.windowStackPos) {
		mn.windowStackPos--;
		for (; i < mn.windowStackPos; i++)
			mn.windowStack[i] = mn.windowStack[i + 1];
		MN_InvalidateMouse();
	}
}

/**
 * @brief Searches the position in the current window stack for a given window id
 * @return -1 if the window is not on the stack
 */
static inline int MN_GetWindowPositionFromStackByName (const char *name)
{
	int i;
	for (i = 0; i < mn.windowStackPos; i++)
		if (!strcmp(mn.windowStack[i]->name, name))
			return i;

	return -1;
}

/**
 * @brief Insert a window at a position of the stack
 * @param[in] window The window to insert
 * @param[in] position Where we want to add the window (0 is the deeper element of the stack)
 */
static inline void MN_InsertWindowIntoStack (menuNode_t *window, int position)
{
	int i;
	assert(position <= mn.windowStackPos);
	assert(position > 0);
	assert(window != NULL);

	/* create space for the new window */
	for (i = mn.windowStackPos; i > position; i--) {
		mn.windowStack[i] = mn.windowStack[i - 1];
	}
	/* insert */
	mn.windowStack[position] = window;
	mn.windowStackPos++;
}

/**
 * @brief Push a window onto the window stack
 * @param[in] name Name of the window to push onto window stack
 * @param[in] parent Window name to link as parent-child (else NULL)
 * @param[in] delete Delete the window from the window stack before adding it again
 * @return pointer to menuNode_t
 */
static menuNode_t* MN_PushWindowDelete (const char *name, const char *parent, qboolean delete)
{
	menuNode_t *window;

	MN_ReleaseInput();

	window = MN_GetWindow(name);
	if (window == NULL) {
		Com_Printf("Window \"%s\" not found.\n", name);
		return NULL;
	}

	/* found the correct add it to stack or bring it on top */
	if (delete)
		MN_DeleteWindowFromStack(window);

	if (mn.windowStackPos < MAX_MENUSTACK)
		if (parent) {
			const int parentPos = MN_GetWindowPositionFromStackByName(parent);
			if (parentPos == -1) {
				Com_Printf("Didn't find parent window \"%s\" for window push of \"%s\"\n", parent, name);
				return NULL;
			}
			MN_InsertWindowIntoStack(window, parentPos + 1);
			WINDOWEXTRADATA(window).parent = mn.windowStack[parentPos];
		} else
			mn.windowStack[mn.windowStackPos++] = window;
	else
		Com_Printf("Window stack overflow\n");

	if (window->behaviour->init) {
		window->behaviour->init(window);
	}

	/* change from e.g. console mode to game input mode (fetch input) */
	Key_SetDest(key_game);

	MN_InvalidateMouse();
	return window;
}

/**
 * @brief Complete function for mn_push
 * @sa Cmd_AddParamCompleteFunction
 * @sa MN_PushWindow
 * @note Does not really complete the input - but shows at least all parsed windows
 */
int MN_CompleteWithWindow (const char *partial, const char **match)
{
	int i;
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	const size_t len = strlen(partial);

	if (len == 0) {
		for (i = 0; i < mn.numWindows; i++)
			Com_Printf("%s\n", mn.windows[i]->name);
		return 0;
	}

	/* check for partial matches */
	for (i = 0; i < mn.numWindows; i++)
		if (!strncmp(partial, mn.windows[i]->name, len)) {
			Com_Printf("%s\n", mn.windows[i]->name);
			localMatch[matches++] = mn.windows[i]->name;
			if (matches >= MAX_COMPLETE) {
				Com_Printf("MN_CompleteWithWindow: hit MAX_COMPLETE\n");
				break;
			}
		}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @brief Push a window onto the menu stack
 * @param[in] name Name of the window to push onto window stack
 * @param[in] parentName Window name to link as parent-child (else NULL)
 * @return pointer to menuNode_t
 */
menuNode_t* MN_PushWindow (const char *name, const char *parentName)
{
	return MN_PushWindowDelete(name, parentName, qtrue);
}

/**
 * @brief Console function to push a child window onto the window stack
 * @sa MN_PushWindow
 */
static void MN_PushChildWindow_f (void)
{
	if (Cmd_Argc() > 1)
		MN_PushWindow(Cmd_Argv(1), Cmd_Argv(2));
	else
		Com_Printf("Usage: %s <name> <parentname>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to push a window onto the window stack
 * @sa MN_PushWindow
 */
static void MN_PushWindow_f (void)
{
	if (Cmd_Argc() > 1)
		MN_PushWindow(Cmd_Argv(1), NULL);
	else
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to push a dropdown window at a position. It work like MN_PushWindow but move the window at the right position
 * @sa MN_PushWindow
 * @note The usage is "mn_push_dropdown sourcenode pointposition destinationnode pointposition"
 * sourcenode must be a node into the window we want to push,
 * we will move the window to have sourcenode over destinationnode
 * pointposition select for each node a position (like a corner).
 */
static void MN_PushDropDownWindow_f (void)
{
	vec2_t source;
	vec2_t destination;
	menuNode_t *node;
	byte pointPosition;
	size_t writtenBytes;
	int result;

	if (Cmd_Argc() != 4 && Cmd_Argc() != 5) {
		Com_Printf("Usage: %s <source-anchor> <point-in-source-anchor> <dest-anchor> <point-in-dest-anchor>\n", Cmd_Argv(0));
		return;
	}

	/* get the source anchor */
	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	result = Com_ParseValue(&pointPosition, Cmd_Argv(2), V_ALIGN, 0, sizeof(pointPosition), &writtenBytes);
	if (result != RESULT_OK) {
		Com_Printf("MN_PushDropDownWindow_f: '%s' in not a V_ALIGN\n", Cmd_Argv(2));
		return;
	}
	MN_NodeGetPoint(node, source, pointPosition);
	MN_NodeRelativeToAbsolutePoint(node, source);

	/* get the destination anchor */
	if (!strcmp(Cmd_Argv(4), "mouse")) {
		destination[0] = mousePosX;
		destination[1] = mousePosY;
	} else {
		/* get the source anchor */
		node = MN_GetNodeByPath(Cmd_Argv(3));
		if (node == NULL) {
			Com_Printf("MN_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(3));
			return;
		}
		result = Com_ParseValue(&pointPosition, Cmd_Argv(4), V_ALIGN, 0, sizeof(pointPosition), &writtenBytes);
		if (result != RESULT_OK) {
			Com_Printf("MN_PushDropDownWindow_f: '%s' in not a V_ALIGN\n", Cmd_Argv(4));
			return;
		}
		MN_NodeGetPoint(node, destination, pointPosition);
		MN_NodeRelativeToAbsolutePoint(node, destination);
	}

	/* update the window and push it */
	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_PushDropDownWindow_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	node = node->root;
	node->pos[0] += destination[0] - source[0];
	node->pos[1] += destination[1] - source[1];
	MN_PushWindow(node->name, NULL);
}

/**
 * @brief Console function to hide the HUD in battlescape mode
 * Note: relies on a "nohud" window existing
 * @sa MN_PushWindow
 */
static void MN_PushNoHud_f (void)
{
	/* can't hide hud if we are not in battlescape */
	if (!CL_BattlescapeRunning())
		return;

	MN_PushWindow("nohud", NULL);
}

static void MN_RemoveWindowAtPositionFromStack (int position)
{
	int i;
	assert(position < mn.windowStackPos);
	assert(position >= 0);

	/* create space for the new window */
	for (i = position; i < mn.windowStackPos; i++) {
		mn.windowStack[i] = mn.windowStack[i + 1];
	}
	mn.windowStack[mn.windowStackPos--] = NULL;
}

static void MN_CloseAllWindow (void)
{
	int i;
	for (i = mn.windowStackPos - 1; i >= 0; i--) {
		menuNode_t *window = mn.windowStack[i];

		if (window->behaviour->close)
			window->behaviour->close(window);

		/* safe: unlink window */
		WINDOWEXTRADATA(window).parent = NULL;
		mn.windowStackPos--;
		mn.windowStack[mn.windowStackPos] = NULL;
	}
}

/**
 * @brief Init the stack to start with a window, and have an alternative window with ESC
 * @param[in] activeMenu The first active window of the stack, else NULL
 * @param[in] mainMenu The alternative window, else NULL if nothing
 * @param[in] popAll If true, clean up the stack first
 * @param[in] pushActive If true, push the active window into the stack
 * @todo remove Cvar_Set we have direct access to the cvar
 * @todo check why activeMenu can be NULL. It should never be NULL: a stack must not be empty
 * @todo We should only call it a very few time. When we switch from/to this different par of the game: main-option-menu / geoscape-and-base / battlescape
 * @todo Update the code: popAll should be every time true
 * @todo Update the code: pushActive should be every time true
 * @todo Illustration about when/how we should use MN_InitStack http://ufoai.ninex.info/wiki/index.php/Image:MN_InitStack.jpg
 */
void MN_InitStack (const char* activeMenu, const char* mainMenu, qboolean popAll, qboolean pushActive)
{
	if (popAll)
		MN_PopWindow(qtrue);
	if (activeMenu) {
		Cvar_Set("mn_sys_active", activeMenu);
		/* prevent calls before UI script initialization */
		if (mn.numWindows != 0) {
			if (pushActive)
				MN_PushWindow(activeMenu, NULL);
		}
	}

	if (mainMenu)
		Cvar_Set("mn_sys_main", mainMenu);
}

/**
 * @brief Check if a named window is on the stack if active windows
 */
qboolean MN_IsWindowOnStack(const char* name)
{
	return MN_GetWindowPositionFromStackByName(name) != -1;
}

/**
 * @todo Find  better name
 */
static void MN_CloseWindowByRef (menuNode_t *window)
{
	int i;

	/** @todo If the focus is not on the window we close, we don't need to remove it */
	MN_ReleaseInput();

	assert(mn.windowStackPos);
	i = MN_GetWindowPositionFromStackByName(window->name);
	if (i == -1) {
		Com_Printf("Window '%s' is not on the active stack\n", window->name);
		return;
	}

	/* close child */
	while (i + 1 < mn.windowStackPos) {
		menuNode_t *m = mn.windowStack[i + 1];
		if (WINDOWEXTRADATA(m).parent != window) {
			break;
		}
		if (window->behaviour->close)
			window->behaviour->close(window);
		WINDOWEXTRADATA(m).parent = NULL;
		MN_RemoveWindowAtPositionFromStack(i + 1);
	}

	/* close the window */
	if (window->behaviour->close)
		window->behaviour->close(window);
	WINDOWEXTRADATA(window).parent = NULL;
	MN_RemoveWindowAtPositionFromStack(i);

	MN_InvalidateMouse();
}

void MN_CloseWindow (const char* name)
{
	menuNode_t *window = MN_GetWindow(name);
	if (window == NULL) {
		Com_Printf("Window '%s' not found\n", name);
		return;
	}

	/* found the correct add it to stack or bring it on top */
	MN_CloseWindowByRef(window);
}

/**
 * @brief Pops a window from the window stack
 * @param[in] all If true pop all windows from stack
 * @sa MN_PopWindow_f
 */
void MN_PopWindow (qboolean all)
{
	menuNode_t *oldfirst = mn.windowStack[0];

	if (all) {
		MN_CloseAllWindow();
	} else {
		menuNode_t *mainMenu = mn.windowStack[mn.windowStackPos - 1];
		if (!mn.windowStackPos)
			return;
		if (WINDOWEXTRADATA(mainMenu).parent)
			mainMenu = WINDOWEXTRADATA(mainMenu).parent;
		MN_CloseWindowByRef(mainMenu);

		if (mn.windowStackPos == 0) {
			/* mn_main contains the window that is the very first window and should be
			 * pushed back onto the stack (otherwise there would be no window at all
			 * right now) */
			if (!strcmp(oldfirst->name, mn_sys_main->string)) {
				if (mn_sys_active->string[0] != '\0')
					MN_PushWindow(mn_sys_active->string, NULL);
				if (!mn.windowStackPos)
					MN_PushWindow(mn_sys_main->string, NULL);
			} else {
				if (mn_sys_main->string[0] != '\0')
					MN_PushWindow(mn_sys_main->string, NULL);
				if (!mn.windowStackPos)
					MN_PushWindow(mn_sys_active->string, NULL);
			}
		}
	}

	/* change from e.g. console mode to game input mode (fetch input) */
	Key_SetDest(key_game);
}

/**
 * @brief Console function to close a named window
 * @sa MN_PushWindow
 */
static void MN_CloseWindow_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	MN_CloseWindow(Cmd_Argv(1));
}

void MN_PopWindowWithEscKey (void)
{
	const menuNode_t *window = mn.windowStack[mn.windowStackPos - 1];

	/* nothing if stack is empty */
	if (mn.windowStackPos == 0)
		return;

	/* some window can prevent escape */
	if (WINDOWEXTRADATACONST(window).preventTypingEscape)
		return;

	MN_PopWindow(qfalse);
}

/**
 * @brief Console function to pop a window from the window stack
 * @sa MN_PopWindow
 */
static void MN_PopWindow_f (void)
{
	if (Cmd_Argc() > 1) {
		Com_Printf("Usage: %s\n", Cmd_Argv(0));
		return;
	}

	MN_PopWindow(qfalse);
}

/**
 * @brief Returns the current active window from the window stack or NULL if there is none
 * @return menuNode_t pointer from window stack
 * @sa MN_GetWindow
 */
menuNode_t* MN_GetActiveWindow (void)
{
	return (mn.windowStackPos > 0 ? mn.windowStack[mn.windowStackPos - 1] : NULL);
}

/**
 * @brief Determine the position and size of render
 */
void MN_GetActiveRenderRect (int *x, int *y, int *width, int *height)
{
	menuNode_t *window = MN_GetActiveWindow();

	/** @todo the better way is to add a 'battlescape' node */
	if (!window || !WINDOWEXTRADATA(window).renderNode)
		if (MN_IsWindowOnStack(mn_hud->string))
			window = MN_GetWindow(mn_hud->string);

	if (window && WINDOWEXTRADATA(window).renderNode) {
		menuNode_t* node = WINDOWEXTRADATA(window).renderNode;
		vec2_t pos;

		MN_Validate(window);

		MN_GetNodeAbsPos(node, pos);
		*x = pos[0] * viddef.rx;
		*y = pos[1] * viddef.ry;
		*width = node->size[0] * viddef.rx;
		*height = node->size[1] * viddef.ry;
	} else {
		*x = 0;
		*y = 0;
		*width = 0;
		*height = 0;
	}
}

/**
 * @brief Returns the name of the current window
 * @return Active window name, else empty string
 * @sa MN_GetActiveWIndow
 */
const char* MN_GetActiveWindowName (void)
{
	const menuNode_t* window = MN_GetActiveWindow();
	if (window == NULL)
		return "";
	return window->name;
}

/**
 * @brief Check if a point is over a window from the stack
 * @sa IN_Parse
 */
qboolean MN_IsPointOnWindow (void)
{
	const menuNode_t *hovered;

	if (MN_GetMouseCapture() != NULL)
		return qtrue;

	if (mn.windowStackPos != 0) {
		if (WINDOWEXTRADATA(mn.windowStack[mn.windowStackPos - 1]).dropdown)
			return qtrue;
	}

	hovered = MN_GetHoveredNode();
	if (hovered) {
		/* else if it is a render node */
		if (hovered->root && hovered == WINDOWEXTRADATACONST(hovered->root).renderNode) {
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
 * @sa MN_GetActiveWindow
 */
menuNode_t *MN_GetWindow (const char *name)
{
	unsigned char min = 0;
	unsigned char max = mn.numWindows;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const char diff = strcmp(mn.windows[mid]->name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return mn.windows[mid];

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
void MN_InvalidateStack (void)
{
	int pos;
	for (pos = 0; pos < mn.windowStackPos; pos++) {
		MN_Invalidate(mn.windowStack[pos]);
	}
	Cvar_SetValue("mn_sys_screenwidth", viddef.virtualWidth);
	Cvar_SetValue("mn_sys_screenheight", viddef.virtualHeight);
}

/**
 * @brief Sets new x and y coordinates for a given window
 * @todo remove that
 */
void MN_SetNewWindowPos (menuNode_t* window, int x, int y)
{
	if (window)
		Vector2Set(window->pos, x, y);
}

/**
 * @brief Add a new window to the list of all windows
 * @note Sort windows by alphabet
 */
void MN_InsertWindow (menuNode_t* window)
{
	int pos = 0;
	int i;

	if (mn.numWindows >= MAX_WINDOWS)
		Com_Error(ERR_FATAL, "MN_InsertWindow: hit MAX_WINDOWS");

	/* search the insertion position */
	for (pos = 0; pos < mn.numWindows; pos++) {
		const menuNode_t* node = mn.windows[pos];
		if (strcmp(window->name, node->name) < 0)
			break;
	}

	/* create the space */
	for (i = mn.numWindows - 1; i >= pos; i--)
		mn.windows[i + 1] = mn.windows[i];

	/* insert */
	mn.windows[pos] = window;
	mn.numWindows++;
}

/**
 * @brief Console command for moving a window
 */
static void MN_SetNewWindowPos_f (void)
{
	menuNode_t* window = MN_GetActiveWindow();

	if (Cmd_Argc() < 3)
		Com_Printf("Usage: %s <x> <y>\n", Cmd_Argv(0));

	MN_SetNewWindowPos(window, atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)));
}

/**
 * @brief This will reinitialize the current visible window
 * @note also available as script command mn_reinit
 * @todo replace that by a common action "call *ufopedia.init"
 */
static void MN_FireInit_f (void)
{
	const menuNode_t* window;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <window>\n", Cmd_Argv(0));
		return;
	}

	window = MN_GetNodeByPath(Cmd_Argv(1));
	if (window == NULL) {
		Com_Printf("MN_FireInit_f: Node '%s' not found\n", Cmd_Argv(1));
		return;
	}

	if (!MN_NodeInstanceOf(window, "window")) {
		Com_Printf("MN_FireInit_f: Node '%s' is not a 'window'\n", Cmd_Argv(1));
		return;
	}

	/* initialize it */
	if (window) {
		if (WINDOWEXTRADATACONST(window).onInit)
			MN_ExecuteEventActions(window, WINDOWEXTRADATACONST(window).onInit);
		Com_DPrintf(DEBUG_CLIENT, "Reinitialize %s\n", window->name);
	}
}

static void MN_InitStack_f (void) {
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

	MN_InitStack(mainWindow, optionWindow, qtrue, qtrue);
}

/**
 * @brief Display in the conde the tree of nodes
 */
static void MN_DebugTree(const menuNode_t *node, int depth)
{
	const menuNode_t *child = node->firstChild;
	int i;

	for (i = 0; i < depth; i++) {
		Com_Printf("    ");
	}
	Com_Printf("+ %s %s\n", node->behaviour->name, node->name);

	while (child) {
		MN_DebugTree(child, depth + 1);
		child = child->next;
	}
}

static void MN_DebugTree_f (void)
{
	const char *window;
	const menuNode_t *node;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mainwindow>\n", Cmd_Argv(0));
		return;
	}

	window = Cmd_Argv(1);
	node = MN_GetWindow(window);
	MN_DebugTree(node, 0);
}

void MN_InitWindows (void)
{
	mn_sys_main = Cvar_Get("mn_sys_main", "", 0, "This is the main window id that is at the very first menu stack - also see mn_sys_active");
	mn_sys_active = Cvar_Get("mn_sys_active", "", 0, "The active window we will return to when hitting esc once - also see mn_sys_main");

	/* add command */
	Cmd_AddCommand("mn_fireinit", MN_FireInit_f, "Call the init function of a window");
	Cmd_AddCommand("mn_push", MN_PushWindow_f, "Push a window to the menustack");
	Cmd_AddParamCompleteFunction("mn_push", MN_CompleteWithWindow);
	Cmd_AddCommand("mn_push_dropdown", MN_PushDropDownWindow_f, "Push a dropdown window at a position");
	Cmd_AddCommand("mn_push_child", MN_PushChildWindow_f, "Push a window to the windowstack with a big dependancy to a parent window");
	Cmd_AddCommand("mn_pop", MN_PopWindow_f, "Pops the current window from the stack");
	Cmd_AddCommand("mn_close", MN_CloseWindow_f, "Close a window");
	Cmd_AddCommand("mn_move", MN_SetNewWindowPos_f, "Moves the window to a new position.");
	Cmd_AddCommand("mn_initstack", MN_InitStack_f, "Initialize the window stack with a main and an option window.");

	Cmd_AddCommand("mn_tree", MN_DebugTree_f, "Display a tree of nodes fropm a window into the console.");
	Cmd_AddParamCompleteFunction("mn_tree", MN_CompleteWithWindow);

	/** @todo move it outside */
	Cmd_AddCommand("hidehud", MN_PushNoHud_f, _("Hide the HUD (press ESC to reactivate HUD)"));
}
