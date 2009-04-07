/**
 * @file cl_sound.h
 * @brief Specifies sound API?
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_SOUND_H
#define CLIENT_SOUND_H

#include <SDL_mixer.h>

#define MAX_CHANNELS 64

/** @brief These sounds are precached in S_RegisterSounds */
enum {
	SOUND_WATER_IN,
	SOUND_WATER_OUT,
	SOUND_WATER_MOVE,

	MAX_SOUNDIDS
};

typedef struct sfx_s {
	char *name;
	int loops;					/**< how many loops - 0 = play only once, -1 = infinite */
	Mix_Chunk* chunk;
	struct sfx_s* hash_next;	/**< next hash entry */
} sfx_t;


typedef struct s_channel_s {
	vec3_t org;  // for temporary entities and other positioned sounds
	sfx_t *sample;
} s_channel_t;

/** @brief the sound environment */
typedef struct s_env_s {
	vec3_t right;  /* for stereo panning */

	s_channel_t channels[MAX_CHANNELS];

	int numChannels;

	qboolean initialized;
} s_env_t;

extern s_env_t s_env;

void S_Init(void);
void S_Shutdown(void);
void S_Frame(void);
void S_StopAllSounds(void);
void S_StartSound(const vec3_t origin, sfx_t* sfx, float relVolume);
void S_StartLocalSound(const char *s);
sfx_t *S_RegisterSound(const char *s);
int S_PlaySoundFromMem(const short* mem, size_t size, int rate, int channel, int ms);

void S_Music_Stop(void);

#endif /* CLIENT_SOUND_H */
