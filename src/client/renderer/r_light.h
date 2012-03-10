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

typedef struct light_s {
	vec3_t origin;
	vec4_t color;
	float radius;
} light_t;

/** @brief sustains are light flashes which slowly decay */
typedef struct sustain_s {
	light_t light;
	float time;
	float sustain;
} sustain_t;

/* cap on number of light sources that can be in a scene; feel free
 * to increase if necessary, but be aware that doing so will increase
 * the time it takes for each entity to sort the light list at each
 * frame (this is based on the number of lights actually used, not
 * the max number allowed) */
#define MAX_STATIC_LIGHTS 256

/** @todo - integrate the particle-based lights into the new dynamic light system */
void R_AddLight(const vec3_t origin, float radius, const vec3_t color);
void R_AddSustainedLight(const vec3_t org, float radius, const vec3_t color, float sustain);
void R_UpdateSustainedLights(void);
void R_EnableWorldLights(void);
void R_EnableModelLights(const light_t **lights, int numLights, qboolean inShadow, qboolean enable);
void R_DisableLights(void);

void R_AddStaticLight(const vec3_t origin, float radius, const vec3_t color);
void R_ClearStaticLights(void);
void R_UpdateLightList(struct entity_s *ent);

#endif
