/**
 * @file
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

/**
 * @brief Create light to be rendered in the current frame (will be removed before the next)
 * @note Call before actual 3D rendering begins to avoid missing or partially rendered lights
 */
void R_AddLight (const vec3_t origin, float radius, const vec3_t color)
{
	int i;

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
	sustain_t* s = r_sustainArray;

	int i;
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
 * @brief Updates state of sustained lights; should be called once per frame right before world rendering
 * @sa R_RenderFrame
 * @sa R_AddSustainedLight
 */
void R_UpdateSustainedLights (void)
{
	sustain_t* s;
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

/**
 * @brief Set up lighting data for the GLSL world shader
 */
void R_EnableWorldLights (void)
{
	int i;
	int maxLights = r_dynamic_lights->integer;
	vec3_t lightPositions[MAX_GL_LIGHTS];
	vec4_t lightParams[MAX_GL_LIGHTS];

	/* with the current blending model, lighting breaks FFP world render */
	if (!r_programs->integer) {
		return;
	}

	if (!r_dynamic_lights->integer)
		return;

	for (i = 0; i < refdef.numDynamicLights && i < maxLights; i++) {
		const light_t* light = &refdef.dynamicLights[i];

		GLPositionTransform(r_locals.world_matrix, light->origin, lightPositions[i]);
		VectorCopy(light->color, lightParams[i]);
		lightParams[i][3] = 16.0 / (light->radius * light->radius);
	}

	/* if there aren't enough active lights, turn off the rest */
	for (;i < maxLights ;i++)
		Vector4Set(lightParams[i], 0, 0, 0, 1);

	/* Send light data to shaders */
	R_ProgramParameter3fvs("LIGHTPOSITIONS", maxLights, (GLfloat*)lightPositions);
	R_ProgramParameter4fvs("LIGHTPARAMS", maxLights, (GLfloat*)lightParams);

	R_EnableAttribute("TANGENTS");
}

/**
 * @brief Enable or disable realtime dynamic lighting for models
 * @param lights The lights to enable
 * @param numLights The amount of lights in the given lights list
 * @param inShadow Whether model is shadowed from the sun
 * @param enable Whether to turn realtime lighting on or off
 */
void R_EnableModelLights (const light_t** lights, int numLights, bool inShadow, bool enable)
{
	int i;
	int maxLights = r_dynamic_lights->integer;
	vec4_t blackColor = {0.0, 0.0, 0.0, 1.0};
	const vec4_t whiteColor = {1.0, 1.0, 1.0, 1.0};
	const vec4_t defaultLight0Position = {0.0, 0.0, 1.0, 0.0};
	vec3_t lightPositions[MAX_GL_LIGHTS];
	vec4_t lightParams[MAX_GL_LIGHTS];

	if (r_programs->integer == 0) { /* Fixed function path renderer got only the sunlight */
		/* Setup OpenGL light #0 to act as a sun and environment light */
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_COLOR_MATERIAL);
#ifndef GL_VERSION_ES_CM_1_0
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
#endif

		glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0);
		glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0);
		glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0);

		glLightfv(GL_LIGHT0, GL_SPECULAR, blackColor);
		glLightfv(GL_LIGHT0, GL_AMBIENT, refdef.modelAmbientColor);

		glLightfv(GL_LIGHT0, GL_POSITION, refdef.sunVector);

		if (enable) {
			if (inShadow) {
				/* ambient only */
				glLightfv(GL_LIGHT0, GL_DIFFUSE, blackColor);
			} else {
				/* Full sunlight */
				glLightfv(GL_LIGHT0, GL_DIFFUSE, refdef.sunDiffuseColor);
			}
		} else {
			/* restore the default OpenGL state */
			glDisable(GL_LIGHTING);
			glDisable(GL_COLOR_MATERIAL);

			glLightfv(GL_LIGHT0, GL_POSITION, defaultLight0Position);
			glLightfv(GL_LIGHT0, GL_AMBIENT, blackColor);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteColor);
			glLightfv(GL_LIGHT0, GL_SPECULAR, whiteColor);
		}
		return;
	}

	assert(numLights <= MAX_GL_LIGHTS);

	if (!enable || !r_state.lighting_enabled) {
		if (r_state.dynamic_lighting_enabled) {
			R_DisableAttribute("TANGENTS"); /** @todo is it a good idea? */

			if (maxLights) {
				for (i = 0; i < maxLights; i++)
					Vector4Set(lightParams[i], 0, 0, 0, 1);

				/* Send light data to shaders */
				R_ProgramParameter3fvs("LIGHTPOSITIONS", maxLights, (GLfloat*)lightPositions);
				R_ProgramParameter4fvs("LIGHTPARAMS", maxLights, (GLfloat*)lightParams);
			}
		}

		r_state.dynamic_lighting_enabled = false;
		return;
	}

	/** @todo assert? */
	if (numLights > maxLights)
		numLights = maxLights;

	r_state.dynamic_lighting_enabled = true;

	R_EnableAttribute("TANGENTS");

	R_UseMaterial(&defaultMaterial);

	R_ProgramParameter3fv("AMBIENT", refdef.modelAmbientColor);

	if (inShadow) {
		R_ProgramParameter3fv("SUNCOLOR", blackColor);
	} else {
		R_ProgramParameter3fv("SUNCOLOR", refdef.sunDiffuseColor);
	}

	if (!maxLights)
		return;

	for (i = 0; i < numLights; i++) {
		const light_t* light = lights[i];

		GLPositionTransform(r_locals.world_matrix, light->origin, lightPositions[i]);
		VectorCopy(light->color, lightParams[i]);
		lightParams[i][3] = 16.0 / (light->radius * light->radius);
	}

	/* if there aren't enough active lights, turn off the rest */
	for (; i < maxLights; i++)
		Vector4Set(lightParams[i], 0, 0, 0, 1);

	/* Send light data to shaders */
	R_ProgramParameter3fvs("LIGHTPOSITIONS", maxLights, (GLfloat*)lightPositions);
	R_ProgramParameter4fvs("LIGHTPARAMS", maxLights, (GLfloat*)lightParams);
}

