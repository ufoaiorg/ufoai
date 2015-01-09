/**
 * @file
 * @brief Interface to game library.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/game.h
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

#include "../shared/ufotypes.h"

/** @note don't change the order - also see edict_s in g_local.h */
class SrvEdict {
public:
	bool inuse;
	int linkcount;		/**< count the amount of server side links - if a link was called,
						 * something on the position or the size of the entity was changed */

	int number;			/**< the number in the global edict array */

	vec3_t origin;		/**< the position in the world */
	vec3_t angles;		/**< the rotation in the world (pitch, yaw, roll) */
	pos3_t pos;			/** < the grid position of the actor */

	solid_t solid;		/** tracing info SOLID_BSP, SOLID_BBOX, ... */

	AABB entBox;		/**< position of min and max points - relative to origin */
	AABB absBox;		/**< position of min and max points - relative to world's origin */
	vec3_t size;

	SrvEdict* _child;	/**< e.g. the trigger for this edict */
	SrvEdict* _owner;	/**< e.g. the door model in case of func_door */
	int modelindex;		/**< inline model index */

	const char* classname;

	inline SrvEdict* child () {	/* only needed for SV_LinkEdict */
		return _child;
	}
	inline SrvEdict* owner () const { /* only used in isParentship, which is only used in SV_ClipMoveToEntities, called only by SV_Trace */
		return _owner;
	}
	inline bool isSameAs (const SrvEdict* other) const {
		if (!other)
			return false;
		return number == other->number;
	}
	inline bool isParentship (const SrvEdict* other) const {
		if (other) {
			if (other->owner() && other->owner()->isSameAs(this))
				return true;
			if (this->owner() && this->owner()->isSameAs(other))
				return true;
		}
		return false;
	}
};
