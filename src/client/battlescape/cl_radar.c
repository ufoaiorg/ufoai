/**
 * @file cl_radar.c
 * @brief Battlescape radar code
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

#include "../client.h"
#include "cl_radar.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_misc.h"
#include "../cl_video.h"
#include "../ui/ui_main.h"
#include "../ui/ui_timer.h"
#include "cl_localentity.h"
#include "cl_camera.h"
#include "cl_battlescape.h"

static uiTimer_t* timer;

static void CL_BattlescapeRadarOpen_f (void)
{
	UI_PushWindow("radarwindow", NULL, NULL);
}

static void CL_BattlescapeRadarClose_f (void)
{
	UI_CloseWindow("radarwindow");
}

/**
 * @param[out] x X position of the rect in the frame buffer (from bottom-to-top according to the screen)
 * @param[out] y Y position of the rect in the frame buffer (from bottom-to-top according to the screen)
 * @param[out] width Width of the rect in the frame buffer (from bottom-to-top according to the screen)
 * @param[out] height Height of the rect in the frame buffer (from bottom-to-top according to the screen)
 * @return the rect where the radarmap should be, when we generate radar images
 * @todo fix that function, map is not well captured
 */
static void CL_BattlescapeRadarMapInFrameBuffer (int *x, int *y, int *width, int *height)
{
	/* Coefficient come from metric (Bunker map, and game with resolution 1024x1024) == 0.350792947 */
	static const float magicCoef =  0.351f;
	const float mapWidth = cl.mapData->mapMax[0] - cl.mapData->mapMin[0];
	const float mapHeight = cl.mapData->mapMax[1] - cl.mapData->mapMin[1];

	/* compute width and height with the same round error on both sides */
	/** @todo viddef.context should be removed */
	const int x2 = (viddef.context.width / 2) + (mapWidth * magicCoef * 0.5);
	const int y2 = (viddef.context.height / 2) + (mapHeight * magicCoef * 0.5) + 1;
	*x = (viddef.context.width / 2) - (mapWidth * magicCoef * 0.5);
	*y = (viddef.context.height / 2) - (mapHeight * magicCoef * 0.5);
	*width = (x2 - *x);
	*height = (y2 - *y);
}

static void CL_BattlescapeRadarGenPreview_f (void)
{
	int x, y, width, height;
	/* map to screen */
	CL_BattlescapeRadarMapInFrameBuffer(&x, &y, &width, &height);

	/* from screen to virtual screen */
	x /= viddef.rx;
	width /= viddef.rx;
	y /= viddef.ry;
	height /= viddef.ry;
	y = viddef.virtualHeight - y - height;

	UI_ExecuteConfunc("mn_radarhud_setmapborder %d %d %d %d", x, y, width, height);
}

/**
 * @brief Take a screen shot of the map with the position of the radar
 *
 * We add 1 pixel into the border to easy check the result:
 * the screen shot must have a border of 1 black pixel
 */
static void CL_BattlescapeRadarGenerate_f (void)
{
	const int border = 0;
	const char *mapName = Cvar_GetString("sv_mapname");

	const int level = Cvar_GetInteger("cl_worldlevel");
	const char *filename = NULL;
	int x, y, width, height;

	CL_BattlescapeRadarMapInFrameBuffer(&x, &y, &width, &height);
	if (mapName)
		filename = va("%s_%i", mapName, level + 1);
	R_ScreenShot(x - border, y - border, width + border * 2, height + border * 2, filename, NULL);
}

static void CL_BattlescapeRadarGenerateAll (uiNode_t *node, uiTimer_t *timer)
{
	int level = (timer->calledTime - 1) / 2;
	int mode = (timer->calledTime - 1) % 2;

	if (level >= cl.mapMaxLevel) {
		Cbuf_AddText("ui_genallradarmaprelease");
		return;
	}

	if (mode == 0)
		Cvar_SetValue("cl_worldlevel", level);
	else
		Cmd_ExecuteString("ui_genradarmap");
}

/**
 * @brief Take all screenshots from lower to upper map level.
 * Use a timer to delay each capture
 */
static void CL_BattlescapeRadarGenerateAll_f (void)
{
	const int delay = 1000;
	timer = UI_AllocTimer(NULL, delay, CL_BattlescapeRadarGenerateAll);
	UI_TimerStart(timer);
}

/**
 * @todo allow to call UI_TimerRelease into timer callback
 */
static void CL_BattlescapeRadarGenerateAllRelease_f (void)
{
	UI_TimerRelease(timer);
	UI_ExecuteConfunc("mn_radarhud_reinit");
}

void CL_BattlescapeRadarInit (void)
{
	Cmd_AddCommand("closeradar", CL_BattlescapeRadarClose_f, NULL);
	Cmd_AddCommand("openradar", CL_BattlescapeRadarOpen_f, NULL);

	Cmd_AddCommand("ui_genpreviewradarmap", CL_BattlescapeRadarGenPreview_f, NULL);
	Cmd_AddCommand("ui_genradarmap", CL_BattlescapeRadarGenerate_f, NULL);
	Cmd_AddCommand("ui_genallradarmap", CL_BattlescapeRadarGenerateAll_f, NULL);
	Cmd_AddCommand("ui_genallradarmaprelease", CL_BattlescapeRadarGenerateAllRelease_f, NULL);
}
