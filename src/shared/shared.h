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

const char *COM_SkipPath(char *pathname);
void COM_StripExtension(const char *in, char *out);
void COM_FileBase(const char *in, char *out);
void COM_FilePath(const char *in, char *out);

const char *COM_Parse(const char **data_p);

#endif /* __SHARED_H__ */
