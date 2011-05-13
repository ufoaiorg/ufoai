/**
 * @file cl_cinematic.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
/* trying to get rid of client.h ... */
#include <assert.h>
#include "../cl_video.h"	/* viddef */
#include "../../shared/shared.h"	/* Q_strncpyz */

void CIN_InitCinematic (cinematic_t *cin)
{
	OBJZERO(*cin);
}

/**
 * @sa UI_Draw
 * @note Coordinates should be relative to VID_NORM_WIDTH and VID_NORM_HEIGHT
 * they are normalized inside this function
 */
void CIN_SetParameters (cinematic_t *cin, int x, int y, int w, int h, int status, qboolean noSound)
{
	cin->x = x * viddef.rx;
	cin->y = y * viddef.ry;
	cin->w = w * viddef.rx;
	cin->h = h * viddef.ry;
	if (status > CIN_STATUS_NONE)
		cin->status = status;
	cin->noSound = noSound;
}

/**
 * @sa CL_Frame
 */
void CIN_RunCinematic (cinematic_t *cin)
{
	assert(cin->status != CIN_STATUS_NONE);

	/* Decode chunks until the desired frame is reached */
	if (cin->cinematicType == CINEMATIC_TYPE_ROQ && CIN_ROQ_RunCinematic(cin))
		return;
	else if (cin->cinematicType == CINEMATIC_TYPE_OGM && CIN_OGM_RunCinematic(cin))
		return;

	/* If we get here, the cinematic has either finished or failed */
	if (cin->replay) {
		char name[MAX_QPATH];
		Q_strncpyz(name, cin->name, sizeof(name));
		CIN_OpenCinematic(cin, name);
		cin->replay = qtrue;
	} else {
		CIN_CloseCinematic(cin);
	}
}

/**
 * @brief Open a cinematic file and store status to a structure
 * @sa CIN_CloseCinematic
 */
void CIN_OpenCinematic (cinematic_t *cin, const char *fileName)
{
	char name[MAX_OSPATH];
	int status = 1;

	Com_StripExtension(fileName, name, sizeof(name));

	/* If already playing a cinematic, stop it */
	CIN_CloseCinematic(cin);

	if (FS_CheckFile("%s.roq", name) >= 0)
		status = CIN_ROQ_OpenCinematic(cin, va("%s.roq", name));
	else if (FS_CheckFile("%s.ogm", name) >= 0)
		status = CIN_OGM_OpenCinematic(cin, va("%s.ogm", name));

	if (status != 0) {
		Com_Printf("Could not load cinematic '%s'\n", name);
		cin->status = CIN_STATUS_INVALID;
	}
}

/**
 * @brief Close a cinematic, and clean up status and memory
 * @sa CIN_PlayCinematic
 */
void CIN_CloseCinematic (cinematic_t *cin)
{
	if (cin->status == CIN_STATUS_NONE)
		return;			/* Not playing */

	cin->status = CIN_STATUS_NONE;

	if (cin->cinematicType == CINEMATIC_TYPE_ROQ)
		CIN_ROQ_CloseCinematic(cin);
	else if (cin->cinematicType == CINEMATIC_TYPE_OGM)
		CIN_OGM_CloseCinematic(cin);

	CIN_InitCinematic(cin);
}

void CIN_Init (void)
{
	CIN_ROQ_Init();
	CIN_OGM_Init();
}

void CIN_Shutdown (void)
{
}
