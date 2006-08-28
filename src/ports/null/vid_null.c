/**
 * @file vid_null.c
 * @brief null video driver to aid porting efforts
 * @note this assumes that one of the refs is statically linked to the executable
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
/* vid_null.c -- null video driver to aid porting efforts */
/* this assumes that one of the refs is statically linked to the executable */

#include "../../client/client.h"

viddef_t	viddef;				/* global video state */

refexport_t	re;

refexport_t GetRefAPI (refimport_t rimp);

/*
==========================================================================
DIRECT LINK GLUE
==========================================================================
*/

#define	MAXPRINTMSG	4096
/**
 * @brief
 */
void VID_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	if (print_level == PRINT_ALL)
		Com_Printf ("%s", msg);
	else
		Com_DPrintf ("%s", msg);
}

/**
 * @brief
 */
void VID_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Com_Error (err_level, "%s", msg);
}

/**
 * @brief
 */
void VID_NewWindow (int width, int height)
{
	viddef.width = width;
	viddef.height = height;

	viddef.rx = (float)width  / VID_NORM_WIDTH;
	viddef.ry = (float)height / VID_NORM_HEIGHT;
}

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
	{ 1280, 960, 8 },
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
	{ 1920, 1200, 19 }
};
#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

/**
 * @brief
 */
qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return qfalse;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return qtrue;
}


/**
 * @brief
 */
void VID_Init (void)
{
	refimport_t	ri;

	viddef.width = 320;
	viddef.height = 240;

	viddef.rx = (float)width  / VID_NORM_WIDTH;
	viddef.ry = (float)height / VID_NORM_HEIGHT;

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_WriteFile = FS_WriteFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_CheckFile = FS_CheckFile;
	ri.FS_ListFiles = FS_ListFiles;
	ri.FS_Gamedir = FS_Gamedir;
	ri.FS_NextPath = FS_NextPath;
	ri.Vid_NewWindow = VID_NewWindow;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	ri.CL_GetFontData = CL_GetFontData;

	re = GetRefAPI(ri);

	if (re.api_version != API_VERSION)
		Com_Error (ERR_FATAL, "Re has incompatible api_version");

	/* call the init function */
	if (re.Init (NULL, NULL) == -1)
		Com_Error (ERR_FATAL, "Couldn't start refresh");
}

/**
 * @brief
 */
void VID_Shutdown (void)
{
	if (re.Shutdown)
		re.Shutdown ();
}

/**
 * @brief
 */
void VID_CheckChanges (void)
{
}

/**
 * @brief
 */
void VID_MenuInit (void)
{
}
