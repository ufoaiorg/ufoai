/**
 * @file snd_openal.h
 * @brief Header for openAL
 * @sa snd_openal.c
 * @note To activate openAL in UFO:AI you have to set the snd_openal cvar to 1
 */

/*
Copyright (C) 1997-2001 UFO:AI team

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
#ifndef __SND_OPENAL_H__
#define __SND_OPENAL_H__

#include "client.h"
#include "snd_loc.h"

qboolean SND_OAL_Init (char* device);
qboolean SND_OAL_LoadSound (char* filename, qboolean looping);
void SND_OAL_PlaySound (void);
void SND_OAL_StopSound (void);
void SND_OAL_DestroySound (void);
qboolean SND_OAL_Stream (char* filename);

#endif /* SND_OPENAL_H */
