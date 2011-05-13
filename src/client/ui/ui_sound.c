/**
 * @file ui_sound.c
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

#include "ui_main.h"
#include "ui_sound.h"
#include "../sound/s_main.h"

cvar_t *ui_sounds;

/**
 * @brief Plays a ui sound
 * @param soundFile Sound file relative to sound/gui
 */
void UI_PlaySound (const char *soundFile)
{
	if (ui_sounds->integer)
		S_StartLocalSample(va("ui/%s", soundFile), SND_VOLUME_DEFAULT);
}
