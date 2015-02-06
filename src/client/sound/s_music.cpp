/**
 * @file
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

#include "s_music.h"
#include "s_local.h"
#include "../cl_shared.h"	/* cl_genericPool */
#include "../../shared/parse.h"
#include "../../ports/system.h"
#include "../../common/filesys.h"	/* for MAX_QPATH */
#include "../../common/common.h"	/* for many */
#include "../../common/scripts.h"
#include "../cl_renderer.h"
#include "../cl_video.h"
#include "../battlescape/cl_camera.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_battlescape.h"

enum {
	MUSIC_MAIN,
	MUSIC_GEOSCAPE,
	MUSIC_BATTLESCAPE,
	MUSIC_AIRCOMBAT,

	MUSIC_MAX
};

typedef struct music_s {
	char currentTrack[MAX_QPATH];
	char nextTrack[MAX_QPATH];
	Mix_Music* data;
	int category;
	byte* buffer;
	bool playingStream; /**< if this is set no action to M_Start and M_Stop might happen, otherwise we might run
	 * into a deadlock. This is due to the installed hook function for music mixing that is used
	 * whenever we stream the music on our own */
	bool interruptStream;
	bool playing;
} music_t;

#define MUSIC_MAX_ENTRIES 64
static char* musicArrays[MUSIC_MAX][MUSIC_MAX_ENTRIES] = {{}, {}};
static int musicArrayLength[MUSIC_MAX] = {};
static music_t music = {{}, {}, nullptr, 0, nullptr, false, false, true};
static cvar_t* snd_music;
static cvar_t* snd_music_volume;
static cvar_t* snd_music_play;

/**
 * @brief Parses music definitions for different situations
 * @note We have lists for geoscape, battlescape, main and aircombat
 */
void M_ParseMusic (const char* name, const char** text)
{
	int i;

	if (Q_streq(name, "geoscape"))
		i = MUSIC_GEOSCAPE;
	else if (Q_streq(name, "battlescape"))
		i = MUSIC_BATTLESCAPE;
	else if (Q_streq(name, "aircombat"))
		i = MUSIC_AIRCOMBAT;
	else if (Q_streq(name, "main"))
		i = MUSIC_MAIN;
	else {
		Com_Printf("M_ParseMusic: Invalid music id '%s'!\n", name);
		linkedList_t* list;
		Com_ParseList(text, &list);
		LIST_Delete(&list);
		return;
	}

	/* get it's body */
	linkedList_t* list;
	if (!Com_ParseList(text, &list)) {
		Com_Error(ERR_DROP, "M_ParseMusic: error while reading music \"%s\"", name);
	}

	for (linkedList_t* element = list; element != nullptr; element = element->next) {
		if (musicArrayLength[i] >= MUSIC_MAX_ENTRIES) {
			Com_Printf("M_ParseMusic: Too many music entries for category: '%s'!\n", name);
			break;
		}
		musicArrays[i][musicArrayLength[i]] = Mem_PoolStrDup((char*)element->data, cl_genericPool, 0);
		musicArrayLength[i]++;
	}

	LIST_Delete(&list);
}

/**
 * @sa M_Start
 */
void M_Stop (void)
{
	/* we should not even have a buffer nor data set - but ... just to be sure */
	if (music.playingStream)
		return;

	if (music.data != nullptr) {
		Mix_HaltMusic();
		Mix_FreeMusic(music.data);
	}

	if (music.buffer)
		FS_FreeFile(music.buffer);

	music.data = nullptr;
	music.buffer = nullptr;
}

/**
 * @sa M_Stop
 */
static void M_Start (const char* file)
{
	char name[MAX_QPATH];
	size_t len;
	byte* musicBuf;
	int size;
	SDL_RWops* rw;

	if (Q_strnull(file))
		return;

	if (!s_env.initialized) {
		Com_Printf("M_Start: No sound started!\n");
		return;
	}

	if (music.playingStream || !music.playing)
		return;

	Com_StripExtension(file, name, sizeof(name));
	len = strlen(name);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("M_Start: MAX_QPATH exceeded: " UFO_SIZE_T "\n", len + 4);
		return;
	}

	/* we are already playing that track */
	if (Q_streq(name, music.currentTrack) && music.data && Mix_PlayingMusic())
		return;

	/* we are still playing some background track - fade it out */
	if (music.data && Mix_PlayingMusic()) {
		if (!Mix_FadeOutMusic(1500))
			M_Stop();
		Q_strncpyz(music.nextTrack, name, sizeof(music.nextTrack));
		return;
	}

	/* make really sure the last track is closed and freed */
	M_Stop();

	/* load it in */
	if ((size = FS_LoadFile(va("music/%s.ogg", name), &musicBuf)) == -1) {
		Com_Printf("M_Start: Could not load '%s' background track!\n", name);
		return;
	}

	rw = SDL_RWFromMem(musicBuf, size);
	if (!rw) {
		Com_Printf("M_Start: Could not load music: 'music/%s'!\n", name);
		FS_FreeFile(musicBuf);
		return;
	}
