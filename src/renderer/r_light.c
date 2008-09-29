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
#include "r_light.h"
#include "r_entity.h"

#define LIGHT_RADIUS_FACTOR 100.0

int r_numLights;
static light_t r_lightsArray[MAX_GL_LIGHTS];

void R_AddLight (vec3_t origin, float radius, const vec3_t color)
{
	int i;

	if (!r_lighting->integer)
		return;

	if (r_numLights == MAX_GL_LIGHTS)
		return;

	i = r_numLights++;

	VectorCopy(origin, r_lightsArray[i].origin);
	r_lightsArray[i].radius = radius;
	VectorCopy(color, r_lightsArray[i].color);
}

void R_EnableLights (void)
{
	light_t *l;
	vec4_t position;
	vec4_t diffuse;
	int i;

	position[3] = diffuse[3] = 1.0;

	for (i = 0, l = r_lightsArray; i < r_numLights; i++, l++) {
		VectorCopy(l->origin, position);
		glLightfv(GL_LIGHT0 + i, GL_POSITION, position);
		VectorCopy(l->color, diffuse);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, l->radius * LIGHT_RADIUS_FACTOR);
	}

	/* disable first light to reset state */
	glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.0);
}
