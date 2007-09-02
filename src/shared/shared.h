/**
 * @file shared.h
 * @brief Info string handling
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef __SHARED_H__
#define __SHARED_H__

/* important units */
#define UNIT_SIZE 32
#define UNIT_HEIGHT 64

/** @brief Map boundary is +/- 4096 - to get into the positive area we
 * add the possible max negative value and divide by the size of a grid unit field */
#define VecToPos(v,p)  (p[0]=(((int)v[0]+4096)/UNIT_SIZE), p[1]=(((int)v[1]+4096)/UNIT_SIZE), p[2]=((int)v[2]/UNIT_HEIGHT))
/** @brief Pos boundary size is +/- 128 - to get into the positive area we add
 * the possible max negative value and multiply with the grid unit size to get
 * back the the vector coordinates - now go into the middle of the grid field
 * by adding the half of the grid unit size to this value */
#define PosToVec(p,v)  (v[0]=((int)p[0]-128)*UNIT_SIZE+UNIT_SIZE/2, v[1]=((int)p[1]-128)*UNIT_SIZE+UNIT_SIZE/2, v[2]=(int)p[2]*UNIT_HEIGHT+UNIT_HEIGHT/2)

const char *COM_SkipPath(char *pathname);
void COM_StripExtension(const char *in, char *out);
void COM_FileBase(const char *in, char *out);
void COM_FilePath(const char *in, char *out);

const char *COM_Parse(const char **data_p);

#endif /* __SHARED_H__ */
