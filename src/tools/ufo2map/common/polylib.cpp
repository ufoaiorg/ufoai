/**
 * @file polylib.c
 * @brief
 * @note Winding = Polyon representation of brushes
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

#include "polylib.h"
#include "shared.h"


#define	BOGUS_RANGE	8192

/**
 * @brief Allocate a new winding (polygon)
 * @param[in] points Amount of points for this winding
 * @sa FreeWinding
 */
winding_t *AllocWinding (int points)
{
	return (winding_t *)Mem_Alloc(sizeof(vec3_t) * points + sizeof(int));
}

/**
 * @sa AllocWinding
 */
void FreeWinding (winding_t *w)
{
	if (*(unsigned *)w == 0xdeaddead)
		Sys_Error("FreeWinding: freed a freed winding");
	*(unsigned *)w = 0xdeaddead;

	Mem_Free(w);
}

void RemoveColinearPoints (winding_t *w)
{
	int i, nump = 0;
	vec3_t v1, v2;
	vec3_t p[MAX_POINTS_ON_WINDING];

	for (i = 0; i < w->numpoints; i++) {
		const int j = (i + 1) % w->numpoints;
		const int k = (i + w->numpoints - 1) % w->numpoints;
		VectorSubtract(w->p[j], w->p[i], v1);
		VectorSubtract(w->p[i], w->p[k], v2);
		VectorNormalize(v1);
		VectorNormalize(v2);
		if (DotProduct(v1, v2) < 0.999) {
			VectorCopy(w->p[i], p[nump]);
			nump++;
		}
	}

	if (nump == w->numpoints)
		return;

	w->numpoints = nump;
	memcpy(w->p, p, nump * sizeof(p[0]));
}

vec_t WindingArea (const winding_t *w)
{
	int i;
	vec3_t d1, d2, cross;
	vec_t total;

	total = 0;
	for (i = 2; i < w->numpoints; i++) {
		VectorSubtract(w->p[i - 1], w->p[0], d1);
		VectorSubtract(w->p[i], w->p[0], d2);
		CrossProduct(d1, d2, cross);
		total += VectorLength(cross);
	}
	return total * 0.5f;
}

