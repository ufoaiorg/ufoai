/**
 * @file lightmap.c
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

#include "lighting.h"
#include "bsp.h"

#define	sun_angles			config.sun_angles[config.compile_for_day]
#define	sun_normal			config.sun_normal[config.compile_for_day]
#define	sun_color			config.sun_color[config.compile_for_day]
#define	sun_intensity		config.sun_intensity[config.compile_for_day]
#define	sun_ambient_color	config.sun_ambient_color[config.compile_for_day]

vec3_t face_offset[MAX_MAP_FACES];		/**< for rotating bmodels */

/** @brief lightinfo is a temporary bucket for lighting calculations */
typedef struct {
	vec_t	facedist;
	vec3_t	facenormal;

	int		numsurfpt;
	vec3_t	*surfpt;

	vec3_t	modelorg;		/**< for origined bmodels */

	vec3_t	texorg;
	vec3_t	worldtotex[2];	/**< s = (world - texorg) . worldtotex[0] */
	vec3_t	textoworld[2];	/**< world = texorg + s * textoworld[0] */

	int		texmins[2];
	int		texsize[2];		/**< the size of the lightmap in pixel */

	int step; /**< step between samples in tex2world units */

	dBspSurface_t	*face;
} lightinfo_t;

/** @brief face extents */
typedef struct extents_s {
	vec3_t mins, maxs;
	vec3_t center;
	vec2_t stmins, stmaxs;
} extents_t;

static extents_t face_extents[MAX_MAP_FACES];

/**
 * @brief Populates face_extents for all dBspSurface_t, prior to light creation.
 * This is done so that sample positions may be nudged outward along
 * the face normal and towards the face center to help with traces.
 */
static void BuildFaceExtents (void)
{
	int k;

	for (k = 0; k < curTile->numfaces; k++) {
		const dBspSurface_t *s = &curTile->faces[k];
		const dBspTexinfo_t *tex = &curTile->texinfo[s->texinfo];

		float *mins = face_extents[k].mins;
		float *maxs = face_extents[k].maxs;

		float *center = face_extents[k].center;

		float *stmins = face_extents[k].stmins;
		float *stmaxs = face_extents[k].stmaxs;
		int i;

		VectorSet(mins, 999999, 999999, 999999);
		VectorSet(maxs, -999999, -999999, -999999);

		stmins[0] = stmins[1] = 999999;
		stmaxs[0] = stmaxs[1] = -999999;

		for (i = 0; i < s->numedges; i++) {
			const int e = curTile->surfedges[s->firstedge + i];
			const dBspVertex_t *v;
			int j;

			if (e >= 0)
				v = curTile->vertexes + curTile->edges[e].v[0];
			else
				v = curTile->vertexes + curTile->edges[-e].v[1];

			for (j = 0; j < 3; j++) {  /* calculate mins, maxs */
				if (v->point[j] > maxs[j])
					maxs[j] = v->point[j];
				if (v->point[j] < mins[j])
					mins[j] = v->point[j];
			}

			for (j = 0; j < 2; j++) {  /* calculate stmins, stmaxs */
				const float offset = tex->vecs[j][3];
				const float val = DotProduct(v->point, tex->vecs[j]) + offset;
				if (val < stmins[j])
					stmins[j] = val;
				if (val > stmaxs[j])
					stmaxs[j] = val;
			}
		}

		for (i = 0; i < 3; i++)
			center[i] = (mins[i] + maxs[i]) / 2.0f;
	}
}

/**
 * @sa BuildFaceExtents
 */
static void CalcLightinfoExtents (lightinfo_t *l)
{
	const dBspSurface_t *s;
	float *stmins, *stmaxs;
	vec2_t lm_mins, lm_maxs;
	int i;
	const int luxelScale = (1 << config.lightquant);

	s = l->face;

	stmins = face_extents[s - curTile->faces].stmins;
	stmaxs = face_extents[s - curTile->faces].stmaxs;

	for (i = 0; i < 2; i++) {
		lm_mins[i] = floor(stmins[i] / luxelScale);
		lm_maxs[i] = ceil(stmaxs[i] / luxelScale);

		l->texmins[i] = lm_mins[i];
		l->texsize[i] = lm_maxs[i] - lm_mins[i] + 1;
	}

	if (l->texsize[0] * l->texsize[1] > MAX_MAP_LIGHTMAP)
		Sys_Error("Surface too large to light (%dx%d)", l->texsize[0], l->texsize[1]);
}

/**
 * @brief Fills in texorg, worldtotex. and textoworld
 */
