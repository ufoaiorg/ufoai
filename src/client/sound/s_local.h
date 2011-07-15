/**
 * @file s_local.h
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

#ifndef CLIENT_SOUND_LOCAL_H
#define CLIENT_SOUND_LOCAL_H

#include <SDL_mixer.h>
#include "../../shared/shared.h"
#include "../../shared/mathlib.h"	/* for vec3_t */
#include "../../common/cvar.h"		/* for cvar_t */
#include "../../common/mem.h"

extern memPool_t *cl_soundSysPool;

/** @brief Supported sound file extensions */
#define SAMPLE_TYPES { "ogg", "wav", NULL }

typedef struct s_sample_s {
	char *name;
	int lastPlayed;		/**< used to determine whether this sample should be send to the mixer or skipped if played
						 * too fast after each other */
	Mix_Chunk* chunk;
	struct s_sample_s* hashNext;	/**< next hash entry */
	int index;			/** index in the array of samples */
} s_sample_t;

typedef struct s_channel_s {
	vec3_t org;  /**< for temporary entities and other positioned sounds */
	s_sample_t *sample;
	float atten;
	int count;
} s_channel_t;

/** @brief the sound environment */
#define MAX_CHANNELS 64

typedef struct s_env_s {
	vec3_t right;  /* for stereo panning */

	s_channel_t channels[MAX_CHANNELS];

	int sampleRepeatRate;	/**< milliseconds that must have passed to replay the same sample again */
	int rate;
	int numChannels;
	uint16_t format;

	qboolean initialized;
} s_env_t;

extern cvar_t *snd_volume;
extern cvar_t *snd_distance_scale;

extern s_env_t s_env;

#endif
