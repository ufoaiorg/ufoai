/**
 * @file byte.h
 * @brief Byte order functions header
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

#ifndef __BYTE_H__
#define __BYTE_H__

#include <SDL/SDL_endian.h>
#include "ufotypes.h"

#define BigShort(X) (short)SDL_SwapBE16(X)
#define LittleShort(X) (short)SDL_SwapLE16(X)
#define BigLong(X) (int)SDL_SwapBE32(X)
#define LittleLong(X) (int)SDL_SwapLE32(X)

typedef union {
	float32_t f;
	int32_t i;
	uint32_t ui;
} floatint_t;

static inline float32_t FloatSwap (const float32_t *f)
{
	floatint_t out;

	out.f = *f;
	out.ui = SDL_SwapLE32(out.ui);

	return out.f;
}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define BigFloat(X) FloatSwap(&(X))
#define LittleFloat(X) (X)
#else
#define BigFloat(X) (X)
#define LittleFloat(X) FloatSwap(&(X))
#endif

void Swap_Init(void);

#endif /* __BYTE_H__ */
