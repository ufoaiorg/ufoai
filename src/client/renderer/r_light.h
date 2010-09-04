/**
 * @file r_light.h
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

#ifndef R_LIGHT_H
#define R_LIGHT_H

struct entity_s;

#include "r_entity.h"

/* cap on number of light sources that can be in a scene; feel free
 * to increase if necessary, but be aware that doing so will increase
 * the time it takes for each entity to sort the light list at each
 * frame (this is based on the number of lights actually used, not
 * the max number allowed) */
#define MAX_DYNAMIC_LIGHTS 64

typedef struct r_light_s {
	vec4_t loc;
	vec4_t ambientColor;
	vec4_t diffuseColor;
	vec4_t specularColor;

	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;

	qboolean enabled;
} r_light_t;

/** @todo - integrate the particle-based lights into the new dynamic light system */
void R_AddLight(const vec3_t origin, float radius, const vec3_t color);
void R_AddSustainedLight(const vec3_t org, float radius, const vec3_t color, float sustain);
void R_EnableLights(void);
void R_ShiftLights(const vec3_t offset);

void R_AddLightsource(const r_light_t *source);
void R_ClearActiveLights(void);
void R_UpdateLightList(struct entity_s *ent);

#endif
