/**
 * @file r_array.c
 * @brief Arrays are "lazily" managed to reduce glArrayPointer calls. Drawing routines
 * should call R_SetArrayState or R_ResetArrayState somewhat early-on.
 */

/*
Copyright(c) 1997-2001 Id Software, Inc.
Copyright(c) 2002 The Quakeforge Project.
Copyright(c) 2006 Quake2World.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "r_local.h"

#define R_ARRAY_VERTEX			0x1
#define R_ARRAY_COLOR			0x2
#define R_ARRAY_NORMAL			0x4
#define R_ARRAY_TANGENT			0x8
#define R_ARRAY_TEX_DIFFUSE		0x10
#define R_ARRAY_TEX_LIGHTMAP	0x20

typedef struct r_array_state_s {
	const mBspModel_t *bspmodel;
	int arrays;
} r_array_state_t;

static r_array_state_t r_array_state;


/**
 * @brief This function is consulted to determine whether or not array
 * bindings are up to date.
 * @return Returns a bitmask representing the arrays which should be enabled according
 * to r_state.
 */
static int R_ArraysMask (void)
{
	int mask;

	mask = R_ARRAY_VERTEX;

	if (r_state.color_array_enabled)
		mask |= R_ARRAY_COLOR;

	if (r_state.lighting_enabled) {
		mask |= R_ARRAY_NORMAL;

		if (r_bumpmap->value > 0.0)
			mask |= R_ARRAY_TANGENT;
	}

	if (texunit_diffuse.enabled)
		mask |= R_ARRAY_TEX_DIFFUSE;

	if (texunit_lightmap.enabled)
		mask |= R_ARRAY_TEX_LIGHTMAP;

	return mask;
}

/**
 * @sa R_SetVertexBufferState
 * @param[in] mod The BSP model to use arrays from
 */
static inline void R_SetVertexArrayState (const mBspModel_t* bsp, int mask)
{
	/* vertex array */
	if (mask & R_ARRAY_VERTEX)
		R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, bsp->verts);

	/* normals and tangents for lighting */
	if (r_state.lighting_enabled) {
		if (mask & R_ARRAY_NORMAL)
			R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, bsp->normals);

		if (mask & R_ARRAY_TANGENT)
			R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, bsp->tangents);
	}

	/* diffuse texcoords */
	if (texunit_diffuse.enabled) {
		if (mask & R_ARRAY_TEX_DIFFUSE)
			R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, bsp->texcoords);
	}

	if (texunit_lightmap.enabled) {  /* lightmap texcoords */
		if (mask & R_ARRAY_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);
			R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, bsp->lmtexcoords);
			R_SelectTexture(&texunit_diffuse);
		}
	}
}

/**
 * @sa R_SetVertexArrayState
 * @param[in] bsp The BSP model to use arrays from
 * @note r_vertexbuffers must be set to 1 to use this
 */
static inline void R_SetVertexBufferState (const mBspModel_t* bsp, int mask)
{
	/* vertex array */
	if (mask & R_ARRAY_VERTEX)
		R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, bsp->vertex_buffer);

	if (r_state.lighting_enabled) { /* normals and tangents for lighting */
		if (mask & R_ARRAY_NORMAL)
			R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, bsp->normal_buffer);

		if (mask & R_ARRAY_TANGENT)
			R_BindBuffer(GL_TANGENT_ARRAY, GL_FLOAT, bsp->tangent_buffer);
	}

	/* diffuse texcoords */
	if (texunit_diffuse.enabled) {
		if (mask & R_ARRAY_TEX_DIFFUSE)
			R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, bsp->texcoord_buffer);
	}

	/* lightmap texcoords */
	if (texunit_lightmap.enabled) {
		if (mask & R_ARRAY_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);
			R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, bsp->lmtexcoord_buffer);
			R_SelectTexture(&texunit_diffuse);
		}
	}
}

void R_SetArrayState (const mBspModel_t *bsp)
{
	int arrays, mask;

	if (r_vertexbuffers->modified) {  /* force a full re-bind */
		r_array_state.bspmodel = NULL;
		r_array_state.arrays = 0xFFFF;
		r_vertexbuffers->modified = qfalse;
	}

	/* resolve the desired arrays mask */
	mask = 0xFFFF, arrays = R_ArraysMask();

	/* try to save some binds */
	if (r_array_state.bspmodel == bsp) {
		const int xor = r_array_state.arrays ^ arrays;
		if (!xor)  /* no changes, we're done */
			return;

		/* resolve what's left to turn on */
		mask = arrays & xor;
	}

	if (r_vertexbuffers->integer && qglGenBuffers)  /* use vbo */
		R_SetVertexBufferState(bsp, mask);
	else  /* or arrays */
		R_SetVertexArrayState(bsp, mask);

	r_array_state.bspmodel = bsp;
	r_array_state.arrays = arrays;
}

void R_ResetArrayState (void)
{
	r_array_state.bspmodel = NULL;
	r_array_state.arrays = 0;

	/* vbo */
	R_BindBuffer(0, 0, 0);

	/* vertex array */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	/* color array */
	if (r_state.color_array_enabled)
		R_BindDefaultArray(GL_COLOR_ARRAY);

	/* normals and tangents */
	if (r_state.lighting_enabled) {
		R_BindDefaultArray(GL_NORMAL_ARRAY);

		R_BindDefaultArray(GL_TANGENT_ARRAY);
	}

	/* diffuse texcoords */
	if (texunit_diffuse.enabled)
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	/* lightmap texcoords */
	if (texunit_lightmap.enabled) {
		R_SelectTexture(&texunit_lightmap);
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
		R_SelectTexture(&texunit_diffuse);
	}
}
