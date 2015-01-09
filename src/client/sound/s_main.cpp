/**
 * @file
 * @brief Main control for any streaming sound output device.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../client.h"
#include "s_main.h"
#include "s_local.h"
#include "s_music.h"
#include "s_sample.h"
#include "s_mix.h"
#include "s_mumble.h"

#define COMPARE_VERSION(major, minor, micro) \
	(SDL_MIXER_MAJOR_VERSION > (major) || \
	(SDL_MIXER_MAJOR_VERSION == (major) && SDL_MIXER_MINOR_VERSION > (minor)) || \
	(SDL_MIXER_MAJOR_VERSION == (major) && SDL_MIXER_MINOR_VERSION == (minor) && \
	 SDL_MIXER_PATCHLEVEL >= (micro)))

s_env_t s_env;

cvar_t* snd_volume;
cvar_t* snd_distance_scale;
static cvar_t* snd_init;
static cvar_t* snd_rate;
static cvar_t* snd_chunkbufsize;

memPool_t* cl_soundSysPool;

/* I know this decl shouldn't be here (Duke). Interim solution...*/
extern s_sample_t* stdSoundPool[MAX_SOUNDIDS];

static void S_Restart_f(void);

/**
 * @brief Stop all channels
 */
void S_Stop (void)
{
	S_MumbleUnlink();
	if (!s_env.initialized)
		return;
	Mix_HaltChannel(-1);
	OBJZERO(s_env.channels);
}

/**
 * @sa CL_Frame
 */
void S_Frame (void)
{
	if (snd_init && snd_init->modified) {
		S_Restart_f();
		snd_init->modified = false;
	}

	if (!s_env.initialized)
		return;

	M_Frame();

	if (CL_OnBattlescape()) {
		/* update right angle for stereo panning */
		VectorCopy(cl.cam.axis[AXIS_RIGHT], s_env.right);
		S_MumbleUpdate(cl.cam.camorg, cl.cam.axis[AXIS_FORWARD], cl.cam.axis[AXIS_RIGHT], cl.cam.axis[AXIS_UP]);

		/* update spatialization for current sounds */
		s_channel_t* ch = s_env.channels;

		for (int i = 0; i < MAX_CHANNELS; i++, ch++) {
			if (!ch->sample)
				continue;

			/* reset channel's count for loop samples */
			ch->count = 0;

			S_SpatializeChannel(ch);
		}

		/* ambient sounds */
		le_t* le = nullptr;
		while ((le = LE_GetNextInUse(le))) {
			if (le->type == ET_SOUND) {
				s_sample_t* sample = S_GetSample(le->sampleIdx);
				int j;
				for (j = 0; j < MAX_CHANNELS; j++) {
					if (s_env.channels[j].sample == sample)
						break;
				}

				if (j == MAX_CHANNELS)
					S_LoopSample(le->origin, sample, le->volume, le->attenuation);
			}
		}
	}
}

/**
 * @brief Plays sound fx files via console
 */
static void S_Play_f (void)
{
	int i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <filename> [<filename> ...]\n", Cmd_Argv(0));
		return;
	}

	i = 1;
	while (i < Cmd_Argc()) {
		S_StartLocalSample(Cmd_Argv(i), SND_VOLUME_DEFAULT);
		i++;
	}
}

/**
 * @brief Restart the sound subsystem so it can pick up new parameters and flush all sounds
 * @sa S_Shutdown
 * @sa S_Init
 */
static void S_Restart_f (void)
{
	Com_Printf("Restarting sound\n");
	S_Shutdown();
	S_Init();
	S_LoadSamples();
}

static int S_CompleteSounds (const char* partial, const char** match)
{
	char const* const soundExtensions[] = SAMPLE_TYPES;

	int n = 0;
	for (char const* const* extension = soundExtensions; *extension; ++extension) {
		char pattern[MAX_OSPATH];
		Com_sprintf(pattern, sizeof(pattern), "sound/**.%s", *extension);
		while (char const* filename = FS_NextFileFromFileList(pattern)) {
			char const* const fileWithPath = filename + strlen("sound/");
			if (Cmd_GenericCompleteFunction(fileWithPath, partial, match)) {
				Com_Printf("%s\n", fileWithPath);
				++n;
			}
		}
		FS_NextFileFromFileList(nullptr);
	}
	return n;
}

/**
 * @sa S_Shutdown
 * @sa S_Restart_f
 */
