/**
 * @file cl_cinematic.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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


#include "cl_cinematic.h"
#include "cl_cinematic_ogm.h"
#include "cl_cinematic_roq.h"
#include "../cl_console.h"
#include "../sound/s_main.h"
#include "../sound/s_music.h"

cinematic_t cin;

static inline void CIN_Reset (void)
{
	memset(&cin, 0, sizeof(cin));
}

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
	if (cin.cinematicType == CINEMATIC_TYPE_ROQ && CIN_ROQ_RunCinematic())
		return;
	else if (cin.cinematicType == CINEMATIC_TYPE_OGM && CIN_OGM_RunCinematic())
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

	Com_StripExtension(fileName, name, sizeof(name));

	if (cls.playingCinematic <= CIN_STATUS_FULLSCREEN) {
		/* Make sure sounds aren't playing */
		S_Stop();
		/* also stop the background music */
		M_Stop();

		Con_Close();
	}

	/* If already playing a cinematic, stop it */
	CIN_StopCinematic();

	if (FS_CheckFile("%s.roq", name) >= 0)
		CIN_ROQ_PlayCinematic(va("%s.roq", name));
	else if (FS_CheckFile("%s.ogm", name) >= 0)
		CIN_OGM_PlayCinematic(va("%s.ogm", name));
	else
		Com_Printf("Could not find cinematic '%s'\n", name);
}

/**
 * @sa CIN_PlayCinematic
 */
void CIN_StopCinematic (void)
{
	if (cls.playingCinematic == CIN_STATUS_NONE)
		return;			/* Not playing */

	cls.playingCinematic = CIN_STATUS_NONE;

	if (cin.cinematicType == CINEMATIC_TYPE_ROQ)
		CIN_ROQ_StopCinematic();
	else if (cin.cinematicType == CINEMATIC_TYPE_OGM)
		CIN_OGM_StopCinematic();

	CIN_Reset();
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

	CIN_PlayCinematic(va("videos/%s", Cmd_Argv(1)));
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

	len = strlen(partial);
	if (!len) {
		FS_BuildFileList("videos/*.roq");
		while ((filename = FS_NextFileFromFileList("videos/*.roq")) != NULL) {
			Com_Printf("%s\n", filename);
		}
		FS_NextFileFromFileList(NULL);

		FS_BuildFileList("videos/*.ogm");
		while ((filename = FS_NextFileFromFileList("videos/*.ogm")) != NULL) {
			Com_Printf("%s\n", filename);
		}
		return 0;
	}

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

	FS_BuildFileList("videos/*.ogm");
	while ((filename = FS_NextFileFromFileList("videos/*.ogm")) != NULL) {
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

	CIN_Reset();

	CIN_ROQ_Init();
	CIN_OGM_Init();
}

void CIN_Shutdown (void)
{
	/* Unregister our commands */
	Cmd_RemoveCommand("cinematic");
	Cmd_RemoveCommand("cinematic_stop");

	/* If playing a cinematic, stop it */
	CIN_StopCinematic();
}
