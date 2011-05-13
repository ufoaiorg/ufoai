/**
 * @file infostring.h
 * @brief Info string handling
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

#ifndef __INFOSTRING_H__
#define __INFOSTRING_H__

#include <stddef.h>
#include "ufotypes.h"

/* key / value info strings */
#define MAX_INFO_KEY        64
#define MAX_INFO_VALUE      64
#define MAX_INFO_STRING     512

const char *Info_ValueForKey(const char *s, const char *key);
const char *Info_BoolForKey(const char *s, const char *key);
int Info_IntegerForKey(const char *s, const char *key);
void Info_RemoveKey(char *s, const char *key);
void Info_SetValueForKey(char *s, const size_t size, const char *key, const char *value);
void Info_SetValueForKeyAsInteger(char *s, const size_t size, const char *key, const int value);
void Info_Print(const char *s);
qboolean Info_Validate(const char *s);

#endif /* __INFOSTRING_H__ */