void S_Init (void)
{
	SDL_version version;

	Com_Printf("\n------- sound initialization -------\n");

	OBJZERO(s_env);

	snd_init = Cvar_Get("snd_init", "1", CVAR_ARCHIVE, "Should the sound renderer get initialized");
	snd_init->modified = false; /* don't restart right away */
	Cmd_AddCommand("snd_restart", S_Restart_f, "Restart the sound renderer");

	if (!snd_init->integer) {
		Com_Printf("not initializing.\n");
		Cmd_AddCommand("music_change", Cmd_Dummy_f, "Dummy command if sound is disabled");
		Cvar_Get("snd_music", "PsymongN3", 0, "Background music track");
		return;
	}

	cl_soundSysPool = Mem_CreatePool("Client: Sound system");

	snd_distance_scale = Cvar_Get("snd_distance_scale", "0.1", 0, "Sound distance scale");
	snd_volume = Cvar_Get("snd_volume", "0.7", CVAR_ARCHIVE, "Sound volume - default is 0.7");
	snd_rate = Cvar_Get("snd_rate", "44100", CVAR_ARCHIVE, "Hz value for sound renderer - default is 44100");
	snd_chunkbufsize = Cvar_Get("snd_chunkbufsize", "1024", CVAR_ARCHIVE, "The sound buffer chunk size");
	/* set volumes to be changed so they are applied again for next sound/music playing */
	/** @todo implement the volume change for already loaded sample chunks */
	snd_volume->modified = true;

	Cmd_AddCommand("snd_play", S_Play_f, "Plays a sound fx file. Pass path relative to base/sound without file extension");
	Cmd_AddParamCompleteFunction("snd_play", S_CompleteSounds);

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if (SDL_Init(SDL_INIT_AUDIO) < 0) {
			Com_Printf("S_Init: %s.\n", SDL_GetError());
			return;
		}
	}

	MIX_VERSION(&version)
	Com_Printf("SDL_mixer version: %d.%d.%d\n", version.major, version.minor, version.patch);
	Com_Printf("... requested audio rate: %i\n", snd_rate->integer);

	if (Mix_OpenAudio(snd_rate->integer, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, snd_chunkbufsize->integer) == -1) {
		Com_Printf("S_Init: %s\n", Mix_GetError());
		return;
	}

	if (Mix_QuerySpec(&s_env.rate, &s_env.format, &s_env.numChannels) == 0) {
		Com_Printf("S_Init: %s\n", Mix_GetError());
		return;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	const int n = SDL_GetNumAudioDrivers();
	if (n == 0) {
		Com_Printf("... no built-in audio drivers\n");
	} else {
		for (int i = 0; i < n; ++i) {
			Com_Printf("... available audio driver %s\n", SDL_GetAudioDriver(i));
		}
	}

	Com_Printf("... actual audio driver: %s\n", SDL_GetCurrentAudioDriver());
#else
	char drivername[MAX_VAR];
	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == nullptr)
		Q_strncpyz(drivername, "(UNKNOWN)", sizeof(drivername));
	Com_Printf("... driver: '%s'\n", drivername);
#endif

	if (Mix_AllocateChannels(MAX_CHANNELS) != MAX_CHANNELS) {
		Com_Printf("S_Init: %s\n", Mix_GetError());
		return;
	}

	Mix_ChannelFinished(S_FreeChannel);

	Com_Printf("... audio rate: %i\n", s_env.rate);
	Com_Printf("... audio channels: %i\n", s_env.numChannels);

#if COMPARE_VERSION(1, 2, 10)
	if (!(Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG)) {
		Com_Printf("... could not load ogg vorbis support - sound initialization failed\n");
		Mix_AllocateChannels(0);
		Mix_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return;
	} else {
		Com_Printf("... loaded ogg vorbis support\n");
	}
#endif

	s_env.initialized = true;

	M_Init();
	S_MumbleInit();
}

/**
 * @sa S_Init
 * @sa S_Restart_f
 */
void S_Shutdown (void)
{
	if (!s_env.initialized)
		return;

	M_Shutdown();

	S_Stop();

	Mix_AllocateChannels(0);

	S_FreeSamples();

	Mix_CloseAudio();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Mem_DeletePool(cl_soundSysPool);

	Cmd_RemoveCommand("snd_play");
	Cmd_RemoveCommand("snd_restart");

#if COMPARE_VERSION(1, 2, 10)
	Mix_Quit();
#endif

	s_env.initialized = false;
}

/**
 * @brief does what the name implies in just one function to avoid exposing s_sample_t
 * @param s name of the sample
 * @param origin where to play it
 * @param attenuation how to reduce volume by distance
 * @param volume well, the volume
 */
bool S_LoadAndPlaySample (const char* s, const vec3_t origin, float attenuation, float volume)
{
	if (Q_strnull(s))
		return false;

	s_sample_t* sample = S_LoadSample(s);
	if (!sample)
		return false;
	S_PlaySample(origin, sample, attenuation, volume);
	return true;
}

/**
 * @brief plays one of the precached samples
 * @param sId ID of the sample
 * @param origin where to play it
 * @param attenuation how to reduce volume by distance
 * @param volume well, the volume
 */
void S_PlayStdSample (const stdsound_t sId, const vec3_t origin, float attenuation, float volume)
{
	S_PlaySample(origin, stdSoundPool[sId], attenuation, volume);
}

/**
 * @brief Loads and registers a sound file for later use
 * @param[in] soundFile The name of the soundfile, relative to the sounds dir
 */
s_sample_t* S_LoadSample (const char* soundFile)
{
	int sampleIdx = S_LoadSampleIdx(soundFile);
	return S_GetSample(sampleIdx);
}

/**
 * @brief Controls the repeat rate for the same sample.
 * @param[in] sampleRepeatRate milliseconds that must have passed to play the same sample again
 */
void S_SetSampleRepeatRate (int sampleRepeatRate)
{
	s_env.sampleRepeatRate = sampleRepeatRate;
}
/**
 * @brief Wrapper for S_PrecacheSamples to avoid exposing it  via s_sample.h
 */
void S_LoadSamples (void)
{
	S_PrecacheSamples();
}
