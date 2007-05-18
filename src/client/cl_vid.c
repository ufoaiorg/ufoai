/**
 * @file cl_vid.c
 * @brief Shared client functions for windowed and fullscreen graphics interface module.
 */

/*
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

viddef_t viddef;	/* global video state; used by other modules */

cvar_t *vid_fullscreen;
cvar_t *vid_grabmouse;
cvar_t *vid_gamma;
cvar_t *vid_ref;
static cvar_t *vid_height;
static cvar_t *vid_width;

#define	MAXPRINTMSG	4096
/**
 * @brief
 */
void VID_Printf (int print_level, const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (print_level == PRINT_ALL)
		Com_Printf("%s", msg);
	else
		Com_DPrintf("%s", msg);
}

/**
 * @brief Calls Com_Error with err_level
 * @sa Com_Error
 */
void VID_Error (int err_level, const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr,fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Error(err_level, "%s", msg);
}

/**
 * @brief
 */
qboolean VID_GetModeInfo (int *width, int *height, int mode)
{
	if (mode >= maxVidModes)
		return qfalse;
	else if (mode < 0) {
		*width = vid_width->integer;
		*height = vid_height->integer;
	} else {
		*width  = vid_modes[mode].width;
		*height = vid_modes[mode].height;
	}

	return qtrue;
}

/**
 * @brief Console command to re-start the video mode and refresh DLL. We do this
 * simply by setting the modified flag for the vid_ref variable, which will
 * cause the entire video mode and refresh DLL to be reset on the next frame.
 */
void VID_Restart_f (void)
{
	vid_ref->modified = qtrue;
}

/**
 * @brief
 * @sa VID_Shutdown
 */
void VID_Init (void)
{
	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE, "Run the game in fullscreen mode");
	vid_grabmouse = Cvar_Get("vid_grabmouse", "1", CVAR_ARCHIVE, "Grab the mouse in the game window");
	vid_gamma = Cvar_Get("vid_gamma", "1", CVAR_ARCHIVE, NULL);
	vid_height = Cvar_Get("vid_height", "768", CVAR_ARCHIVE, "Custom video height - set gl_mode to -1 to use this");
	vid_width = Cvar_Get("vid_width", "1024", CVAR_ARCHIVE, "Custom video width - set gl_mode to -1 to use this");

	/* Add some console commands that we want to handle */
	Cmd_AddCommand("vid_restart", VID_Restart_f, "Restart the video subsystem");
	Sys_Vid_Init();

	/* Start the graphics mode and load refresh DLL */
	VID_CheckChanges();
}
