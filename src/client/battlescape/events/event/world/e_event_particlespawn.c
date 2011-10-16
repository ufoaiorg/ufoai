/**
 * @file e_event_particlespawn.c
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
#include "../../../cl_particle.h"
#include "e_event_particlespawn.h"

int CL_ParticleSpawnEventTime (const struct eventRegister_s *self, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	if (eventTiming->parsedDeath) { /* drop items after death (caused by impact) */
		return eventTiming->impactTime + 400;
	} else if (eventTiming->impactTime > cl.time) { /* item thrown on the ground */
		return eventTiming->impactTime + 75;
	}

	return eventTiming->nextTime;
}

/**
 * @brief Let a particle spawn for the client
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg holds the network data
 * @sa CL_ParticleSpawn
 * @sa EV_PARTICLE_SPAWN
 */
void CL_ParticleSpawnEvent (const eventRegister_t *self, struct dbuffer *msg)
{
	char particle[MAX_VAR];
	int levelflags;
	vec3_t s, v, a;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &levelflags, &s, &v, &a, particle, sizeof(particle));

	CL_ParticleSpawn(particle, levelflags, s, v, a);
}
