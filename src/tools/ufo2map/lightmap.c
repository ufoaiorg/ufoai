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


#include "radiosity.h"
#include "../../common/tracing.h"

static int facelinks[MAX_MAP_FACES];
static int planelinks[2][MAX_MAP_PLANES];

vec3_t face_offset[MAX_MAP_FACES];		/**< for rotating bmodels */

/**
 * @sa RadWorld
 */
void LinkPlaneFaces (void)
{
	int i;
	const dBspFace_t *f;

	f = curTile->faces;
	for (i = 0; i < curTile->numfaces; i++, f++) {
		facelinks[i] = planelinks[f->side][f->planenum];
		planelinks[f->side][f->planenum] = i;
	}
}

/*
=================================================================
POINT TRIANGULATION
=================================================================
*/

typedef struct triedge_s {
	int			p0, p1;
	vec3_t		normal;
	vec_t		dist;
	struct triangle_s	*tri;
} triedge_t;

typedef struct triangle_s {
	triedge_t	*edges[3];
} triangle_t;

#define	MAX_TRI_POINTS		1024
#define	MAX_TRI_EDGES		(MAX_TRI_POINTS*6)
#define	MAX_TRI_TRIS		(MAX_TRI_POINTS*2)

typedef struct {
	int			numpoints;
	int			numedges;
	int			numtris;
	dBspPlane_t	*plane;
	triedge_t	*edgematrix[MAX_TRI_POINTS][MAX_TRI_POINTS];
	patch_t		*points[MAX_TRI_POINTS];
	triedge_t	edges[MAX_TRI_EDGES];
	triangle_t	tris[MAX_TRI_TRIS];
} triangulation_t;

/**
 * @sa FreeTriangulation
 */
static triangulation_t *AllocTriangulation (dBspPlane_t *plane)
{
	triangulation_t *tr;

	tr = malloc(sizeof(*tr));
	memset(tr, 0, sizeof(*tr));
	tr->plane = plane;

	return tr;
}

/**
 * @sa AllocTriangulation
 */
static void FreeTriangulation (triangulation_t *tr)
{
	free(tr);
}

/**
 * @sa TriangulatePoints
 */
static triedge_t *FindEdge (triangulation_t *trian, int p0, int p1)
{
	triedge_t *e, *be;
	vec3_t v1, normal;
	vec_t dist;

	if (trian->edgematrix[p0][p1])
		return trian->edgematrix[p0][p1];

	if (trian->numedges > MAX_TRI_EDGES - 2)
		Sys_Error("trian->numedges > MAX_TRI_EDGES-2");

	VectorSubtract(trian->points[p1]->origin, trian->points[p0]->origin, v1);
	VectorNormalize(v1);
	CrossProduct(v1, trian->plane->normal, normal);
	dist = DotProduct(trian->points[p0]->origin, normal);

	e = &trian->edges[trian->numedges];
	e->p0 = p0;
	e->p1 = p1;
	e->tri = NULL;
	VectorCopy(normal, e->normal);
	e->dist = dist;
	trian->numedges++;
	trian->edgematrix[p0][p1] = e;

	be = &trian->edges[trian->numedges];
	be->p0 = p1;
	be->p1 = p0;
	be->tri = NULL;
	VectorNegate(normal, be->normal);
	be->dist = -dist;
	trian->numedges++;
	trian->edgematrix[p1][p0] = be;

	return e;
}

static inline triangle_t *AllocTriangle (triangulation_t *trian)
{
	triangle_t *t;

	if (trian->numtris >= MAX_TRI_TRIS)
		Sys_Error("trian->numtris >= MAX_TRI_TRIS");

	t = &trian->tris[trian->numtris];
	trian->numtris++;

	return t;
}

static void TriEdge_r (triangulation_t *trian, triedge_t *e)
{
	int i, bestp = 0;
	vec3_t v1, v2;
	vec_t *p0, *p1;
	vec_t best, ang;
	triangle_t *nt;

	if (e->tri)
		return;		/* already connected by someone */

	/* find the point with the best angle */
	p0 = trian->points[e->p0]->origin;
	p1 = trian->points[e->p1]->origin;
	best = 1.1;
	for (i = 0; i < trian->numpoints; i++) {
		const vec_t *p = trian->points[i]->origin;
		/* a 0 dist will form a degenerate triangle */
		if (DotProduct(p, e->normal) - e->dist <= 0)
			continue;	/* behind edge */
		VectorSubtract(p0, p, v1);
		VectorSubtract(p1, p, v2);
		if (!VectorNormalize(v1))
			continue;
		if (!VectorNormalize(v2))
			continue;
		ang = DotProduct(v1, v2);
		if (ang < best) {
			best = ang;
			bestp = i;
		}
	}
	if (best >= 1)
		return;		/* edge doesn't match anything */

	/* make a new triangle */
	nt = AllocTriangle(trian);
	nt->edges[0] = e;
	nt->edges[1] = FindEdge(trian, e->p1, bestp);
	nt->edges[2] = FindEdge(trian, bestp, e->p0);
	for (i = 0; i < 3; i++)
		nt->edges[i]->tri = nt;
	TriEdge_r(trian, FindEdge(trian, bestp, e->p1));
	TriEdge_r(trian, FindEdge(trian, e->p0, bestp));
}

