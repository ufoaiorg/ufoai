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

#include "../cl_shared.h"
#include "cl_radar.h"
#include "../ui/ui_windows.h"

static void CL_BattlescapeRadarOpen_f (void)
{
	UI_PushWindow("radarwindow", NULL, NULL);
}

static void CL_BattlescapeRadarClose_f (void)
{
	UI_CloseWindow("radarwindow");
}

void CL_BattlescapeRadarInit (void)
{
	Cmd_AddCommand("closeradar", CL_BattlescapeRadarClose_f, NULL);
	Cmd_AddCommand("openradar", CL_BattlescapeRadarOpen_f, NULL);
}