/**
 * @brief Add static light for model lighting (world already got them baked into lightmap)
 * @note To be called from map loader only
 * @sa SP_light
 */
void R_AddStaticLight (const vec3_t origin, float radius, const vec3_t color)
{
	if (refdef.numStaticLights >= MAX_STATIC_LIGHTS) {
		Com_Printf("Failed to add lightsource: MAX_STATIC_LIGHTS exceeded\n");
		return;
	}

	Com_DPrintf(DEBUG_RENDERER, "added static light, color (%f, %f, %f) position (%f, %f, %f)  radius=%f\n",
		color[0], color[1], color[2],
		origin[0], origin[1], origin[2],
		radius);

	light_t* light = &refdef.staticLights[refdef.numStaticLights++];

	VectorCopy(origin, light->origin);
	VectorCopy(color, light->color);
	light->color[3] = 1.0; /* needed if we pass this light as parameter to glLightxxx() */
	light->radius = radius;
}

/* If glow was enabled, disable it before calling this function, or rendering state will become incoherent */
void R_DisableLights (void)
{
	vec4_t blackColor = {0.0, 0.0, 0.0, 1.0};

	for (int i = 0; i < MAX_GL_LIGHTS; i++) {
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, blackColor);
		glLightfv(GL_LIGHT0 + i, GL_AMBIENT, blackColor);
		glLightfv(GL_LIGHT0 + i, GL_SPECULAR, blackColor);
		glDisable(GL_LIGHT0 + i);
	}
	glDisable(GL_LIGHTING);
}

/**
 * @brief Remove all static light data
 * @note To be called before loading a new map
 */