static void CalcLightinfoVectors (lightinfo_t *l)
{
	const dBspTexinfo_t *tex;
	int i;
	vec3_t texnormal;
	vec_t distscale, dist;

	tex = &curTile->texinfo[l->face->texinfo];

	for (i = 0; i < 2; i++)
		VectorCopy(tex->vecs[i], l->worldtotex[i]);

	/* calculate a normal to the texture axis.  points can be moved along this
	 * without changing their S/T */
	texnormal[0] = tex->vecs[1][1] * tex->vecs[0][2]
					- tex->vecs[1][2] * tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2] * tex->vecs[0][0]
					- tex->vecs[1][0] * tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0] * tex->vecs[0][1]
					- tex->vecs[1][1] * tex->vecs[0][0];
	VectorNormalize(texnormal);

	/* flip it towards plane normal */
	distscale = DotProduct(texnormal, l->facenormal);
	if (!distscale) {
		Verb_Printf(VERB_EXTRA, "WARNING: Texture axis perpendicular to face\n");
		distscale = 1.0;
	}
	if (distscale < 0.0) {
		distscale = -distscale;
		VectorSubtract(vec3_origin, texnormal, texnormal);
	}

	/* distscale is the ratio of the distance along the texture normal to
	 * the distance along the plane normal */
	distscale = 1.0 / distscale;

	for (i = 0; i < 2; i++) {
		const vec_t len = VectorLength(l->worldtotex[i]);
		const vec_t distance = DotProduct(l->worldtotex[i], l->facenormal) * distscale;
		VectorMA(l->worldtotex[i], -distance, texnormal, l->textoworld[i]);
		VectorScale(l->textoworld[i], (1.0f / len) * (1.0f / len), l->textoworld[i]);
	}

	/* calculate texorg on the texture plane */
	for (i = 0; i < 3; i++)
		l->texorg[i] =
			-tex->vecs[0][3] * l->textoworld[0][i] -
			tex->vecs[1][3] * l->textoworld[1][i];

	/* project back to the face plane */
	dist = DotProduct(l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMA(l->texorg, -dist, texnormal, l->texorg);

	/* compensate for org'd bmodels */
	VectorAdd(l->texorg, l->modelorg, l->texorg);

	/* total sample count */
	l->numsurfpt = l->texsize[0] * l->texsize[1];
	l->surfpt = (vec3_t *)Mem_Alloc(l->numsurfpt * sizeof(vec3_t));
	if (!l->surfpt)
		Sys_Error("Surface too large to light ("UFO_SIZE_T")", l->numsurfpt * sizeof(*l->surfpt));

	/* distance between samples */
	l->step = 1 << config.lightquant;
}

/**
 * @brief For each texture aligned grid point, back project onto the plane
 * to get the world xyz value of the sample point
 * @param[in,out] l The resulting lightinfo data
 * @param[in] sofs The sample offset for the s coordinates
 * @param[in] tofs The sample offset for the t coordinates
 */
static void CalcPoints (lightinfo_t *l, float sofs, float tofs)
{
	int s, t, j;
	int w, h, step;
	vec_t starts, startt;
	vec_t *surf;

	/* fill in surforg
	 * the points are biased towards the center of the surfaces
	 * to help avoid edge cases just inside walls */
	surf = l->surfpt[0];

	h = l->texsize[1];
	w = l->texsize[0];

	step = l->step;
	starts = l->texmins[0] * step;
	startt = l->texmins[1] * step;

	for (t = 0; t < h; t++) {
		for (s = 0; s < w; s++, surf += 3) {
			const vec_t us = starts + (s + sofs) * step;
			const vec_t ut = startt + (t + tofs) * step;

			/* calculate texture point */
			for (j = 0; j < 3; j++)
				surf[j] = l->texorg[j] + l->textoworld[0][j] * us +
						l->textoworld[1][j] * ut;
		}
	}
}

/** @brief buckets for sample accumulation - clipped by the surface */
typedef struct facelight_s {
	int numsamples;
	float *samples;		/**< lightmap samples */
	float *directions;	/**< for specular lighting/bumpmapping */
} facelight_t;

static facelight_t facelight[LIGHTMAP_MAX][MAX_MAP_FACES];

/** @brief Different types of sources emitting light */
typedef enum {
	emit_surface,		/**< surface light via SURF_LIGHT */
	emit_point,			/**< point light given via light entity */
	emit_spotlight		/**< spotlight given via light entity (+target) or via light_spot entity */
} emittype_t;

/** @brief a light source */
typedef struct light_s {
	struct light_s *next;	/**< next light in the chain */
	emittype_t	type;		/**< light type */

	float		intensity;	/**< brightness */
	vec3_t		origin;		/**< the origin of the light */
	vec3_t		color;		/**< the color (RGB) of the light */
	vec3_t		normal;		/**< spotlight direction */
	float		stopdot;	/**< spotlights cone */
} light_t;

