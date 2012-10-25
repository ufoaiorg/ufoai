/**
 * @file
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

#pragma once

#include "ufotypes.h"
#include "vector.h"

/**
 * @brief Axis-aligned bounding box
 */
class AABB {
public:
	AABB ();
	AABB (const vec3_t mini, const vec3_t maxi);

	inline bool isZero () const;
	void add (const vec3_t point);

	/**
	 * @brief Checks if the aabb touches or intersects with the given aabb
	 * @param[in] other The other aabb
	 */
	inline bool doesIntersect (const AABB& other) const;

	/**
	 * @brief Calculates the center of the bounding box
	 * @param[out] center The target center vector
	 */
	inline void getCenter (vec3_t center) const;

	/**
	 * @brief Sets mins and maxs to their starting points before using addPoint
	 */
	inline void clearBounds ();


	vec3_t mins;
	vec3_t maxs;
};

inline bool AABB::isZero () const
{
	return VectorEmpty(mins) && VectorEmpty(maxs);
}

inline bool AABB::doesIntersect (const AABB& other) const
{
	return !(mins[0] > other.maxs[0] || mins[1] > other.maxs[1] || mins[2] > other.maxs[2] || maxs[0] < other.mins[0]
			|| maxs[1] < other.mins[1] || maxs[2] < other.mins[2]);
}

inline void AABB::clearBounds ()
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

inline void AABB::getCenter (vec3_t center) const
{
	VectorAdd(mins, maxs, center);
	VectorScale(center, 0.5, center);
}
