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

static sustain_t r_sustainArray[MAX_GL_LIGHTS];

void R_AddLight (const vec3_t origin, float radius, const vec3_t color)
{
	int i;

	if (!r_lights->integer)
		return;

	if (refdef.numDynamicLights == MAX_GL_LIGHTS)
		return;

	i = refdef.numDynamicLights++;

	VectorCopy(origin, refdef.dynamicLights[i].origin);
	refdef.dynamicLights[i].radius = radius;
	VectorCopy(color, refdef.dynamicLights[i].color);

	Com_DPrintf(DEBUG_RENDERER, "added dynamic light, color (%f, %f, %f) position (%f, %f, %f)  radius=%f\n",
		color[0], color[1], color[2],
		origin[0], origin[1], origin[2],
		radius);
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

/* currently, this func processes only the world lights */
void R_EnableLights (void)
{
	light_t *l;
	vec4_t position;
	vec4_t diffuse;
	int i;
	int maxLights = r_dynamic_lights->integer;
	vec4_t blackColor = {0.0, 0.0, 0.0, 1.0};

	/* with the current blending model, lighting breaks FFP world render */
	if (!r_programs->integer) {
		glDisable(GL_LIGHTING);
		return;
	}

	R_AddSustainedLights();

	position[3] = diffuse[3] = 1.0;

	/* Light #0 is reserved for the Sun, which is already factored into lightmaps */
	glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, blackColor);
	glLightfv(GL_LIGHT0, GL_AMBIENT, blackColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, blackColor);
	glDisable(GL_LIGHT0);

	for (i = 1, l = refdef.dynamicLights; i <= refdef.numDynamicLights && i < maxLights && i < MAX_GL_LIGHTS; i++, l++) {
		VectorCopy(l->origin, position);
		glLightfv(GL_LIGHT0 + i, GL_POSITION, position);
		VectorCopy(l->color, diffuse);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
		glLightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, 16.0 / (l->radius * l->radius));
		glEnable(GL_LIGHT0 + i);
	}

	for (; i < MAX_GL_LIGHTS; i++) {  /* disable the rest */
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, blackColor);
		glDisable(GL_LIGHT0 + i);
	}
	glEnable(GL_LIGHTING);
}

void R_AddStaticLight (const vec3_t origin, float radius, const vec3_t color)
{
	light_t *l;
	if (r_state.numStaticLights >= MAX_STATIC_LIGHTS) {
		Com_Printf("Failed to add lightsource: MAX_STATIC_LIGHTS exceeded\n");
		return;
	}

	Com_DPrintf(DEBUG_RENDERER, "added static light, color (%f, %f, %f) position (%f, %f, %f)  radius=%f\n",
		color[0], color[1], color[2],
		origin[0], origin[1], origin[2],
		radius);

	l = &r_state.staticLights[r_state.numStaticLights++];

	VectorCopy(origin, l->origin);
	VectorCopy(color, l->color);
	l->color[3] = 1.0; /* needed if we pass this light as parameter to glLightxxx() */
	l->radius = radius;
}

void R_ClearStaticLights (void)
{
	int i;
	vec4_t blackColor = {0.0, 0.0, 0.0, 1.0};

	r_state.numStaticLights = 0;
	glDisable(GL_LIGHTING);
	for (i = 0; i < MAX_GL_LIGHTS; i++) {
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, blackColor);
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
 * up to date version available.
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
/**
 * @brief Adds light to the entity's lights list, sorted by distance
 * @note Since list is very small (8 elements), insertion sort is used - it beats qsort and other fancy algos in case of a small list
 * and allows a better integration with other parts of code.
 * @param[in] ent The entity for which light is added
 * @param[in] light The light itself
 * @param[in] distSqr Squared distance from entity's origin to the light
 * @sa R_UpdateLightList
 */
static void R_AddLightToEntity (entity_t *ent, light_t *light, const float distSqr)
{
	int i;
	light_t **el = ent->lights;
	/* we have to use the offset from accumulated transform matrix, because origin is relative to attachment point for submodels */
	const vec_t *pos = ent->transform.matrix + 12; /* type system hack, sorry */

	for (i = 0; i < ent->numLights; i++) {
		if (i == MAX_ENTITY_LIGHTS)
			return;
		if (distSqr < VectorDistSqr((el[i]->origin), pos)) { /** @todo will caching VectorDistSqr() results improve the rendering speed? */
			/* found more distant light, push it down the list and insert this one*/
			if (i + 1 == MAX_ENTITY_LIGHTS) {
				/* shortcut in case light we are replacing is the last light possible; also acts as the overflow guard */
				el[i] = light;
				return;
			}

			while (i < ent->numLights) {
				light_t *tmp = el[i];
				el[i++] = light;
				light = tmp;
			}

			el[i++] = light;
			ent->numLights = i;
			return;
		}
	}

	if (i == MAX_ENTITY_LIGHTS)
		return;

	el[i++] = light;
	ent->numLights = i;
}

/** @todo bad implementation -- copying light pointers every frame is a very wrong idea
 * @brief Recalculate active lights list for the given entity
 * @note to accelerate math, the diagonal of aabb is used to approximate max distance from entity's origin to its most distant point
 * while this is a gross exaggeration for many models, the sole purpose of it is to be used for filtering out distant lights,
 * so nothing is broken by it.
 * @param[in] ent Entity to recalculate lights for
 * @sa R_AddLightToEntity
 */
void R_UpdateLightList (entity_t *ent)
{
	int i;
	/* we have to use the offset from accumulated transform matrix, because origin is relative to attachment point for submodels */
	const vec_t *pos = ent->transform.matrix + 12; /* type system hack, sorry */
	vec3_t diametralVec; /** < conservative estimate of entity's bounding sphere diameter, in vector form */
	float diameter; /** < value of this entity's diameter (approx) */

	VectorSubtract(ent->maxs, ent->mins, diametralVec); /** @todo what if origin is NOT inside aabb? then this estimate will not be conservative enough */
	diameter = VectorLength(diametralVec);

	ent->numLights = 0; /* clear the list of lights */

	for (i = 0; i < r_state.numStaticLights; i++) {
		light_t *light = &r_state.staticLights[i];
		const float distSqr = VectorDistSqr(pos, light->origin);

		if (distSqr > (diameter + light->radius) * (diameter + light->radius))
			continue;

		R_AddLightToEntity(ent, light, distSqr);
	}

	/* add dynamic lights, too */
	for (i = 0; i < refdef.numDynamicLights; i++) {
		light_t *light = &refdef.dynamicLights[i];
		const float distSqr = VectorDistSqr(pos, light->origin);

		if (distSqr > (diameter + light->radius) * (diameter + light->radius))
			continue;

		R_AddLightToEntity(ent, light, distSqr);
	}
}
