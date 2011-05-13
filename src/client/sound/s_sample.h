/**
 * @file s_sample.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/sound.h
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

#ifndef CLIENT_SOUND_SAMPLE_H
#define CLIENT_SOUND_SAMPLE_H

#include "s_local.h"		/* for s_sample_t */

void S_PrecacheSamples(void);
void S_FreeSamples(void);
s_sample_t *S_LoadSample(const char *s);
s_sample_t *S_GetSample (const int soundIdx);
void S_PlaySample(const vec3_t origin, s_sample_t* sample, float atten, float volume);

#endif
