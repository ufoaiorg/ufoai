/**
 * @file common.h
 */

/*
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

#ifndef _QCOMMON_MEM_H
#define _QCOMMON_MEM_H

void Mem_Free(void *ptr);
void *Mem_Alloc(size_t size);		/* returns 0 filled memory */
void *Mem_TagMalloc(size_t size, int tag);
void Mem_FreeTags(int tag);
void Mem_Init(void);
void Mem_RegisterCommands(void);

#endif /* _QCOMMON_MEM_H */