void R_ClearStaticLights (void)
{
	refdef.numStaticLights = 0;
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
 * @param[in] pos Origin of entity for which light is added
 * @param[in] ltng Lighting data for entity being updated
 * @param[in] light The light itself
 * @param[in] distSqr Squared distance from entity's origin to the light
 * @sa R_UpdateLightList
 */
static void R_AddLightToEntity (const vec_t* pos, lighting_t* ltng, const light_t* light, const float distSqr)
{
	int i;
	int maxLights = r_dynamic_lights->integer;
	const light_t** el = ltng->lights;

	for (i = 0; i < ltng->numLights; i++) {
		if (i == maxLights)
			return;
		if (distSqr < VectorDistSqr((el[i]->origin), pos)) { /** @todo will caching VectorDistSqr() results improve the rendering speed? */
			/* found more distant light, push it down the list and insert this one*/
			if (i + 1 == maxLights) {
				/* shortcut in case light we are replacing is the last light possible; also acts as the overflow guard */
				el[i] = light;
				return;
			}

			while (i < ltng->numLights) {
				const light_t* tmp = el[i];
				el[i++] = light;
				light = tmp;
			}

			break;
		}
	}

	if (i == maxLights)
		return;

	el[i++] = light;
	ltng->numLights = i;
}

static lighting_t fakeLightingData; /**< To return if no actual lighting data is provided */

/** @todo bad implementation -- copying light pointers every frame is a very wrong idea
 * @brief Recalculate active lights list for the given entity; R_CalcTransform(ent) should be called before this
 * @note to accelerate math, the diagonal of aabb is used to approximate max distance from entity's origin to its most distant point
 * while this is a gross exaggeration for many models, the sole purpose of it is to be used for filtering out distant lights,
 * so nothing is broken by it.
 * @param[in] ent Entity to recalculate lights for
 * @sa R_AddLightToEntity
 */
void R_UpdateLightList (entity_t* ent)
{
	int i;
	vec_t* pos;				/**< Worldspace position for which lighting is calculated */
	entity_t* rootEnt;		/**< The root entitity of tagent tree, which holds the lighting data (af any) */
	lighting_t* ltng;		/**< Lighting data for the entity being processed */
	vec3_t diametralVec;	/**< conservative estimate of entity's bounding sphere diameter, in vector form */
	float diameter;			/**< value of this entity's diameter (approx) */
	bool cached = false;

	/* Find the root of tagent tree which actually owns the lighting data; it is assumed that there is no loops,
	 * since R_CalcTransform calls Com_Error on those */
	for (rootEnt = ent; rootEnt->tagent; rootEnt = ent->tagent)
		;

	ltng = ent->lighting = rootEnt->lighting;

	if (!ltng) {
		/* Entity got no lighting data, so substitute defaults (no dynamic lights, but exposed to sunlight) */
		/** @todo Replace this hack with something more legit (hack can cause bizarre stencil shadows if enabled) */
		OBJZERO(fakeLightingData);
		ent->lighting = &fakeLightingData;
		return;
	}

	if (ltng->lastLitFrame == r_locals.frame)
		return; /* nothing to do, already calculated lighting for this frame */

	ltng->lastLitFrame = r_locals.frame;

	/* we have to use the offset from (accumulated) transform matrix, because entity origin is not necessarily the point where model is acually rendered */
	pos = ent->transform.matrix + 12; /* type system hack, sorry */

	ent->eBox.getDiagonal(diametralVec); /** @todo what if origin is NOT inside aabb? then this estimate will not be conservative enough */
	diameter = VectorLength(diametralVec);

	/** @todo clear caches when r_dynamic_lights cvar is changed OR always keep maximal # of static lights in the cache */
	if (VectorDist(pos, ltng->lastCachePos) < CACHE_CLEAR_TRESHOLD) {
		cached = true;
	} else {
		ltng->numLights = 0; /* clear the list of lights */
		ltng->numCachedLights = 0;
		VectorCopy(pos, ltng->lastCachePos);
	}

	/* Check if origin of this entity is hit by sunlight (not the best test, but at least fast) */
	if (!cached) {
		if (ent->flags & RF_ACTOR) { /** @todo Hack to avoid dropships being shadowed by lightclips placed at them. Should be removed once correct global illumination model is done */
			vec3_t fakeSunPos; /**< as if sun wasn't at infinite distance */
			VectorMA(pos, 8192.0, refdef.sunVector, fakeSunPos);
			R_Trace(pos, fakeSunPos, 0, MASK_SOLID);
			ltng->inShadow = refdef.trace.fraction != 1.0;
		} else {
			ltng->inShadow = false;
		}
	}

	if (!r_dynamic_lights->integer)
		return;

	if (!cached) {
		/* Rebuild list of static lights */
		for (i = 0; i < refdef.numStaticLights; i++) {
			light_t* light = &refdef.staticLights[i];
			const float distSqr = VectorDistSqr(pos, light->origin);

			if (distSqr > (diameter + light->radius) * (diameter + light->radius))
				continue;

			R_Trace(pos, light->origin, 0, MASK_SOLID);

			if (refdef.trace.fraction == 1.0)
				R_AddLightToEntity(pos, ltng, light, distSqr);
		}
		/* Save static lights to cache */
		for (i = 0; i < ltng->numLights; i++)
			ltng->cachedLights[i] = ltng->lights[i];
		ltng->numCachedLights = ltng->numLights;
	} else {
		/* Copy static lights from cache */
		for (i = 0; i < ltng->numCachedLights; i++)
			ltng->lights[i] = ltng->cachedLights[i];
		ltng->numLights = ltng->numCachedLights;
	}

	/* add dynamic lights, too */
	for (i = 0; i < refdef.numDynamicLights; i++) {
		light_t* light = &refdef.dynamicLights[i];
		const float distSqr = VectorDistSqr(pos, light->origin);

		if (distSqr > (diameter + light->radius) * (diameter + light->radius))
			continue;

		R_Trace(pos, light->origin, 0, MASK_SOLID);

		if (refdef.trace.fraction == 1.0)
			R_AddLightToEntity(pos, ltng, light, distSqr);
	}
}
