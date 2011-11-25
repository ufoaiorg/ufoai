/**
 * @file r_model_alias.h
 * @brief Shared alias model functions
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

#ifndef R_MODEL_ALIAS_H
#define R_MODEL_ALIAS_H

#define MODEL_MAX_PATH 64
#define MAX_ALIAS_TRIS 4096
#define MAX_ALIAS_VERTS 8192

#define mAliasCoord_t vec2_t

typedef struct mAliasVertex_s {
	vec3_t	point;
	vec3_t	normal;
} mAliasVertex_t;

typedef struct mAliasComplexVertex_s {
	vec3_t	point;
	vec3_t	normal;
	vec4_t	tangent;
} mAliasComplexVertex_t;

typedef struct mAliasBoneMatrix_s {
	vec4_t	matrix[3];
} mAliasBoneMatrix_t;

typedef struct mAliasBoneVertex_s {
	vec3_t	origin; /**< vertex location (these blend) */
	float	influence; /**< influence fraction (these must add up to 1) */
	vec3_t	normal; /**< surface normal (these blend) */
	unsigned int bonenum; /**< number of the bone */
} mAliasBoneVertex_t;

typedef struct mAliasFrame_s {
	vec3_t	mins, maxs;
	vec3_t	translate;
	vec3_t	scale;
	float	radius;
	mAliasBoneMatrix_t	*boneMatrix;
} mAliasFrame_t;

/**
 * The translation and rotation values for one frame of a tag
 */
typedef struct mAliasTagOrientation_s {
	/** the translation vector */
	vec3_t origin;
	/** the rotation vectors */
	vec3_t axis[3];
} mAliasTagOrientation_t;

/**
 * A tag is a reference point that allows us to place other (child-)meshes to that
 * particular location.
 *
 * The tag has a rotation and a translation vector on a per-frame base. This allows
 * us to place e.g. weapons onto the tag_rweapon tag of a character model and also
 * move this weapon while the character models moves its arm.
 */
typedef struct mAliasTag_s {
	/** the tagname */
	char name[MODEL_MAX_PATH];
	/** a list of rotation and translation vectors for each frame */
	mAliasTagOrientation_t *orient;
} mAliasTag_t;

typedef	struct mAliasSkin_s {
	char	name[MODEL_MAX_PATH];
	int	shader;
	image_t *skin;
} mAliasSkin_t;

typedef struct mIndexList_s {
	int length;
	int32_t *list;
} mIndexList_t;

typedef	struct mAliasMesh_s {
	int32_t	num_verts;
	char	name[MODEL_MAX_PATH];
	mAliasVertex_t	*vertexes;
	mAliasCoord_t	*stcoords;

	/* static meshes have vertex arrays */
	vec_t *verts;
	vec_t *texcoords;
	vec_t *normals;
	vec_t *tangents;
	vec_t *next_verts;
	vec_t *next_normals;
	vec_t *next_tangents;

	int	num_tris;
	int32_t	*indexes;
	int32_t num_indexes;
	mIndexList_t *revIndexes;

	int		num_bones;
	mAliasBoneVertex_t	*bonesVertexes;

	int		num_skins;
	int		skinHeight;
	int		skinWidth;
	mAliasSkin_t	*skins;
} mAliasMesh_t;

#define MAX_ANIMS		128
#define MAX_ANIMNAME	16

typedef struct mAliasAnim_s {
	char name[MAX_ANIMNAME];	/**< the name of this animation from the anm file */
	int from;					/**< the frame this animation is starting on */
	int to;						/**< the frame this animation is ending on */
	int time;
} mAliasAnim_t;

typedef struct mAliasBone_s {
	char name[MAX_VAR];
	int flags;
	int parent; /* -1 for no parent */
} mAliasBone_t;

typedef	struct	mAliasModel_s {
	int		num_frames;
	mAliasFrame_t	*frames;
	int		curFrame;
	int		oldFrame;

	/** amount of different tags for the model */
	int		num_tags;
	/** a list of tags */
	mAliasTag_t	*tags;

	int		num_meshes;
	mAliasMesh_t	*meshes;

	int		num_bones;
	mAliasBone_t	*bones;

	/** animation data */
	char animname[MAX_QPATH];
	int num_anims;
	mAliasAnim_t *animdata;
	int curAnim;
} mAliasModel_t;

void R_ModLoadAnims(mAliasModel_t *mod, const char *buffer);
qboolean R_ModLoadMDX(struct model_s *mod);
void R_ModCalcUniqueNormalsAndTangents(mAliasMesh_t *mesh, int nFrames, float smoothness);
void R_FillArrayData(mAliasModel_t* mod, mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum, qboolean prerender);
void R_ModLoadArrayData(mAliasModel_t *mod, mAliasMesh_t *mesh, qboolean loadNormals);
#endif