#if SDL_VERSION_ATLEAST(2,0,0)
	music.data = Mix_LoadMUS_RW(rw, 1);
#else
	music.data = Mix_LoadMUS_RW(rw);
#endif
	if (!music.data) {
		Com_Printf("M_Start: Could not load music: 'music/%s' (%s)!\n", name, Mix_GetError());
		SDL_FreeRW(rw);
		FS_FreeFile(musicBuf);
		return;
	}

	Q_strncpyz(music.currentTrack, name, sizeof(music.currentTrack));
	music.buffer = musicBuf;
	if (Mix_FadeInMusic(music.data, 1, 1500) == -1)
		Com_Printf("M_Start: Could not play music: 'music/%s' (%s)!\n", name, Mix_GetError());
}

/**
 * @brief Plays the music file given via commandline parameter
 */
static void M_Play_f (void)
{
	if (Cmd_Argc() == 2)
		Cvar_Set("snd_music", "%s", Cmd_Argv(1));

	M_Start(Cvar_GetString("snd_music"));
}

/**
 * @brief Sets the music cvar to a random track
 */
static void M_RandomTrack_f (void)
{
	if (!s_env.initialized || !music.playing)
		return;

	const int musicTrackCount = FS_BuildFileList("music/*.ogg");
	if (musicTrackCount) {
		int randomID = rand() % musicTrackCount;
		Com_DPrintf(DEBUG_SOUND, "M_RandomTrack_f: random track id: %i/%i\n", randomID, musicTrackCount);

		const char* filename;
		while ((filename = FS_NextFileFromFileList("music/*.ogg")) != nullptr) {
			if (!randomID) {
				const char* musicTrack = Com_SkipPath(filename);
				Com_Printf("..playing next music track: '%s'\n", musicTrack);
				Cvar_Set("snd_music", "%s", musicTrack);
			}
			randomID--;
		}
		FS_NextFileFromFileList(nullptr);
	} else {
		Com_DPrintf(DEBUG_SOUND, "M_RandomTrack_f: No music found!\n");
	}
}

static bool M_PlayRandomByCategory (int category)
{
	if (category != MUSIC_BATTLESCAPE && CL_OnBattlescape())
		return false;
	if (!musicArrayLength[category])
		return false;
	const int rnd = rand() % musicArrayLength[category];
	music.category = category;
	Com_Printf("Music: track changed from %s to %s.\n", music.currentTrack, musicArrays[category][rnd]);
	Cvar_Set("snd_music", "%s", musicArrays[category][rnd]);
	return snd_music->modified;
}

/**
 * @brief Changes the music if it suits the current situation
 * @todo Make the music a scriptable list
 */
static void M_Change_f (void)
{
	const char* type;
	int category;

	if (!s_env.initialized || !music.playing)
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <geoscape|battlescape|main|aircombat>\n", Cmd_Argv(0));
		return;
	}
	type = Cmd_Argv(1);
	if (Q_streq(type, "geoscape")) {
		category = MUSIC_GEOSCAPE;
	} else if (Q_streq(type, "battlescape")) {
		category = MUSIC_BATTLESCAPE;
	} else if (Q_streq(type, "main")) {
		category = MUSIC_MAIN;
	} else if (Q_streq(type, "aircombat")) {
		category = MUSIC_AIRCOMBAT;
	} else {
		Com_Printf("Invalid parameter given!\n");
		return;
	}

	if (category != MUSIC_BATTLESCAPE && CL_OnBattlescape()) {
		Com_DPrintf(DEBUG_SOUND, "Not changing music to %s - we are on the battlescape!\n", type);
		return;
	}

	if (!musicArrayLength[category]) {
		Com_Printf("M_Change_f: Could not find any %s themed music tracks!\n", type);
		return;
	}

	M_PlayRandomByCategory(category);
}

static int M_CompleteMusic (const char* partial, const char** match)
{
	int n = 0;
	while (char const* const filename = FS_NextFileFromFileList("music/*.ogg")) {
		if (Cmd_GenericCompleteFunction(filename, partial, match)) {
			Com_Printf("%s\n", filename);
			++n;
		}
	}
	FS_NextFileFromFileList(nullptr);
	return n;
}

static void M_MusicStreamUpdate (void)
{
	if (music.interruptStream) {
		music.interruptStream = false;
		M_StopMusicStream(nullptr);
	}
}

void M_Frame (void)
{
	if (snd_music_play && snd_music_play->modified) {
		music.playing = snd_music_play->integer != 0;
		snd_music_play->modified = false;
	}
	if (!music.playing) {
		if (Mix_PlayingMusic())
			M_Stop();
		return;
	}
	if (snd_music->modified) {
		M_Start(snd_music->string);
		snd_music->modified = false;
	}
	if (snd_music_volume->modified) {
		Mix_VolumeMusic(snd_music_volume->integer);
		snd_music_volume->modified = false;
	}

	if (music.playingStream) {
		M_MusicStreamUpdate();
	} else if (!Mix_PlayingMusic()) {
		M_Stop(); /* free the allocated memory */
		if (Q_strvalid(music.nextTrack)) {
			M_Start(music.nextTrack);
			music.nextTrack[0] = '\0';
		} else {
			if (!M_PlayRandomByCategory(music.category))
				M_Start(music.currentTrack);
		}
	}
}