/**
 * @sa FindEdge
 */
static void TriangulatePoints (triangulation_t *trian)
{
	vec_t d, bestd;
	vec3_t v1;
	int bp1 = 0, bp2 = 0, i, j;
	vec_t *p1, *p2;
	triedge_t *e, *e2;

	if (trian->numpoints < 2)
		return;

	/* find the two closest points */
	bestd = 9999;
	for (i = 0; i < trian->numpoints; i++) {
		p1 = trian->points[i]->origin;
		for (j = i + 1; j < trian->numpoints; j++) {
			p2 = trian->points[j]->origin;
			VectorSubtract(p2, p1, v1);
			d = VectorLength(v1);
			if (d < bestd) {
				bestd = d;
				bp1 = i;
				bp2 = j;
			}
		}
	}

	e = FindEdge(trian, bp1, bp2);
	e2 = FindEdge(trian, bp2, bp1);
	TriEdge_r(trian, e);
	TriEdge_r(trian, e2);
}

static void AddPointToTriangulation (patch_t *patch, triangulation_t *trian)
{
	if (trian->numpoints == MAX_TRI_POINTS)
		Sys_Error("trian->numpoints == MAX_TRI_POINTS (%i)", trian->numpoints);
	trian->points[trian->numpoints] = patch;
	trian->numpoints++;
}

/**
 * @param[in] trian
 * @param[in] t
 * @param[in] point
 * @param[out] color
 */
static void LerpTriangle (const triangulation_t *trian, const triangle_t *t, const vec3_t point, vec3_t color)
{
	const patch_t *p1, *p2, *p3;
	vec3_t base, d1, d2;
	float x, y, x1, y1, x2, y2;

	p1 = trian->points[t->edges[0]->p0];
	p2 = trian->points[t->edges[1]->p0];
	p3 = trian->points[t->edges[2]->p0];

	VectorCopy(p1->totallight, base);
	VectorSubtract(p2->totallight, base, d1);
	VectorSubtract(p3->totallight, base, d2);

	x = DotProduct(point, t->edges[0]->normal) - t->edges[0]->dist;
	y = DotProduct(point, t->edges[2]->normal) - t->edges[2]->dist;

	x1 = 0;
	y1 = DotProduct(p2->origin, t->edges[2]->normal) - t->edges[2]->dist;

	x2 = DotProduct(p3->origin, t->edges[0]->normal) - t->edges[0]->dist;
	y2 = 0;

	if (fabs(y1) < ON_EPSILON || fabs(x2) < ON_EPSILON) {
		VectorCopy(base, color);
		return;
	}

	VectorMA(base, x / x2, d2, color);
	VectorMA(color, y / y1, d1, color);
}

static qboolean PointInTriangle (const vec3_t point, const triangle_t *t)
{
	int i;

	for (i = 0; i < 3; i++) {
		const triedge_t *e = t->edges[i];
		const vec_t d = DotProduct(e->normal, point) - e->dist;
		if (d < 0)
			return qfalse;	/* not inside */
	}

	return qtrue;
}

static void SampleTriangulation (const vec3_t point, const triangulation_t *trian, vec3_t color)
{
	const triangle_t *t;
	const triedge_t *e;
	vec_t d, best;
	patch_t *p0, *p1;
	vec3_t v1, v2;
	int i, j;

	if (trian->numpoints == 0) {
		VectorClear(color);
		return;
	}
	if (trian->numpoints == 1) {
		VectorCopy(trian->points[0]->totallight, color);
		return;
	}

	/* search for triangles */
	for (t = trian->tris, j = 0; j < trian->numtris; t++, j++) {
		if (!PointInTriangle(point, t))
			continue;

		/* this is it */
		LerpTriangle(trian, t, point, color);
		return;
	}

	/* search for exterior edge */
	for (e = trian->edges, j = 0; j < trian->numedges; e++, j++) {
		if (e->tri)
			continue;		/* not an exterior edge */

		d = DotProduct(point, e->normal) - e->dist;
		if (d < 0)
			continue;	/* not in front of edge */

		p0 = trian->points[e->p0];
		p1 = trian->points[e->p1];

		VectorSubtract(p1->origin, p0->origin, v1);
		VectorNormalize(v1);
		VectorSubtract(point, p0->origin, v2);
		d = DotProduct(v2, v1);
		if (d < 0)
			continue;
		if (d > 1)
			continue;
		for (i = 0; i < 3; i++)
			color[i] = p0->totallight[i] + d * (p1->totallight[i] - p0->totallight[i]);
		return;
	}

	/* search for nearest point */
	best = 99999;
	p1 = NULL;
	for (j = 0; j < trian->numpoints; j++) {
		p0 = trian->points[j];
		VectorSubtract(point, p0->origin, v1);
		d = VectorLength(v1);
		if (d < best) {
			best = d;
			p1 = p0;
		}
	}

	if (!p1)
		Sys_Error("SampleTriangulation: no points");

	VectorCopy(p1->totallight, color);
}

