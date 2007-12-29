/**
 * @file r_surface.c
 * @brief surface-related refresh code
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

#include "r_local.h"
#include "r_lightmap.h"
#include "r_error.h"

mBspSurface_t *r_opaque_surfaces;
mBspSurface_t *r_opaque_warp_surfaces;

mBspSurface_t *r_alpha_surfaces;
mBspSurface_t *r_alpha_warp_surfaces;

static float surface_warp, surface_flow;

/**
 * @brief Set the surface state according to surface flags and bind the texture
 * @sa R_DrawSurfaces
 */
static void R_SetSurfaceState (const mBspSurface_t *surf)
{
	image_t *image;

	if (r_state.blend_enabled) {  /* alpha blend */
		float a;
		switch (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
		case SURF_TRANS33:
			a = 0.33;
			break;
		case SURF_TRANS66:
			a = 0.66;
			break;
		default:  /* both flags mean use the texture's alpha channel */
			a = 1.0;
			break;
		}

		qglColor4f(1.0, 1.0, 1.0, a);
	}

	if (surf->texinfo->flags & SURF_ALPHATEST) {  /* alpha test */
		R_EnableAlphaTest(qtrue);
	} else {
		R_EnableAlphaTest(qfalse);
	}

	surface_warp = 0;
	if (surf->texinfo->flags & SURF_WARP) {  /* warping */
		surface_warp = refdef.time / 8.0;
	}

	surface_flow = 0;
	if (surf->texinfo->flags & SURF_FLOWING) {  /* flowing */
		surface_flow = refdef.time / -4.0;
		surface_warp = surface_warp / 2.0;
	}

	image = surf->texinfo->image;

	if (r_state.multitexture_enabled) {  /* bind diffuse and lightmap */
		R_BindMultitexture(&r_state.texture_texunit, image->texnum,
			&r_state.lightmap_texunit, r_state.lightmap_texnum + surf->lightmaptexturenum);
	} else {  /* just bind diffuse */
		R_Bind(image->texnum);
	}
}

/**
 * @brief Use the vertex, texture and normal arrays to draw a surface
 * @sa R_DrawSurfaces
 */
static void R_DrawSurface (const mBspSurface_t *surf)
{
	int i, j, k, nv;
	float *v;
	vec3_t norm;

	nv = surf->polys->numverts;
	for (i = 0, v = surf->polys->verts[0]; i < nv; i++, v += VERTEXSIZE) {
		j = i * 2;
		k = i * 3;

		/* diffuse coords */
		r_state.texture_texunit.texcoords[j + 0] = v[3] + surface_flow;
		r_state.texture_texunit.texcoords[j + 1] = v[4];

		if (r_state.warp_enabled) {  /* warp coords */
			r_state.lightmap_texunit.texcoords[j + 0] = v[3] + surface_flow + surface_warp;
			r_state.lightmap_texunit.texcoords[j + 1] = v[4] + surface_warp;
		} else if (r_state.multitexture_enabled) {  /* lightmap coords */
			r_state.lightmap_texunit.texcoords[j + 0] = v[5];
			r_state.lightmap_texunit.texcoords[j + 1] = v[6];
		}

		/* vertex */
		memcpy(&r_state.vertex_array_3d[k], v, sizeof(vec3_t));

		/* normal vector for lights */
		if (r_state.lighting_enabled) {
			if (surf->flags & SURF_PLANEBACK)
				VectorNegate(surf->plane->normal, norm);
			else
				VectorCopy(surf->plane->normal, norm);

			memcpy(&r_state.normal_array[k], norm, sizeof(vec3_t));
		}
	}

	qglDrawArrays(GL_POLYGON, 0, i);
	R_CheckError();

	c_brush_polys++;
}

/**
 * @brief General surface drawing function, that draw the surface chains
 * @note The needed states for the surfaces must be set before you call this
 * @sa R_DrawSurface
 * @sa R_SetSurfaceState
 */
static void R_DrawSurfaces (const mBspSurface_t *surfs)
{
	const mBspSurface_t *surf;

	for (surf = surfs; surf; surf = surf->next) {
		R_SetSurfaceState(surf);
		R_DrawSurface(surf);
	}

	R_Color(color_white);
}

/**
 * @brief Draw the surface chain with multitexture enabled and blend enabled
 * @sa R_DrawOpaqueSurfaces
 */
void R_DrawAlphaSurfaces (const mBspSurface_t *surfs)
{
	if (!surfs)
		return;

	assert(r_state.blend_enabled);
	R_EnableMultitexture(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableMultitexture(qfalse);
}

/**
 * @brief Draw the surface chain with multitexture enabled and light enabled
 * @sa R_DrawAlphaSurfaces
 */
void R_DrawOpaqueSurfaces (const mBspSurface_t *surfs)
{
	if (!surfs)
		return;

	R_EnableMultitexture(qtrue);

	R_EnableLighting(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableLighting(qfalse);

	R_EnableMultitexture(qfalse);
}

/**
 * @brief Draw the surfaces via warp shader
 * @sa R_DrawAlphaWarpSurfaces
 */
void R_DrawOpaqueWarpSurfaces (mBspSurface_t *surfs)
{
	if (!surfs)
		return;

	R_EnableWarp(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableWarp(qfalse);
}

/**
 * @brief Draw the alpha surfaces via warp shader and with blend enabled
 * @sa R_DrawOpaqueWarpSurfaces
 */
void R_DrawAlphaWarpSurfaces (mBspSurface_t *surfs)
{
	if (!surfs)
		return;

	assert(r_state.blend_enabled);
	R_EnableWarp(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableWarp(qfalse);
}

void R_CreateSurfacePoly (mBspSurface_t *surf, int shift[3], model_t *mod)
{
	int i, lindex, lnumverts;
	mBspEdge_t *pedges, *r_pedge;
	int vertpage;
	float *vec;
	float s, t;
	mBspPoly_t *poly;
	vec3_t total;

	/* reconstruct the polygon */
	pedges = mod->bsp.edges;
	lnumverts = surf->numedges;
	vertpage = 0;

	VectorClear(total);

	/* draw texture */
	poly = VID_TagAlloc(vid_modelPool, sizeof(mBspPoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float), 0);
	poly->next = surf->polys;
	surf->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = mod->bsp.surfedges[surf->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = mod->bsp.vertexes[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = mod->bsp.vertexes[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		s /= surf->texinfo->image->width;

		t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t /= surf->texinfo->image->height;

		VectorAdd(total, vec, total);
		VectorAdd(vec, shift, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		/* lightmap texture coordinates */
		s = DotProduct(vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		s -= surf->stmins[0];
		s += surf->light_s << surf->lquant;
		s += 1 << (surf->lquant - 1);
		s /= LIGHTMAP_BLOCK_WIDTH << surf->lquant;

		t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t -= surf->stmins[1];
		t += surf->light_t << surf->lquant;
		t += 1 << (surf->lquant - 1);
		t /= LIGHTMAP_BLOCK_HEIGHT << surf->lquant;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}
