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

#include "ufotypes.h"

short BigShort(uint16_t l);
short LittleShort(uint16_t l);
int BigLong(uint32_t l);
int LittleLong(uint32_t l);
float BigFloat(float l);
float LittleFloat(float l);

void Swap_Init(void);

#endif /* __BYTE_H__ */
