/**
 * @file s_music.c
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "s_local.h"
#include "s_main.h"
#include "s_music.h"
#include "../../shared/parse.h"
#include "../../ports/system.h"

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
	Mix_Music *data;
	byte *buffer;
	qboolean playingStream; /**< if this is set no action to M_Start and M_Stop might happen, otherwise we might run
							 * into a deadlock. This is due to the installed hook function for music mixing that is used
							 * whenever we stream the music on our own */
} music_t;

#define MUSIC_MAX_ENTRIES 64
static char *musicArrays[MUSIC_MAX][MUSIC_MAX_ENTRIES];
static int musicArrayLength[MUSIC_MAX];
static music_t music;
static int musicTrackCount;
static cvar_t *snd_music;
static cvar_t *snd_music_volume;

/**
 * @brief Parses music definitions for different situations
 * @note We can have lists for geoscape and battlescape e.g.
 */
void M_ParseMusic (const char *name, const char **text)
{
	const char *errhead = "M_ParseMusic: unexpected end of file (campaign ";
	const char *token;
	int i;

	if (!strcmp(name, "geoscape"))
		i = MUSIC_GEOSCAPE;
	else if (!strcmp(name, "battlescape"))
		i = MUSIC_BATTLESCAPE;
	else if (!strcmp(name, "aircombat"))
		i = MUSIC_AIRCOMBAT;
	else if (!strcmp(name, "main"))
		i = MUSIC_MAIN;
	else {
		Com_Printf("M_ParseMusic: Invalid music id '%s'\n", name);
		FS_SkipBlock(text);
		return;
	}

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("M_ParseMusic: music def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (musicArrayLength[i] >= MUSIC_MAX_ENTRIES) {
			Com_Printf("M_ParseMusic: Too many music entries for category: '%s'\n", name);
			FS_SkipBlock(text);
			break;
		}
		musicArrays[i][musicArrayLength[i]] = Mem_PoolStrDup(token, cl_genericPool, 0);
		musicArrayLength[i]++;
	} while (*text);
}

/**
 * @sa M_Start
 */
void M_Stop (void)
{
	/* we should not even have a buffer nor data set - but ... just to be sure */
	if (music.playingStream)
		return;

	if (music.data != NULL) {
		Mix_HaltMusic();
		Mix_FreeMusic(music.data);
	}

	if (music.buffer)
		FS_FreeFile(music.buffer);

	music.data = NULL;
	music.buffer = NULL;
}

/**
 * @sa M_Stop
 */
static void M_Start (const char *file)
{
	char name[MAX_QPATH];
	size_t len;
	byte *musicBuf;
	int size;
	SDL_RWops *rw;

	if (!file || file[0] == '\0')
		return;

	if (!s_env.initialized) {
		Com_Printf("M_Start: no sound started\n");
		return;
	}

	if (music.playingStream)
		return;

	Com_StripExtension(file, name, sizeof(name));
	len = strlen(name);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("M_Start: MAX_QPATH exceeded: "UFO_SIZE_T"\n", len + 4);
		return;
	}

	/* we are already playing that track */
	if (!strcmp(name, music.currentTrack))
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
		Com_Printf("M_Start: Could not load '%s' background track\n", name);
		return;
	}

	rw = SDL_RWFromMem(musicBuf, size);
	if (!rw) {
		Com_Printf("M_Start: Could not load music: 'music/%s'\n", name);
		FS_FreeFile(musicBuf);
		return;
	}
	music.data = Mix_LoadMUS_RW(rw);
	if (!music.data) {
		Com_Printf("M_Start: Could not load music: 'music/%s' (%s)\n", name, Mix_GetError());
		SDL_FreeRW(rw);
		FS_FreeFile(musicBuf);
		return;
	}

	Q_strncpyz(music.currentTrack, name, sizeof(music.currentTrack));
	music.buffer = musicBuf;
	if (Mix_FadeInMusic(music.data, -1, 1500) == -1)
		Com_Printf("M_Start: Could not play music: 'music/%s' (%s)\n", name, Mix_GetError());
}

/**
 * @brief Plays the music file given via commandline parameter
 */
static void M_Play_f (void)
{
	if (Cmd_Argc() == 2)
		Cvar_Set("snd_music", Cmd_Argv(1));

	M_Start(Cvar_GetString("snd_music"));
}

/**
 * @brief Sets the music cvar to a random track
 */
static void M_RandomTrack_f (void)
{
	const char *filename;
	char findname[MAX_OSPATH];
	int randomID;
	const char *musicTrack;

	if (!s_env.initialized)
		return;

	musicTrackCount = FS_BuildFileList("music/*.ogg");
	if (musicTrackCount) {
		randomID = rand() % musicTrackCount;
		Com_DPrintf(DEBUG_SOUND, "M_RandomTrack_f: random track id: %i/%i\n", randomID, musicTrackCount);

		while ((filename = FS_NextFileFromFileList("music/*.ogg")) != NULL) {
			if (!randomID) {
				Com_sprintf(findname, sizeof(findname), "%s", filename);
				musicTrack = Com_SkipPath(findname);
				Com_Printf("..playing next: '%s'\n", musicTrack);
				Cvar_Set("snd_music", musicTrack);
			}
			randomID--;
		}
		FS_NextFileFromFileList(NULL);
	} else {
		Com_DPrintf(DEBUG_SOUND, "M_RandomTrack_f: No musics found!\n");
	}
}

