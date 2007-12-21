/**
 * @file r_light.c
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "r_local.h"
#include "r_error.h"

static int r_numLights;
light_t r_lights[MAX_GL_LIGHTS];
static light_t r_sunLight;

void R_AddLight (vec3_t origin, float intensity, vec3_t color)
{
	int i;

	if (r_numLights == MAX_GL_LIGHTS - 1)
		return;

	i = r_numLights++;

	VectorCopy(origin, r_lights[i].origin);
	r_lights[i].origin[3] = 1; /* no sun light */
	r_lights[i].intensity = intensity;
	VectorCopy(color, r_lights[i].color);
}

/**
 * @sa CL_ParticleEditor_f
 * @sa CL_ParseEntitystring
 * @sa CL_SequenceStart_f
 */
void R_AddSunLight (const light_t* sunlight)
{
	memcpy(&r_sunLight, sunlight, sizeof(light_t));
}

/**
 * @brief Set light parameters
 * @sa R_RenderFrame
 */
void R_AddLights (void)
{
	vec4_t position;
	vec3_t diffuse;
	int i;

	/* the first light is our sun */
	qglLightfv(GL_LIGHT0, GL_POSITION, r_sunLight.origin);
	qglLightfv(GL_LIGHT0, GL_DIFFUSE, r_sunLight.color);
	/*qglLightfv(GL_LIGHT0, GL_AMBIENT, r_sunLight.ambient);*/
	qglEnable(GL_LIGHT0);

	R_CheckError();

	for (i = 0; i < r_numLights; i++) {  /* now add all lights */
		VectorCopy(r_lights[i].origin, position);
		position[3] = 1;	/* no sun light */

		VectorScale(r_lights[i].color, r_lights[i].intensity, diffuse);

		qglLightfv(GL_LIGHT1 + i, GL_POSITION, position);
		qglLightfv(GL_LIGHT1 + i, GL_DIFFUSE, diffuse);
		/*qglLightfv(GL_LIGHT1 + i, GL_AMBIENT, r_sunLight.ambient);*/
		qglEnable(GL_LIGHT1 + i);
	}

	for (i = r_numLights; i < MAX_GL_LIGHTS - 1; i++)
		qglDisable(GL_LIGHT1 + i);

	R_CheckError();

	r_numLights = 0;
}
