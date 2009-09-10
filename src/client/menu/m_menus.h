/**
 * @file m_menus.h
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#ifndef CLIENT_MENU_M_MENUS_H
#define CLIENT_MENU_M_MENUS_H

#include "../../common/common.h"

/* prototype */
struct menuNode_s;

/* module initialization */
void MN_InitMenus(void);

/* menu stack */
int MN_GetLastFullScreenWindow(void);
struct menuNode_s* MN_PushMenu(const char *name, const char *parentName);
void MN_InitStack(char* activeMenu, char* mainMenu, qboolean popAll, qboolean pushActive);
void MN_PopMenu(qboolean all);
void MN_PopMenuWithEscKey(void);
void MN_CloseMenu(const char* name);
struct menuNode_s* MN_GetActiveMenu(void);
int MN_CompleteWithMenu(const char *partial, const char **match);
qboolean MN_IsMenuOnStack(const char* name);
qboolean MN_IsPointOnMenu(int x, int y);
void MN_InvalidateStack(void);
void MN_InsertMenu(struct menuNode_s* menu);

/* deprecated */
const char* MN_GetActiveMenuName(void);
void MN_GetActiveRenderRect(int *x, int *y, int *width, int *height);

/** @todo move it on m_nodes, its a common getter/setter */
void MN_SetNewMenuPos(struct menuNode_s* menu, int x, int y);
void MN_SetNewMenuPos_f (void);
struct menuNode_s *MN_GetMenu(const char *name);

#endif
