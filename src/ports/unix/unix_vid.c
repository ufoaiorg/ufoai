/**
 * @file unix_vid.c
 * @brief Main windowed and fullscreen graphics interface module.
 * @note This module is used for the OpenGL rendering versions of the UFO refresh engine.
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

#include <assert.h>
#include <dlfcn.h> /* ELF dl loader */
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
/* #include <uuid/uuid.h> */

#include "../../client/client.h"

#include "unix_input.h"

/* Console variables that we need to access from this module */
extern cvar_t *vid_xpos;			/* X coordinate of window position */
extern cvar_t *vid_ypos;			/* Y coordinate of window position */
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_grabmouse;
extern cvar_t *vid_gamma;
extern cvar_t *vid_ref;			/* Name of Refresh DLL loaded */

/* Global variables used internally by this module */
extern viddef_t viddef;				/* global video state; used by other modules */
qboolean	reflib_active = qfalse;

/*
==========================================================================
DLL GLUE
==========================================================================
*/

int maxVidModes;
/**
 * @brief
 */
const vidmode_t vid_modes[] =
{
	{ 320, 240,   0 },
	{ 400, 300,   1 },
	{ 512, 384,   2 },
	{ 640, 480,   3 },
	{ 800, 600,   4 },
	{ 960, 720,   5 },
	{ 1024, 768,  6 },
	{ 1152, 864,  7 },
	{ 1280, 1024, 8 },
	{ 1600, 1200, 9 },
	{ 2048, 1536, 10 },
	{ 1024,  480, 11 }, /* Sony VAIO Pocketbook */
	{ 1152,  768, 12 }, /* Apple TiBook */
	{ 1280,  854, 13 }, /* Apple TiBook */
	{ 640,  400, 14 }, /* generic 16:10 widescreen*/
	{ 800,  500, 15 }, /* as found modern */
	{ 1024,  640, 16 }, /* notebooks    */
	{ 1280,  800, 17 },
	{ 1680, 1050, 18 },
	{ 1920, 1200, 19 },
	{ 1400, 1050, 20 }, /* samsung x20 */
	{ 1440, 900, 21 }
};

/**
 * @brief
 */
void VID_NewWindow (int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	viddef.rx = (float)width  / VID_NORM_WIDTH;
	viddef.ry = (float)height / VID_NORM_HEIGHT;
}

/**
 * @brief This function gets called once just before drawing each frame, and it's sole purpose in life
 * is to check to see if any of the video mode parameters have changed, and if they have to
 * update the rendering DLL and/or video mode to match.
 */
void VID_CheckChanges (void)
{
}

/**
 * @brief
 */
void Sys_Vid_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get("vid_ref", "sdl", CVAR_ARCHIVE, "Video renderer");

	maxVidModes = VID_NUM_MODES;
}

/**
 * @brief
 */
void VID_Shutdown (void)
{
	KBD_Close();
	IN_Shutdown();
	R_Shutdown();
}
