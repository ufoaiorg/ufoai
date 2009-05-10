/**
 * @file cl_sound.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.

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
#include "cl_music.h"
#include "cl_le.h"

#define DISTANCE_SCALE 0.8

/** @brief Support sound file extensions */
static const char *soundExtensions[] = {"ogg", "wav", NULL};


s_env_t s_env;

#define SFX_HASH_SIZE 64
static sfx_t *sfx_hash[SFX_HASH_SIZE];

static cvar_t *snd_volume;
static cvar_t *snd_init;
static cvar_t *snd_rate;

static int      audioRate;
static int      audioChannels;
static uint16_t audioFormat;

/**
 * @brief
 * @sa S_RegisterSound
 */
static Mix_Chunk *S_LoadSound (const char *sound)
{
	size_t len;
	byte *buf;
	const char **extension = soundExtensions;
	SDL_RWops *rw;
	Mix_Chunk *chunk;

	if (!sound || sound[0] == '*')
		return NULL;

	len = strlen(sound);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_LoadSound: MAX_QPATH exceeded for: '%s'\n", sound);
		return NULL;
	}

	while (*extension) {
		if ((len = FS_LoadFile(va("sound/%s.%s", sound, *extension++), &buf)) == -1)
			continue;

		if (!(rw = SDL_RWFromMem(buf, len))){
			FS_FreeFile(buf);
			continue;
		}

		if (!(chunk = Mix_LoadWAV_RW(rw, qfalse)))
			Com_Printf("S_LoadSound: %s.\n", Mix_GetError());

		FS_FreeFile(buf);

		SDL_FreeRW(rw);

		if (chunk)
			return chunk;
	}

	Com_Printf("S_LoadSound: Could not find sound file: '%s'\n", sound);
	return NULL;
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

	if (!s_env.initialized)
		return -1;

	/* Build the audio converter and create conversion buffers */
	if (SDL_BuildAudioCVT(&wavecvt, AUDIO_S16, channel, rate,
			audioFormat, audioChannels, audioRate) < 0) {
		return -1;
	}

	wavecvt.len = size & ~(samplesize - 1);
	wavecvt.buf = (byte *)Mem_PoolAlloc(wavecvt.len * wavecvt.len_mult, cl_soundSysPool, 0);
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
	sample.volume = MIX_MAX_VOLUME * snd_volume->value;

	/** @todo Free the channel data after the channel ended - memleak */
	return Mix_PlayChannelTimed(-1, &sample, 0, ms);
}

/**
 * @brief Loads and registers a sound file for later use
 * @param[in] name The name of the soundfile, relative to the sounds dir
 * @sa S_LoadSound
 */
sfx_t *S_RegisterSound (const char *soundFile)
{
	Mix_Chunk *chunk;
	sfx_t *sfx;
	unsigned hash;
	char name[MAX_QPATH];

	if (!s_env.initialized)
		return NULL;

	Com_StripExtension(soundFile, name, sizeof(name));

	hash = Com_HashKey(name, SFX_HASH_SIZE);
	for (sfx = sfx_hash[hash]; sfx; sfx = sfx->hash_next)
		if (!strcmp(name, sfx->name))
			return sfx;

	/* make sure the sound is loaded */
	chunk = S_LoadSound(name);
	if (!chunk)
		return NULL;		/* couldn't load the sound's data */

	sfx = Mem_PoolAlloc(sizeof(*sfx), cl_soundSysPool, 0);
	sfx->name = Mem_PoolStrDup(name, cl_soundSysPool, 0);
	sfx->chunk = chunk;
	sfx->loops = 0; /* play once */
	Mix_VolumeChunk(sfx->chunk, snd_volume->value * MIX_MAX_VOLUME);
	sfx->hash_next = sfx_hash[hash];
	sfx_hash[hash] = sfx;
	return sfx;
}

static int S_AllocChannel (void)
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (!s_env.channels[i].sample)
			return i;
	}

	return -1;
}

/**
 * @brief Set distance and stereo panning for the specified channel.
 */
static void S_SpatializeChannel (const s_channel_t *ch)
{
	vec3_t origin, delta;
	float dist, angle;
	const int c = (int)((ptrdiff_t)(ch - s_env.channels));
	const le_t *actor;

	VectorCopy(ch->org, origin);

	actor = LE_GetClosestActor(origin);
	if (actor)
		VectorSubtract(origin, actor->origin, delta);
	else
		VectorSubtract(origin, cl.cam.camorg, delta);

	dist = VectorNormalize(delta) * DISTANCE_SCALE;

	if (dist > 255.0)  /* clamp to max */
		dist = 255.0;

	if (dist > 50.0) {  /* resolve stereo panning */
		const float dot = DotProduct(s_env.right, delta);
		angle = acos(dot) * 180.0 / M_PI - 90.0;
		angle = (int)(360.0 - angle) % 360;
	} else
		angle = 0;

	Mix_SetPosition(c, (int)angle, (int)dist);
}

