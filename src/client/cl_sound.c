/**
 * @file cl_sound.c
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

enum {
	MUSIC_MAIN,
	MUSIC_GEOSCAPE,
	MUSIC_BATTLESCAPE,
	MUSIC_AIRCOMBAT,

	MUSIC_MAX
};

#define MUSIC_MAX_ENTRIES 64
static char *musicArrays[MUSIC_MAX][MUSIC_MAX_ENTRIES];
static int musicArrayLength[MUSIC_MAX];

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
/* HACK: Remove this as soon as the reason was found */
static cvar_t *snd_music_crackleWorkaround;

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
		/* this is freed in the SDL_mixer callback functions */
		music.musicSrc = NULL;
		Q_strncpyz(music.currentlyPlaying, "", sizeof(music.currentlyPlaying));
	}
}

/* prototype */
static void S_Music_Start(const char *file);

static void S_MusicHookFinished (void)
{
	S_Music_Stop();
	S_Music_Start(Cvar_VariableString("snd_music"));
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
		if (!Mix_FadeOutMusic(1500))
			S_Music_Stop();
		if (music.nextMusicTrack)
			Mem_Free(music.nextMusicTrack);
		music.nextMusicTrack = Mem_PoolStrDup(name, cl_soundSysPool, CL_TAG_NONE);
		return;
	}

	/* make really sure the last track is closed and freed */
	S_Music_Stop();

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
		if (snd_music_crackleWorkaround->integer) {
			if (Mix_FadeInMusic(music.data, 1, 1500) == -1)
				Com_Printf("S_Music_Start: Could not play music: 'music/%s' (%s)\n", name, Mix_GetError());
			Mix_HookMusicFinished(S_MusicHookFinished);
		} else {
			if (Mix_FadeInMusic(music.data, -1, 1500) == -1)
				Com_Printf("S_Music_Start: Could not play music: 'music/%s' (%s)\n", name, Mix_GetError());
		}
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
	/** @todo the chunk below would make S_Music_Play_f() a wrapper for S_Music_Start() */
#if 0
	if (Cmd_Argc() == 2)
		Cvar_Set("snd_music", Cmd_Argv(1));

	S_Music_Start(Cvar_VariableString("snd_music"));
#endif

	if (!sound_started) {
		Com_Printf("No audio activated\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		if (Mix_PlayingMusic()) {
			S_Music_Start(Cmd_Argv(1));
		} else {
			Cvar_Set("snd_music", Cmd_Argv(1));
		}
	} else
		snd_music->modified = qtrue;
}

static int musicTrackCount = 0;

/**
 * @brief Sets the music cvar to a random track
 */
static void S_Music_RandomTrack_f (void)
{
	const char *filename;
	char findname[MAX_OSPATH];
	int randomID;
	const char *musicTrack;

	if (!sound_started)
		return;

	musicTrackCount = FS_BuildFileList("music/*.ogg");
	if (musicTrackCount){
		randomID = rand() % musicTrackCount;
		Com_DPrintf(DEBUG_SOUND, "S_Music_RandomTrack_f: random track id: %i/%i\n", randomID, musicTrackCount);

		while ((filename = FS_NextFileFromFileList("music/*.ogg")) != NULL) {
			if (!randomID) {
				Com_sprintf(findname, sizeof(findname), "%s", filename);
				musicTrack = COM_SkipPath(findname);
				Com_Printf("..playing next: '%s'\n", musicTrack);
				Cvar_Set("snd_music", musicTrack);
			}
			randomID--;
		}
		FS_NextFileFromFileList(NULL);
	} else {
		Com_DPrintf(DEBUG_SOUND, "S_Music_RandomTrack_f: No musics found!\n");
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
	size_t len;
	SDL_RWops *src;
	byte *sfxBuf;
	int size;
	Mix_Chunk *mix = NULL;

	if (!sound || *sound == '*')
		return NULL;

	len = strlen(sound);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_LoadSound: MAX_QPATH exceeded for: '%s'\n", sound);
		return NULL;
	}

	/* load it in */
	if ((size = FS_LoadFile(va("sound/%s.ogg", sound), (byte **)&sfxBuf)) != -1) {
		src = SDL_RWFromMem(sfxBuf, size);
		mix = Mix_LoadWAV_RW(src, 1);
	} else if ((size = FS_LoadFile(va("sound/%s.wav", sound), (byte **)&sfxBuf)) != -1) {
		src = SDL_RWFromMem(sfxBuf, size);
		mix = Mix_LoadWAV_RW(src, 1);
	} else {
		Com_Printf("S_LoadSound: Could not find sound file: '%s'\n", sound);
		return NULL;
	}

	if (!mix)
		Com_Printf("S_LoadSound: '%s' doesn't seam to be a valid ogg or wav file\n", sound);
	return mix;
}