static const cmdList_t musicCmds[] = {
	{"music_play", M_Play_f, "Plays a music track."},
	{"music_change", M_Change_f, "Changes the music theme (valid values:battlescape/geoscape/main/aircombat)."},
	{"music_stop", M_Stop, "Stops currently playing music track."},
	{"music_randomtrack", M_RandomTrack_f, "Plays a random music track."},
	{nullptr, nullptr, nullptr}
};

void M_Init (void)
{
	if (Cmd_Exists("music_change"))
		Cmd_RemoveCommand("music_change");
	Cmd_TableAddList(musicCmds);
	Cmd_AddParamCompleteFunction("music_play", M_CompleteMusic);
	snd_music = Cvar_Get("snd_music", "PsymongN3", 0, "Background music track");
	snd_music_volume = Cvar_Get("snd_music_volume", "128", CVAR_ARCHIVE, "Music volume - default is 128.");
	snd_music_volume->modified = true;
	snd_music_play = Cvar_Get("snd_music_play", "1", CVAR_ARCHIVE, "Enable background music.");
	music.playing = snd_music_play->integer != 0;
}

void M_Shutdown (void)
{
	M_Stop();

	Cmd_TableRemoveList(musicCmds);
}

static void M_MusicStreamCallback (musicStream_t* userdata, byte* stream, int length)
{
	int tries = 0;
	while (1) {
		if (!userdata->playing) {
			music.interruptStream = true;
			return;
		}
		const int availableBytes = (userdata->mixerPos > userdata->samplePos) ? MAX_RAW_SAMPLES - userdata->mixerPos + userdata->samplePos : userdata->samplePos - userdata->mixerPos;
		if (length < availableBytes)
			break;
		if (++tries > 50) {
			userdata->playing = false;
			return;
		}
		Sys_Sleep(10);
	}

	if (userdata->mixerPos + length <= MAX_RAW_SAMPLES) {
		memcpy(stream, userdata->sampleBuf + userdata->mixerPos, length);
		userdata->mixerPos += length;
		userdata->mixerPos %= MAX_RAW_SAMPLES;
	} else {
		const int end = MAX_RAW_SAMPLES - userdata->mixerPos;
		const int start = length - end;
		memcpy(stream, userdata->sampleBuf + userdata->mixerPos, end);
		memcpy(stream, userdata->sampleBuf, start);
		userdata->mixerPos = start;
	}
}

static void M_PlayMusicStream (musicStream_t* userdata)
{
	if (userdata->playing)
		return;

	M_Stop();

	userdata->playing = true;
	music.playingStream = true;
	Mix_HookMusic((void (*)(void*, Uint8*, int)) M_MusicStreamCallback, userdata);
}

/**
 * @brief Add stereo samples with a 16 byte width to the stream buffer
 * @param[in] samples The amount of stereo samples that should be added to the stream buffer (this
 * is usually 1/4 of the size of the data buffer, one sample should have 4 bytes, 2 for
 * each channel)
 * @param[in] data The stereo sample buffer
 * @param[in,out] userdata The music stream
 * @param[in] rate The sample rate
 */
void M_AddToSampleBuffer (musicStream_t* userdata, int rate, int samples, const byte* data)
{
	int i;

	if (!s_env.initialized)
		return;

	M_PlayMusicStream(userdata);

	if (rate != s_env.rate) {
		const float scale = (float)rate / s_env.rate;
		for (i = 0;; i++) {
			const int src = i * scale;
			short* ptr = (short*)&userdata->sampleBuf[userdata->samplePos];
			if (src >= samples)
				break;
			*ptr = LittleShort(((const short*) data)[src * 2]);
			ptr++;
			*ptr = LittleShort(((const short*) data)[src * 2 + 1]);

			userdata->samplePos += 4;
			userdata->samplePos %= MAX_RAW_SAMPLES;
		}
	} else {
		for (i = 0; i < samples; i++) {
			short* ptr = (short*)&userdata->sampleBuf[userdata->samplePos];
			*ptr = LittleShort(((const short*) data)[i * 2]);
			ptr++;
			*ptr = LittleShort(((const short*) data)[i * 2 + 1]);

			userdata->samplePos += 4;
			userdata->samplePos %= MAX_RAW_SAMPLES;
		}
	}
}

void M_StopMusicStream (musicStream_t* userdata)
{
	if (userdata != nullptr)
		userdata->playing = false;
	music.playingStream = false;
	music.interruptStream = false;
	Mix_HookMusic(nullptr, nullptr);
}
