/**
 * @file
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

#pragma once

#include "ufotypes.h"
#include "vector.h"
#include "line.h"
#include "byte.h"
#include "defines.h"	/* for MAX_WORLD_WIDTH */
#include <algorithm>
#include "stdio.h"

/**
 * @brief Axis-aligned bounding box
 */
const float MWW = MAX_WORLD_WIDTH;
#define AABB_STRING 64

class AABB {
public:
	static const AABB EMPTY;
	/*==================
	 *		ctors
	 *==================*/
	AABB ();
	AABB (const vec3_t mini, const vec3_t maxi);
	AABB (const vec_t minX, const vec_t minY, const vec_t minZ, const vec_t maxX, const vec_t maxY, const vec_t maxZ);
	AABB (const Line& line);

	/*==================
	 *		setters
	 *==================*/
	/**
	 * @brief Copies the values from the given aabb
	 * @param[in] other The other aabb
	 */
	inline void set (const AABB& other) {
		VectorCopy(other.mins, mins);
		VectorCopy(other.maxs, maxs);
	}
	inline void set (const vec3_t mini, const vec3_t maxi) {
		VectorCopy(mini, mins);
		VectorCopy(maxi, maxs);
	}
	inline void setMins (const vec3_t mini) {
		VectorCopy(mini, mins);
	}
	inline void setMaxs (const vec3_t maxi) {
		VectorCopy(maxi, maxs);
	}
	inline void setMins (int x, int y, int z) {
		VectorSet(mins, x, y, z);
	}
	inline void setMaxs (int x, int y, int z) {
		VectorSet(maxs, x, y, z);
	}
	inline void setMaxZ (float zVal) {
		maxs[2] = zVal;
	}
	inline void reset () {
		mins[0] = mins[1] = mins[2] = 0;
		maxs[0] = maxs[1] = maxs[2] = 0;
	}
	inline void setFromLittleFloat (const AABB& other) {
		mins[0] = LittleFloat(other.mins[0]);
		mins[1] = LittleFloat(other.mins[1]);
		mins[2] = LittleFloat(other.mins[2]);
		maxs[0] = LittleFloat(other.maxs[0]);
		maxs[1] = LittleFloat(other.maxs[1]);
		maxs[2] = LittleFloat(other.maxs[2]);
	}
	/**
	 * @brief Sets mins and maxs to their starting points before using addPoint
	 */
	inline void setNegativeVolume () {
		mins[0] = mins[1] = mins[2] = 99999;
		maxs[0] = maxs[1] = maxs[2] = -99999;
	}
	/**
	 * @brief Set from another box and a (trace)Line
	 */
	inline void set (const AABB& trBox, const Line& trLine) {
		AABB endBox(trBox);			/* get the moving object */
		endBox.shift(trLine.stop);	/* move it to end position */
		set(trBox);
		shift(trLine.start);		/* object in starting position */
		add(endBox);				/* the whole box */
	}

	/*==================
	 *		getters
	 *==================*/
	inline const vec3_t& getMins () const {
		return mins;
	}
	inline float getMinX () const {
		return mins[0];
	}
	inline float getMinY () const {
		return mins[1];
	}
	inline float getMinZ () const {
		return mins[2];
	}
	inline const vec3_t& getMaxs () const {
		return maxs;
	}
	inline float getMaxX () const {
		return maxs[0];
	}
	inline float getMaxY () const {
		return maxs[1];
	}
	inline float getMaxZ () const {
		return maxs[2];
	}

	inline float getWidthX () const {
		return getMaxX() - getMinX();
	}
	inline float getWidthY () const {
		return getMaxY() - getMinY();
	}
	inline float getWidthZ () const {
		return getMaxZ() - getMinZ();
	}

	/**
	 * @brief Calculates the center of the bounding box
	 * @param[out] center The target center vector
	 */
	inline void getCenter (vec3_t center) const {
		VectorAdd(mins, maxs, center);
		VectorScale(center, 0.5, center);
	}
	inline void getDiagonal (vec3_t diagonal) const {
		VectorSubtract(maxs, mins, diagonal);
	}
	/**
	 * @brief Prints a representation of the box
	 * @param[out] str The output string, expected to be at least AABB_STRING wide
	 * @param[in] len The length of the output buffer
	 */
	inline void asIntString (char* str, size_t len) {
		snprintf(str, len, "(%i, %i, %i) (%i, %i, %i)",
			(int) mins[0], (int) mins[1], (int) mins[2],
			(int) maxs[0], (int) maxs[1], (int) maxs[2]	);
	}

	/*==================
	 *		checkers
	 *==================*/
	inline bool isZero () const {
		return VectorEmpty(mins) && VectorEmpty(maxs);
	}
	/**
	 * @brief Checks if the aabb touches or intersects with the given aabb
	 * @param[in] other The other aabb
	 */
	inline bool doesIntersect (const AABB& other) const {
		return !(getMinX() > other.getMaxX() || getMinY() > other.getMaxY() || getMinZ() > other.getMaxZ() || getMaxX() < other.getMinX()
				|| getMaxY() < other.getMinY() || getMaxZ() < other.getMinZ());
	}
	/**
	 * @brief Checks if the given line has a chance to hit our box
	 * @param[in] line The line that might hit us
	 * @return false - impossible; true - maybe
	 */
	inline bool canBeHitBy (const Line& line) const {
		return !(  (line.start[0] > getMaxX() && line.stop[0] > getMaxX())
				|| (line.start[1] > getMaxY() && line.stop[1] > getMaxY())
				|| (line.start[2] > getMaxZ() && line.stop[2] > getMaxZ())
				|| (line.start[0] < getMinX() && line.stop[0] < getMinX())
				|| (line.start[1] < getMinY() && line.stop[1] < getMinY())
				|| (line.start[2] < getMinZ() && line.stop[2] < getMinZ()));
	}
	inline bool contains (const vec3_t point) const {
		return (point[0] >= getMinX() && point[0] <= getMaxX()
			&& point[1] >= getMinY() && point[1] <= getMaxY()
			&& point[2] >= getMinZ() && point[2] <= getMaxZ() );
	}
	inline bool contains (const AABB& other) const {
		return (other.getMinX() >= getMinX() && other.getMaxX() <= getMaxX()
			&& other.getMinY() >= getMinY() && other.getMaxY() <= getMaxY()
			&& other.getMinZ() >= getMinZ() && other.getMaxZ() <= getMaxZ() );
	}

	/*==================
	 *	manipulators
	 *==================*/
	void add (const vec3_t point);
	void add (const AABB& other);

	/**
	 * @brief Rotates bounding box around given origin point; note that it will expand the box unless all angles are multiples of 90 degrees
	 * @note Not fully verified so far
	 */
	void rotateAround(const vec3_t origin, const vec3_t angles);

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
	/** @brief expand the box in all directions, but clip them to the maximum boundaries */
	inline void expand (const float byVal) {
		mins[2] -= byVal;
		maxs[2] += byVal;
		expandXY(byVal);
	}
	/** @brief shove the whole box by the given vector */
	inline void shift (const vec3_t shiftVec) {
		VectorAdd(mins, shiftVec, mins);
		VectorAdd(maxs, shiftVec, maxs);
		clipToWorld();
	}

	/*==================
	 *		data
	 *==================*/
	/** we explicitly don't make them private for now, because the goal of this class is to NOT handle them separately */
//	private:
	vec3_t mins;
	vec3_t maxs;
};