static light_t *lights[LIGHTMAP_MAX];
static int numlights[LIGHTMAP_MAX];

/**
 * @brief Create lights out of patches and entity lights
 * @sa LightWorld
 * @sa BuildPatch
 */
void BuildLights (void)
{
	int i;
	light_t *l;

	/* surfaces */
	for (i = 0; i < MAX_MAP_FACES; i++) {
		const patch_t *p = face_patches[i];
		while (p) {  /* iterate subdivided patches */
			if (VectorEmpty(p->light))
				continue;

			numlights[config.compile_for_day]++;
			l = Mem_AllocType(light_t);

			VectorCopy(p->origin, l->origin);

			l->next = lights[config.compile_for_day];
			lights[config.compile_for_day] = l;

			l->type = emit_surface;

			l->intensity = ColorNormalize(p->light, l->color);
			l->intensity *= p->area * config.surface_scale;

			p = p->next;
		}
	}

	/* entities (skip the world) */
	for (i = 1; i < num_entities; i++) {
		float intensity;
		const char *color;
		const char *target;
		const entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		if (!Q_strstart(name, "light"))
			continue;

		/* remove those lights that are only for the night version */
		if (config.compile_for_day) {
			const int spawnflags = atoi(ValueForKey(e, "spawnflags"));
			if (!(spawnflags & 1))	/* day */
				continue;
		}

		numlights[config.compile_for_day]++;
		l = Mem_AllocType(light_t);

		GetVectorForKey(e, "origin", l->origin);

		/* link in */
		l->next = lights[config.compile_for_day];
		lights[config.compile_for_day] = l;

		intensity = FloatForKey(e, "light");
		if (!intensity)
			intensity = 300.0;
		color = ValueForKey(e, "_color");
		if (color && color[0] != '\0'){
			if (sscanf(color, "%f %f %f", &l->color[0], &l->color[1], &l->color[2]) != 3)
				Sys_Error("Invalid _color entity property given: %s", color);
			ColorNormalize(l->color, l->color);
		} else
			VectorSet(l->color, 1.0, 1.0, 1.0);
		l->intensity = intensity * config.entity_scale;
		l->type = emit_point;

		target = ValueForKey(e, "target");
		if (target[0] != '\0' || Q_streq(name, "light_spot")) {
			l->type = emit_spotlight;
			l->stopdot = FloatForKey(e, "_cone");
			if (!l->stopdot)
				l->stopdot = 10;
			l->stopdot = cos(l->stopdot * torad);
			if (target[0] != '\0') {	/* point towards target */
				entity_t *e2 = FindTargetEntity(target);
				if (!e2)
					Com_Printf("WARNING: light at (%i %i %i) has missing target '%s' - e.g. create an info_null that has a 'targetname' set to '%s'\n",
						(int)l->origin[0], (int)l->origin[1], (int)l->origin[2], target, target);
				else {
					vec3_t dest;
					GetVectorForKey(e2, "origin", dest);
					VectorSubtract(dest, l->origin, l->normal);
					VectorNormalize(l->normal);
				}
			} else {	/* point down angle */
				const float angle = FloatForKey(e, "angle");
				if (angle == ANGLE_UP) {
					l->normal[0] = l->normal[1] = 0.0;
					l->normal[2] = 1.0;
				} else if (angle == ANGLE_DOWN) {
					l->normal[0] = l->normal[1] = 0.0;
					l->normal[2] = -1.0;
				} else {
					l->normal[2] = 0;
					l->normal[0] = cos(angle * torad);
					l->normal[1] = sin(angle * torad);
				}
			}
		}
	}

	/* handle worldspawn light settings */
	{
		const entity_t *e = &entities[0];
		const char *ambient, *light, *angles, *color;
		float f;
		int i;

		if (config.compile_for_day) {
			ambient = ValueForKey(e, "ambient_day");
			light = ValueForKey(e, "light_day");
			angles = ValueForKey(e, "angles_day");
			color = ValueForKey(e, "color_day");
		} else {
			ambient = ValueForKey(e, "ambient_night");
			light = ValueForKey(e, "light_night");
			angles = ValueForKey(e, "angles_night");
			color = ValueForKey(e, "color_night");
		}

		if (light[0] != '\0')
			sun_intensity = atoi(light);

		if (angles[0] != '\0') {
			VectorClear(sun_angles);
			if (sscanf(angles, "%f %f", &sun_angles[0], &sun_angles[1]) != 2)
				Sys_Error("wrong angles values given: '%s'", angles);
			AngleVectors(sun_angles, sun_normal, NULL, NULL);
		}

		if (color[0] != '\0') {
			GetVectorFromString(color, sun_color);
			ColorNormalize(sun_color, sun_color);
		}

		if (ambient[0] != '\0')
			GetVectorFromString(ambient, sun_ambient_color);

		/* optionally pull brightness from worldspawn */
		f = FloatForKey(e, "brightness");
		if (f > 0.0)
			config.brightness = f;

		/* saturation as well */
		f = FloatForKey(e, "saturation");
		if (f > 0.0)
			config.saturation = f;
		else
			Verb_Printf(VERB_EXTRA, "Invalid saturation setting (%f) in worldspawn found\n", f);

		f = FloatForKey(e, "contrast");
		if (f > 0.0)
			config.contrast = f;
		else
			Verb_Printf(VERB_EXTRA, "Invalid contrast setting (%f) in worldspawn found\n", f);

		/* lightmap resolution downscale (e.g. 4 = 1 << 4) */
		i = atoi(ValueForKey(e, "quant"));
		if (i >= 1 && i <= 6)
			config.lightquant = i;
		else
			Verb_Printf(VERB_EXTRA, "Invalid quant setting (%i) in worldspawn found\n", i);
	}

	Verb_Printf(VERB_EXTRA, "light settings:\n * intensity: %i\n * sun_angles: pitch %f yaw %f\n * sun_color: %f:%f:%f\n * sun_ambient_color: %f:%f:%f\n",
		sun_intensity, sun_angles[0], sun_angles[1], sun_color[0], sun_color[1], sun_color[2], sun_ambient_color[0], sun_ambient_color[1], sun_ambient_color[2]);
	Verb_Printf(VERB_NORMAL, "%i direct lights for %s lightmap\n", numlights[config.compile_for_day], (config.compile_for_day ? "day" : "night"));
}

