/**
 * @file screen.h
 * @brief Header for certain screen operations.
 *
 * TODO: Work out what this is for.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Added inclusion guard.

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

void SCR_UpdateScreen(void);

void SCR_SizeUp(void);
void SCR_SizeDown(void);
void SCR_CenterPrint(char *str);
void SCR_BeginLoadingPlaque(void);
void SCR_EndLoadingPlaque(void);

void SCR_DebugGraph(float value, int color);

void SCR_TouchPics(void);

void SCR_RunConsole(void);

extern float scr_con_current;
extern float scr_conlines;		/* lines of console to display */

extern int sb_lines;

extern cvar_t *scr_viewsize;
extern cvar_t *cursor;

extern vrect_t scr_vrect;		/* position of render window */

extern char cursor_pic[MAX_QPATH];

void SCR_AddDirtyPoint(int x, int y);
void SCR_DirtyScreen(void);

#endif /* CLIENT_SCREEN_H */
