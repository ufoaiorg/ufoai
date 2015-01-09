/**
 * @file
 * @brief Header file for cinematics
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "../../shared/ufotypes.h"	/* for bool */
#include "../../common/filesys.h"	/* for MAX_QPATH */

enum {
	CINEMATIC_NO_TYPE,
	CINEMATIC_TYPE_ROQ,
	CINEMATIC_TYPE_OGM
};

typedef struct cinematic_s {
	char			name[MAX_QPATH];	/**< virtual filesystem path with file suffix */

	bool		replay;		/**< autmatically replay in endless loop */
	int				x, y, w, h; /**< for drawing in the menu maybe */

	bool		noSound;	/**< no sound while playing the cinematic */
	bool		fullScreen;	/**< if true, video is displayed in fullscreen */

	int cinematicType;

	int status;					/**< Status of the playing */

	void* codecData;
} cinematic_t;

void CIN_OpenCinematic(cinematic_t* cin, const char* name);
void CIN_CloseCinematic(cinematic_t* cin);
void CIN_SetParameters(cinematic_t* cin, int x, int y, int w, int h, int cinStatus, bool noSound);
void CIN_RunCinematic(cinematic_t* cin);
void CIN_InitCinematic(cinematic_t* cin);

void CIN_Init(void);
void CIN_Shutdown(void);

typedef enum {
	CIN_STATUS_NONE,	/**< not playing */
	CIN_STATUS_INVALID,
	CIN_STATUS_PLAYING,
	CIN_STATUS_PAUSE
} cinStatus_t;
