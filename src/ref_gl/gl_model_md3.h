/**
 * @file gl_model_md3.h
 * @brief md3 alias model loading
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

typedef	struct {
	char	name[MD3_MAX_PATH];
	dorientation_t	orient;
} maliastag_t;

typedef	struct {
	char	name[MD3_MAX_PATH];
	int	shader;
} maliasskin_t;

typedef	struct {
	int	num_verts;
	char	name[MD3_MAX_PATH];
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

void R_DrawAliasMD3Model(entity_t *e);

void Mod_LoadAliasMD3Model(model_t *mod, void *buffer, int bufSize);
