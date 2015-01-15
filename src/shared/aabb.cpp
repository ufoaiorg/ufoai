/**
 * @file
 * @brief AABB is the acronym for 'axis aligned bounding box'.
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "aabb.h"
#include "mathlib.h"

AABB::AABB ()
{
	VectorCopy(vec3_origin, mins);
	VectorCopy(vec3_origin, maxs);
}
AABB::AABB (const vec3_t mini, const vec3_t maxi)
{
	VectorCopy(mini, mins);
	VectorCopy(maxi, maxs);
}
AABB::AABB (const vec_t minX, const vec_t minY, const vec_t minZ, const vec_t maxX, const vec_t maxY, const vec_t maxZ)
{
	mins[0] = minX;
	mins[1] = minY;
	mins[2] = minZ;
	maxs[0] = maxX;
	maxs[1] = maxY;
	maxs[2] = maxZ;
}
AABB::AABB (const Line& line)
{
	VectorSet(mins, std::min(line.start[0], line.stop[0]), std::min(line.start[1], line.stop[1]), std::min(line.start[2], line.stop[2]));
	VectorSet(maxs, std::max(line.start[0], line.stop[0]), std::max(line.start[1], line.stop[1]), std::max(line.start[2], line.stop[2]));
}

/**
 * @brief If the point is outside the box, expand the box to accommodate it.
 */
void AABB::add (const vec3_t point)
{
	for (int i = 0; i < 3; i++) {
		const vec_t val = point[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}
/**
 * @brief If the given box is outside our box, expand our box to accommodate it.
 * @note  We only have to check min vs. min and max vs. max here. So adding a box is far more
 * efficient than adding it's min and max as points.
 */
void AABB::add (const AABB& other)
{
	for (int i = 0; i < 3; i++) {
		if (other.mins[i] < mins[i])
			mins[i] = other.mins[i];
		if (other.maxs[i] > maxs[i])
			maxs[i] = other.maxs[i];
	}
}

/**
 * @brief Rotates AABB around given origin point; note that it will expand the box unless all angles are multiples of 90 degrees
 * @note Not fully verified so far
 */
void AABB::rotateAround (const vec3_t origin, const vec3_t angles) {
	/* reject non-rotations */
	if (VectorEmpty(angles))
		return;

	/* construct box-centered coordinates (center and corners) */
	vec3_t center, halfDiagonal;

	VectorInterpolation(mins, maxs, 0.5f, center);
	VectorSubtract(maxs, center, halfDiagonal);

	/* offset coordinate frame to rotation origin */
	VectorSubtract(center, origin, center);

	/* rotate center by given angles */
	vec3_t m[3];
	VectorCreateRotationMatrix(angles, m);

	vec3_t newCenter;
	VectorRotate(m, center, newCenter);

	/* short-circuit calculation of the rotated box half-extents */
	/* shortcut is: instead of calculating all 8 AABB corners, use the symmetry by rotating box around it's center. */
	VectorAbs(m[0]);
	VectorAbs(m[1]);
	VectorAbs(m[2]);

	vec3_t newHalfDiagonal;
	VectorRotate(m, halfDiagonal, newHalfDiagonal);

	/* de-offset coordinate frame from rotation origin */
	VectorAdd(newCenter, origin, newCenter);

	/* finally, combine results into new AABB */
	VectorAdd(newCenter, newHalfDiagonal, maxs);
	VectorSubtract(newCenter, newHalfDiagonal, mins);
}

const AABB AABB::EMPTY;
