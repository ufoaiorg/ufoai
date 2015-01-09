/**
 * @file
 * @brief Info string handling
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "mathlib.h"
#include <stdio.h>

/**
 * @brief A pair of strings representing a key and a value
 * The value string can be trimmed and rendered in the data type it is expected to be.
 * We might add some validity checking here.
 */
class KeyValuePair {
private:
	const char* _keyStr;
	const char* _valStr;
public:
	KeyValuePair (const char* keyStr, const char* valStr) {
		_keyStr = keyStr;
		_valStr = valStr;
	}
	inline void set (const char* keyStr, const char* valStr) {
		_keyStr = keyStr;
		_valStr = valStr;
	}
	inline bool isKey(const char* name) const {
		return !strcmp(_keyStr, name);
	}
	inline float asFloat () const {
		return atof(_valStr);
	}
	inline int asInt () const {
		return atoi(_valStr);
	}
	inline bool asBool () const {
		return asInt() != 0 ? true : false;
	}
	inline const char* asString () const {
		return _valStr;
	}
	void asVec3 (vec3_t vec) const {
		if (sscanf(_valStr, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3)
			VectorCopy(vec3_origin, vec);
	}
};
