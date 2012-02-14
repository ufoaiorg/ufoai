/**
 * @file cl_input.h
 * @brief External (non-keyboard) input devices.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/
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

#ifndef CLIENT_INPUT_H
#define CLIENT_INPUT_H

typedef enum {
	MS_NULL,
	MS_UI,			/**< we are over some menu node */
	MS_WORLD,		/**< we are in tactical mode */

	MS_MAX
} mouseSpace_t;

typedef enum {
	key_game,		/**< input focus is on the menu (or the keyBindings) */
	key_console		/**< console is opened */
} keydest_t;

#define STATE_FORWARD	1
#define STATE_RIGHT		2
#define STATE_ZOOM		3
#define STATE_ROT		4
#define STATE_TILT		5

extern mouseSpace_t mouseSpace;
extern int mousePosX, mousePosY;

#define IN_GetMouseSpace() mouseSpace

void IN_Init(void);
void IN_Frame(void);
void IN_SendKeyEvents(void);
void IN_SetMouseSpace(mouseSpace_t mouseSpace);

void IN_EventEnqueue(unsigned int key, unsigned short, qboolean down);
float CL_GetKeyMouseState(int dir);

#endif /* CLIENT_INPUT_H */
