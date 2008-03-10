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
static cvar_t *snd_channels;
static cvar_t *snd_rate;
static cvar_t *snd_music;
static cvar_t *snd_music_volume;

static int audio_rate, audio_channels;
uint16_t audio_format;

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
		Q_strncpyz(music.currentlyPlaying, "", sizeof(music.currentlyPlaying));
	}
}

/**
 * @brief
 * @sa S_Music_Stop
 */
static void S_Music_Start (const char *file)
{
	char name[MAX_QPATH];
	size_t len;
	byte *musicBuf;
	int size;

	if (!sound_started)
		return;

	if (!file || !*file)
		return;

	if (!sound_started) {
		Com_Printf("S_Music_Start: no sound started\n");
		return;
	}

	COM_StripExtension(file, name, sizeof(name));
	len = strlen(name);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_Music_Start: MAX_QPATH exceeded: "UFO_SIZE_T"\n", len + 4);
		return;
	}

	/* we are already playing that track */
	if (!Q_strcmp(name, music.currentlyPlaying))
		return;

	/* we are still playing some background track - fade it out @sa S_Frame */
	if (Mix_PlayingMusic()) {
		Mix_FadeOutMusic(1500);
		if (music.nextMusicTrack)
			Mem_Free(music.nextMusicTrack);
		music.currentlyPlaying[0] = 0;
		music.nextMusicTrack = Mem_PoolStrDup(name, cl_soundSysPool, CL_TAG_NONE);
		return;
	}

	/* load it in */
	if ((size = FS_LoadFile(va("music/%s.ogg", name), (byte **)&musicBuf)) != -1) {
		music.musicSrc = SDL_RWFromMem(musicBuf, size);
	} else if ((size = FS_LoadFile(va("music/%s.mp3", name), (byte **)&musicBuf)) != -1) {
		music.musicSrc = SDL_RWFromMem(musicBuf, size);
	} else {
		Com_Printf("Could not load '%s' background track\n", name);
		return;
	}
	music.data = Mix_LoadMUS_RW(music.musicSrc);

	if (music.data) {
		Com_Printf("S_Music_Start: Playing music: 'music/%s'\n", name);
		Q_strncpyz(music.currentlyPlaying, name, sizeof(music.currentlyPlaying));
		if (Mix_FadeInMusic(music.data, -1, 1500) == -1)
			Com_Printf("S_Music_Start: Could not play music: 'music/%s' (%s)\n", name, Mix_GetError());
	} else {
		Com_Printf("S_Music_Start: Could not load music: 'music/%s' (%s)\n", name, Mix_GetError());
	}
}

/**
 * @brief Plays the music file given via commandline parameter
 * @todo Is it safe to simply have this as a wrapper for S_Music_Start() ?
 */
