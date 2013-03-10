/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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
#include "line.h"
#include "defines.h"	/* for MAX_WORLD_WIDTH */
#include <algorithm>

/**
 * @brief Axis-aligned bounding box
 */
const float MWW = MAX_WORLD_WIDTH;

class AABB {
public:
	AABB ();
	AABB (const vec3_t mini, const vec3_t maxi);
	AABB (const vec_t minX, const vec_t minY, const vec_t minZ, const vec_t maxX, const vec_t maxY, const vec_t maxZ);
	AABB (const Line &line);

	inline bool isZero () const;
	void add (const vec3_t point);
	void add (const AABB& other);

	/**
	 * @brief Copies the values from the given aabb
	 * @param[in] other The other aabb
	 */
	inline void set (const AABB& other);
	inline void set (const vec3_t mini, const vec3_t maxi);

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

	/** @brief clip the box to the maximum boundaries */
	inline void clipToWorld () {
		mins[0] = std::max(mins[0], -MWW);
		mins[1] = std::max(mins[1], -MWW);
		/* Hmm, we don't have a MAX_WORLD_HEIGHT ?!? */
		maxs[0] = std::min(maxs[0], MWW);
		maxs[1] = std::min(maxs[1], MWW);
	}
	/** @brief expand the box in four directions, but clip them to the maximum boundaries */
	inline void expandXY (const float byVal) {
		mins[0] -= byVal;
		mins[1] -= byVal;
		maxs[0] += byVal;
		maxs[1] += byVal;
		clipToWorld();
	}
	/** @brief shove the whole box by the given vector */
	inline void shift (const vec3_t shiftVec) {
		VectorAdd(mins, shiftVec, mins);
		VectorAdd(maxs, shiftVec, maxs);
		clipToWorld();
	}
	/** we explicitly don't make them private for now, because the goal of this class is to NOT handle them separately */
	vec3_t mins;
	vec3_t maxs;
};

inline void AABB::set (const AABB& other)
{
	VectorCopy(other.mins, mins);
	VectorCopy(other.maxs, maxs);
}

inline void AABB::set (const vec3_t mini, const vec3_t maxi)
{
	VectorCopy(mini, mins);
	VectorCopy(maxi, maxs);
}

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
