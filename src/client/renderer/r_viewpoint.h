/**
 * @file r_viewpoint.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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


#ifndef R_VIEWPOINT_H
#define R_VIEWPOINT_H

#include "r_viewport.h"
#include "r_entity.h"

/* @sa renderData_t in ../cl_renderer.h */
typedef struct viewpoint_s {
	vec3_t viewOrigin;
	vec3_t viewAngles;
	float fieldOfViewX, fieldOfViewY;
	r_viewport_t viewport;

	float modelviewMat[16];
	float projMat[16];

	float time;					/**< time is used to auto animate */
	int rendererFlags;				/**< RDF_NOWORLDMODEL, etc */
	int worldlevel;
	//int brushCount, aliasCount;

	int weather;				/**< weather effects */
	vec4_t fogColor;

	struct entityListNode_s *ents; /**< entities to draw for this view */
} viewpoint_t;

#endif
