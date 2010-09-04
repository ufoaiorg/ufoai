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
#include "r_state.h"

#define LIGHT_RADIUS_FACTOR 80.0


/** @todo - merge this stuff into the new lighitng system using r_light_t objects */
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

void R_AddLightsource (const r_light_t *source)
{
	if (r_state.numActiveLights >= MAX_DYNAMIC_LIGHTS) {
		Com_Printf("Failed to add lightsource: MAX_DYNAMIC_LIGHTS exceeded\n");
		return;
	}

	Com_Printf("added light, ambient=%f\n", source->ambientColor[0]);

	r_state.dynamicLights[r_state.numActiveLights++] = *source;
}

void R_ClearActiveLights (void)
{
	int i;

	r_state.numActiveLights = 0;
	glDisable(GL_LIGHTING);
	for (i = 0; i < r_dynamic_lights->integer; i++) {
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
		glDisable(GL_LIGHT0 + i);
	}
}

/** @todo - the updating of the per-entity list of nearest
 * lights doesn't necessarily need to be updated before
 * every frame; if the list is a few frames (or even a few seconds)
 * out of date, it would still probably look just fine.
 * Ideally, this updating could be done in the background
 * by a separate, low-priority thread that constantly looped
 * through all the entities and updated their light lists,
 * while the high-priority rendering thread just used the most
 * up to date version available.  The current version isn't really
 * thread-safe because it uses a global variable, which is needed
 * due to the way that the standard qsort() function works, but
 * this would be easy to fix if we used our own sorting algorithm,
 * which we should probably do anyway, since qsort() isn't actually
 * likely to be very efficient given that the ordering of the list
 * isn't likely to change very much between calls.  Bubble sort
 * would probably be faster on average.
 *
 * In the long run, it would probably be highly beneficial to
 * separate the rendering of the world from the updating of the
 * world.  Having a "rendering" thread that was separate from a
 * "thinking" thread (that was responsible for updating the
 * sorted light list, updating models and textures based on
 * animations or material properties, updating the state
 * of the world based on user input, etc.).  It would require
 * that care to be taken to ensure proper synchronization and
 * avoid race-conditions, but it would allow us to decouple the
 * framerate of the renderer from the speed at which we can
 * compute all the other stuff which isn't directly linked to
 * the frame-by-frame rendering process (this includes anything
 * that is supposed to change at a fixed rate of time, like
 * animations; we don't want the animation speed to be linked
 * to the framerate).
 *
 * Obviously, we would need to maintain compatibility with
 * single-core systems, but multi-core systems are becoming
 * so common that it would make sense to take advantage of
 * that extra power when it exists.  Additionally, this type
 * of threaded structure can be effective even on a single
 * processor system by allowing us to prioritize things which
 * must be done in real-time (ie. rendering) from things which
 * won't be noticable to the user if they happen a bit slower, or
 * are updated a bit less often.  */


/* global variable - NOT THREAD SAFE */
static vec3_t origin;

static inline int R_LightDistCompare (const void *a, const void *b)
{
	const r_light_t *light1 = *(const r_light_t * const *)a;
	const r_light_t *light2 = *(const r_light_t * const *)b;
	return
		light1->loc[3] == 0 ? -1 :
		light2->loc[3] == 0 ?  1 :
		VectorDistSqr(light1->loc, origin) - VectorDistSqr(light2->loc, origin);
}

static inline void R_SortLightList_qsort (r_light_t **list)
{
	qsort(list, r_state.numActiveLights, sizeof(*list), &R_LightDistCompare);
}

/**
 * @todo qsort may not be the best thing to use here,
 * given that the ordering of the list usually won't change
 * much (if at all) between calls.  Something like bubble-sort
 * might actually be more efficient in practice.
 */
static void R_SortLightList (r_light_t **list, vec3_t v)
{
	VectorCopy(v, origin);
	R_SortLightList_qsort(list);
}

void R_UpdateLightList (entity_t *ent)
{
	if (ent->numLights > r_state.numActiveLights)
		ent->numLights = 0;

	while (ent->numLights < r_state.numActiveLights) {
		ent->lights[ent->numLights] = &r_state.dynamicLights[ent->numLights];
		ent->numLights++;
	}

	R_SortLightList(ent->lights, ent->origin);
}
