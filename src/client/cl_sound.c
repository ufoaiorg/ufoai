/**
 * @file snd_main.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_sound.h"

#ifdef _WIN32
# include "../ports/windows/win_local.h"
#endif

/*
=======================================================================
Internal sound data & structures
=======================================================================
*/

#define SFX_HASH_SIZE 64
static sfx_t *sfx_hash[SFX_HASH_SIZE];

static music_t music;

static qboolean sound_started = qfalse;

static cvar_t *snd_volume;
static cvar_t *snd_init;
static cvar_t *snd_bits;
static cvar_t *snd_channels;
static cvar_t *snd_rate;
static cvar_t *snd_music;
static cvar_t *snd_music_volume;

/*
MUSIC FUNCTIONS
*/

/**
 * @brief
 * @sa S_Music_Start
 */
void S_Music_Stop (void)
{
	if (music.data != NULL) {
		Mix_HaltMusic();
		Mix_FreeMusic(music.data);
		music.data = NULL;
/*		SDL_RWclose(music.musicSrc);*/
		music.musicSrc = NULL;
	}
}

/**
 * @brief
 * @sa S_Music_Stop
 */
void S_Music_Start (const char *file)
{
	char name[MAX_QPATH];
	size_t len;
	byte *musicBuf;
	int size;

	if (!file || !*file)
		return;
	/*Mix_FadeOutMusic();*/

	if (!sound_started) {
		Com_Printf("S_Music_Start: no sound started\n");
		return;
	}

	Q_strncpyz(name, file, sizeof(name));
	len = strlen(name);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_Music_Start: MAX_QPATH exceeded: "UFO_SIZE_T"\n", len + 4);
		return;
	}

	/* we are already playing that track */
	if (!Q_strcmp(name, music.currentlyPlaying))
		return;

	S_Music_Stop();

	/* load it in */
	if ((size = FS_LoadFile(va("music/%s.ogg", name), (byte **)&musicBuf)) != -1) {
		music.musicSrc = SDL_RWFromMem(musicBuf, size);
	} else if ((size = FS_LoadFile(va("music/%s.mp3", name), (byte **)&musicBuf)) != -1) {
		music.musicSrc = SDL_RWFromMem(musicBuf, size);
	} else {
		Com_Printf("Could not load %s background track\n", name);
		return;
	}
	music.data = Mix_LoadMUS_RW(music.musicSrc);

	if (music.data) {
		Com_Printf("S_Music_Start: Playing music: 'music/%s.ogg'\n", file);
		Q_strncpyz(music.currentlyPlaying, name, sizeof(music.currentlyPlaying));
		if (Mix_PlayMusic(music.data, 1500) == -1)
			Com_Printf("S_Music_Start: Could not play music: 'music/%s.ogg'\n", file);
	} else {
		Com_Printf("S_Music_Start: Could not load music: 'music/%s.ogg'\n", file);
	}
}

/**
 * @brief Plays the music file given via commandline parameter
 */
static void S_Music_Play_f (void)
{
	if (Cmd_Argc() == 2) {
		if (Mix_PlayingMusic()) {
			Mix_FadeOutMusic(3000);
			music.nextMusicTrack = Mem_PoolStrDup(Cmd_Argv(1), cl_soundSysPool, CL_TAG_NONE);
		} else {
			S_Music_Start(Cmd_Argv(1));
		}
	}
}

static int musicTrackCount = 0;

/**
 * @brief Sets the music cvar to a random track
 */
static void S_Music_RandomTrack_f (void)
{
	char findname[MAX_OSPATH];
	int i, randomID;
	char **dirnames;
	const char *musicTrack;
	int ndirs;
	searchpath_t *search;
	pack_t *pak;
	int count = 0;

	if (musicTrackCount == 0) {
		/* search through the path, one element at a time */
		for (search = fs_searchpaths; search; search = search->next) {
			/* is the element a pak file? */
			if (search->pack) {
				/* look through all the pak file elements */
				pak = search->pack;
				for (i = 0; i < pak->numfiles; i++)
					if (strstr(pak->files[i].name, ".ogg"))
						musicTrackCount++;
			} else {
				Com_sprintf(findname, sizeof(findname), "%s/music/*.ogg", search->filename);
				FS_NormPath(findname);

				if ((dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM)) != 0) {
					for (i = 0; i < ndirs - 1; i++) {
						musicTrackCount++;
						Mem_Free(dirnames[i]);
					}
					Mem_Free(dirnames);
				}
			}
		}
	}

	randomID = rand() % musicTrackCount;
	Com_DPrintf(DEBUG_SOUND, "S_Music_RandomTrack_f: random track id: %i/%i\n", randomID, musicTrackCount);

	/* search through the path, one element at a time */
	for (search = fs_searchpaths; search; search = search->next) {
		/* is the element a pak file? */
		if (search->pack) {
			/* look through all the pak file elements */
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (strstr(pak->files[i].name, ".ogg")) {
					count++;
					if (randomID == count) {
						Com_sprintf(findname, sizeof(findname), "%s", pak->files[i].name);
						musicTrack = COM_SkipPath(findname);
						Com_Printf("..playing next: '%s'\n", musicTrack);
						Cvar_Set("snd_music", musicTrack);
					}
				}
		} else {
			Com_sprintf(findname, sizeof(findname), "%s/music/*.ogg", search->filename);
			FS_NormPath(findname);

			if ((dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM)) != 0) {
				for (i = 0; i < ndirs - 1; i++) {
					count++;
					if (randomID == count) {
						musicTrack = COM_SkipPath(dirnames[i]);
						Com_Printf("..playing next: '%s'\n", musicTrack);
						Cvar_Set("snd_music", musicTrack);
					}
					Mem_Free(dirnames[i]);
				}
				Mem_Free(dirnames);
			}
		}
	}
}

