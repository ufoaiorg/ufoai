/**
 * @file e_event_sound.c
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_sound.h"

/**
 * @brief Play a sound on the client side
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg holds the network data
 * @sa EV_SOUND
 * @note if the last character of the sound file string that was sent by the
 * server is a '+', we will select a random sound file by replacing the '+'
 * character with a number between 01..99
 */
void CL_SoundEvent (const eventRegister_t *self, struct dbuffer *msg)
{
	char sound[MAX_QPATH];
	vec3_t origin;
	int number;
	le_t *le;
	size_t length;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &number, &origin, &sound, sizeof(sound));

	le = LE_Get(number);
	if (le) {
		if (LE_IsLivingActor(le) && le->team != cls.team) {
			/** @todo render */
		} else if (LE_IsDoor(le) || LE_IsBreakable(le)) {
			/** @todo render */
		}
	}

	length = strlen(sound) - 1;
	if (sound[length] == '+') {
		int i;

		sound[length] = '\0';
		for (i = 1; i <= 99; i++) {
			if (FS_CheckFile("sounds/%s%02i", sound, i) == -1)
				break;
		}

		Com_sprintf(sound + length, sizeof(sound) - length, "%02i", rand() % i + 1);
	}

	Com_DPrintf(DEBUG_SOUND, "Play network sample %s at (%f:%f:%f)\n", sound, origin[0], origin[1], origin[2]);
	S_LoadAndPlaySample(sound, origin, SOUND_ATTN_NORM, SND_VOLUME_DEFAULT);
}
