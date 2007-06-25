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

typedef	struct	maliascoord_s {
	vec2_t	st;
} maliascoord_t;

typedef	struct	maliasvertex_s {
	vec3_t	point;
	vec3_t	normal;
} maliasvertex_t;

typedef	struct {
	vec3_t	mins, maxs;
	vec3_t	translate;
	float	radius;
} maliasframe_t;

typedef struct {
	vec3_t origin;
	float axis[3][3];
} maliastagorientation_t;

typedef	struct {
	char	name[MODEL_MAX_PATH];
	maliastagorientation_t	orient;
} maliastag_t;

typedef	struct {
	char	name[MODEL_MAX_PATH];
	int	shader;
} maliasskin_t;

typedef	struct {
	int	num_verts;
	char	name[MODEL_MAX_PATH];
	maliasvertex_t	*vertexes;
	maliascoord_t	*stcoords;

	int	num_tris;
	index_t	*indexes;
	int	*trneighbors;

	int		num_skins;
	maliasskin_t	*skins;
} maliasmesh_t;

typedef	struct	maliasmodel_s {
	int		num_frames;
	maliasframe_t	*frames;

	int		num_tags;
	maliastag_t	*tags;

	int		num_meshes;
	maliasmesh_t	*meshes;

	int		num_skins;
	maliasskin_t	*skins;
} maliasmodel_t;

void Mod_BuildTriangleNeighbors(neighbors_t *neighbors, dtriangle_t *tris, int numtris);
int Mod_FindTriangleWithEdge(neighbors_t *neighbors, dtriangle_t *tris, int numtris, int triIndex, int edgeIndex);
void Mod_LoadAnims(model_t * mod, void *buffer);
