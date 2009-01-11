/**
 * @file cl_sound.h
 * @brief Specifies sound API?
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

/** @brief only begin attenuating sound volumes when outside the FULLVOLUME range */
#define SOUND_FULLVOLUME 100
/** @brief A sound is only hearable when not farer than this value */
#define SOUND_MAX_DISTANCE 600

enum {
	SOUND_CHANNEL_OVERRIDE,	/**< entchannel 0 is always overwritten */
	SOUND_CHANNEL_WEAPON,	/**< shooting sound */
	SOUND_CHANNEL_ACTOR,	/**< actor die sound, footsteps */
	SOUND_CHANNEL_AMBIENT
};

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
	Mix_Chunk* data;
	int channel;				/**< the channel the sfx is played on */
	int volume;					/**< current volume for this chunk */
	struct sfx_s* hash_next;	/**< next hash entry */
} sfx_t;

typedef struct music_s {
	char currentlyPlaying[MAX_QPATH];
	Mix_Music *data;
	SDL_RWops *musicSrc;		/**< freed by SDL_mixer */
	char *nextMusicTrack;
} music_t;

void S_Init(void);
void S_Shutdown(void);
void S_Frame(void);
void S_SetVolume(sfx_t *sfx, int volume);
void S_StopAllSounds(void);
qboolean S_Playing(const sfx_t* sfx);
void S_StopSound(const sfx_t* sfx);
void S_StartSound(const vec3_t origin, sfx_t* sfx, float relVolume);
void S_StartLocalSound(const char *s);
sfx_t *S_RegisterSound(const char *s);
int S_PlaySoundFromMem(const short* mem, size_t size, int rate, int channel, int ms);
void CL_ParseMusic(const char *name, const char **text);

void S_Music_Stop(void);

#endif /* CLIENT_SOUND_H */