/*
=================================================================
LIGHTMAP SAMPLE GENERATION
=================================================================
*/


#define	SINGLEMAP	(256 * 256 * 4)

typedef struct {
	vec_t	facedist;
	vec3_t	facenormal;

	vec3_t center;

	int		numsurfpt;
	vec3_t	surfpt[SINGLEMAP];

	vec3_t	modelorg;		/* for origined bmodels */

	vec3_t	texorg;
	vec3_t	worldtotex[2];	/* s = (world - texorg) . worldtotex[0] */
	vec3_t	textoworld[2];	/* world = texorg + s * textoworld[0] */

	int		texmins[2], texsize[2];
	int		surfnum;
	dBspFace_t	*face;
} lightinfo_t;


/**
 * @brief Fills in s->texmins[] and s->texsize[]
 */
static void CalcFaceExtents (lightinfo_t *l)
{
	const dBspFace_t *s;
	vec3_t mins, maxs;
	vec2_t stmins, stmaxs;
	int i, j;
	const dBspVertex_t *v;
	const dBspTexinfo_t *tex;

	s = l->face;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);
	Vector2Set(stmins, 999999, 999999);
	Vector2Set(stmaxs, -999999, -999999);

	tex = &curTile->texinfo[s->texinfo];

	for (i = 0; i < s->numedges; i++) {
		const int e = curTile->surfedges[s->firstedge + i];
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
			const float val = DotProduct(v->point, tex->vecs[j]) + tex->vecs[j][3];
			if (val < stmins[j])
				stmins[j] = val;
			if (val > stmaxs[j])
				stmaxs[j] = val;
		}
	}

	for (i = 0; i < 3; i++)
		l->center[i] = (mins[i] + maxs[i]) / 2;

	for (i = 0; i < 2; i++) {
		stmins[i] = floor(stmins[i] / (1 << config.lightquant));
		stmaxs[i] = ceil(stmaxs[i] / (1 << config.lightquant));

		l->texmins[i] = stmins[i];
		l->texsize[i] = stmaxs[i] - stmins[i];
		if (l->texsize[0] * l->texsize[1] > SINGLEMAP)
			Sys_Error("Surface too large to light %i - %i (%i)", l->texsize[0], l->texsize[1], SINGLEMAP);
	}
}

/**
 * @brief Fills in texorg, worldtotex. and textoworld
 */
static void CalcFaceVectors (lightinfo_t *l)
{
	const dBspTexinfo_t *tex;
	int i, j, w, h;
	vec3_t texnormal;
	vec_t distscale, dist;

	tex = &curTile->texinfo[l->face->texinfo];

	/* convert from float to double */
	for (i = 0; i < 2; i++)
		for (j = 0; j < 3; j++)
			l->worldtotex[i][j] = tex->vecs[i][j];

	/* calculate a normal to the texture axis.  points can be moved along this
	 * without changing their S/T */
	texnormal[0] = tex->vecs[1][1] * tex->vecs[0][2] - tex->vecs[1][2] * tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2] * tex->vecs[0][0] - tex->vecs[1][0] * tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0] * tex->vecs[0][1] - tex->vecs[1][1] * tex->vecs[0][0];
	VectorNormalize(texnormal);

	/* flip it towards plane normal */
	distscale = DotProduct(texnormal, l->facenormal);
	if (!distscale) {
		Sys_FPrintf(SYS_VRB, "WARNING: Texture axis perpendicular to face\n");
		distscale = 1;
	}
	if (distscale < 0) {
		distscale = -distscale;
		VectorSubtract(vec3_origin, texnormal, texnormal);
	}

	/* distscale is the ratio of the distance along the texture normal to
	 * the distance along the plane normal */
	distscale = 1 / distscale;

	for (i = 0; i < 2; i++) {
		const vec_t len = VectorLength(l->worldtotex[i]);
		const vec_t distance = DotProduct(l->worldtotex[i], l->facenormal) * distscale;
		VectorMA(l->worldtotex[i], -distance, texnormal, l->textoworld[i]);
		VectorScale(l->textoworld[i], (1 / len) * (1 / len), l->textoworld[i]);
	}

	/* calculate texorg on the texture plane */
	for (i = 0; i < 3; i++)
		l->texorg[i] = -tex->vecs[0][3] * l->textoworld[0][i] - tex->vecs[1][3] * l->textoworld[1][i];

	/* project back to the face plane */
	dist = DotProduct(l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMA(l->texorg, -dist, texnormal, l->texorg);

	/* compensate for org'd bmodels */
	VectorAdd(l->texorg, l->modelorg, l->texorg);

	/* total sample count */
	h = l->texsize[1] + 1;
	w = l->texsize[0] + 1;
	l->numsurfpt = w * h;
}

