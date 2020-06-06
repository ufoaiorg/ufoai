/**
 * @file
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

#pragma once

#define MAX_ANIMLIST	8
#define MAX_ENTITY_LIGHTS 7

#include "r_light.h"
#include "r_lighting.h"

/** entity->flags (render flags) */
#define RF_NONE				0x00000000	/**< for initialization */
#define RF_TRANSLUCENT      0x00000001
#define RF_BOX              0x00000002	/**< actor selection box */
#define RF_PATH             0x01000000	/**< pathing marker, debugging only */
#define RF_ARROW            0x02000000	/**< arrow, debugging only */

/** the following ent flags also draw entity effects */
#define RF_NO_SHADOW        0x00000004	/**< shadow (when living) for this entity */
#define RF_BLOOD            0x00000008	/**< blood (when dead) for this entity */
#define RF_SELECTED         0x00000010	/**< selected actor */
#define RF_MEMBER           0x00000020	/**< actor in the same team */
#define RF_ALLIED           0x00000040	/**< actor in an allied team (controlled by another player) */
#define RF_ACTOR            0x00000080	/**< this is an actor */
#define RF_PULSE            0x00000100	/**< glowing entity */
#define RF_IRGOGGLES        0x00000200	/**< this is visible if the actor uses ir goggles */
#define RF_NEUTRAL			0x00000400	/**< actor from a neutral team */
#define RF_SHADOW           0x00000800	/**< shadow (when living) for this entity */
#define RF_OPPONENT         0x00001000	/**< opponent */
#define RF_IRGOGGLESSHOT	0x00002000	/**< this is the actor that used an irgoggle */

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

	animState_s() :
		frame(0),
		oldframe(0),
		backlerp(0.0f),
		time(0),
		dt(0),
		mesh(0),
		lcur(0),
		ladd(0),
		change(0)
	{
		OBJZERO(list);
	}
} animState_t;

/**
 * @brief entity transform matrix
 */
typedef struct transform_s {
	bool done;					/**< already calculated */
	bool processing;			/**< currently doing the calculation */
	float matrix[16];			/**< the matrix that holds the result */

	transform_s() : done(false), processing(false)
	{
		OBJZERO(matrix);
	}
} transform_t;

typedef struct entity_s {
	struct model_s* model;
	vec3_t angles;
	vec3_t scale;
	vec3_t color;
	vec3_t origin;
	vec3_t oldorigin;
	AABB eBox;

	/* tag positioning */
	struct entity_s* tagent;	/**< pointer to the parent entity */
	const char* tagname;		/**< name of the tag */

	/* misc */
	int skinnum;
	float alpha;				/**< ignore if RF_TRANSLUCENT isn't set */
	int flags;
	float distanceFromViewOrigin;

	bool isOriginBrushModel;	/**< true for bmodels that have an origin set */

	animState_t as;

	transform_t transform;

	vec4_t shell;				/**< shell color */

	const image_t* texture;

	lighting_t* lighting;		/**< cached static light source information */

	struct entity_s* next;		/**< for chaining */


	inline entity_s (int flag = RF_NONE) :
		model(nullptr),
		eBox(AABB()),
		tagent(nullptr),
		tagname(nullptr),
		skinnum(0),
		alpha(0.0f),
		flags(flag),
		distanceFromViewOrigin(0.0f),
		isOriginBrushModel(false),
		as(animState_s()),
		transform(transform_s()),
		texture(nullptr),
		lighting(nullptr),
		next(nullptr)
	{
		VectorClear(angles);
		VectorClear(scale);
		VectorClear(color);
		VectorClear(origin);
		VectorClear(oldorigin);
		Vector4Clear(shell);
	}

	inline void setScale(const vec3_t scale_)
	{
		VectorCopy(scale_, scale);
	}

	inline vec_t getScaleX() const
	{
		return scale[0];
	}
} entity_t;

/* entity chains for rendering */
extern entity_t* r_opaque_mesh_entities;
extern entity_t* r_blend_mesh_entities;
extern entity_t* r_null_entities;
extern entity_t* r_special_entities;


int R_AddEntity(const entity_t* ent);
entity_t* R_GetFreeEntity(void);
entity_t* R_GetEntity(int id);
void R_EntitySetOrigin(entity_t* ent, const vec3_t origin);
void R_EntityAddToOrigin(entity_t* ent, const vec3_t offset);
void R_TransformForEntity(const entity_t* e, const vec3_t in, vec3_t out);

void R_DrawEntityEffects(void);
void R_DrawMeshEntities(entity_t* ents);
void R_DrawOpaqueMeshEntities(entity_t* ents);
void R_DrawBlendMeshEntities(entity_t* ents);
void R_DrawSpecialEntities(const entity_t* ents);
void R_DrawNullEntities(const entity_t* ents);
