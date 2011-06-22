/**
 * @file cl_particle.h
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

#ifndef CLIENT_CL_PARTICLE_H
#define CLIENT_CL_PARTICLE_H

void CL_AddMapParticle(const char *particle, const vec3_t origin, const vec2_t wait, const char *info, int levelflags);
void CL_ParticleCheckRounds(void);
void CL_ParticleFree(ptl_t *p);

void CL_ParticleRegisterArt(void);
void PTL_InitStartup(void);
void CL_ParticleRun(void);
void CL_RunMapParticles(void);
void CL_ParseParticle(const char *name, const char **text);
void CL_InitParticles(void);
ptl_t *CL_ParticleSpawn(const char *name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a);
ptlDef_t *CL_ParticleGet(const char *particleID);
void CL_ParticleVisible(ptl_t *p, qboolean hide);

#endif