/**
 * @brief Checks traces against a single-tile map, optimized for ufo2map. This trace is only for visible levels.
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @sa TR_TestLine
 * @sa GatherSampleLight
 * @return qfalse if not blocked
 */
static qboolean TR_TestLineSingleTile (const vec3_t start, const vec3_t stop, int *headhint)
{
	int i;
	static int shared_lastthead = 0;
	int lastthead = *headhint;

	if (!lastthead) {
		lastthead = shared_lastthead;
		*headhint = lastthead;
	}

	assert(mapTiles.numTiles == 1);

	/* ufo2map does many traces to the same endpoint.
	 * Often an occluding node will be found in the same thead
	 * as the last trace, so test that one first. */
	if (curTile->theadlevel[lastthead] <= LEVEL_LASTLIGHTBLOCKING
		&& TR_TestLine_r(curTile, curTile->thead[lastthead], start, stop))
		return qtrue;

	for (i = 0; i < curTile->numtheads; i++) {
		const int level = curTile->theadlevel[i];
		if (i == lastthead)
			continue;
		if (level > LEVEL_LASTLIGHTBLOCKING)
			continue;
		if (TR_TestLine_r(curTile, curTile->thead[i], start, stop)) {
			shared_lastthead = *headhint = i;
			return qtrue;
		}
	}
	return qfalse;
}

/**
 * @brief A follow-up to GatherSampleLight, simply trace along the sun normal, adding sunlight
 */
static void GatherSampleSunlight (const vec3_t pos, const vec3_t normal, float *sample, float *direction, float scale, int *headhint)
{
	vec3_t delta;
	float dot, light;

	if (!sun_intensity)
		return;

	dot = DotProduct(sun_normal, normal);
	if (dot <= 0.001)
		return; /* wrong direction */

	/* don't use only 512 (which would be the 8 level max unit) but a
	 * higher value - because the light angle is not fixed at 90 degree */
	VectorMA(pos, 8192, sun_normal, delta);

	if (TR_TestLineSingleTile(pos, delta, headhint))
		return; /* occluded */

	light = sun_intensity * dot;
	if (light > 255)
		light = 255;
	light *= scale;

	/* add some light to it */
	VectorMA(sample, light, sun_color, sample);

	/* and accumulate the direction */
	VectorMix(normal, sun_normal, light / sun_intensity, delta);
	VectorMA(direction, light * scale, delta, direction);
}


/**
 * @param[out] sample The sample color
 * @param[in] normal The light direction (normal vector)
 * @param[in] pos The point in the world that receives the light
 * @param[in] scale is the normalizer for multisampling
 * @param[in,out] headhints An array of theads for each light to optimize the tracing
 */
