/**
 * @file m_sound.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "m_main.h"
#include "m_sound.h"
#include "../sound/s_main.h"

cvar_t *mn_sounds;

/**
 * @brief Plays a ui sound
 * @param soundFile Sound file relative to sound/gui
 */
void MN_PlaySound (const char *soundFile)
{
	if (mn_sounds->integer)
		S_StartLocalSample(va("gui/%s", soundFile), SND_VOLUME_DEFAULT);
}
