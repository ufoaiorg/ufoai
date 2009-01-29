/**
 * @file cl_hud.c
 * @brief HUD related routines.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "cl_hud.h"

/**
 * @brief Displays a message on the hud.
 * @sa MN_DisplayNotice
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void HUD_DisplayMessage (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}

void HUD_InitStartup (void)
{
}
