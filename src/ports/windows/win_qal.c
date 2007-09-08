/**
 * @file win_qal.c
 */

/*
Copyright (C) 1997-2001 UFO:AI team

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

#ifdef HAVE_OPENAL

#include <windows.h>
#include "../../client/qal.h"
#include "win_qal.h"

/**
 * @brief Binds our QAL function pointers to the appropriate AL stuff
 * @sa QAL_Shutdown
 */
qboolean QAL_Init (void)
{
	Com_DPrintf(DEBUG_SYSTEM, "...calling LoadLibrary( '%s' ): ", AL_DRIVER_OPENAL);

	if ((oalState.lib = LoadLibrary(AL_DRIVER_OPENAL)) == NULL) {
		Com_Printf("OpenAL loading failed\n");
		return qfalse;
	}

	if (!QAL_Link())
		return qfalse;

	openal_active = qtrue;

	return qtrue;
}

#endif /* HAVE_OPENAL */
