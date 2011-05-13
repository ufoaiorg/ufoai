/**
 * @file src/tools/ufo2map/map.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef UFO2MAP_MAP_H
#define UFO2MAP_MAP_H

#include "../../shared/mathlib.h"
#include "../../shared/defines.h"
#include "common/polylib.h"

typedef struct brush_texture_s {
	vec2_t		shift;
	vec_t		rotate;
	vec2_t		scale;
	char		name[MAX_TEXPATH];	/**< texture name - relative to base/textures */
	uint32_t	surfaceFlags;
	int			value;
} brush_texture_t;

typedef struct face_s {
	struct face_s	*next;		/**< on node */

	/** the chain of faces off of a node can be merged or split,
	 * but each face_t along the way will remain in the chain
	 * until the entire tree is freed */
	struct face_s	*merged;	/**< if set, this face isn't valid anymore */
	struct face_s	*split[2];	/**< if set, this face isn't valid anymore */

	struct portal_s	*portal;
	int				texinfo;
	uint16_t		planenum;
	uint32_t		contentFlags;	/**< faces in different contents can't merge */
	winding_t		*w;
	int				numpoints;
	int				vertexnums[MAXEDGES];
} face_t;

typedef struct side_s {
	uint16_t	planenum;
	int			texinfo;
	winding_t	*winding;
	struct side_s	*original;	/**< bspbrush_t sides will reference the mapbrush_t sides */
	uint32_t	contentFlags;	/**< from miptex */
	uint32_t	surfaceFlags;	/**< from miptex */
	qboolean	visible;		/**< choose visible planes first */
	qboolean	tested;			/**< this plane already checked as a split */
	qboolean	bevel;			/**< don't ever use for bsp splitting */
	qboolean	isCompositeMember;	/**< forms a side with sides from other brushes @sa Check_FindCompositeSides */

	struct mapbrush_s *brush;		/**< backlink to the brush this side belongs to */
} side_t;

typedef struct mapbrush_s {
	int		entitynum;		/**< the entity number in the map - 0 is the world - everything else is a bmodel */
	int		brushnum;		/**< the brush number in the map */

	uint32_t	contentFlags;

	vec3_t	mins, maxs;

	int		numsides;
	struct side_s	*original_sides;

	/**list of brushes that are near to this one.
	 * not necessarily initialised. call Check_NearList() to make sure it has been initialised
	 * this will return quickly if it has already been done. */
	struct	mapbrush_s **nearBrushes;
	int	numNear;

	qboolean skipWriteBack; /**< in fix mode do not write back to the source .map file */

	qboolean finished;
} mapbrush_t;

/** @sa mapplanes */
typedef struct plane_s {
	vec3_t	normal;			/**< unit (magnitude == 1) normal defining the direction of the plane */
	vec_t	dist;			/**< distance from the origin to the plane. unit normal and distance
							 * description is http://mathworld.wolfram.com/HessianNormalForm.html
							 * though the sign of the distance seems to differ from the standard definition.
							 * (or the direction of the normal differs, the effect is the same when calculating
							 * the distance of a point from the plane.)*/
	int		type;
	vec3_t	planeVector[3]; /**< 3 points on the plane, from the map file */
	struct plane_s	*hash_chain;
} plane_t;

typedef struct portal_s {
	plane_t		plane;
	struct node_s		*onnode;		/**< NULL = outside box */
	struct node_s		*nodes[2];		/**< [0] = front side of plane */
	struct portal_s	*next[2];
	winding_t	*winding;

	qboolean	sidefound;		/**< false if ->side hasn't been checked */
	struct side_s		*side;			/**< NULL = non-visible */
	face_t		*face[2];		/**< output face in bsp file */
} portal_t;


#endif
