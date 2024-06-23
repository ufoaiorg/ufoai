/**
 * @file
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

#include "cl_video.h"
#include "client.h"
#include "battlescape/cl_view.h"
#include "renderer/r_main.h"
#include "renderer/r_sdl.h"
#include "ui/ui_main.h"
#include "cgame/cl_game.h"

viddef_t viddef;	/* global video state; used by other modules */

cvar_t* vid_stretch;
cvar_t* vid_fullscreen;
cvar_t* vid_mode;
cvar_t* vid_grabmouse;
cvar_t* vid_gamma;
cvar_t* vid_ignoregamma;

static cvar_t* vid_left;
static cvar_t* vid_top;
static cvar_t* vid_width;
static cvar_t* vid_height;

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
	{ 1024,  600, 22 }, /* EEE PC */
	{  800,  480, 23 }, /* OpenPandora */
	{ 1920, 1080, 24 },  /* 1080p */
	{ 1366,  768, 25 }
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

bool VID_GetModeInfo (int modeIndex, vidmode_t* modeInfo)
{
	if (modeIndex < 0) {
		modeInfo->width = vid_width->integer;
		modeInfo->height = vid_height->integer;
		if (modeInfo->width <= 0 || modeInfo->height <= 0) {
			Com_Printf("I: using the desktop resolution because vid_mode, vid_height and vid_width are set to -1 (%ix%i)\n",
					r_sdl_config.desktopWidth, r_sdl_config.desktopHeight);
			modeInfo->width = r_sdl_config.desktopWidth;
			modeInfo->height = r_sdl_config.desktopHeight;
		}
	} else if (modeIndex < VID_GetModeNums()) {
		int width, height;
		if (r_sdl_config.numModes > 0) {
			width = r_sdl_config.modes[modeIndex][0];
			height = r_sdl_config.modes[modeIndex][1];
		} else {
			width = vid_modes[modeIndex].width;
			height = vid_modes[modeIndex].height;
		}
		modeInfo->width = width;
		modeInfo->height = height;
	} else {
		return false;
	}

	return true;
}

/**
 * @brief Perform a renderer restart
 */
static void VID_Restart_f (void)
{
	refdef.ready = false;

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

static bool CL_CvarCheckVidGamma (cvar_t* cvar)
{
	return Cvar_AssertValue(cvar, 0.1, 3.0, false);
}

static bool CL_CvarCheckVidMode (cvar_t* cvar)
{
	return Cvar_AssertValue(cvar, -1, VID_GetModeNums(), true);
}

void VID_Minimize (void)
{
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_MinimizeWindow(cls.window);
#else
	SDL_WM_IconifyWindow();
#endif
}

/**
 * @sa R_Shutdown
 */
void VID_Init (void)
{
	vid_stretch = Cvar_Get("vid_stretch", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Backward compatibility to stretch the screen with a 4:3 ratio");
	vid_fullscreen = Cvar_Get("vid_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Run the game in fullscreen mode");
	vid_mode = Cvar_Get("vid_mode", "-1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "The video mode - set to -1 and use vid_width and vid_height to use a custom resolution");
	Cvar_SetCheckFunction("vid_mode", CL_CvarCheckVidMode);
	vid_grabmouse = Cvar_Get("vid_grabmouse", "0", CVAR_ARCHIVE, "Grab the mouse in the game window - open the console to switch back to your desktop via Alt+Tab");
	vid_gamma = Cvar_Get("vid_gamma", "1", CVAR_ARCHIVE, "Controls the gamma settings");
	vid_ignoregamma = Cvar_Get("vid_ignoregamma", "0", CVAR_ARCHIVE, "Don't control the gamma settings if set to 1");
	Cvar_SetCheckFunction("vid_gamma", CL_CvarCheckVidGamma);
	vid_left = Cvar_Get("vid_left", va("%d", VID_POS_UNSET), CVAR_ARCHIVE, "Screen position for windowed mode, left coordinate");
	vid_top = Cvar_Get("vid_top", va("%d", VID_POS_UNSET), CVAR_ARCHIVE, "Screen position for windowed mode, top coordinate");
	vid_width = Cvar_Get("vid_width", "-1", CVAR_ARCHIVE, "Custom video width - set vid_mode to -1 to use this");
	vid_height = Cvar_Get("vid_height", "-1", CVAR_ARCHIVE, "Custom video height - set vid_mode to -1 to use this");

	Cmd_AddCommand("vid_restart", VID_Restart_f, "Restart the renderer - or change the resolution");
	Cmd_AddCommand("vid_minimize", VID_Minimize, "Minimize the game window");

	/* memory pools */
	vid_genericPool = Mem_CreatePool("Vid: Generic");
	vid_imagePool = Mem_CreatePool("Vid: Image system");
	vid_lightPool = Mem_CreatePool("Vid: Light system");
	vid_modelPool = Mem_CreatePool("Vid: Model system");

	/* Start the graphics mode */
	R_Init();
}