static void GatherSampleLight (vec3_t pos, const vec3_t normal, float *sample, float *direction, float scale, int *headhints)
{
	light_t *l;
	vec3_t delta;
	float dot, dot2;
	float dist;
	int *headhint;

	for (l = lights[config.compile_for_day], headhint = headhints; l; l = l->next, headhint++) {
		float light = 0.0;

		/* Com_Printf("Looking with next hint.\n"); */

		VectorSubtract(l->origin, pos, delta);
		dist = VectorNormalize(delta);

		dot = DotProduct(delta, normal);
		if (dot <= 0.001)
			continue;	/* behind sample surface */

		switch (l->type) {
		case emit_point:
			/* linear falloff */
			light = (l->intensity - dist) * dot;
			break;

		case emit_surface:
			/* exponential falloff */
			light = (l->intensity / (dist * dist)) * dot;
			break;

		case emit_spotlight:
			/* linear falloff with cone */
			dot2 = -DotProduct(delta, l->normal);
			if (dot2 > l->stopdot) {
				/* inside the cone */
				light = (l->intensity - dist) * dot;
			} else {
				/* outside the cone */
				light = (l->intensity * (dot2 / l->stopdot) - dist) * dot;
			}
			break;
		default:
			Sys_Error("Bad l->type");
		}

		if (light <= 0.5)  /* almost no light */
			continue;

		if (TR_TestLineSingleTile(pos, l->origin, headhint))
			continue;	/* occluded */

		if (light > 255)
			light = 255;
		/* add some light to it */
		VectorMA(sample, light * scale, l->color, sample);

		/* and add some direction */
		VectorMix(normal, delta, 2.0 * light / l->intensity, delta);
		VectorMA(direction, light * scale, delta, direction);
	}

	/* Com_Printf("Looking with last hint.\n"); */
	GatherSampleSunlight(pos, normal, sample, direction, scale, headhint);
}

#define SAMPLE_NUDGE 0.25

/**
 * @brief Move the incoming sample position towards the surface center and along the
 * surface normal to reduce false-positive traces.
 */
static inline void NudgeSamplePosition (const vec3_t in, const vec3_t normal, const vec3_t center, vec3_t out)
{
	vec3_t dir;

	VectorCopy(in, out);

	/* move into the level using the normal and surface center */
	VectorSubtract(out, center, dir);
	VectorNormalize(dir);

	VectorMA(out, SAMPLE_NUDGE, dir, out);
	VectorMA(out, SAMPLE_NUDGE, normal, out);
}

#define MAX_VERT_FACES 256

/**
 * @brief Populate faces with indexes of all dBspFace_t's referencing the specified edge.
 * @param[out] nfaces The number of dBspFace_t's referencing edge
 */
static void FacesWithVert (int vert, int *faces, int *nfaces)
{
	int i, j, k;

	k = 0;
	for (i = 0; i < curTile->numfaces; i++) {
		const dBspSurface_t *face = &curTile->faces[i];
		const dBspTexinfo_t *tex = &curTile->texinfo[face->texinfo];

		/* only build vertex normals for phong shaded surfaces */
		if (!(tex->surfaceFlags & SURF_PHONG))
			continue;

		for (j = 0; j < face->numedges; j++) {
			const int e = curTile->surfedges[face->firstedge + j];
			const int v = e >= 0 ? curTile->edges[e].v[0] : curTile->edges[-e].v[1];

			/* face references vert */
			if (v == vert) {
				faces[k++] = i;
				if (k == MAX_VERT_FACES)
					Sys_Error("MAX_VERT_FACES");
				break;
			}
		}
	}
	*nfaces = k;
}

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by
 * finding all of the faces which share a given vertex, and calculating a weighted
 * average of their normals.
 */
void BuildVertexNormals (void)
{
	int vert_faces[MAX_VERT_FACES];
	int num_vert_faces;
	vec3_t norm, delta;
	float scale;
	int i, j;

	BuildFaceExtents();

	for (i = 0; i < curTile->numvertexes; i++) {
		VectorClear(curTile->normals[i].normal);

		FacesWithVert(i, vert_faces, &num_vert_faces);
		if (!num_vert_faces)  /* rely on plane normal only */
			continue;

		for (j = 0; j < num_vert_faces; j++) {
			const dBspSurface_t *face = &curTile->faces[vert_faces[j]];
			const dBspPlane_t *plane = &curTile->planes[face->planenum];
			extents_t *extends = &face_extents[vert_faces[j]];

			/* scale the contribution of each face based on size */
			VectorSubtract(extends->maxs, extends->mins, delta);
			scale = VectorLength(delta);

			if (face->side)
				VectorScale(plane->normal, -scale, norm);
			else
				VectorScale(plane->normal, scale, norm);

			VectorAdd(curTile->normals[i].normal, norm, curTile->normals[i].normal);
		}
		VectorNormalize(curTile->normals[i].normal);
	}
}


