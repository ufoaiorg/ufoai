/**
 * @file r_entity.h
 */

/*
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

#ifndef R_ENTITY_H
#define R_ENTITY_H

#define MAX_ANIMLIST	8

#include "r_light.h"

typedef struct animState_s {
	int frame, oldframe;
	float backlerp;				/**< linear interpolation from previous frame */
	int time, dt;
	int mesh;

	byte list[MAX_ANIMLIST];
	byte lcur, ladd;
	byte change;
} animState_t;

/**
 * @brief entity transform matrix
 */
typedef struct {
	qboolean done;			/**< already calculated */
	qboolean processing;	/**< currently doing the calculation */
	float matrix[16];		/**< the matrix that holds the result */
} transform_t;

typedef struct entity_s {
	struct model_s *model;
	vec3_t angles;
	vec3_t scale;
	vec3_t color;
	vec3_t origin;
	vec3_t oldorigin;

	vec3_t mins, maxs;

	/* tag positioning */
	struct entity_s *tagent;	/**< pointer to the parent entity */
	const char *tagname;				/**< name of the tag */

	/* misc */
	int skinnum;
	float alpha;				/**< ignore if RF_TRANSLUCENT isn't set */
	int flags;
	float distanceFromViewOrigin;

	qboolean isOriginBrushModel;	/**< true for bmodels that have an origin set */

	animState_t as;

	transform_t transform;

	vec4_t shell;					/**< shell color */

	r_light_t *lights[MAX_DYNAMIC_LIGHTS];		/**< dynamic lights sorted by distance */
	int numLights;

	image_t *deathTexture;

	struct entity_s *next;		/**< for chaining */
} entity_t;

int R_AddEntity(const entity_t *ent);
entity_t *R_GetFreeEntity(void);
entity_t *R_GetEntity(int id);

#endif
