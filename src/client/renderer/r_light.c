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

#define LIGHT_RADIUS_FACTOR 80.0

static light_t r_lightsArray[MAX_GL_LIGHTS];
static sustain_t r_sustainArray[MAX_GL_LIGHTS];

void R_AddLight (const vec3_t origin, float radius, const vec3_t color)
{
	int i;

	if (!r_lights->integer)
		return;

	if (refdef.numLights == MAX_GL_LIGHTS)
		return;

	i = refdef.numLights++;

	VectorCopy(origin, r_lightsArray[i].origin);
	r_lightsArray[i].radius = radius;
	VectorCopy(color, r_lightsArray[i].color);
}

/**
 * @sa R_AddSustainedLights
 */
void R_AddSustainedLight (const vec3_t org, float radius, const vec3_t color, float sustain)
{
	sustain_t *s;
	int i;

	if (!r_lights->integer)
		return;

	s = r_sustainArray;

	for (i = 0; i < MAX_GL_LIGHTS; i++, s++)
		if (!s->sustain)
			break;

	if (i == MAX_GL_LIGHTS)
		return;

	VectorCopy(org, s->light.origin);
	s->light.radius = radius;
	VectorCopy(color, s->light.color);

	s->time = refdef.time;
	s->sustain = refdef.time + sustain;
}

/**
 * @sa R_EnableLights
 * @sa R_AddSustainedLight
 */
static void R_AddSustainedLights (void)
{
	sustain_t *s;
	int i;

	/* sustains must be recalculated every frame */
	for (i = 0, s = r_sustainArray; i < MAX_GL_LIGHTS; i++, s++) {
		float intensity;
		vec3_t color;

		if (s->sustain <= refdef.time) {  /* clear it */
			s->sustain = 0;
			continue;
		}

		intensity = (s->sustain - refdef.time) / (s->sustain - s->time);
		VectorScale(s->light.color, intensity, color);

		R_AddLight(s->light.origin, s->light.radius, color);
	}
}

static vec3_t lights_offset;

/**
 * Light sources must be translated for bsp submodel entities.
 */
void R_ShiftLights (const vec3_t offset)
{
	VectorCopy(offset, lights_offset);
}

void R_EnableLights (void)
{
	light_t *l;
	vec4_t position;
	vec4_t diffuse;
	int i;

	R_AddSustainedLights();

	position[3] = diffuse[3] = 1.0;

	for (i = 0, l = r_lightsArray; i < refdef.numLights; i++, l++) {
		VectorSubtract(l->origin, lights_offset, position);
		glLightfv(GL_LIGHT0 + i, GL_POSITION, position);
		VectorCopy(l->color, diffuse);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, l->radius * LIGHT_RADIUS_FACTOR);
	}

	for (; i < MAX_GL_LIGHTS; i++)  /* disable the rest */
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
}
