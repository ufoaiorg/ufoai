/**
 * @file polylib.c
 * @brief
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

#include "cmdlib.h"
#include "mathlib.h"
#include "polylib.h"


extern int numthreads;

/* counters are only bumped when running single threaded, */
/* because they are an awefull coherence problem */
int	c_active_windings;
int	c_peak_windings;
int	c_winding_allocs;
int	c_winding_points;

#define	BOGUS_RANGE	8192

/**
 * @brief
 * @sa FreeWinding
 */
winding_t *AllocWinding (int points)
{
	winding_t *w;
	int s;

	if (numthreads == 1) {
		c_winding_allocs++;
		c_winding_points += points;
		c_active_windings++;
		if (c_active_windings > c_peak_windings)
			c_peak_windings = c_active_windings;
	}
	s = sizeof(vec_t)*3*points + sizeof(int);
	w = malloc (s);
	memset (w, 0, s);
	return w;
}

/**
 * @brief
 * @sa AllocWinding
 */
void FreeWinding (winding_t *w)
{
	if (*(unsigned *)w == 0xdeaddead)
		Error ("FreeWinding: freed a freed winding");
	*(unsigned *)w = 0xdeaddead;

	if (numthreads == 1)
		c_active_windings--;
	free (w);
}

static int c_removed;

/**
 * @brief
 */
void RemoveColinearPoints (winding_t *w)
{
	int		i, j, k;
	vec3_t	v1, v2;
	int		nump;
	vec3_t	p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for (i=0 ; i<w->numpoints ; i++) {
		j = (i+1)%w->numpoints;
		k = (i+w->numpoints-1)%w->numpoints;
		VectorSubtract (w->p[j], w->p[i], v1);
		VectorSubtract (w->p[i], w->p[k], v2);
		VectorNormalize(v1,v1);
		VectorNormalize(v2,v2);
		if (DotProduct(v1, v2) < 0.999) {
			VectorCopy (w->p[i], p[nump]);
			nump++;
		}
	}

	if (nump == w->numpoints)
		return;

	if (numthreads == 1)
		c_removed += w->numpoints - nump;
	w->numpoints = nump;
	memcpy (w->p, p, nump*sizeof(p[0]));
}

/**
 * @brief
 */
void WindingPlane (winding_t *w, vec3_t normal, vec_t *dist)
{
	vec3_t	v1, v2;

	VectorSubtract (w->p[1], w->p[0], v1);
	VectorSubtract (w->p[2], w->p[0], v2);
	CrossProduct (v2, v1, normal);
	VectorNormalize (normal, normal);
	*dist = DotProduct (w->p[0], normal);
}

/**
 * @brief
 */
vec_t WindingArea (winding_t *w)
{
	int		i;
	vec3_t	d1, d2, cross;
	vec_t	total;

	total = 0;
	for (i=2 ; i<w->numpoints ; i++) {
		VectorSubtract (w->p[i-1], w->p[0], d1);
		VectorSubtract (w->p[i], w->p[0], d2);
		CrossProduct (d1, d2, cross);
		total += 0.5 * VectorLength ( cross );
	}
	return total;
}

/**
 * @brief
 */
