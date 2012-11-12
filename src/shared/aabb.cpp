/**
 * @file
 * @brief AABB is the acronym for 'axis aligned bounding box'.
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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
/**
 * @brief If the point is outside the box, expand the box to accommodate it.
 */
void AABB::add (const vec3_t point)
{
	int i;
	vec_t val;

	for (i = 0; i < 3; i++) {
		val = point[i];
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
	int i;

	for (i = 0; i < 3; i++) {
		if (other.mins[i] < mins[i])
			mins[i] = other.mins[i];
		if (other.maxs[i] > maxs[i])
			maxs[i] = other.maxs[i];
	}
}
