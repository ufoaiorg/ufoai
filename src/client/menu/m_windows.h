/**
 * @file m_windows.h
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

#ifndef CLIENT_MENU_M_WINDOWS_H
#define CLIENT_MENU_M_WINDOWS_H

#include "../../common/common.h"

/* prototype */
struct menuNode_s;

/* module initialization */
void MN_InitWindows(void);

/* window stack */
int MN_GetLastFullScreenWindow(void);
struct menuNode_s* MN_PushWindow(const char *name, const char *parentName);
void MN_InitStack(const char* activeMenu, const char* mainMenu, qboolean popAll, qboolean pushActive);
void MN_PopWindow(qboolean all);
void MN_PopWindowWithEscKey(void);
void MN_CloseWindow(const char* name);
struct menuNode_s* MN_GetActiveWindow(void);
int MN_CompleteWithWindow(const char *partial, const char **match);
qboolean MN_IsWindowOnStack(const char* name);
qboolean MN_IsPointOnWindow(void);
void MN_InvalidateStack(void);
void MN_InsertWindow(struct menuNode_s* window);
void MN_MoveWindowOnTop (struct menuNode_s * window);

/* deprecated */
const char* MN_GetActiveWindowName(void);
void MN_GetActiveRenderRect(int *x, int *y, int *width, int *height);

/** @todo move it on m_nodes, its a common getter/setter */
void MN_SetNewWindowPos(struct menuNode_s* menu, int x, int y);
struct menuNode_s *MN_GetWindow(const char *name);

#endif
