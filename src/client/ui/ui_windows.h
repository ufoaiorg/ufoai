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

#pragma once

#include "../../common/common.h"

/* prototype */
struct uiNode_t;
struct uiCallContext_s;

/* module initialization */
void UI_InitWindows(void);

/* window stack */
int UI_GetLastFullScreenWindow(void);
uiNode_t* UI_PushWindow(const char* name, const char* parentName = nullptr, linkedList_t* params = nullptr);
void UI_InitStack(const char* activeWindow, const char* mainWindow);
void UI_PopWindow(bool all = false);
void UI_PopWindowWithEscKey(void);
void UI_CloseWindow(const char* name);
uiNode_t* UI_GetActiveWindow(void);
int UI_CompleteWithWindow(const char* partial, const char** match);
bool UI_IsWindowOnStack(const char* name);
bool UI_IsMouseOnWindow(void);
void UI_InvalidateStack(void);
void UI_InsertWindow(uiNode_t* window);
void UI_MoveWindowOnTop(uiNode_t* window);

/* deprecated */
const char* UI_GetActiveWindowName(void);

/** @todo move it on m_nodes, its a common getter/setter */
void UI_SetNewWindowPos(uiNode_t* window, int x, int y);
uiNode_t* UI_GetWindow(const char* name);
