/**
 * @file s_main.h
 * @brief Specifies sound API?
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_SOUND_MAIN_H
#define CLIENT_SOUND_MAIN_H

#include <SDL_mixer.h>
#include "../../shared/mathlib.h"	/* for vec3_t */

#define MAX_CHANNELS 64

/** @brief These sounds are precached in S_LoadSamples */
typedef enum {
	SOUND_WATER_IN,
	SOUND_WATER_OUT,
	SOUND_WATER_MOVE,

	MAX_SOUNDIDS
} stdsound_t;

typedef struct s_sample_s {
	char *name;
	int lastPlayed;		/**< used to determine whether this sample should be send to the mixer or skipped if played
						 * too fast after each other */
	Mix_Chunk* chunk;
	struct s_sample_s* hashNext;	/**< next hash entry */
} s_sample_t;

typedef struct s_channel_s {
	vec3_t org;  /**< for temporary entities and other positioned sounds */
	s_sample_t *sample;
	float atten;
	int count;
} s_channel_t;

/** @brief the sound environment */
typedef struct s_env_s {
	vec3_t right;  /* for stereo panning */

	s_channel_t channels[MAX_CHANNELS];

	int sampleRepeatRate;	/**< milliseconds that must have passed to replay the same sample again */
	int rate;
	int numChannels;
	uint16_t format;

	qboolean initialized;
} s_env_t;

#define SND_VOLUME_DEFAULT 1.0f
#define SND_VOLUME_WEAPONS 1.0f

void S_Init(void);
void S_Shutdown(void);
void S_Frame(void);
void S_Stop(void);
void S_PlayStdSample(const stdsound_t sId, const vec3_t origin, float atten, float volume);
void S_PlaySample(const vec3_t origin, s_sample_t* sample, float atten, float volume);
void S_StartLocalSample(const char *s, float volume);
s_sample_t *S_LoadSample(const char *s);
qboolean S_LoadAndPlaySample(const char *s, const vec3_t origin, float atten, float volume);

#endif