/**
 * @brief Validates the parms and queues the sound up
 * @param[in] origin if is NULL, the sound will be dynamically sourced from the entity
 * @param[in] sfx The soundfile to play
 * @param[in] relVolume Max mixer volume factor (0.0 - 1.0)
 * @sa S_StartLocalSound
 * @sa S_SetVolume
 */
void S_StartSound (const vec3_t origin, sfx_t* sfx, float atten)
{
	s_channel_t *ch;
	int i;

	if (!s_env.initialized)
		return;

	/* maybe the sound file couldn't be loaded */
	if (!sfx)
		return;

	if ((i = S_AllocChannel()) == -1)
		return;

	ch = &s_env.channels[i];

	if (origin != NULL) {
		VectorCopy(origin, ch->org);
		S_SpatializeChannel(ch);
	}

	ch->sample = sfx;

	Mix_PlayChannel(i, ch->sample->chunk, sfx->loops);
}

/**
 * @sa S_StartSound
 */
void S_StartLocalSound (const char *sound)
{
	sfx_t *sfx;

	if (!s_env.initialized)
		return;

	sfx = S_RegisterSound(sound);
	if (!sfx) {
		Com_Printf("S_StartLocalSound: can't load %s\n", sound);
		return;
	}
	S_StartSound(NULL, sfx, 0.0);
}

/**
 * @brief Stop all channels
 */
void S_StopAllSounds (void)
{
	if (!s_env.initialized)
		return;
	Mix_HaltChannel(-1);

	memset(s_env.channels, 0, sizeof(s_env.channels));
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

	if (!s_env.initialized) {
		Com_Printf("No audio activated\n");
		return;
	}

	filename = Cmd_Argv(1);
	S_StartLocalSound(filename);
}


/**
 * @note Called at precache phase - only load these soundfiles once at startup or on sound restart
 * @sa S_Restart_f
 */
static void S_RegisterSounds (void)
{
	int i, j, k;

	/* load weapon sounds */
	for (i = 0; i < csi.numODs; i++) { /* i = obj */
		for (j = 0; j < csi.ods[i].numWeapons; j++) {	/* j = weapon-entry per obj */
			for (k = 0; k < csi.ods[i].numFiredefs[j]; j++) { /* k = firedef per wepaon */
				if (csi.ods[i].fd[j][k].fireSound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].fireSound);
				if (csi.ods[i].fd[j][k].impactSound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].impactSound);
				if (csi.ods[i].fd[j][k].hitBodySound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].hitBodySound);
				if (csi.ods[i].fd[j][k].bounceSound[0] != '\0')
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
}

/**
 * @sa CL_Frame
 */
void S_Frame (void)
{
	if (snd_init && snd_init->modified) {
		S_Restart_f();
		snd_init->modified = qfalse;
	}

	if (!s_env.initialized)
		return;

	/* update right angle for stereo panning */
	AngleVectors(cl.cam.angles, NULL, s_env.right, NULL);

	M_Frame();
}

static void S_FreeChannel (int c)
{
	memset(&s_env.channels[c], 0, sizeof(s_env.channels[0]));
}

/** @todo This should be removed and read from the dir tree instead */
static const char *soundFileSubDirs[] = {
	"aliens", "ambience", "civilians", "doors", "footsteps", "geoscape", "misc", "soldiers", "weapons", NULL
};

static int S_CompleteSounds (const char *partial, const char **match)
{
	const char *filename;
	int matches = 0;
	char *localMatch[MAX_COMPLETE];
	size_t len = strlen(partial);
	const char **dirList = soundFileSubDirs;
	const char **extension = soundExtensions;
	int returnValue;

	/* check for partial matches */
	while (*dirList) {
		while (*extension) {
			char pattern[MAX_OSPATH];
			Com_sprintf(pattern, sizeof(pattern), "sound/%s/*.%s", *dirList, *extension);
			FS_BuildFileList(pattern);
			while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
				char fileWithPath[MAX_OSPATH];
				Com_sprintf(fileWithPath, sizeof(fileWithPath), "%s/%s", *dirList, filename);
				if (!len) {
					Com_Printf("%s\n", fileWithPath);
				} else if (!strncmp(partial, fileWithPath, len)) {
					Com_Printf("%s\n", fileWithPath);
					localMatch[matches++] = strdup(fileWithPath);
					if (matches >= MAX_COMPLETE)
						break;
				}
			}
			FS_NextFileFromFileList(NULL);
			extension++;
		}
		dirList++;
	}

	returnValue = Cmd_GenericCompleteFunction(len, match, matches, (const char **)localMatch);
	while (--matches >= 0)
		free(localMatch[matches]);
	return returnValue;
}