/*
SOUND FILES
*/

/**
 * @brief
 * @sa S_RegisterSound
 */
static Mix_Chunk *S_LoadSound (const char *sound)
{
	char name[MAX_QPATH];
	size_t len;
	SDL_RWops *src;
	byte *sfxBuf;
	int size;
	Mix_Chunk *mix = NULL;

	if (!sound || *sound == '*')
		return NULL;

	Q_strncpyz(name, sound, sizeof(name));
	len = strlen(name);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_LoadSound: MAX_QPATH exceeded: "UFO_SIZE_T"\n", len + 4);
		return NULL;
	}

	/* load it in */
	if ((size = FS_LoadFile(va("sound/%s.ogg", name), (byte **)&sfxBuf)) != -1) {
		src = SDL_RWFromMem(sfxBuf, size);
		mix = Mix_LoadWAV_RW(src, 1);
	} else if ((size = FS_LoadFile(va("sound/%s.wav", name), (byte **)&sfxBuf)) != -1) {
		src = SDL_RWFromMem(sfxBuf, size);
		mix = Mix_LoadWAV_RW(src, 1);
	}
	return mix;
}

/**
 * @brief
 * @sa S_LoadSound
 */
sfx_t *S_RegisterSound (const char *name)
{
	Mix_Chunk *mix;
	sfx_t *sfx;
	unsigned hash;

	if (!sound_started)
		return NULL;

	hash = Com_HashKey(name, SFX_HASH_SIZE);
	for (sfx = sfx_hash[hash]; sfx; sfx = sfx->hash_next)
		if (!Q_stricmp(name, sfx->name))
			return sfx;

	/* make sure the sound is loaded */
	mix = S_LoadSound(name);
	if (!mix)
		return NULL;		/* couldn't load the sound's data */

	sfx = Mem_PoolAlloc(sizeof(sfx_t), cl_soundSysPool, CL_TAG_NONE);
	sfx->name = Mem_PoolStrDup(name, cl_soundSysPool, CL_TAG_NONE);
	sfx->data = mix;
	sfx->channel = -1; /* just a free channel */
	sfx->loops = 0; /* play once */
	sfx->hash_next = sfx_hash[hash];
	sfx_hash[hash] = sfx;
	return sfx;
}

/**
 * @brief Validates the parms and ques the sound up
 * @param[in] origin if is NULL, the sound will be dynamically sourced from the entity
 * @param[in] name The soundfile to play
 * @param[in] relVolume - 0 - 1
 * @param[in] attenuation the reduction in amplitude and intensity of a signal
 * @sa S_StartLocalSound
 */
void S_StartSound (const vec3_t origin, sfx_t* sfx, float relVolume, float attenuation)
{
	int volume = MIX_MAX_VOLUME * relVolume;

	if (!sound_started) {
		Com_DPrintf(DEBUG_SOUND, "S_StartSound: no sound started\n");
		return;
	}

	sfx->channel = Mix_PlayChannel(sfx->channel, sfx->data, sfx->loops);
#ifdef DEBUG
	if (volume > MIX_MAX_VOLUME) {
		Com_DPrintf(DEBUG_SOUND, "Mixer volume is too high: %i - max value is %i\n", MIX_MAX_VOLUME, volume);
		volume = MIX_MAX_VOLUME;
	}
#endif
	Mix_VolumeChunk(sfx->data, volume);
	if (origin) {
		le_t* le = LE_GetClosestActor(origin);
		if (le)
			Mix_SetDistance(sfx->channel, VectorDist(origin, le->origin));
	}
	Com_DPrintf(DEBUG_SOUND, "Mix '%s' (volume: %i, channel: %i)\n", sfx->name, volume, sfx->channel);
}

/**
 * @sa S_StartSound
 */