/**
 * @brief For each texture aligned grid point, back project onto the plane
 * to get the world xyz value of the sample point
 */
static void CalcPoints (lightinfo_t *l, float sofs, float tofs)
{
	int s, t, j, w, h, step;
	vec_t starts, startt;
	vec_t *surf;
	vec3_t pos;
	const dBspLeaf_t *leaf;

	/* fill in surforg
	 * the points are biased towards the center of the surfaces
	 * to help avoid edge cases just inside walls */
	surf = l->surfpt[0];

	h = l->texsize[1] + 1;
	w = l->texsize[0] + 1;
	l->numsurfpt = w * h;

	step = 1 << config.lightquant;
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

			VectorMA(surf, 2, l->facenormal, pos);
			leaf = Rad_PointInLeaf(pos);
			if (leaf->contentFlags == CONTENTS_SOLID)
				continue;
		}
	}
}

typedef struct {
	int			numsamples;
	float		*origins;
	float		*samples;
} facelight_t;

static directlight_t *directlights[2];
static facelight_t facelight[2][MAX_MAP_FACES];
static int numdlights[2];

/*#define	DIRECT_LIGHT	3000 */
#define	DIRECT_LIGHT	3

/**
 * @brief Create directlights out of patches and lights
 * @sa RadWorld
 */
