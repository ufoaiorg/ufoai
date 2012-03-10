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
#define MAX_ENTITY_LIGHTS 7

#include "r_light.h"
#include "r_lighting.h"

typedef struct animState_s {
	int frame;
	int oldframe;
	float backlerp;				/**< linear interpolation from previous frame */
	int time;
	int dt;
	int mesh;					/**< the mesh index of the model to animate */

	byte list[MAX_ANIMLIST];	/**< the list of @c mAliasAnim_t array indices that should be played */
	byte lcur;					/**< the current position in the animation list */
	byte ladd;
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

	const light_t *lights[MAX_ENTITY_LIGHTS];		/**< static and dynamic lights sorted by distance */
	int numLights;
	qboolean inShadow; /**< true if entity is shadowed from the sun */

	const image_t *texture;

	lighting_t *lighting;		/**< cached static light source information */

	struct entity_s *next;		/**< for chaining */
} entity_t;

/* entity chains for rendering */
entity_t *r_opaque_mesh_entities;
entity_t *r_blend_mesh_entities, *r_null_entities;
entity_t *r_special_entities;


int R_AddEntity(const entity_t *ent);
entity_t *R_GetFreeEntity(void);
entity_t *R_GetEntity(int id);
void R_EntitySetOrigin(entity_t *ent, const vec3_t origin);
void R_EntityAddToOrigin(entity_t *ent, const vec3_t offset);
void R_TransformForEntity(const entity_t *e, const vec3_t in, vec3_t out);

void R_DrawEntityEffects(void);
void R_DrawMeshEntities(entity_t *ents);
void R_DrawOpaqueMeshEntities(entity_t *ents);
void R_DrawBlendMeshEntities(entity_t *ents);
void R_DrawSpecialEntities(const entity_t *ents);
void R_DrawNullEntities(const entity_t *ents);

#endif