static void S_Music_Play_f (void)
{
/* @todo the chunk below would make S_Music_Play_f() a wrapper for S_Music_Start()
	if (Cmd_Argc() == 2)
		Cvar_Set("snd_music", Cmd_Argv(1));

	S_Music_Start(Cvar_VariableString("snd_music"));
*/
	if (!sound_started) {
		Com_Printf("No audio activated\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		S_Music_Start(Cmd_Argv(1));
	} else
		snd_music->modified = qtrue;
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

	if (!sound_started)
		return;

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

#define WAVE_HEADER_SIZE 44
/**
 * @brief Loads a buffer from memory into the mixer
 * @param[in] mem 16 byte (short) buffer with data
 */
void S_PlaySoundFromMem (short* mem, size_t size, int rate, int channel, int ms)
{
	SDL_AudioCVT wavecvt;
	Mix_Chunk sample;
	static int memChannel = -1;
	const int samplesize = 2 * channel;

	if (!sound_started)
		return;

	/* Build the audio converter and create conversion buffers */
	if (SDL_BuildAudioCVT(&wavecvt, AUDIO_S16, channel, rate,
			audio_format, audio_channels, audio_rate) < 0) {
		return;
	}

	wavecvt.len = size & ~(samplesize - 1);
	wavecvt.buf = (byte *)Mem_PoolAlloc(wavecvt.len * wavecvt.len_mult, cl_soundSysPool, CL_TAG_NONE);
	if (wavecvt.buf == NULL)
		return;
	memcpy(wavecvt.buf, mem, size);

	/* Run the audio converter */
	if (SDL_ConvertAudio(&wavecvt) < 0) {
		Mem_Free(wavecvt.buf);
		return;
	}

	sample.allocated = 0;
	sample.abuf = wavecvt.buf;
	sample.alen = wavecvt.len_cvt;
	sample.volume = MIX_MAX_VOLUME;

	if (Mix_Playing(memChannel))
		Mix_HaltChannel(memChannel);

	/* FIXME: this is a nasty hack - no real mixing is performed and sound quality is still
	 * very bad with this one */
	memChannel = Mix_PlayChannelTimed(memChannel, &sample, 1, ms);
	Mem_Free(sample.abuf);
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

	/* maybe the sound file couldn't be loaded */
	if (!sfx)
		return;

#ifdef DEBUG
	if (volume > MIX_MAX_VOLUME) {
		Com_DPrintf(DEBUG_SOUND, "S_StartSound: Mixer volume is too high: %i - max value is %i\n", MIX_MAX_VOLUME, volume);
		volume = MIX_MAX_VOLUME;
	}
#endif

	/* @todo: Make display the RF_HIGHLIGHT effect for all actors that are in hear range */
	if (origin) {
		le_t* le = LE_GetClosestActor(origin);
		if (le) {
			float dist = VectorDist(origin, le->origin);
			Com_DPrintf(DEBUG_SOUND, "S_StartSound: world coord distance: %.2f\n", dist);
			if (dist >= SOUND_FULLVOLUME) {
				/* @todo: use attenuation value */
				dist = 1.0 - (dist / SOUND_MAX_DISTANCE);
				if (dist < 0.)
					/* too far away */
					volume = 0;
				else {
					/* close enough to hear it, but apply a distance effect though
					 * because it's farer than SOUND_FULLVOLUME */
					volume *= dist;
					le->hearTime = cls.realtime;
				}
				Com_DPrintf(DEBUG_SOUND, "S_StartSound: dist: %.2f\n", dist);
			}
		}
	}

	Mix_VolumeChunk(sfx->data, volume);
	Com_DPrintf(DEBUG_SOUND, "Mix '%s' (volume: %i, channel: %i)\n", sfx->name, volume, sfx->channel);

	sfx->channel = Mix_PlayChannel(sfx->channel, sfx->data, sfx->loops);
	if (sfx->channel == -1) {
		Com_Printf("S_StartSound: could not play '%s' (%s)\n", sfx->name, Mix_GetError());
		return;
	}
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
	if (!sound_started)
		return;
	Mix_HaltChannel(-1);
}

/**
 * @brief Plays a sound fx file via console
 */
static void S_Play_f (void)
{
	const char *filename;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	if (!sound_started) {
		Com_Printf("No audio activated\n");
		return;
	}

	filename = Cmd_Argv(1);
	S_StartLocalSound(filename);
}

/**
 * @brief Restart the sound subsystem so it can pick up new parameters and flush all sounds
 * @sa S_Shutdown
 * @sa S_Init
 * @sa CL_RegisterSounds
 */
static void S_Restart_f (void)
{
	Com_Printf("Restarting sound\n");
	S_Shutdown();
	S_Init();

	if (!sound_started)
		return;

	CL_RegisterSounds();
	/* restart the music, too */
	snd_music->modified = qtrue;
}

/*
INIT AND SHUTDOWN
*/

/**
 * @sa CL_Frame
 */
void S_Frame (void)
{
	if (snd_init && snd_init->modified) {
		S_Restart_f();
		snd_init->modified = qfalse;
	}

	if (!sound_started)
		return;

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
	SDL_version version;
	char drivername[MAX_VAR];

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

	if (Mix_OpenAudio(snd_rate->integer, MIX_DEFAULT_FORMAT, snd_channels->integer, 1024) == -1) {
		Com_Printf("Unable to open audio\n");
		return qfalse;
	}

	Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
	Com_Printf("...audio rate: %i\n...audio channels: %i\n", audio_rate, audio_channels);
	if (audio_rate != snd_rate->integer)
		Cvar_SetValue("snd_rate", audio_rate);
	if (audio_channels != snd_channels->integer)
		Cvar_SetValue("snd_channels", audio_channels);

	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
		strncpy(drivername, "(UNKNOWN)", sizeof(drivername) - 1);
	Com_Printf("...driver: '%s'\n", drivername);

	return qtrue;
}

/**
 * @sa S_Shutdown
 * @sa S_Restart_f
 * @todo: openal should be an replacement for the 'normal' snd_ref
 */
void S_Init (void)
{
	Com_Printf("\n------- sound initialization -------\n");

	snd_init = Cvar_Get("snd_init", "1", CVAR_ARCHIVE, "Should the sound renderer get initialized");
	snd_init->modified = qfalse; /* don't restart right away */
	Cmd_AddCommand("snd_restart", S_Restart_f, "Restart the sound renderer");

	if (!snd_init->integer) {
		Com_Printf("not initializing.\n");
		return;
	}

	snd_channels = Cvar_Get("snd_channels", "2", CVAR_ARCHIVE, "Sound channels");
	snd_volume = Cvar_Get("snd_volume", "128", CVAR_ARCHIVE, "Sound volume - default is 128");
	snd_rate = Cvar_Get("snd_rate", "44100", CVAR_ARCHIVE, "Hz value for sound renderer - default is 44100");
	snd_music = Cvar_Get("snd_music", "PsymongN3", 0, "Background music track");
	snd_music_volume = Cvar_Get("snd_music_volume", "128", CVAR_ARCHIVE, "Music volume - default is 128");

	Cmd_AddCommand("snd_play", S_Play_f, "Plays a sound fx file");
	Cmd_AddCommand("music_play", S_Music_Play_f, "Plays a background sound track");
	Cmd_AddCommand("music_start", S_Music_Play_f, "Starts the background music track from cvar snd_music");
	Cmd_AddCommand("music_stop", S_Music_Stop, "Stops currently playing music tracks");
	Cmd_AddCommand("music_randomtrack", S_Music_RandomTrack_f, "Plays a random background track");
	/* @todo: Complete functions */

	if (!SND_Init()) {
		Com_Printf("SND_Init failed\n");
		sound_started = qfalse;
		return;
	}

	sound_started = qtrue;

	memset(&music, 0, sizeof(music_t));
}

/**
 * @sa S_Init
 * @sa S_Restart_f
 */
void S_Shutdown (void)
{
	int i;
	sfx_t* sfx;

	if (!sound_started)
		return;

	for (i = 0; i < SFX_HASH_SIZE; i++)
		for (sfx = sfx_hash[i]; sfx; sfx = sfx->hash_next) {
			Mix_FreeChunk(sfx->data);
		}
	S_Music_Stop();

	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Mem_FreePool(cl_soundSysPool);
	memset(sfx_hash, 0, sizeof(sfx_hash));
	sound_started = qfalse;
}