void WindingBounds (winding_t *w, vec3_t mins, vec3_t maxs)
{
	vec_t	v;
	int		i,j;

	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;

	for (i=0 ; i<w->numpoints ; i++) {
		for (j=0 ; j<3 ; j++) {
			v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
}

/**
 * @brief
 */
void WindingCenter (winding_t *w, vec3_t center)
{
	int		i;
	float	scale;

	VectorCopy (vec3_origin, center);
	for (i=0 ; i<w->numpoints ; i++)
		VectorAdd (w->p[i], center, center);

	scale = 1.0/w->numpoints;
	VectorScale (center, scale, center);
}

/**
 * @brief
 */
winding_t *BaseWindingForPlane (vec3_t normal, vec_t dist)
{
	int		i, x;
	vec_t	max, v;
	vec3_t	org, vright, vup;
	winding_t	*w;

	/* find the major axis */
	max = -BOGUS_RANGE;
	x = -1;
	for (i=0 ; i<3; i++) {
		v = fabs(normal[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x==-1)
		Error ("BaseWindingForPlane: no axis found");

	VectorCopy (vec3_origin, vup);
	switch (x) {
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = DotProduct (vup, normal);
	VectorMA (vup, -v, normal, vup);
	VectorNormalize (vup, vup);

	VectorScale (normal, dist, org);

	CrossProduct (vup, normal, vright);

	VectorScale (vup, 8192, vup);
	VectorScale (vright, 8192, vright);

	/* project a really big	axis aligned box onto the plane */
	w = AllocWinding (4);

	VectorSubtract (org, vright, w->p[0]);
	VectorAdd (w->p[0], vup, w->p[0]);

	VectorAdd (org, vright, w->p[1]);
	VectorAdd (w->p[1], vup, w->p[1]);

	VectorAdd (org, vright, w->p[2]);
	VectorSubtract (w->p[2], vup, w->p[2]);

	VectorSubtract (org, vright, w->p[3]);
	VectorSubtract (w->p[3], vup, w->p[3]);

	w->numpoints = 4;

	return w;
}

/**
 * @brief
 */
winding_t *CopyWinding (winding_t *w)
{
	ptrdiff_t	size;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	size = (ptrdiff_t)((winding_t *)0)->p[w->numpoints];
	memcpy (c, w, size);
	return c;
}

/**
 * @brief
 */
winding_t *ReverseWinding (winding_t *w)
{
	int			i;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	for (i=0 ; i<w->numpoints ; i++) {
		VectorCopy (w->p[w->numpoints-1-i], c->p[i]);
	}
	c->numpoints = w->numpoints;
	return c;
}

/**
 * @brief
 */
void ClipWindingEpsilon (winding_t *in, vec3_t normal, vec_t dist,
		vec_t epsilon, winding_t **front, winding_t **back)
{
	vec_t dists[MAX_POINTS_ON_WINDING+4];
	int sides[MAX_POINTS_ON_WINDING+4];
	int counts[3];
	static vec_t dot;		/* VC 4.2 optimizer bug if not static */
	int i, j;
	vec_t *p1, *p2;
	vec3_t mid;
	winding_t *f, *b;
	int maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	/* determine sides for each point */
	for (i=0 ; i<in->numpoints ; i++) {
		dot = DotProduct (in->p[i], normal);
		dot -= dist;
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

	*front = *back = NULL;

	if (!counts[0]) {
		*back = CopyWinding (in);
		return;
	}
	if (!counts[1]) {
		*front = CopyWinding (in);
		return;
	}

	maxpts = in->numpoints+4;	/* cant use counts[0]+2 because */
								/* of fp grouping errors */

	*front = f = AllocWinding (maxpts);
	*back = b = AllocWinding (maxpts);

	for (i=0 ; i<in->numpoints ; i++) {
		p1 = in->p[i];

		if (sides[i] == SIDE_ON) {
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
			VectorCopy (p1, b->p[b->numpoints]);
			b->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
		}
		if (sides[i] == SIDE_BACK) {
			VectorCopy (p1, b->p[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		/* generate a split point */
		p2 = in->p[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++) {	/* avoid round off error when possible */
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, f->p[f->numpoints]);
		f->numpoints++;
		VectorCopy (mid, b->p[b->numpoints]);
		b->numpoints++;
	}

	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Error ("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING || b->numpoints > MAX_POINTS_ON_WINDING)
		Error ("ClipWinding: MAX_POINTS_ON_WINDING");
}


/**
 * @brief
 */
void ChopWindingInPlace (winding_t **inout, vec3_t normal, vec_t dist, vec_t epsilon)
{
	winding_t *in;
	vec_t dists[MAX_POINTS_ON_WINDING+4];
	int sides[MAX_POINTS_ON_WINDING+4];
	int counts[3];
	static vec_t dot;		/* VC 4.2 optimizer bug if not static */
	int i, j;
	vec_t *p1, *p2;
	vec3_t mid;
	winding_t *f;
	int maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	if ( ! in ) return;

	/* determine sides for each point */
	for (i=0 ; i<in->numpoints ; i++) {
		dot = DotProduct (in->p[i], normal);
		dot -= dist;
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
		FreeWinding (in);
		*inout = NULL;
		return;
	}
	if (!counts[1])
		return;		/* inout stays the same */

	maxpts = in->numpoints+4;	/* cant use counts[0]+2 because */
								/* of fp grouping errors */

	f = AllocWinding (maxpts);

	for (i=0 ; i<in->numpoints ; i++) {
		p1 = in->p[i];

		if (sides[i] == SIDE_ON) {
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		/* generate a split point */
		p2 = in->p[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++) {	/* avoid round off error when possible */
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, f->p[f->numpoints]);
		f->numpoints++;
	}

	if (f->numpoints > maxpts)
		Error ("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Error ("ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWinding (in);
	*inout = f;
}


/**
 * @brief
 * @return the fragment of in that is on the front side of the cliping plane.
 * @note The original is freed.
 */
winding_t *ChopWinding (winding_t *in, vec3_t normal, vec_t dist)
{
	winding_t	*f, *b;

	ClipWindingEpsilon (in, normal, dist, ON_EPSILON, &f, &b);
	FreeWinding (in);
	if (b)
		FreeWinding (b);
	return f;
}


/**
 * @brief
 */
void CheckWinding (winding_t *w)
{
	int		i, j;
	vec_t	*p1, *p2;
	vec_t	d, edgedist;
	vec3_t	dir, edgenormal, facenormal;
	vec_t	area;
	vec_t	facedist;

	if (w->numpoints < 3)
		Error ("CheckWinding: %i points",w->numpoints);

	area = WindingArea(w);
	if (area < 1)
		Error ("CheckWinding: %f area", area);

	WindingPlane (w, facenormal, &facedist);

	for (i=0 ; i<w->numpoints ; i++) {
		p1 = w->p[i];

		for (j=0 ; j<3 ; j++)
			if (p1[j] > BOGUS_RANGE || p1[j] < -BOGUS_RANGE)
				Error ("CheckFace: BUGUS_RANGE: %f",p1[j]);

		j = i+1 == w->numpoints ? 0 : i+1;

		/* check the point is on the face plane */
		d = DotProduct (p1, facenormal) - facedist;
		if (d < -ON_EPSILON || d > ON_EPSILON)
			Error ("CheckWinding: point off plane");

		/* check the edge isnt degenerate */
		p2 = w->p[j];
		VectorSubtract (p2, p1, dir);

		if (VectorLength (dir) < ON_EPSILON)
			Error ("CheckWinding: degenerate edge");

		CrossProduct (facenormal, dir, edgenormal);
		VectorNormalize (edgenormal, edgenormal);
		edgedist = DotProduct (p1, edgenormal);
		edgedist += ON_EPSILON;

		/* all other points must be on front side */
		for (j=0 ; j<w->numpoints ; j++) {
			if (j == i)
				continue;
			d = DotProduct (w->p[j], edgenormal);
			if (d > edgedist)
				Error ("CheckWinding: non-convex");
		}
	}
}


/**
 * @brief
 */
int WindingOnPlaneSide (winding_t *w, vec3_t normal, vec_t dist)
{
	qboolean front, back;
	int i;
	vec_t d;

	front = qfalse;
	back = qfalse;
	for (i=0 ; i<w->numpoints ; i++) {
		d = DotProduct (w->p[i], normal) - dist;
		if (d < -ON_EPSILON) {
			if (front)
				return SIDE_CROSS;
			back = qtrue;
			continue;
		}
		if (d > ON_EPSILON) {
			if (back)
				return SIDE_CROSS;
			front = qtrue;
			continue;
		}
	}

	if (back)
		return SIDE_BACK;
	if (front)
		return SIDE_FRONT;
	return SIDE_ON;
}

