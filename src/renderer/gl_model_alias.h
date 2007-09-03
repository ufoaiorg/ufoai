/**
 * @file gl_model_alias.h
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

#define MODEL_MAX_PATH 64

typedef struct mAliasNeighbors_s {
	int n[3];
} mAliasNeighbors_t;

typedef	struct	mAliasCoord_s {
	vec2_t	st;
} mAliasCoord_t;

typedef	struct	mAliasVertex_s {
	vec3_t	point;
	vec3_t	normal;
} mAliasVertex_t;

typedef	struct mAliasFrame_s {
	vec3_t	mins, maxs;
	vec3_t	translate;
	float	radius;
} mAliasFrame_t;

typedef struct mAliasTagOrientation_s {
	vec3_t origin;
	float axis[3][3];
} mAliasTagOrientation_t;

typedef	struct mAliasTag_s {
	char	name[MODEL_MAX_PATH];
	mAliasTagOrientation_t	orient;
} mAliasTag_t;

typedef	struct mAliasSkin_s {
	char	name[MODEL_MAX_PATH];
	int	shader;
} mAliasSkin_t;

typedef	struct mAliasMesh_s {
	int	num_verts;
	char	name[MODEL_MAX_PATH];
	mAliasVertex_t	*vertexes;
	mAliasCoord_t	*stcoords;

	int	num_tris;
	unsigned int	*indexes;
	int	*trneighbors;

	int		num_skins;
	mAliasSkin_t	*skins;
} mAliasMesh_t;

/** @brief Only for obj */
typedef struct mAliasNormals_s {
	vec3_t ijk;
} mAliasNormals_t;

#define MAX_ANIMS		128
#define MAX_ANIMNAME	16

typedef struct mAliasAnim_s {
	char name[MAX_ANIMNAME];
	int from, to;
	int time;
} mAliasAnim_t;

typedef	struct	mAliasModel_s {
	int		num_frames;
	mAliasFrame_t	*frames;

	int		num_tags;
	mAliasTag_t	*tags;

	int		num_meshes;
	mAliasMesh_t	*meshes;

	int		num_skins;
	mAliasSkin_t	*skins;

	int num_normals;			/**< number of normal vectors */
	mAliasNormals_t	*normals;

	/** animation data */
	char animname[MAX_QPATH];
	int numanims;
	mAliasAnim_t *animdata;

	qboolean noshadow;

	/* FIXME */
	image_t *skins_img[MD2_MAX_SKINS];
	void *extraData;

	/** tag data */
	char tagname[MAX_QPATH];
	void *tagdata;

	mAliasNeighbors_t *neighbors;
} mAliasModel_t;

void Mod_BuildTriangleNeighbors(mAliasNeighbors_t *neighbors, dtriangle_t *tris, int numtris);
int Mod_FindTriangleWithEdge(mAliasNeighbors_t *neighbors, dtriangle_t *tris, int numtris, int triIndex, int edgeIndex);
void Mod_LoadAnims(mAliasModel_t * mod, void *buffer);
