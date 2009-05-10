/**
 * @file s_mix.c
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
#include "s_mix.h"
#include "s_main.h"

#define DISTANCE_SCALE 0.8

void S_FreeChannel (int c)
{
	memset(&s_env.channels[c], 0, sizeof(s_env.channels[0]));
}

/**
 * @brief Set distance and stereo panning for the specified channel.
 */
void S_SpatializeChannel (const s_channel_t *ch)
{
	vec3_t origin, delta;
	float dist, angle;
	const int c = (int)((ptrdiff_t)(ch - s_env.channels));
	const le_t *actor;

	VectorCopy(ch->org, origin);

	actor = LE_GetClosestActor(origin);
	if (actor)
		VectorSubtract(origin, actor->origin, delta);
	else
		VectorSubtract(origin, cl.cam.camorg, delta);

	dist = VectorNormalize(delta) * DISTANCE_SCALE;

	if (dist > 255.0)  /* clamp to max */
		dist = 255.0;

	if (dist > 50.0) {  /* resolve stereo panning */
		const float dot = DotProduct(s_env.right, delta);
		angle = acos(dot) * 180.0 / M_PI - 90.0;
		angle = (int)(360.0 - angle) % 360;
	} else
		angle = 0;

	Mix_SetPosition(c, (int)angle, (int)dist);
}
