/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <float.h>

#include "mathlib.h"

static const aabb_t g_aabb_null = {
	{ 0, 0, 0, },
	{ -FLT_MAX, -FLT_MAX, -FLT_MAX, },
};

void aabb_extend_by_point (aabb_t *aabb, const vec3_t point)
{
	int i;
	vec_t min, max, displacement;
	for (i = 0; i < 3; i++) {
		displacement = point[i] - aabb->origin[i];
		if (fabs(displacement) > aabb->extents[i]) {
			if (aabb->extents[i] < 0) { /* degenerate */
				min = max = point[i];
			} else if (displacement > 0) {
				min = aabb->origin[i] - aabb->extents[i];
				max = aabb->origin[i] + displacement;
			} else {
				max = aabb->origin[i] + aabb->extents[i];
				min = aabb->origin[i] + displacement;
			}
			aabb->origin[i] = (min + max) * 0.5f;
			aabb->extents[i] = max - aabb->origin[i];
		}
	}
}

void aabb_extend_by_aabb (aabb_t *aabb, const aabb_t *aabb_src)
{
	int i;
	vec_t min, max, displacement, difference;
	for (i = 0; i < 3; i++) {
		displacement = aabb_src->origin[i] - aabb->origin[i];
		difference = aabb_src->extents[i] - aabb->extents[i];
		if (aabb->extents[i] < 0
		        || difference >= fabs(displacement)) {
			/* 2nd contains 1st */
			aabb->extents[i] = aabb_src->extents[i];
			aabb->origin[i] = aabb_src->origin[i];
		} else if (aabb_src->extents[i] < 0
		           || -difference >= fabs(displacement)) {
			/* 1st contains 2nd */
			continue;
		} else {
			/* not contained */
			if (displacement > 0) {
				min = aabb->origin[i] - aabb->extents[i];
				max = aabb_src->origin[i] + aabb_src->extents[i];
			} else {
				min = aabb_src->origin[i] - aabb_src->extents[i];
				max = aabb->origin[i] + aabb->extents[i];
			}
			aabb->origin[i] = (min + max) * 0.5f;
			aabb->extents[i] = max - aabb->origin[i];
		}
	}
}

void aabb_corners (const aabb_t* aabb, vec3_t corners[8])
{
	vec3_t min, max;
	VectorSubtract(aabb->origin, aabb->extents, min);
	VectorAdd(aabb->origin, aabb->extents, max);
	VectorSet(corners[0], min[0], max[1], max[2]);
	VectorSet(corners[1], max[0], max[1], max[2]);
	VectorSet(corners[2], max[0], min[1], max[2]);
	VectorSet(corners[3], min[0], min[1], max[2]);
	VectorSet(corners[4], min[0], max[1], min[2]);
	VectorSet(corners[5], max[0], max[1], min[2]);
	VectorSet(corners[6], max[0], min[1], min[2]);
	VectorSet(corners[7], min[0], min[1], min[2]);
}
