/**
 * @file r_array.c
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

/**
 * @sa R_SetVertexBufferState
 * @param[in] mod The loaded maptile (more than one bsp loaded at the same time)
 */
static inline void R_SetVertexArrayState (const model_t* mod)
{
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->bsp.verts);

	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.texcoords);

	if (texunit_lightmap.enabled) {  /* lightmap texcoords */
		R_SelectTexture(&texunit_lightmap);
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.lmtexcoords);
		R_SelectTexture(&texunit_diffuse);
	}

	if (r_state.lighting_enabled) { /* normal vectors for lighting */
		R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->bsp.normals);

		/* tangent vectors for bump mapping */
		if (r_bumpmap->value)
			R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, mod->bsp.tangents);
	}
}

/**
 * @sa R_SetVertexArrayState
 * @param[in] mod The loaded maptile (more than one bsp loaded at the same time)
 * @note r_vertexbuffers must be set to 1 to use this
 */
static inline void R_SetVertexBufferState (const model_t* mod)
{
	R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->bsp.vertex_buffer);

	R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.texcoord_buffer);

	if (texunit_lightmap.enabled) {  /* lightmap texcoords */
		R_SelectTexture(&texunit_lightmap);
		R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.lmtexcoord_buffer);
		R_SelectTexture(&texunit_diffuse);
	}

	if (r_state.lighting_enabled) { /* normal vectors for lighting */
		R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->bsp.normal_buffer);

		/* tangent vectors for bump mapping */
		if (r_bumpmap->value)
			R_BindBuffer(GL_TANGENT_ARRAY, GL_FLOAT, mod->bsp.tangent_buffer);
	}
}

void R_SetArrayState (const model_t *mod)
{
	if (r_locals.bufferMapTile != mod) {
		r_locals.bufferMapTile = mod;

		if (qglBindBuffer && r_vertexbuffers->integer)
			R_SetVertexBufferState(mod);
		else
			R_SetVertexArrayState(mod);
	}
}

void R_ResetArrayState (void)
{
	/* vbo */
	R_BindBuffer(0, 0, 0);

	/* vertex array */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	/* lightmap texcoords */
	if (texunit_lightmap.enabled) {
		R_SelectTexture(&texunit_lightmap);
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
		R_SelectTexture(&texunit_diffuse);
	}

	/* diffuse texcoords */
	if (texunit_diffuse.enabled)
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	/* normals and tangents */
	if (r_state.lighting_enabled) {
		R_BindDefaultArray(GL_NORMAL_ARRAY);

		/* tangent vectors for bump mapping */
		if (r_bumpmap->value)
			R_BindDefaultArray(GL_TANGENT_ARRAY);
	}
	r_locals.bufferMapTile = NULL;
}
