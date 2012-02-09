/**
 * @file cl_screen.h
 * @brief Header for certain screen operations.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/screen.h
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef CLIENT_SCREEN_H
#define CLIENT_SCREEN_H

void SCR_Init(void);
void SCR_ChangeCursor(int cursor);
void SCR_UpdateScreen(void);
void SCR_DrawLoading(int percent, const char *loadingMessages);
void SCR_BeginLoadingPlaque(void);
void SCR_EndLoadingPlaque(void);
void SCR_RunConsole(void);
void SCR_DrawLoadingScreen(qboolean string, int percent);

#endif /* CLIENT_SCREEN_H */