/**
 * @brief Changes the music if it suits the current situation
 * @todo Make the music a scriptable list
 */
static void M_Change_f (void)
{
	const char* type;
	int rnd;
	int category;

	if (!s_env.initialized)
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <geoscape|battlescape|main|aircombat>\n", Cmd_Argv(0));
		return;
	}
	type = Cmd_Argv(1);
	if (!strcmp(type, "geoscape")) {
		category = MUSIC_GEOSCAPE;
	} else if (!strcmp(type, "battlescape")) {
		category = MUSIC_BATTLESCAPE;
	} else if (!strcmp(type, "main")) {
		category = MUSIC_MAIN;
	} else if (!strcmp(type, "aircombat")) {
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

static int M_CompleteMusic (const char *partial, const char **match)
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
		if (!strncmp(partial, filename, len)) {
			Com_Printf("%s\n", filename);
			localMatch[matches++] = filename;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}
	FS_NextFileFromFileList(NULL);

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

void M_Frame (void)
{
	if (snd_music->modified) {
		M_Start(snd_music->string);
		snd_music->modified = qfalse;
	}
	if (snd_music_volume->modified) {
		Mix_VolumeMusic(snd_music_volume->integer);
		snd_music_volume->modified = qfalse;
	}
	if (!music.playingStream && music.nextTrack[0] != '\0') {
		if (!Mix_PlayingMusic()) {
			M_Stop(); /* free the allocated memory */
			M_Start(music.nextTrack);
			music.nextTrack[0] = '\0';
		}
	}
}

void M_Init (void)
{
	if (Cmd_Exists("music_change"))
		Cmd_RemoveCommand("music_change");
	Cmd_AddCommand("music_play", M_Play_f, "Plays a background sound track");
	Cmd_AddCommand("music_change", M_Change_f, "Changes the music theme");
	Cmd_AddCommand("music_stop", M_Stop, "Stops currently playing music tracks");
	Cmd_AddCommand("music_randomtrack", M_RandomTrack_f, "Plays a random background track");
	Cmd_AddParamCompleteFunction("music_play", M_CompleteMusic);
	snd_music = Cvar_Get("snd_music", "PsymongN3", 0, "Background music track");
	snd_music_volume = Cvar_Get("snd_music_volume", "128", CVAR_ARCHIVE, "Music volume - default is 128");
	snd_music_volume->modified = qtrue;

	memset(&music, 0, sizeof(music));
}

void M_Shutdown (void)
{
	M_Stop();

	Cmd_RemoveCommand("music_play");
	Cmd_RemoveCommand("music_change");
	Cmd_RemoveCommand("music_stop");
	Cmd_RemoveCommand("music_randomtrack");
}

static void M_MusicStreamCallback (musicStream_t *userdata, byte *stream, int length)
{
	while (1) {
		const int availableBytes = (userdata->mixerPos > userdata->samplePos) ? MAX_RAW_SAMPLES - userdata->mixerPos + userdata->samplePos : userdata->samplePos - userdata->mixerPos;
		if (!userdata->playing)
			return;
		if (length < availableBytes)
			break;
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

/**
 * @brief Add stereo samples with a 16 byte width to the stream buffer
 * @param[in] samples The amount of stereo samples that should be added to the stream buffer (this
 * is usually 1/4 of the size of the data buffer, one sample should have 4 bytes, 2 for
 * each channel)
 * @param[in] data The stereo sample buffer
 * @param[in,out] userdata The music stream
 * @param[in] rate The sample rate
 */
void M_AddToSampleBuffer (musicStream_t *userdata, int rate, int samples, const byte *data)
{
	int i;

	if (!s_env.initialized)
		return;

	if (rate != s_env.rate) {
		const float scale = (float)rate / s_env.rate;
		for (i = 0;; i++) {
			const int src = i * scale;
			short *ptr = (short *)&userdata->sampleBuf[userdata->samplePos];
			if (src >= samples)
				break;
			*ptr = LittleShort(((const short *) data)[src * 2]);
			ptr++;
			*ptr = LittleShort(((const short *) data)[src * 2 + 1]);

			userdata->samplePos += 4;
			userdata->samplePos %= MAX_RAW_SAMPLES;
		}
	} else {
		for (i = 0; i < samples; i++) {
			short *ptr = (short *)&userdata->sampleBuf[userdata->samplePos];
			*ptr = LittleShort(((const short *) data)[i * 2]);
			ptr++;
			*ptr = LittleShort(((const short *) data)[i * 2 + 1]);

			userdata->samplePos += 4;
			userdata->samplePos %= MAX_RAW_SAMPLES;
		}
	}
}

void M_PlayMusicStream (musicStream_t *userdata)
{
	M_Stop();

	userdata->playing = qtrue;
	music.playingStream = qtrue;
	Mix_HookMusic((void (*)(void*, Uint8*, int)) M_MusicStreamCallback, userdata);
}

void M_StopMusicStream (musicStream_t *userdata)
{
	userdata->playing = qfalse;
	music.playingStream = qfalse;
	Mix_HookMusic(NULL, NULL);
}