/**
 * @brief Loads a buffer from memory into the mixer
 * @param[in] mem 16 byte (short) buffer with data
 * @returns the channel the sound is played on or -1 in case of an error
 */
int S_PlaySoundFromMem (const short* mem, size_t size, int rate, int channel, int ms)
{
	SDL_AudioCVT wavecvt;
	Mix_Chunk sample;
	const int samplesize = 2 * channel;

	if (!sound_started)
		return -1;

	/* Build the audio converter and create conversion buffers */
	if (SDL_BuildAudioCVT(&wavecvt, AUDIO_S16, channel, rate,
			audio_format, audio_channels, audio_rate) < 0) {
		return -1;
	}

	wavecvt.len = size & ~(samplesize - 1);
	wavecvt.buf = (byte *)Mem_PoolAlloc(wavecvt.len * wavecvt.len_mult, cl_soundSysPool, CL_TAG_NONE);
	if (wavecvt.buf == NULL)
		return -1;
	memcpy(wavecvt.buf, mem, size);

	/* Run the audio converter */
	if (SDL_ConvertAudio(&wavecvt) < 0) {
		Mem_Free(wavecvt.buf);
		return -1;
	}

	sample.allocated = 0;
	sample.abuf = wavecvt.buf;
	sample.alen = wavecvt.len_cvt;
	sample.volume = MIX_MAX_VOLUME;

	/** @todo Free the channel data after the channel ended - memleak */
	return Mix_PlayChannelTimed(-1, &sample, 0, ms);
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

	sfx = Mem_PoolAlloc(sizeof(*sfx), cl_soundSysPool, CL_TAG_NONE);
	sfx->name = Mem_PoolStrDup(name, cl_soundSysPool, CL_TAG_NONE);
	sfx->data = mix;
	sfx->channel = -1; /* just a free channel */
	sfx->loops = 0; /* play once */
	sfx->hash_next = sfx_hash[hash];
	sfx_hash[hash] = sfx;
	return sfx;
}

/**
 * @brief Set volume for mixer chunk relative to snd_volume
 */
void S_SetVolume (sfx_t *sfx, int volume)
{
	const float volRate = snd_volume->value / MIX_MAX_VOLUME;

	if (volume < 0)
		return;

	if (volume >= MIX_MAX_VOLUME)
		return;

	Com_DPrintf(DEBUG_SOUND, "Volume changed from %i to %i for sound '%s' on channel %i\n",
		sfx->volume, volume, sfx->name, sfx->channel);

	/* only change the chunk volume if it has changed */
	if (sfx->volume != volume) {
		sfx->volume = volume;
		Mix_VolumeChunk(sfx->data, sfx->volume * volRate);
	}
}

/**
 * @brief Validates the parms and ques the sound up
 * @param[in] origin if is NULL, the sound will be dynamically sourced from the entity
 * @param[in] sfx The soundfile to play
 * @param[in] relVolume - 0 - 1
 * @sa S_StartLocalSound
 */
