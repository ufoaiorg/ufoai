/**
 * @file s_music.h
 * @brief Specifies music API
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

#ifndef CLIENT_SOUND_MUSIC_H
#define CLIENT_SOUND_MUSIC_H

#include "../../shared/ufotypes.h"	/* for qboolean & byte */

/* 2 channels and a width of 2 bytes (short) */
#define SAMPLE_SIZE 4
#define MAX_RAW_SAMPLES 4096 * SAMPLE_SIZE

typedef struct musicStream_s {
	qboolean playing;
	byte sampleBuf[MAX_RAW_SAMPLES];
	int mixerPos;
	int samplePos;
} musicStream_t;

void M_PlayMusicStream(musicStream_t *userdata);
void M_AddToSampleBuffer(musicStream_t *userdata, int rate, int samples, const byte *data);
void M_StopMusicStream(musicStream_t *userdata);
void M_ParseMusic(const char *name, const char **text);
void M_Frame(void);
void M_Init(void);
void M_Shutdown(void);
void M_Stop(void);

#endif
