/**
 * @file input.h
 * @brief External (non-keyboard) input devices.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Added inclusion guard.

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

void IN_Init(void);

void IN_Shutdown(void);

void IN_Commands(void);

/* oportunity for devices to stick commands on the script buffer */

void IN_Frame(void);

void IN_GetMousePos(int *mx, int *my);

/* add additional movement on top of the keyboard move cmd */

void IN_Activate(qboolean active);

#endif /* CLIENT_INPUT_H */