void S_StartSound (const vec3_t origin, sfx_t* sfx, float relVolume)
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

	if (origin) {
		le_t* le = LE_GetClosestActor(origin);
		if (le) {
			float dist = VectorDist(origin, le->origin);
			Com_DPrintf(DEBUG_SOUND, "S_StartSound: world coord distance: %.2f\n", dist);
			if (dist >= SOUND_FULLVOLUME) {
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

	S_SetVolume(sfx, volume);

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
	S_StartSound(NULL, sfx, DEFAULT_SOUND_PACKET_VOLUME);
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
 * @note Called at precache phase - only load these soundfiles once
 * @sa CL_LoadMedia
 * @sa S_Restart_f
 * @sa CL_RequestNextDownload
 */
void S_RegisterSounds (void)
{
	int i, j, k;

	/* load weapon sounds */
	for (i = 0; i < csi.numODs; i++) { /* i = obj */
		for (j = 0; j < csi.ods[i].numWeapons; j++) {	/* j = weapon-entry per obj */
			for (k = 0; k < csi.ods[i].numFiredefs[j]; j++) { /* k = firedef per wepaon */
				if (csi.ods[i].fd[j][k].fireSound[0])
					S_RegisterSound(csi.ods[i].fd[j][k].fireSound);
				if (csi.ods[i].fd[j][k].impactSound[0])
					S_RegisterSound(csi.ods[i].fd[j][k].impactSound);
				if (csi.ods[i].fd[j][k].hitBodySound[0])
					S_RegisterSound(csi.ods[i].fd[j][k].hitBodySound);
				if (csi.ods[i].fd[j][k].bounceSound[0])
					S_RegisterSound(csi.ods[i].fd[j][k].bounceSound);
			}
		}
	}

	/* precache the sound pool */
	cls.sound_pool[SOUND_WATER_IN] = S_RegisterSound("footsteps/water_in");
	cls.sound_pool[SOUND_WATER_OUT] = S_RegisterSound("footsteps/water_out");
	cls.sound_pool[SOUND_WATER_MOVE] = S_RegisterSound("footsteps/water_under");
}

/**
 * @brief Restart the sound subsystem so it can pick up new parameters and flush all sounds
 * @sa S_Shutdown
 * @sa S_Init
 * @sa S_RegisterSounds
 */
static void S_Restart_f (void)
{
	Com_Printf("Restarting sound\n");
	S_Shutdown();
	S_Init();

	if (!sound_started)
		return;

	S_RegisterSounds();
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
			S_Music_Stop(); /* free the allocated memory */
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
	Com_Printf("... audio rate: %i\n... audio channels: %i\n", audio_rate, audio_channels);
	if (audio_rate != snd_rate->integer)
		Cvar_SetValue("snd_rate", audio_rate);
	if (audio_channels != snd_channels->integer)
		Cvar_SetValue("snd_channels", audio_channels);

	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
		strncpy(drivername, "(UNKNOWN)", sizeof(drivername) - 1);
	Com_Printf("... driver: '%s'\n", drivername);

	return qtrue;
}

/**
 * @brief Changes the music if it suits the current situation
 * @todo Make the music a scriptable list
 */
static void S_Music_Change_f (void)
{
	const char* type;
	int rnd;
	int category;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <geoscape|battlescape|main|aircombat>\n", Cmd_Argv(0));
		return;
	}
	type = Cmd_Argv(1);
	if (!Q_strcmp(type, "geoscape")) {
		category = MUSIC_GEOSCAPE;
	} else if (!Q_strcmp(type, "battlescape")) {
		category = MUSIC_BATTLESCAPE;
	} else if (!Q_strcmp(type, "main")) {
		category = MUSIC_MAIN;
	} else if (!Q_strcmp(type, "aircombat")) {
		category = MUSIC_AIRCOMBAT;
	} else {
		Com_Printf("Invalid parameter given\n");
		return;
	}

	if (category != MUSIC_BATTLESCAPE && cls.state == ca_active) {
		Com_DPrintf(DEBUG_SOUND, "Not changing music to %s - we are in Battlescape\n", type);
		return;
	}

	if (!musicArrayLength[category]) {
		Com_Printf("Could not find any %s music tracks\n", type);
		return;
	}
	rnd = rand() % musicArrayLength[category];
	Com_Printf("music change to %s (from %s)\n", musicArrays[category][rnd], snd_music->string);
	Cvar_Set("snd_music", musicArrays[category][rnd]);
}

static int S_CompleteMusic (const char *partial, const char **match)
{
	const char *filename;
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;

	FS_BuildFileList("music/*.ogg");

	len = strlen(partial);
	if (!len) {
		while ((filename = FS_NextFileFromFileList("music/*.ogg")) != NULL) {
			Com_Printf("%s\n", filename);
		}
		FS_NextFileFromFileList(NULL);
		return 0;
	}

	/* start from first file entry */
	FS_NextFileFromFileList(NULL);

	/* check for partial matches */
	while ((filename = FS_NextFileFromFileList("music/*.ogg")) != NULL) {
		if (!Q_strncmp(partial, filename, len)) {
			Com_Printf("%s\n", filename);
			localMatch[matches++] = filename;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}
	FS_NextFileFromFileList(NULL);

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @sa S_Shutdown
 * @sa S_Restart_f
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
	snd_music_crackleWorkaround = Cvar_Get("snd_music_crackleWorkaround", "0", CVAR_ARCHIVE, "Set to 1 and issue \"music_stop; music_start\" if you experience crackling when background music loops");

	Cmd_AddCommand("snd_play", S_Play_f, "Plays a sound fx file");
	Cmd_AddCommand("music_play", S_Music_Play_f, "Plays a background sound track");
	Cmd_AddCommand("music_start", S_Music_Play_f, "Starts the background music track from cvar snd_music");
	Cmd_AddCommand("music_change", S_Music_Change_f, "Changes the music theme");
	Cmd_AddCommand("music_stop", S_Music_Stop, "Stops currently playing music tracks");
	Cmd_AddCommand("music_randomtrack", S_Music_RandomTrack_f, "Plays a random background track");
	Cmd_AddParamCompleteFunction("music_start", S_CompleteMusic);
	/** @todo Complete functions */

	if (!SND_Init()) {
		Com_Printf("SND_Init failed\n");
		sound_started = qfalse;
		return;
	}

	sound_started = qtrue;

	memset(&music, 0, sizeof(music));
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

/**
 * @brief Parses music definitions for different situations
 * @note We can have lists for geoscape and battlescape e.g.
 */
void CL_ParseMusic (const char *name, const char **text)
{
	const char *errhead = "CL_ParseMusic: unexpected end of file (campaign ";
	const char *token;
	int i;

	if (!Q_strcmp(name, "geoscape"))
		i = MUSIC_GEOSCAPE;
	else if (!Q_strcmp(name, "battlescape"))
		i = MUSIC_BATTLESCAPE;
	else if (!Q_strcmp(name, "aircombat"))
		i = MUSIC_AIRCOMBAT;
	else if (!Q_strcmp(name, "main"))
		i = MUSIC_MAIN;
	else {
		Com_Printf("CL_ParseMusic: Invalid music id '%s'\n", name);
		FS_SkipBlock(text);
		return;
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseMusic: music def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (musicArrayLength[i] >= MUSIC_MAX_ENTRIES) {
			Com_Printf("Too many music entries for category: '%s'\n", name);
			FS_SkipBlock(text);
			break;
		}
		musicArrays[i][musicArrayLength[i]] = Mem_PoolStrDup(token, cl_soundSysPool, CL_TAG_NONE);
		musicArrayLength[i]++;
	} while (*text);
}