/**
 * @brief For Phong-shaded samples, interpolate the vertex normals for the surface in
 * question, weighting them according to their proximity to the sample position.
 * @todo Implement it (just clones the normal of nearest vertex for now)
 */
static void SampleNormal (const lightinfo_t *l, const vec3_t pos, vec3_t normal)
{
	vec3_t temp;
	float dist[MAX_VERT_FACES];
	float nearest;
	int i, v, nearv;

	nearest = 9999.0;
	nearv = 0;

	/* calculate the distance to each vertex */
	for (i = 0; i < l->face->numedges; i++) {  /* find nearest and farthest verts */
		const int e = curTile->surfedges[l->face->firstedge + i];
		if (e >= 0)
			v = curTile->edges[e].v[0];
		else
			v = curTile->edges[-e].v[1];

		VectorSubtract(pos, curTile->vertexes[v].point, temp);
		dist[i] = VectorLength(temp);
		if (dist[i] < nearest) {
			nearest = dist[i];
			nearv = v;
		}
	}
	VectorCopy(curTile->normals[nearv].normal, normal);
}


#define MAX_SAMPLES 5
#define SOFT_SAMPLES 4
static const float sampleofs[2][MAX_SAMPLES][2] = {
	{{0.0, 0.0}, {-0.125, -0.125}, {0.125, -0.125}, {0.125, 0.125}, {-0.125, 0.125}},
	{{-0.66, 0.33}, {-0.33, -0.66}, {0.33, 0.66}, {0.66, -0.33},{0.0,0.0}}
};

/**
 * @brief
 * @sa FinalLightFace
 */
void BuildFacelights (unsigned int facenum)
{
	dBspSurface_t *face;
	dBspPlane_t *plane;
	dBspTexinfo_t *tex;
	float *center;
	float *sdir, *tdir;
	vec3_t normal, binormal;
	vec4_t tangent;
	lightinfo_t li;
	float scale;
	int i, j, numsamples;
	facelight_t *fl;
	int *headhints;
	const int grid_type = config.soft ? 1 : 0;

	if (facenum >= MAX_MAP_FACES) {
		Com_Printf("MAX_MAP_FACES hit\n");
		return;
	}

	face = &curTile->faces[facenum];
	plane = &curTile->planes[face->planenum];
	tex = &curTile->texinfo[face->texinfo];

	if (tex->surfaceFlags & SURF_WARP)
		return;		/* non-lit texture */

	sdir = tex->vecs[0];
	tdir = tex->vecs[1];

	/* lighting -extra antialiasing */
	if (config.extrasamples)
		numsamples = config.soft ? SOFT_SAMPLES : MAX_SAMPLES;
	else
		numsamples = 1;

	OBJZERO(li);

	scale = 1.0 / numsamples; /* each sample contributes this much */

	li.face = face;
	li.facedist = plane->dist;
	VectorCopy(plane->normal, li.facenormal);
	/* negate the normal and dist */
	if (face->side) {
		VectorNegate(li.facenormal, li.facenormal);
		li.facedist = -li.facedist;
	}

	/* get the origin offset for rotating bmodels */
	VectorCopy(face_offset[facenum], li.modelorg);

	/* calculate lightmap texture mins and maxs */
	CalcLightinfoExtents(&li);

	/* and the lightmap texture vectors */
	CalcLightinfoVectors(&li);

	/* now generate all of the sample points */
	CalcPoints(&li, 0, 0);

	fl = &facelight[config.compile_for_day][facenum];
	fl->numsamples = li.numsurfpt;
	fl->samples = (float *)Mem_Alloc(fl->numsamples * sizeof(vec3_t));
	fl->directions = (float *)Mem_Alloc(fl->numsamples * sizeof(vec3_t));

	center = face_extents[facenum].center;  /* center of the face */

	/* Also setup the hints.  Each hint is specific to each light source, including sunlight. */
	headhints = (int *)Mem_Alloc((numlights[config.compile_for_day] + 1) * sizeof(int));

	/* calculate light for each sample */
	for (i = 0; i < fl->numsamples; i++) {
		float *sample = fl->samples + i * 3;			/* accumulate lighting here */
		float *direction = fl->directions + i * 3;		/* accumulate direction here */

		if (tex->surfaceFlags & SURF_PHONG)
			/* interpolated normal */
			SampleNormal(&li, li.surfpt[i], normal);
		else
			/* or just plane normal */
			VectorCopy(li.facenormal, normal);

		for (j = 0; j < numsamples; j++) {  /* with antialiasing */
			vec3_t pos;

			/* add offset for supersampling */
			VectorMA(li.surfpt[i], sampleofs[grid_type][j][0] * li.step, li.textoworld[0], pos);
			VectorMA(pos, sampleofs[grid_type][j][1] * li.step, li.textoworld[1], pos);

			NudgeSamplePosition(pos, normal, center, pos);

			GatherSampleLight(pos, normal, sample, direction, scale, headhints);
		}
		if (VectorNotEmpty(direction)) {
			vec3_t dir;

			/* normalize it */
			VectorNormalize(direction);

			/* finalize the lighting direction for the sample */
			TangentVectors(normal, sdir, tdir, tangent, binormal);

			dir[0] = DotProduct(direction, tangent);
			dir[1] = DotProduct(direction, binormal);
			dir[2] = DotProduct(direction, normal);

			VectorCopy(dir, direction);
		}
	}

	/* Free the hints. */
	Mem_Free(headhints);

	for (i = 0; i < fl->numsamples; i++) {  /* pad them */
		float *direction = fl->directions + i * 3;
		if (VectorEmpty(direction))
			VectorSet(direction, 0.0, 0.0, 1.0);
	}

	/* free the sample positions for the face */
	Mem_Free(li.surfpt);
}

