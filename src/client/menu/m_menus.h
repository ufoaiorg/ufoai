/**
 * @file m_menus.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

/* prototype */
struct menuNode_s;

void MN_InitMenus(void);

int MN_GetLastFullScreenWindow(void);
void MN_SetNewMenuPos(struct menuNode_s* menu, int x, int y);

void MN_SetNewMenuPos_f (void);

void MN_DragMenu(void);

struct menuNode_s* MN_PushMenu(const char *name, const char *parentName);
void MN_PopMenu(qboolean all);
void MN_CloseMenu(const char* name);
qboolean MN_MenuIsOnStack(const char* name);
struct menuNode_s* MN_GetActiveMenu(void);
const char* MN_GetActiveMenuName(void);

struct menuNode_s* MN_GetNodeFromCurrentMenu(const char *name);
qboolean MN_CursorOnMenu(int x, int y);
struct menuNode_s *MN_GetMenu(const char *name);

int MN_CompletePushMenu(const char *partial, const char **match);

#endif
