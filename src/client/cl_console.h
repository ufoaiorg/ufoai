/**
 * @file cl_console.h
 * @brief Console header file.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/console.h
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

#ifndef CLIENT_CL_CONSOLE_H
#define CLIENT_CL_CONSOLE_H

void Con_DrawString(const char *text, int x, int y, unsigned int width);
void Con_Scroll(int scroll);
void Con_CheckResize(void);
void Con_Init(void);
void Con_DrawConsole(float frac);
void Con_Print(const char *txt);
void Con_ToggleConsole_f(void);
void Con_Close(void);

void Con_SaveConsoleHistory(void);
void Con_LoadConsoleHistory(void);

#define CONSOLE_PROMPT_CHAR ']'

extern const int con_fontHeight;
extern const int con_fontWidth;
extern const int con_fontShift;

#endif /* CLIENT_CL_CONSOLE_H */
