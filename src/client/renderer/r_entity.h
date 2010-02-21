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

typedef struct animState_s {
	int frame, oldframe;
	float backlerp;				/**< linear interpolation from previous frame */
	int time, dt;
	int mesh;

	byte list[MAX_ANIMLIST];
	byte lcur, ladd;
	byte change;
} animState_t;

typedef struct static_lighting_s {
	vec3_t origin;		/**< starting point, entity origin */
	vec3_t point;		/**< impact point, shadow origin */
	vec3_t normal;		/**< shadow direction */
	vec3_t color;		/**< light color */
	vec3_t position;	/**< and position */
	float time;			/**< lerping interval */
	vec3_t colors[2];	/**< lerping color */
	vec3_t positions[2];/**< and positions */
	qboolean dirty;		/**< cache invalidation */
} static_lighting_t;

/**
 * @brief entity transform matrix
 */
typedef struct {
	qboolean done;			/**< already calculated */
	qboolean processing;	/**< currently doing the calculation */
	float matrix[16];		/**< the matrix that holds the result */
} transform_t;

typedef struct entity_s {
	struct model_s *model;		/**< opaque type outside refresh */
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
	int state;					/**< same state as the le->state */
	int flags;

	qboolean isOriginBrushModel;	/**< true for bmodels that have an origin set */

	animState_t as;

	transform_t transform;

	static_lighting_t *lighting;	/**< cached static lighting info */

	struct entity_s *next;		/**< for chaining */
} entity_t;

int R_AddEntity(const entity_t *ent);
entity_t *R_GetFreeEntity(void);
entity_t *R_GetEntity(int id);
void R_EntityDrawBBox(const vec3_t mins, const vec3_t maxs);
void R_TransformForEntity(const entity_t *e, const vec3_t in, vec3_t out);

extern int r_numEntities;

#endif
