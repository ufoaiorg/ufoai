/**
 * @file cl_video.c
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
#include "battlescape/cl_view.h"
#include "renderer/r_main.h"
#include "renderer/r_sdl.h"
#include "ui/ui_main.h"
#include "cl_game.h"

viddef_t viddef;	/* global video state; used by other modules */

cvar_t *vid_strech;
cvar_t *vid_fullscreen;
cvar_t *vid_mode;
cvar_t *vid_grabmouse;
cvar_t *vid_gamma;
cvar_t *vid_ignoregamma;
static cvar_t *vid_height;
static cvar_t *vid_width;

/**
 * @brief All possible video modes
 */
static const vidmode_t vid_modes[] =
{
	{  320,  240,  0 },
	{  400,  300,  1 },
	{  512,  384,  2 },
	{  640,  480,  3 },
	{  800,  600,  4 },
	{  960,  720,  5 },
	{ 1024,  768,  6 },
	{ 1152,  864,  7 },
	{ 1280, 1024,  8 },
	{ 1600, 1200,  9 },
	{ 2048, 1536, 10 },
	{ 1024,  480, 11 }, /* Sony VAIO Pocketbook */
	{ 1152,  768, 12 }, /* Apple TiBook */
	{ 1280,  854, 13 }, /* Apple TiBook */
	{  640,  400, 14 }, /* generic 16:10 widescreen*/
	{  800,  500, 15 }, /* as found modern */
	{ 1024,  640, 16 }, /* notebooks */
	{ 1280,  800, 17 },
	{ 1680, 1050, 18 },
	{ 1920, 1200, 19 },
	{ 1400, 1050, 20 }, /* samsung x20 */
	{ 1440,  900, 21 },
	{ 1024,  600, 22 } /* EEE PC */
};

/**
 * @brief Returns the amount of available video modes
 */
int VID_GetModeNums (void)
{
	if (r_sdl_config.numModes > 0)
		return r_sdl_config.numModes;
	return lengthof(vid_modes);
}

qboolean VID_GetModeInfo (int modeIndex, vidmode_t *modeInfo)
{
	if (modeIndex < 0) {
		modeInfo->width = vid_width->integer;
		modeInfo->height = vid_height->integer;
	} else if (modeIndex < VID_GetModeNums()) {
		int width, height;
		if (r_sdl_config.numModes > 0) {
			width = r_sdl_config.modes[modeIndex]->w;
			height = r_sdl_config.modes[modeIndex]->h;
		} else {
			width = vid_modes[modeIndex].width;
			height = vid_modes[modeIndex].height;
		}
		modeInfo->width = width;
		modeInfo->height = height;
	} else {
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Perform a renderer restart
 */
void VID_Restart_f (void)
{
	refdef.ready = qfalse;

	Com_Printf("renderer restart\n");

	R_Shutdown();
	R_Init();
	UI_Reinit();
	/** @todo only reload the skins, not all models */
	CL_ViewPrecacheModels();

	/** @todo going back into the map isn't working as long as GAME_ReloadMode is called */
	/*CL_ViewLoadMedia();*/
	GAME_ReloadMode();
}

static qboolean CL_CvarCheckVidGamma (cvar_t *cvar)
{
	return Cvar_AssertValue(cvar, 0.1, 3.0, qfalse);
}

static qboolean CL_CvarCheckVidMode (cvar_t *cvar)
{
	return Cvar_AssertValue(cvar, -1, VID_GetModeNums(), qtrue);
}

/**
 * @sa R_Shutdown
 */
void VID_Init (void)
{
	vid_strech = Cvar_Get("vid_strech", "0", CVAR_ARCHIVE, "Backward compatibility to stretch the screen with a 4:3 ratio");
	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE, "Run the game in fullscreen mode");
	vid_mode = Cvar_Get("vid_mode", "-1", CVAR_ARCHIVE, "The video mode - set to -1 and use vid_width and vid_height to use a custom resolution");
	Cvar_SetCheckFunction("vid_mode", CL_CvarCheckVidMode);
	vid_grabmouse = Cvar_Get("vid_grabmouse", "0", CVAR_ARCHIVE, "Grab the mouse in the game window - open the console to switch back to your desktop via Alt+Tab");
	vid_gamma = Cvar_Get("vid_gamma", "1", CVAR_ARCHIVE, "Controls the gamma settings");
	vid_ignoregamma = Cvar_Get("vid_ignoregamma", "0", CVAR_ARCHIVE, "Don't control the gamma settings if set to 1");
	Cvar_SetCheckFunction("vid_gamma", CL_CvarCheckVidGamma);
	vid_height = Cvar_Get("vid_height", "768", CVAR_ARCHIVE, "Custom video height - set vid_mode to -1 to use this");
	vid_width = Cvar_Get("vid_width", "1024", CVAR_ARCHIVE, "Custom video width - set vid_mode to -1 to use this");

	Cmd_AddCommand("vid_restart", VID_Restart_f, "Restart the renderer - or change the resolution");

	/* memory pools */
	vid_genericPool = Mem_CreatePool("Vid: Generic");
	vid_imagePool = Mem_CreatePool("Vid: Image system");
	vid_lightPool = Mem_CreatePool("Vid: Light system");
	vid_modelPool = Mem_CreatePool("Vid: Model system");

	/* Start the graphics mode */
	R_Init();
}