#define TGA_HEADER_SIZE 18
static void WriteTGA24 (const char *filename, const byte * data, int width, int height, int offset)
{
	int i, size;
	byte *buffer;
	qFILE file;

	size = width * height * 3;
	/* allocate a buffer and set it up */
	buffer = (byte *)Mem_Alloc(size + TGA_HEADER_SIZE);
	memset(buffer, 0, TGA_HEADER_SIZE);
	buffer[2] = 2;
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;
	/* create top-down TGA */
	buffer[17] = 32;

	/* swap rgb to bgr */
	for (i = 0; i < size; i += 3) {
		buffer[i + TGA_HEADER_SIZE] = data[i*2 + offset + 2];	/* blue */
		buffer[i + TGA_HEADER_SIZE + 1] = data[i*2 + offset + 1];	/* green */
		buffer[i + TGA_HEADER_SIZE + 2] = data[i*2 + offset + 0];	/* red */
	}

	/* write it and free the buffer */
	if (FS_OpenFile(filename, &file, FILE_WRITE) > 0)
		Sys_Error("Unable to open %s for writing", filename);

	FS_Write(buffer, size + TGA_HEADER_SIZE, &file);

	/* close the file */
	FS_CloseFile(&file);
	Mem_Free(buffer);
}

/**
 * @brief Calculates the texture width for the lightmap texture. This depends on the surface
 * mins and maxs and the texture scale
 * @param[in] s The surface to calculate the lightmap size for
 * @param[out] texsize The resulting texture size vector. First value is width, second value is height
 * @param[in] scale The scale (1/scale) of the lightmap texture in relation to the surface texture
 */
static void CalcTextureSize (const dBspSurface_t *s, vec2_t texsize, int scale)
{
	const float *stmins = face_extents[s - curTile->faces].stmins;
	const float *stmaxs = face_extents[s - curTile->faces].stmaxs;
	int i;

	for (i = 0; i < 2; i++) {
		const float mins = floor(stmins[i] / scale);
		const float maxs = ceil(stmaxs[i] / scale);

		texsize[i] = maxs - mins + 1;
	}
}

/**
 * @brief Export all the faces for one particular lightmap (day or night)
 * @param path The path to write the files into
 * @param name The name of the map to export the lightmap for
 * @param day @c true to export the day lightmap data, @c false to export the night lightmap data
 */
static void ExportLightmap (const char *path, const char *name, qboolean day)
{
	int i;
	const int lightmapIndex = day ? 1 : 0;
	const byte *bspLightBytes = curTile->lightdata[lightmapIndex];
	const byte quant = *bspLightBytes;
	const int scale = 1 << quant;

	for (i = 0; i < curTile->numfaces; i++) {
		const dBspSurface_t *face = &curTile->faces[i];
		const byte *lightmap = bspLightBytes + face->lightofs[lightmapIndex];
		vec2_t texSize;

		CalcTextureSize(face, texSize, scale);

		/* write a tga image out */
		if (Vector2NotEmpty(texSize)) {
			char filename[MAX_QPATH];
			Com_sprintf(filename, sizeof(filename), "%s/%s_lightmap_%04d%c.tga", path, name, i, day ? 'd' : 'n');
			Com_Printf("Writing %s (%.0fx%.0f)\n", filename, texSize[0], texSize[1]);
			WriteTGA24(filename, lightmap, texSize[0], texSize[1], 0);
			Com_sprintf(filename, sizeof(filename), "%s/%s_direction_%04d%c.tga", path, name, i, day ? 'd' : 'n');
			Com_Printf("Writing %s (%.0fx%.0f)\n", filename, texSize[0], texSize[1]);
			WriteTGA24(filename, lightmap, texSize[0], texSize[1], 3);
		}
	}
}