void WindingBounds (const winding_t *w, vec3_t mins, vec3_t maxs)
{
	int i, j;

	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;

	for (i = 0; i < w->numpoints; i++) {
		for (j = 0; j < 3; j++) {
			const vec_t v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
}

void WindingCenter (const winding_t *w, vec3_t center)
{
	int i;
	vec_t scale;

	VectorCopy(vec3_origin, center);
	for (i = 0; i < w->numpoints; i++)
		VectorAdd(w->p[i], center, center);

	scale = 1.0 / w->numpoints;
	VectorScale(center, scale, center);
}

winding_t *BaseWindingForPlane (const vec3_t normal, const vec_t dist)
{
	int i, x;
	vec_t max, v;
	vec3_t org, vright, vup;
	winding_t *w;

	/* find the major axis */
	max = -BOGUS_RANGE;
	x = -1;
	for (i = 0; i < 3; i++) {
		v = fabs(normal[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x == -1)
		Sys_Error("BaseWindingForPlane: no axis found");

	VectorCopy(vec3_origin, vup);
	/* axis */
	switch (x) {
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = DotProduct(vup, normal);
	VectorMA(vup, -v, normal, vup);
	VectorNormalize(vup);

	VectorScale(normal, dist, org);

	CrossProduct(vup, normal, vright);

	VectorScale(vup, 8192, vup);
	VectorScale(vright, 8192, vright);

	/* project a really big axis aligned box onto the plane */
	w = AllocWinding(4);
	if (!w)
		return NULL;

	VectorSubtract(org, vright, w->p[0]);
	VectorAdd(w->p[0], vup, w->p[0]);

	VectorAdd(org, vright, w->p[1]);
	VectorAdd(w->p[1], vup, w->p[1]);

	VectorAdd(org, vright, w->p[2]);
	VectorSubtract(w->p[2], vup, w->p[2]);

	VectorSubtract(org, vright, w->p[3]);
	VectorSubtract(w->p[3], vup, w->p[3]);

	w->numpoints = 4;

	return w;
}

/**
 * @brief Copy a winding with all its points allocated
 * @param[in] w The winding to copy
 * @returns the new winding
 */
winding_t *CopyWinding (const winding_t *w)
{
	winding_t *c = AllocWinding(w->numpoints);
	const ptrdiff_t size = (ptrdiff_t)((winding_t *)0)->p[w->numpoints];
	memcpy(c, w, size);
	return c;
}

winding_t *ReverseWinding (const winding_t *w)
{
	int i;
	winding_t *c = AllocWinding(w->numpoints);

	for (i = 0; i < w->numpoints; i++) {
		VectorCopy(w->p[w->numpoints - 1 - i], c->p[i]);
	}
	c->numpoints = w->numpoints;
	return c;
}

void ClipWindingEpsilon (const winding_t *in, const vec3_t normal, const vec_t dist,
		const vec_t epsilon, winding_t **front, winding_t **back)
{
	vec_t dists[MAX_POINTS_ON_WINDING + 4];
	int sides[MAX_POINTS_ON_WINDING + 4];
	int counts[3];
	int i, j;
	winding_t *f, *b;
	int maxpts;

	VectorClear(counts);

	/* determine sides for each point */
	for (i = 0; i < in->numpoints; i++) {
		const vec_t dot = DotProduct(in->p[i], normal) - dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0]) {
		*back = CopyWinding(in);
		*front = NULL;
		return;
	}
	if (!counts[1]) {
		*front = CopyWinding(in);
		*back = NULL;
		return;
	}

	/* can't use counts[0] + 2 because of floating point grouping errors */
	maxpts = in->numpoints + 4;

	*front = f = AllocWinding(maxpts);
	*back = b = AllocWinding(maxpts);

	for (i = 0; i < in->numpoints; i++) {
		const vec_t *p1 = in->p[i];
		const vec_t *p2;
		vec_t dot;
		vec3_t mid;

		if (sides[i] == SIDE_ON) {
			VectorCopy(p1, f->p[f->numpoints]);
			f->numpoints++;
			VectorCopy(p1, b->p[b->numpoints]);
			b->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, f->p[f->numpoints]);
			f->numpoints++;
		}
		if (sides[i] == SIDE_BACK) {
			VectorCopy(p1, b->p[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		/* generate a split point */
		p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		/* avoid round off error when possible */
		for (j = 0; j < 3; j++) {
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy(mid, f->p[f->numpoints]);
		f->numpoints++;
		VectorCopy(mid, b->p[b->numpoints]);
		b->numpoints++;
	}

	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Sys_Error("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING || b->numpoints > MAX_POINTS_ON_WINDING)
		Sys_Error("ClipWinding: MAX_POINTS_ON_WINDING");
}

void ChopWindingInPlace (winding_t **inout, const vec3_t normal, const vec_t dist, const vec_t epsilon)
{
	winding_t *in;
	/** @todo Why + 4? */
	vec_t dists[MAX_POINTS_ON_WINDING + 4];
	int sides[MAX_POINTS_ON_WINDING + 4];
	int counts[3];
	int i, j;
	vec3_t mid;
	winding_t *f;
	int maxpts;

	in = *inout;
	VectorClear(counts);

	/* determine sides for each point */
	for (i = 0; i < in->numpoints; i++) {
		const vec_t dot = DotProduct(in->p[i], normal) - dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0]) {
		FreeWinding(in);
		*inout = NULL;
		return;
	}
	if (!counts[1])
		return;		/* inout stays the same */

	/* cant use counts[0] + 2 because of fp grouping errors */
	maxpts = in->numpoints + 4;

	f = AllocWinding(maxpts);

	for (i = 0; i < in->numpoints; i++) {
		const vec_t *p1 = in->p[i];
		const vec_t *p2;
		vec_t dot;

		if (sides[i] == SIDE_ON) {
			VectorCopy(p1, f->p[f->numpoints]);
			f->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, f->p[f->numpoints]);
			f->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		/* generate a split point */
		p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		/* avoid round off error when possible */
		for (j = 0; j < 3; j++) {
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy(mid, f->p[f->numpoints]);
		f->numpoints++;
	}

	if (f->numpoints > maxpts)
		Sys_Error("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Sys_Error("ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWinding(in);
	*inout = f;
}


/**
 * @return the fragment of in that is on the front side of the cliping plane.
 * @note The original is freed.
 */
winding_t *ChopWinding (winding_t *in, vec3_t normal, vec_t dist)
{
	winding_t *f, *b;

	ClipWindingEpsilon(in, normal, dist, ON_EPSILON, &f, &b);
	FreeWinding(in);
	if (b)
		FreeWinding(b);
	return f;
}

#define EDGE_LENGTH 0.2
/**
 * @brief Returns true if the winding would be crunched out of existance by the
 * vertex snapping.
 */
qboolean WindingIsTiny (winding_t *w)
{
	int i, edges;
	vec_t len;
	vec3_t delta;

	edges = 0;
	for (i = 0; i < w->numpoints; i++) {
		const int j = ((i == w->numpoints - 1) ? 0 : i) + 1;
		VectorSubtract(w->p[j], w->p[i], delta);
		len = VectorLength(delta);
		if (len > EDGE_LENGTH) {
			if (++edges == 3)
				return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief Returns true if the winding still has one of the points from
 * basewinding for plane
 */
qboolean WindingIsHuge (const winding_t *w)
{
	int i, j;

	for (i = 0; i < w->numpoints; i++) {
		for (j = 0; j < 3; j++)
			if (w->p[i][j] < -MAX_WORLD_WIDTH || w->p[i][j] > MAX_WORLD_WIDTH)
				return qtrue;
	}
	return qfalse;
}

#define SNAP_EPSILON	0.01

/**
 * @brief welds two vec3_t's into a third, taking into account nearest-to-integer
 * instead of averaging
 */
static void SnapWeldVector (const vec3_t a, const vec3_t b, vec3_t out)
{
	int i;
	vec_t outi;

	/* dummy check */
	if (a == NULL || b == NULL || out == NULL)
		return;

	/* do each element */
	for (i = 0; i < 3; i++) {
		/* round to integer */
		const double ai = rint(a[i]);
		const double bi = rint(a[i]);

		/* prefer exact integer */
		if (ai == a[i])
			out[i] = a[i];
		else if (bi == b[i])
			out[i] = b[i];

		/* use nearest */
		else if (fabs(ai - a[i]) < fabs(bi < b[i]))
			out[i] = a[i];
		else
			out[i] = b[i];

		/* snap */
		outi = rint(out[i]);
		if (fabs(outi - out[i]) <= SNAP_EPSILON)
			out[i] = outi;
	}
}

/**
 * @brief removes degenerate edges from a winding
 * @returns qtrue if the winding is valid
 */
qboolean FixWinding (winding_t *w)
{
	qboolean valid;
	int i, k;

	/* dummy check */
	if (!w)
		return qfalse;

	valid = qtrue;

	/* check all verts */
	for (i = 0; i < w->numpoints; i++) {
		/* get second point index */
		const int j = (i + 1) % w->numpoints;
		vec3_t vec;
		float dist;

		/* don't remove points if winding is a triangle */
		if (w->numpoints == 3)
			return valid;

		/* degenerate edge? */
		VectorSubtract(w->p[i], w->p[j], vec);
		dist = VectorLength(vec);
		if (dist < ON_EPSILON) {
			valid = qfalse;

			/* create an average point (ydnar 2002-01-26: using nearest-integer weld preference) */
			SnapWeldVector(w->p[i], w->p[j], vec);
			VectorCopy(vec, w->p[i]);

			/* move the remaining verts */
			for (k = i + 2; k < w->numpoints; k++)
				VectorCopy(w->p[k], w->p[k - 1]);

			w->numpoints--;
		}
	}

	/* one last check and return */
	if (w->numpoints < 3)
		valid = qfalse;
	return valid;
}
