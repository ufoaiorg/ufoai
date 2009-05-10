/**
 * @file s_sample.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "s_local.h"
#include "s_sample.h"

/**
 * @note Called at precache phase - only load these soundfiles once at startup or on sound restart
 * @sa S_Restart_f
 */
void S_RegisterSounds (void)
{
	int i, j, k;

	/* load weapon sounds */
	for (i = 0; i < csi.numODs; i++) { /* i = obj */
		for (j = 0; j < csi.ods[i].numWeapons; j++) {	/* j = weapon-entry per obj */
			for (k = 0; k < csi.ods[i].numFiredefs[j]; j++) { /* k = firedef per wepaon */
				if (csi.ods[i].fd[j][k].fireSound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].fireSound);
				if (csi.ods[i].fd[j][k].impactSound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].impactSound);
				if (csi.ods[i].fd[j][k].hitBodySound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].hitBodySound);
				if (csi.ods[i].fd[j][k].bounceSound[0] != '\0')
					S_RegisterSound(csi.ods[i].fd[j][k].bounceSound);
			}
		}
	}

	/* precache the sound pool */
	cls.sound_pool[SOUND_WATER_IN] = S_RegisterSound("footsteps/water_in");
	cls.sound_pool[SOUND_WATER_OUT] = S_RegisterSound("footsteps/water_out");
	cls.sound_pool[SOUND_WATER_MOVE] = S_RegisterSound("footsteps/water_under");
}