/**
 * @brief Export the day and night lightmap and direction data for the given map.
 * @note The bsp file must already be loaded.
 * @param bspFileName The path of the loaded bsp file.
 */
void ExportLightmaps (const char *bspFileName)
{
	char path[MAX_QPATH], lightmapName[MAX_QPATH];
	const char *fileName = Com_SkipPath(bspFileName);

	Com_FilePath(bspFileName, path);
	Com_StripExtension(fileName, lightmapName, sizeof(lightmapName));

	/* note it */
	Com_Printf("--- ExportLightmaps ---\n");

	BuildFaceExtents();

	ExportLightmap(path, lightmapName, qtrue);
	ExportLightmap(path, lightmapName, qfalse);
}

static const vec3_t luminosity = {0.2125, 0.7154, 0.0721};

/**
 * @brief Add the indirect lighting on top of the direct
 * lighting and save into final map format
 * @sa BuildFacelights
 */
void FinalLightFace (unsigned int facenum)
{
	dBspSurface_t *f;
	int j, k;
	vec3_t dir, intensity;
	facelight_t	*fl;
	float max, d;
	byte *dest;

	f = &curTile->faces[facenum];
	fl = &facelight[config.compile_for_day][facenum];

	/* none-lit texture */
	if (curTile->texinfo[f->texinfo].surfaceFlags & SURF_WARP)
		return;

	ThreadLock();

	f->lightofs[config.compile_for_day] = curTile->lightdatasize[config.compile_for_day];
	curTile->lightdatasize[config.compile_for_day] += fl->numsamples * 3;
	/* account for light direction data as well */
	curTile->lightdatasize[config.compile_for_day] += fl->numsamples * 3;

	if (curTile->lightdatasize[config.compile_for_day] > MAX_MAP_LIGHTING)
		Sys_Error("MAX_MAP_LIGHTING (%i exceeded %i) - try to reduce the brush size (%s)",
			curTile->lightdatasize[config.compile_for_day], MAX_MAP_LIGHTING,
			curTile->texinfo[f->texinfo].texture);

	ThreadUnlock();

	/* write it out */
	dest = &curTile->lightdata[config.compile_for_day][f->lightofs[config.compile_for_day]];

	for (j = 0; j < fl->numsamples; j++) {
		vec3_t temp;

		/* start with raw sample data */
		VectorCopy((fl->samples + j * 3), temp);

		/* convert to float */
		VectorScale(temp, 1.0 / 255.0, temp);

		/* add an ambient term if desired */
		VectorAdd(temp, sun_ambient_color, temp);

		/* apply global scale factor */
		VectorScale(temp, config.brightness, temp);

		max = 0.0;

		/* find the brightest component */
		for (k = 0; k < 3; k++) {
			/* enforcing positive values */
			if (temp[k] < 0.0)
				temp[k] = 0.0;

			if (temp[k] > max)
				max = temp[k];
		}

		if (max > 255.0)  /* clamp without changing hue */
			VectorScale(temp, 255.0 / max, temp);

		for (k = 0; k < 3; k++) {  /* apply contrast */
			temp[k] -= 0.5;  /* normalize to -0.5 through 0.5 */

			temp[k] *= config.contrast;  /* scale */

			temp[k] += 0.5;

			if (temp[k] > 1.0)  /* clamp */
				temp[k] = 1.0;
			else if (temp[k] < 0)
				temp[k] = 0;
		}

		/* apply saturation */
		d = DotProduct(temp, luminosity);

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, temp, config.saturation, temp);

		for (k = 0; k < 3; k++) {
			temp[k] *= 255.0;  /* back to byte */

			if (temp[k] > 255.0)  /* clamp */
				temp[k] = 255.0;
			else if (temp[k] < 0.0)
				temp[k] = 0.0;

			*dest++ = (byte)temp[k];
		}

		/* also write the directional data */
		VectorCopy((fl->directions + j * 3), dir);
		for (k = 0; k < 3; k++)
			*dest++ = (byte)((dir[k] + 1.0f) * 127.0f);
	}
}