void CreateDirectLights (void)
{
	int i;
	patch_t *p;
	directlight_t *dl;
	const dBspLeaf_t *leaf;

	/* surfaces */
	for (i = 0, p = patches; i < num_patches; i++, p++) {
		if (p->totallight[0] < DIRECT_LIGHT && p->totallight[1] < DIRECT_LIGHT
			&& p->totallight[2] < DIRECT_LIGHT)
			continue;

		numdlights[config.compile_for_day]++;
		dl = malloc(sizeof(*dl));
		memset(dl, 0, sizeof(*dl));

		VectorCopy(p->origin, dl->origin);

		leaf = Rad_PointInLeaf(dl->origin);
		dl->next = directlights[config.compile_for_day];
		directlights[config.compile_for_day] = dl;

		dl->type = emit_surface;
		VectorCopy(p->plane->normal, dl->normal);

		dl->intensity = ColorNormalize(p->totallight, dl->color);
		dl->intensity *= p->area * config.direct_scale;
		VectorClear(p->totallight);	/* all sent now */
	}

	/* entities (without world) */
	for (i = 1; i < num_entities; i++) {
		float intensity;
		const char *color;
		const char *target;
		const entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		if (strncmp(name, "light", 5))
			continue;

		/* remove those lights that are only for the night version */
		if (config.compile_for_day) {
			const int spawnflags = atoi(ValueForKey(e, "spawnflags"));
			if (!(spawnflags & 1))	/* day */
				continue;
		}

		numdlights[config.compile_for_day]++;
		dl = malloc(sizeof(*dl));
		memset(dl, 0, sizeof(*dl));

		GetVectorForKey(e, "origin", dl->origin);

		/* link in */
		dl->next = directlights[config.compile_for_day];
		directlights[config.compile_for_day] = dl;

		intensity = FloatForKey(e, "light");
		if (!intensity)
			intensity = 100;
		color = ValueForKey(e, "_color");
		if (color[1]) {
			sscanf(color, "%f %f %f", &dl->color[0], &dl->color[1], &dl->color[2]);
			ColorNormalize(dl->color, dl->color);
		} else
			dl->color[0] = dl->color[1] = dl->color[2] = 1.0;
		dl->intensity = intensity * config.entity_scale;
		dl->type = emit_point;

		target = ValueForKey(e, "target");
		if (!strcmp(name, "light_spot") || target[0]) {
			dl->type = emit_spotlight;
			dl->stopdot = FloatForKey (e, "_cone");
			if (!dl->stopdot)
				dl->stopdot = 10;
			dl->stopdot = cos(dl->stopdot / 180.0f * M_PI);
			if (target[0]) {	/* point towards target */
				entity_t *e2 = FindTargetEntity(target);
				if (!e2)
					Com_Printf("WARNING: light at (%i %i %i) has missing target '%s' - e.g. create an info_null that has a 'targetname' set to '%s'\n",
						(int)dl->origin[0], (int)dl->origin[1], (int)dl->origin[2], target, target);
				else {
					vec3_t dest;
					GetVectorForKey(e2, "origin", dest);
					VectorSubtract(dest, dl->origin, dl->normal);
					VectorNormalize(dl->normal);
				}
			} else {	/* point down angle */
				float angle = FloatForKey(e, "angle");
				if (angle == ANGLE_UP) {
					dl->normal[0] = dl->normal[1] = 0;
					dl->normal[2] = 1;
				} else if (angle == ANGLE_DOWN) {
					dl->normal[0] = dl->normal[1] = 0;
					dl->normal[2] = -1;
				} else {
					dl->normal[2] = 0;
					dl->normal[0] = cos(angle / 180.0f * M_PI);
					dl->normal[1] = sin(angle / 180.0f * M_PI);
				}
			}
		}
	}

	/* handle worldspawn light settings */
	{
		const entity_t *e = &entities[0];
		if (config.compile_for_day) {
			const char *ambient = ValueForKey(e, "ambient_day");
			const char *light = ValueForKey(e, "light_day");
			const char *angles = ValueForKey(e, "angles_day");
			const char *color = ValueForKey(e, "color_day");
			vec3_t vector;

			if (light[0] != '\0')
				config.day_sun_intensity = atoi(light);

			if (angles[0] != '\0') {
				GetVectorForKey(e, "angles_day", vector);
				config.day_sun_pitch = vector[PITCH] * torad;
				config.day_sun_yaw = vector[YAW] * torad;
				VectorSet(config.day_sun_dir,
					cos(config.day_sun_yaw) * sin(config.day_sun_pitch),
					sin(config.day_sun_yaw) * sin(config.day_sun_pitch),
					cos(config.day_sun_pitch));
			}

			if (color[0] != '\0') {
				GetVectorForKey(e, "color_day", vector);
				VectorCopy(vector, config.day_sun_color);
			}

			if (ambient[0] != '\0') {
				GetVectorForKey(e, "ambient_day", vector);
				config.day_ambient_red = vector[0];
				config.day_ambient_green = vector[1];
				config.day_ambient_blue = vector[2];
			}
		} else {
			const char *ambient = ValueForKey(e, "ambient_night");
			const char *light = ValueForKey(e, "light_night");
			const char *angles = ValueForKey(e, "angles_night");
			const char *color = ValueForKey(e, "color_night");
			vec3_t vector;

			if (light[0] != '\0')
				config.night_sun_intensity = atoi(light);

			if (angles[0] != '\0') {
				GetVectorForKey(e, "angles_night", vector);
				config.night_sun_pitch = vector[PITCH] * torad;
				config.night_sun_yaw = vector[YAW] * torad;
				VectorSet(config.night_sun_dir,
					cos(config.night_sun_yaw) * sin(config.night_sun_pitch),
					sin(config.night_sun_yaw) * sin(config.night_sun_pitch),
					cos(config.night_sun_pitch));
			}

			if (color[0] != '\0') {
				GetVectorForKey(e, "color_night", vector);
				VectorCopy(vector, config.night_sun_color);
			}

			if (ambient[0] != '\0') {
				GetVectorForKey(e, "ambient_night", vector);
				config.night_ambient_red = vector[0];
				config.night_ambient_green = vector[1];
				config.night_ambient_blue = vector[2];
			}
		}
	}

	ColorNormalize(config.day_sun_color, config.day_sun_color);
	ColorNormalize(config.night_sun_color, config.night_sun_color);
	if (config.compile_for_day) {
		config.day_ambient_red *= 128;
		config.day_ambient_green *= 128;
		config.day_ambient_blue *= 128;
	} else {
		config.night_ambient_red *= 128;
		config.night_ambient_green *= 128;
		config.night_ambient_blue *= 128;
	}

	Com_Printf("%i direct lights for %s lightmap\n", numdlights[config.compile_for_day], (config.compile_for_day ? "day" : "night"));
}


/**
 * @param[in] lightscale is the normalizer for multisampling
 */