/**
 * @brief
 * @return true if the value needed adjustment and was changed
 */
static qboolean S_CvarCheckSoundRate (cvar_t *cvar)
{
	const int sndRates[] = {48000, 44100, 22050, 11025};
	const int n = lengthof(sndRates);
	int i;

	for (i = 0; i < n; i++)
		if (cvar->integer == sndRates[i])
			break;
	if (i == n) {
		Com_Printf("Invalid value for %s, valid values are: ", cvar->name);
		for (i = 0; i < n; i++)
			Com_Printf(" %i", sndRates[i]);
		Com_Printf("\n");
		Cvar_Set(cvar->name, "44100");
		cvar->modified = qfalse;
		return qtrue;
	}

	return qfalse;
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

	MIX_VERSION(&version)
	Com_Printf("SDL_mixer version: %d.%d.%d\n", version.major, version.minor, version.patch);

	if (Mix_OpenAudio(snd_rate->integer, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		Com_Printf("Unable to open audio\n");
		return qfalse;
	}

	Mix_QuerySpec(&audioRate, &audioFormat, &audioChannels);
	Com_Printf("... audio rate: %i\n... audio channels: %i\n", audioRate, audioChannels);
	if (audioRate != snd_rate->integer)
		Cvar_SetValue("snd_rate", audioRate);

	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
		strncpy(drivername, "(UNKNOWN)", sizeof(drivername) - 1);
	Com_Printf("... driver: '%s'\n", drivername);

	s_env.numChannels = Mix_AllocateChannels(MAX_CHANNELS);
	Com_Printf("... channels to mix sounds: %i\n", s_env.numChannels);

	Mix_ChannelFinished(S_FreeChannel);

	return qtrue;
}

/**
 * @sa S_Shutdown
 * @sa S_Restart_f
 */
void S_Init (void)
{
	Com_Printf("\n------- sound initialization -------\n");

	memset(&s_env, 0, sizeof(s_env));

	snd_init = Cvar_Get("snd_init", "1", CVAR_ARCHIVE, "Should the sound renderer get initialized");
	snd_init->modified = qfalse; /* don't restart right away */
	Cmd_AddCommand("snd_restart", S_Restart_f, "Restart the sound renderer");

	if (!snd_init->integer) {
		Com_Printf("not initializing.\n");
		return;
	}

	snd_volume = Cvar_Get("snd_volume", "0.7", CVAR_ARCHIVE, "Sound volume - default is 0.7");
	snd_rate = Cvar_Get("snd_rate", "44100", CVAR_ARCHIVE, "Hz value for sound renderer - default is 44100");
	Cvar_SetCheckFunction("snd_rate", S_CvarCheckSoundRate);
	/* set volumes to be changed so they are applied again for next sound/music playing */
	snd_volume->modified = qtrue;

	Cmd_AddCommand("snd_play", S_Play_f, "Plays a sound fx file. Pass path relative to base/sound without file extension");
	Cmd_AddParamCompleteFunction("snd_play", S_CompleteSounds);

	if (!SND_Init()) {
		Com_Printf("SND_Init failed\n");
		s_env.initialized = qfalse;
		return;
	}

	s_env.initialized = qtrue;

	M_Init();
	S_RegisterSounds();
}

/**
 * @sa S_Init
 * @sa S_Restart_f
 */
void S_Shutdown (void)
{
	int i;
	sfx_t* sfx;

	if (!s_env.initialized)
		return;

	S_StopAllSounds();

	for (i = 0; i < SFX_HASH_SIZE; i++)
		for (sfx = sfx_hash[i]; sfx; sfx = sfx->hash_next)
			Mix_FreeChunk(sfx->chunk);

	M_Shutdown();

	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Mem_FreeTag(cl_soundSysPool, 0);
	memset(sfx_hash, 0, sizeof(sfx_hash));
	s_env.initialized = qfalse;
}
