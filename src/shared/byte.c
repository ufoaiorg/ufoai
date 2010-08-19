/**
 * @file byte.c
 * @brief Byte order functions
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include <SDL/SDL_endian.h>
#include "../common/common.h"

/* can't just use function pointers, or dll linkage can */
/* mess up when common is included in multiple places */
static float (*_BigFloat)(float l);
static float (*_LittleFloat)(float l);

short BigShort (uint16_t l)
{
	return SDL_SwapBE16(l);
}
short LittleShort (uint16_t l)
{
	return SDL_SwapLE16(l);
}
int BigLong (uint32_t l)
{
	return SDL_SwapBE32(l);
}
int LittleLong (uint32_t l)
{
	return SDL_SwapLE32(l);
}
float BigFloat (float l)
{
	return _BigFloat(l);
}
float LittleFloat (float l)
{
	return _LittleFloat(l);
}

/**
 * @sa FloatNoSwap
 */
static float FloatSwap (float f)
{
	union float_u {
		float f;
		byte b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

/**
 * @sa FloatSwap
 */
static float FloatNoSwap (float f)
{
	return f;
}

void Swap_Init (void)
{
	Com_Printf("---- endianness initialization -----\n");
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	Com_Printf("found little endian system\n");
	_BigFloat = FloatSwap;
	_LittleFloat = FloatNoSwap;
#else
	Com_Printf("found big endian system\n");
	_BigFloat = FloatNoSwap;
	_LittleFloat = FloatSwap;
#endif
}