static void GatherSampleLight (vec3_t pos, const vec3_t normal, const vec3_t center,
	float *styletable, int offset, int mapsize, float lightscale,
	const float sun_intensity, const vec3_t sun_color, const vec3_t sun_dir)
{
	directlight_t *l;
	vec3_t dir, delta;
	float dot, dot2, dist;
	float scale = 0.0f;
	float *dest;

	/* move into the level using the normal and surface center */
	VectorSubtract(pos, center, dir);
	VectorNormalize(dir);

	VectorMA(pos, 0.5, dir, pos);
	VectorMA(pos, 0.5, normal, pos);

	for (l = directlights[config.compile_for_day]; l; l = l->next) {
		VectorSubtract(l->origin, pos, delta);
		dist = VectorNormalize(delta);
		dot = DotProduct(delta, normal);
		if (dot <= 0.001)
			continue;	/* behind sample surface */

		switch (l->type) {
		case emit_point:
			/* linear falloff */
			scale = (l->intensity - dist) * dot;
			break;

		case emit_surface:
			dot2 = -DotProduct(delta, l->normal);
			if (dot2 <= 0.001)
				continue;	/* outside light cone */
			scale = (l->intensity / (dist * dist)) * dot * dot2;
			break;

		case emit_spotlight:
			/* linear falloff */
			dot2 = -DotProduct(delta, l->normal);
			if (dot2 <= l->stopdot)
				continue;	/* outside light cone */
			scale = (l->intensity - dist) * dot;
			break;
		default:
			Sys_Error("Bad l->type");
		}

		if (scale <= 0)
			continue;

		if (TR_TestLine(pos, l->origin, TL_FLAG_NONE))
			continue;	/* occluded */

		dest = styletable + offset;
		/* add some light to it */
		VectorMA(dest, scale * lightscale, l->color, dest);
	}

	/* add sun light */
	if (!sun_intensity)
		return;

	dot = DotProduct(sun_dir, normal);
	if (dot <= 0.001)
		return; /* wrong direction */

	/* don't use only 512 (which would be the 8 level max unit) but a
	 * higher value - because the light angle is not fixed at 90 degree */
	VectorMA(pos, 8192, sun_dir, delta);

	if (TR_TestLine(pos, delta, TL_FLAG_NONE))
		return; /* occluded */

	assert(styletable);
	dest = styletable + offset;

	/* add some light to it */
	VectorMA(dest, sun_intensity * dot * lightscale, sun_color, dest);
}

/**
 * @brief Take the sample's collected light and
 * add it back into the apropriate patch
 * for the radiosity pass.
 *
 * The sample is added to all patches that might include
 * any part of it. They are counted and averaged, so it
 * doesn't generate extra light.
 */
static inline void AddSampleToPatch (const vec3_t pos, const vec3_t color, const int facenum)
{
	patch_t *patch;
	vec3_t mins, maxs;
	int i;

	if (color[0] + color[1] + color[2] < 3)
		return;

	for (patch = face_patches[facenum]; patch; patch = patch->next) {
		/* see if the point is in this patch (roughly) */
		WindingBounds(patch->winding, mins, maxs);
		for (i = 0; i < 3; i++) {
			if (mins[i] > pos[i] + 8)
				goto nextpatch;
			if (maxs[i] < pos[i] - 8)
				goto nextpatch;
		}

		/* add the sample to the patch */
		patch->samples++;
		VectorAdd(patch->samplelight, color, patch->samplelight);
nextpatch:;
	}
}

#define MAX_VERT_FACES 256

/**
 * @brief Populate faces with indexes of all dBspFace_t's referencing the specified edge.
 * @param[out] nfaces The number of dBspFace_t's referencing edge
 * @param[out] faces
 * @param[in] vert
 */
static void FacesWithVert (int vert, int *faces, int *nfaces)
{
	int i, j, k;

	k = 0;
	for (i = 0; i < curTile->numfaces; i++) {
		const dBspFace_t *face = &curTile->faces[i];

		if (!(curTile->texinfo[face->texinfo].surfaceFlags & SURF_PHONG))
			continue;

		for (j = 0; j < face->numedges; j++) {
			const int e = curTile->surfedges[face->firstedge + j];
			const int v = e >= 0 ? curTile->edges[e].v[0] : curTile->edges[-e].v[1];

			/* compare using surfedge reference or point equality */
			if (v == vert || VectorCompareEps(curTile->vertexes[v].point, curTile->vertexes[vert].point, EQUAL_EPSILON)) {
				faces[k++] = i;
				if (k >= MAX_VERT_FACES)
					Sys_Error("MAX_VERT_FACES");
				break;
			}
		}
	}
	*nfaces = k;
}

static vec3_t vertexnormals[MAX_MAP_VERTS];

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors.  This is done by
 * finding all of the faces which use a given vertex, and calculating an average
 * of their normal vectors.
 */
void BuildVertexNormals (void)
{
	int vert_faces[MAX_VERT_FACES];
	int num_vert_faces;
	vec3_t norm;
	int i, j;

	for (i = 0; i < curTile->numvertexes; i++) {
		VectorSet(vertexnormals[i], 0, 0, 0);
		FacesWithVert(i, vert_faces, &num_vert_faces);

		for (j = 0; j < num_vert_faces; j++) {
			const dBspFace_t *face = &curTile->faces[vert_faces[j]];
			const dBspPlane_t *plane = &curTile->planes[face->planenum];

			if (face->side)
				VectorNegate(plane->normal, norm);
			else
				VectorCopy(plane->normal, norm);

			VectorAdd(vertexnormals[i], norm, vertexnormals[i]);
		}
		VectorScale(vertexnormals[i], 1.0 / num_vert_faces, vertexnormals[i]);
		VectorNormalize(vertexnormals[i]);
	}
}


/**
 * @brief For Phong-shaded samples, interpolate the vertex normals for the surface,
 * weighting them according to their proximity to pos.
 */
