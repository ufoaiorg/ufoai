/**
 * @file snd_ogg.h
 * @brief Header file for OGG vorbis sound code
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/snd_dma.c
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

#if defined (__linux__) || defined (__FreeBSD__) || defined(__NetBSD__)
#include <vorbis/vorbisfile.h>
#else
#include "../ports/win32/vorbisfile.h"
#endif

typedef struct music_s {
	OggVorbis_File ovFile; /**< currently playing ogg vorbis file */
	char newFilename[MAX_QPATH]; /**< after fading out ovFile play newFilename */
	char ovBuf[4096]; /**< ogg vorbis buffer */
	int ovSection; /**< number of the current logical bitstream */
	float fading; /**< current volumn - if le zero play newFilename */
	char ovPlaying[MAX_QPATH]; /**< currently playing ogg tracks basename */
	int rate;
	unsigned format;
} music_t;

extern music_t music;

void S_OGG_Play(void);
void S_OGG_Start(void);
qboolean S_OGG_Open(const char *filename);
void S_OGG_Stop(void);
int S_OGG_Read(void);
void S_OGG_Info(void);
void S_OGG_Shutdown(void);
void S_OGG_Init(void);
void S_OGG_List_f(void);
void S_OGG_RandomTrack_f(void);
sfxcache_t *S_OGG_LoadSFX(sfx_t *s);

extern cvar_t *snd_music_volume;
extern cvar_t *snd_music_loop;
