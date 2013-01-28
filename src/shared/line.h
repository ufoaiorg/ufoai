/**
 * @file
 * @brief A simple line between two points
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

/**
 * @brief Axis-aligned bounding box
 */
const float MWW = MAX_WORLD_WIDTH;

class Line {
public:
	Line (const vec3_t _start, const vec3_t _stop) : start(_start), stop(_stop);

	/**
	 * @brief Copies the values from the given Line
	 * @param[in] other The other Line
	 */
	inline void set (const Line& other);
	{
		VectorCopy(other.start, start);
		VectorCopy(other.stop, stop);
	}

	/** we explicitly don't make them private for now, because the goal of this class is to NOT handle them separately */
	vec3_t start;
	vec3_t stop;
};