static void SampleNormal (const lightinfo_t *l, const vec3_t pos, vec3_t normal)
{
	vec3_t temp;
	float dist, neardist, fardist;
	int nearEdge, farEdge;
	int i, v;

	neardist = 999999;
	fardist = -999999;

	nearEdge = farEdge = 0;

	for (i = 0; i < l->face->numedges; i++) {  /* find nearest and farthest verts */
		const int e = curTile->surfedges[l->face->firstedge + i];
		if (e >= 0)
			v = curTile->edges[e].v[0];
		else
			v = curTile->edges[-e].v[1];

		VectorSubtract(pos, curTile->vertexes[v].point, temp);
		dist = VectorLength(temp);

		if (dist < neardist) {
			neardist = dist;
			nearEdge = v;
		}

		if (dist > fardist) {
			fardist = dist;
			farEdge = v;
		}
	}

#if 0
	/* interpolate between nearest and farthest verts */
	VectorScale(vertexnormals[near], fardist / (neardist + fardist), temp);
	VectorScale(vertexnormals[far], neardist / (neardist + fardist), temp2);
	VectorAdd(temp, temp2, normal);
#endif

	VectorCopy(vertexnormals[nearEdge], normal);
}


#define MAX_SAMPLES 5
static const float sampleofs[MAX_SAMPLES][2] = { {0,0}, {-0.4, -0.4}, {0.4, -0.4}, {0.4, 0.4}, {-0.4, 0.4} };

/**
 * @brief
 * @sa FinalLightFace
 */
void BuildFacelights (unsigned int facenum)
{
	dBspFace_t *f;
	lightinfo_t *l;
	float *styletable, lightscale;
	int i, j, numsamples;
	patch_t *patch;
	size_t tablesize;
	facelight_t *fl;
	vec3_t normal, sun_color, sun_dir;
	float sun_intensity;

	if (facenum >= MAX_MAP_FACES) {
		Com_Printf("MAX_MAP_FACES hit\n");
		return;
	}

	f = &curTile->faces[facenum];

	if (curTile->texinfo[f->texinfo].surfaceFlags & SURF_WARP)
		return;		/* non-lit texture */

	if (config.extrasamples)
		numsamples = MAX_SAMPLES;
	else
		numsamples = 1;

	l = malloc(numsamples * sizeof(*l));
	lightscale = 1.0 / numsamples;

	for (i = 0; i < numsamples; i++) {
		memset(&l[i], 0, sizeof(l[i]));
		l[i].surfnum = facenum;
		l[i].face = f;
		/* rotate plane */
		VectorCopy(curTile->planes[f->planenum].normal, l[i].facenormal);
		l[i].facedist = curTile->planes[f->planenum].dist;
		if (f->side) {
			VectorSubtract(vec3_origin, l[i].facenormal, l[i].facenormal);
			l[i].facedist = -l[i].facedist;
		}

		/* get the origin offset for rotating bmodels */
		VectorCopy(face_offset[facenum], l[i].modelorg);

		CalcFaceVectors(&l[i]);
		CalcFaceExtents(&l[i]);
		CalcPoints(&l[i], sampleofs[i][0], sampleofs[i][1]);
	}

	tablesize = l[0].numsurfpt * sizeof(vec3_t);
	styletable = malloc(tablesize);
	memset(styletable, 0, tablesize);

	fl = &facelight[config.compile_for_day][facenum];
	fl->numsamples = l[0].numsurfpt;
	fl->origins = malloc(tablesize);
	memcpy(fl->origins, l[0].surfpt, tablesize);

	/* add sun light */
	if (config.compile_for_day) {
		if (!config.day_sun_intensity)
			return;
		sun_intensity = config.day_sun_intensity;
		VectorCopy(config.day_sun_dir, sun_dir);
		VectorCopy(config.day_sun_color, sun_color);
	} else {
		if (!config.night_sun_intensity)
			return;
		sun_intensity = config.night_sun_intensity;
		VectorCopy(config.night_sun_dir, sun_dir);
		VectorCopy(config.night_sun_color, sun_color);
	}

	/* get the light samples */
	for (i = 0; i < l[0].numsurfpt; i++) {
		for (j = 0; j < numsamples; j++) {
			/* calculate interpolated normal for phong shading */
			if (curTile->texinfo[l[0].face->texinfo].surfaceFlags & SURF_PHONG)
				SampleNormal(&l[0], l[j].surfpt[i], normal);
			else /* or just use the plane normal */
				VectorCopy(l[0].facenormal, normal);

			GatherSampleLight(l[j].surfpt[i], normal, l[0].center,
				styletable, i * 3, tablesize, lightscale, sun_intensity,
				sun_color, sun_dir);
		}

		/* contribute the sample to one or more patches */
		if (config.numbounce > 0)
			AddSampleToPatch(l[0].surfpt[i], styletable + i * 3, facenum);
	}

	/* average up the direct light on each patch for radiosity */
	for (patch = face_patches[facenum]; patch; patch = patch->next)
		if (patch->samples)
			VectorScale(patch->samplelight, 1.0 / patch->samples, patch->samplelight);

	fl->samples = styletable;

	/* the light from DIRECT_LIGHTS is sent out, but the
	 * texture itself should still be full bright */
	if (face_patches[facenum] &&
		(face_patches[facenum]->baselight[0] >= DIRECT_LIGHT ||
		face_patches[facenum]->baselight[1] >= DIRECT_LIGHT ||
		face_patches[facenum]->baselight[2] >= DIRECT_LIGHT)) {
		float *spot = fl->samples;
		for (i = 0; i < l[0].numsurfpt; i++, spot += 3)
			VectorAdd(spot, face_patches[facenum]->baselight, spot);
	}

	free(l);
}