void S_StartLocalSound (const char *sound)
{
	sfx_t *sfx;

	if (!sound_started) {
		Com_DPrintf(DEBUG_SOUND, "S_StartLocalSound: no sound started\n");
		return;
	}

	sfx = S_RegisterSound(sound);
	if (!sfx) {
		Com_DPrintf(DEBUG_SOUND, "S_StartLocalSound: can't load %s\n", sound);
		return;
	}
	S_StartSound(NULL, sfx, DEFAULT_SOUND_PACKET_VOLUME, DEFAULT_SOUND_PACKET_ATTENUATION);
}

/**
 * @brief Stop all channels
 */
void S_StopAllSounds (void)
{
	Mix_HaltChannel(-1);
}

/**
 * @brief Plays a sound fx file via console
 */
static void S_Play_f (void)
{
	const char *filename;

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	filename = Cmd_Argv(1);
	S_StartLocalSound(filename);
}

/*
INIT AND SHUTDOWN
*/

/**
 * @sa CL_Frame
 */
void S_Frame (void)
{
	if (snd_music->modified) {
		S_Music_Start(snd_music->string);
		snd_music->modified = qfalse;
	}
	if (snd_music_volume->modified) {
		Mix_VolumeMusic(snd_music_volume->integer);
		snd_music_volume->modified = qfalse;
	}
	if (music.nextMusicTrack) {
		if (!Mix_PlayingMusic()) {
			S_Music_Start(music.nextMusicTrack);
			Mem_Free(music.nextMusicTrack);
			music.nextMusicTrack = NULL;
		}
	}
}

/**
 * @sa S_Shutdown
 * @sa S_Init
 */
static qboolean SND_Init (void)
{
	int audio_rate = snd_rate->integer;
	Uint16 audio_format = MIX_DEFAULT_FORMAT; /* 16-bit stereo */
	int audio_channels = snd_channels->integer;
	int audio_buffers = 1024;
	SDL_version version;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		if (SDL_Init(SDL_INIT_AUDIO) == -1) {
			Com_Printf("Couldn't init SDL audio: %s\n", SDL_GetError());
			return qfalse;
		}
	} else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) == -1) {
			Com_Printf("Couldn't init SDL audio subsystem: %s\n", SDL_GetError());
			return qfalse;
		}
	}

	MIX_VERSION(&version);
	Com_Printf("SDL_mixer version: %d.%d.%d\n", version.major, version.minor, version.patch);

	if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) == -1) {
		Com_Printf("Unable to open audio\n");
		return qfalse;
	}

	Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
	Com_Printf("...audio rate: %i\n...audio channels: %i\n", audio_rate, audio_channels);

	return qtrue;
}

/**
 * @sa S_Shutdown
 * @sa CL_Snd_Restart_f
 * @todo: openal should be an replacement for the 'normal' snd_ref
 */
void S_Init (void)
{
	Com_Printf("\n------- sound initialization -------\n");

	snd_init = Cvar_Get("snd_init", "1", CVAR_ARCHIVE, "Should the sound renderer get initialized");

	if (!snd_init->integer) {
		Com_Printf("not initializing.\n");
		return;
	}

	snd_bits = Cvar_Get("snd_bits", "16", CVAR_ARCHIVE, "Sound bits");
	snd_channels = Cvar_Get("snd_channels", "2", CVAR_ARCHIVE, "Sound channels");
	snd_volume = Cvar_Get("snd_volume", "128", CVAR_ARCHIVE, "Sound volume - default is 128");
	snd_rate = Cvar_Get("snd_rate", "44100", CVAR_ARCHIVE, "Hz value for sound renderer - default is 44100");
	snd_music = Cvar_Get("snd_music", "PsymongN3", 0, "Background music track");
	snd_music_volume = Cvar_Get("snd_music_volume", "128", CVAR_ARCHIVE, "Music volume - default is 128");

	if (!SND_Init()) {
		Com_Printf("SND_Init failed\n");
		sound_started = qfalse;
		return;
	}

	Cmd_AddCommand("snd_play", S_Play_f, "Plays a sound fx file");
	Cmd_AddCommand("music_play", S_Music_Play_f, "Plays a background sound track");
	Cmd_AddCommand("music_start", S_Music_Play_f, "Starts the background music track from cvar music");
	Cmd_AddCommand("music_stop", S_Music_Stop, "Stops currently playing music tracks");
	Cmd_AddCommand("music_randomtrack", S_Music_RandomTrack_f, "Plays a random background track");
	/* @todo: Complete functions */

	sound_started = qtrue;

	memset(&music, 0, sizeof(music_t));
}

/**
 * @sa S_Init
 * @sa CL_Snd_Restart_f
 */
void S_Shutdown (void)
{
	if (!sound_started)
		return;

	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Mem_FreePool(cl_soundSysPool);
	sound_started = qfalse;
}
