/**
 * @file ui_windows.h
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

#ifndef CLIENT_UI_UI_WINDOWS_H
#define CLIENT_UI_UI_WINDOWS_H

#include "../../common/common.h"

/* prototype */
struct uiNode_s;
struct uiCallContext_s;

/* module initialization */
void UI_InitWindows(void);

/* window stack */
int UI_GetLastFullScreenWindow(void);
struct uiNode_s* UI_PushWindow(const char *name, const char *parentName, linkedList_t *params);
void UI_InitStack(const char* activeWindow, const char* mainWindow, qboolean popAll, qboolean pushActive);
void UI_PopWindow(qboolean all);
void UI_PopWindowWithEscKey(void);
void UI_CloseWindow(const char* name);
struct uiNode_s* UI_GetActiveWindow(void);
int UI_CompleteWithWindow(const char *partial, const char **match);
qboolean UI_IsWindowOnStack(const char* name);
qboolean UI_IsMouseOnWindow(void);
void UI_InvalidateStack(void);
void UI_InsertWindow(struct uiNode_s* window);
void UI_MoveWindowOnTop (struct uiNode_s * window);

/* deprecated */
const char* UI_GetActiveWindowName(void);

/** @todo move it on m_nodes, its a common getter/setter */
void UI_SetNewWindowPos(struct uiNode_s* window, int x, int y);
struct uiNode_s *UI_GetWindow(const char *name);

#endif