/**
 * @brief Add the indirect lighting on top of the direct
 * lighting and save into final map format
 * @sa BuildFacelights
 */
void FinalLightFace (unsigned int facenum)
{
	dBspFace_t *f;
	int i, j, k, pfacenum;
	patch_t *patch;
	triangulation_t *trian = NULL;
	facelight_t	*fl;
	float max, newmax;
	byte *dest;
	vec3_t facemins, facemaxs;

	f = &curTile->faces[facenum];
	fl = &facelight[config.compile_for_day][facenum];

	/* none-lit texture */
	if (curTile->texinfo[f->texinfo].surfaceFlags & SURF_WARP)
		return;

	ThreadLock();

	f->lightofs[config.compile_for_day] = curTile->lightdatasize[config.compile_for_day];
	curTile->lightdatasize[config.compile_for_day] += fl->numsamples * 3;

	if (curTile->lightdatasize[config.compile_for_day] > MAX_MAP_LIGHTING)
		Sys_Error("MAX_MAP_LIGHTING (%i exceeded %i) - try to reduce the brush size",
			curTile->lightdatasize[config.compile_for_day], MAX_MAP_LIGHTING);

	ThreadUnlock();

	/* set up the triangulation */
	if (config.numbounce > 0) {
		ClearBounds(facemins, facemaxs);
		for (i = 0; i < f->numedges; i++) {
			const int ednum = curTile->surfedges[f->firstedge + i];
			if (ednum >= 0)
				AddPointToBounds(curTile->vertexes[curTile->edges[ednum].v[0]].point, facemins, facemaxs);
			else
				AddPointToBounds(curTile->vertexes[curTile->edges[-ednum].v[1]].point, facemins, facemaxs);
		}

		trian = AllocTriangulation(&curTile->planes[f->planenum]);

		/* for all faces on the plane, add the nearby patches
		 * to the triangulation */
		for (pfacenum = planelinks[f->side][f->planenum]; pfacenum; pfacenum = facelinks[pfacenum]) {
			for (patch = face_patches[pfacenum]; patch; patch = patch->next) {
				for (i = 0; i < 3; i++) {
					if (facemins[i] - patch->origin[i] > config.subdiv * 2)
						break;
					if (patch->origin[i] - facemaxs[i] > config.subdiv * 2)
						break;
				}
				if (i != 3)
					continue;	/* not needed for this face */
				AddPointToTriangulation(patch, trian);
			}
		}
		for (i = 0; i < trian->numpoints; i++)
			memset(trian->edgematrix[i], 0, trian->numpoints * sizeof(trian->edgematrix[0][0]));
		TriangulatePoints(trian);
	}

	/* sample the triangulation */

	dest = &curTile->lightdata[config.compile_for_day][f->lightofs[config.compile_for_day]];

	for (j = 0; j < fl->numsamples; j++) {
		vec3_t lb;

		VectorCopy((fl->samples + j * 3), lb);
		if (config.numbounce > 0) {
			vec3_t add;
			SampleTriangulation(fl->origins + j * 3, trian, add);
			VectorAdd(lb, add, lb);
		}
		/* add an ambient term if desired */
		if (config.compile_for_day) {
			lb[0] += config.day_ambient_red;
			lb[1] += config.day_ambient_green;
			lb[2] += config.day_ambient_blue;
		} else {
			lb[0] += config.night_ambient_red;
			lb[1] += config.night_ambient_green;
			lb[2] += config.night_ambient_blue;
		}

		VectorScale(lb, config.lightscale, lb);

		/* we need to clamp without allowing hue to change */
		for (k = 0; k < 3; k++)
			if (lb[k] < 1)
				lb[k] = 1;
		max = lb[0];
		if (lb[1] > max)
			max = lb[1];
		if (lb[2] > max)
			max = lb[2];
		newmax = max;
		if (newmax < 0)
			newmax = 0;		/* roundoff problems */
		if (newmax > config.maxlight)
			newmax = config.maxlight;
		for (k = 0; k < 3; k++) {
			*dest++ = lb[k] * newmax / max;
		}
	}

	if (config.numbounce > 0)
		FreeTriangulation(trian);
}
