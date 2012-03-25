/**
 * @file byte.c
 * @brief Byte order functions
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../common/common.h"

void Swap_Init (void)
{
	union {
		byte b[2];
		unsigned short s;
	} swaptest;

	Com_Printf("---- endianness initialization -----\n");

	swaptest.b[0] = 1;
	swaptest.b[1] = 0;

	/* set the byte swapping variables in a portable manner */
	if (swaptest.s == 1) {
		Com_Printf("found little endian system\n");
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		Sys_Error("SDL was compiled in big endian mode");
#endif
	} else {
		Com_Printf("found big endian system\n");
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		Sys_Error("SDL was compiled in little endian mode");
#endif
	}
}
