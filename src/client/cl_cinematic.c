/**
 * @file cl_cinematic.c
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "cl_cinematic.h"
#include "cl_cinematic_roq.h"
#include "cl_sound.h"
#include "cl_music.h"

cinematic_t cin;

/**
 * @sa MN_Draw
 * @note Coordinates should be relative to VID_NORM_WIDTH and VID_NORM_HEIGHT
 * they are normalized inside this function
 */
void CIN_SetParameters (int x, int y, int w, int h, int cinStatus, qboolean noSound)
{
	cin.x = x * viddef.rx;
	cin.y = y * viddef.ry;
	cin.w = w * viddef.rx;
	cin.h = h * viddef.ry;
	if (cinStatus > CIN_STATUS_NONE)
		cls.playingCinematic = cinStatus;
	cin.noSound = noSound;
}

/**
 * @sa CL_Frame
 */
void CIN_RunCinematic (void)
{
	assert(cls.playingCinematic != CIN_STATUS_NONE);

	/* Decode chunks until the desired frame is reached */
	if (CIN_ROQ_RunCinematic())
		return;

	/* If we get here, the cinematic has either finished or failed */
	if (cin.replay) {
		char name[MAX_QPATH];
		Q_strncpyz(name, cin.name, sizeof(name));
		CIN_PlayCinematic(name);
		cin.replay = qtrue;
	} else {
		CIN_StopCinematic();
	}
}

/**
 * @sa CIN_StopCinematic
 */
void CIN_PlayCinematic (const char *fileName)
{
	char name[MAX_OSPATH];

	Com_sprintf(name, sizeof(name), "videos/%s", fileName);
	Com_DefaultExtension(name, sizeof(name), ".roq");

	if (cls.playingCinematic <= CIN_STATUS_FULLSCREEN) {
		/* Make sure sounds aren't playing */
		S_StopAllSounds();
		/* also stop the background music */
		M_Shutdown();
	}

	/* If already playing a cinematic, stop it */
	CIN_StopCinematic();

	CIN_ROQ_PlayCinematic(name);
}

/**
 * @sa CIN_PlayCinematic
 */
void CIN_StopCinematic (void)
{
	if (cls.playingCinematic == CIN_STATUS_NONE)
		return;			/* Not playing */

	cls.playingCinematic = CIN_STATUS_NONE;

	if (!cin.noSound) {
		/** @todo only stop the cin.soundChannel channel - but don't call
		 * @c Mix_HaltChannel directly */
		S_StopAllSounds();
	}

	CIN_ROQ_StopCinematic();

	memset(&cin, 0, sizeof(cin));
}

static void CIN_Cinematic_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <videoName>\n", Cmd_Argv(0));
		return;
	}

	/* If running a local server, kill it */
	SV_Shutdown("Server quit", qfalse);

	/* If connected to a server, disconnect */
	CL_Disconnect();

	CIN_PlayCinematic(Cmd_Argv(1));
}

/**
 * @brief Stop the current active cinematic
 */
static void CIN_CinematicStop_f (void)
{
	if (cls.playingCinematic == CIN_STATUS_NONE) {
		Com_Printf("No cinematic active\n");
		return;
	}
	CIN_StopCinematic();
}

static int CIN_CompleteCommand (const char *partial, const char **match)
{
	const char *filename;
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;

	FS_BuildFileList("videos/*.roq");

	len = strlen(partial);
	if (!len) {
		while ((filename = FS_NextFileFromFileList("videos/*.roq")) != NULL) {
			Com_Printf("%s\n", filename);
		}
		FS_NextFileFromFileList(NULL);
		return 0;
	}

	/* start from first file entry */
	FS_NextFileFromFileList(NULL);

	/* check for partial matches */
	while ((filename = FS_NextFileFromFileList("videos/*.roq")) != NULL) {
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

void CIN_Init (void)
{
	/* Register our commands */
	Cmd_AddCommand("cinematic", CIN_Cinematic_f, "Plays a cinematic");
	Cmd_AddParamCompleteFunction("cinematic", CIN_CompleteCommand);
	Cmd_AddCommand("cinematic_stop", CIN_CinematicStop_f, "Stops a cinematic");

	memset(&cin, 0, sizeof(cin));

	CIN_ROQ_Init();
}

void CIN_Shutdown (void)
{
	/* Unregister our commands */
	Cmd_RemoveCommand("cinematic");
	Cmd_RemoveCommand("cinematic_stop");

	/* If playing a cinematic, stop it */
	CIN_StopCinematic();
}
