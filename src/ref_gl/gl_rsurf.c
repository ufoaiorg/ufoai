/**
 * @file gl_rsurf.c
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

#include "gl_local.h"

static vec3_t modelorg;			/* relative to viewpoint */

msurface_t *r_alpha_surfaces;

#define DYNAMIC_LIGHT_WIDTH  256
#define DYNAMIC_LIGHT_HEIGHT 256

#define LIGHTMAP_BYTES 4

#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

#define	MAX_LIGHTMAPS	256

int c_visible_lightmaps;
int c_visible_textures;

#define GL_LIGHTMAP_FORMAT GL_RGBA

typedef struct {
	int internal_format;
	int current_lightmap_texture;

	msurface_t *lightmap_surfaces[MAX_LIGHTMAPS];

	int allocated[BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in */
	/* main memory so texsubimage can update properly */
	byte lightmap_buffer[4 * BLOCK_WIDTH * BLOCK_HEIGHT];
} gllightmapstate_t;

static gllightmapstate_t gl_lms;


static void LM_InitBlock(void);
static void LM_UploadBlock(qboolean dynamic);
static qboolean LM_AllocBlock(int w, int h, int *x, int *y);

extern void R_SetCacheState(msurface_t * surf);
extern void R_BuildLightMap(msurface_t * surf, byte * dest, int stride);

/*
=============================================================
BRUSH MODELS
=============================================================
*/

/**
 * @brief Returns the proper texture for a given time and base texture
 */
static image_t *R_TextureAnimation(mtexinfo_t * tex)
{
	int c;

	if (!tex->next)
		return tex->image;

	c = currententity->as.frame % tex->numframes;
	while (c) {
		tex = tex->next;
		c--;
	}

	return tex->image;
}

/**
 * @brief
 */
static void DrawGLPoly(glpoly_t * p)
{
	int i;
	float *v;

	qglBegin(GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		qglTexCoord2f(v[3], v[4]);
		qglVertex3fv(v);
	}
	qglEnd();
}

/**
 * @brief version of DrawGLPoly that handles scrolling texture
 */
static void DrawGLFlowingPoly(msurface_t * fa)
{
	int i;
	float *v;
	glpoly_t *p;
	float scroll;

	p = fa->polys;

	scroll = -64 * ((r_newrefdef.time / 40.0) - (int) (r_newrefdef.time / 40.0));
	if (scroll == 0.0)
		scroll = -64.0;

	qglBegin(GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		qglTexCoord2f((v[3] + scroll), v[4]);
		qglVertex3fv(v);
	}
	qglEnd();
}

/**
 * @brief
 */
void R_DrawTriangleOutlines(void)
{
	int i, j;
	glpoly_t *p;

	if (!gl_showtris->value)
		return;

	qglDisable(GL_TEXTURE_2D);
	qglDisable(GL_DEPTH_TEST);
	qglColor4f(1, 1, 1, 1);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		msurface_t *surf;

		for (surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain) {
			p = surf->polys;
			for (; p; p = p->chain) {
				for (j = 2; j < p->numverts; j++) {
					qglBegin(GL_LINE_STRIP);
					qglVertex3fv(p->verts[0]);
					qglVertex3fv(p->verts[j - 1]);
					qglVertex3fv(p->verts[j]);
					qglVertex3fv(p->verts[0]);
					qglEnd();
				}
			}
		}
	}

	qglEnable(GL_DEPTH_TEST);
	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief
 */
static void DrawGLPolyChain(glpoly_t * p, float soffset, float toffset)
{
	if (soffset == 0 && toffset == 0) {
		for (; p != 0; p = p->chain) {
			float *v;
			int j;

			qglBegin(GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				qglTexCoord2f(v[5], v[6]);
				qglVertex3fv(v);
			}
			qglEnd();
		}
	} else {
		for (; p != 0; p = p->chain) {
			float *v;
			int j;

			qglBegin(GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				qglTexCoord2f(v[5] - soffset, v[6] - toffset);
				qglVertex3fv(v);
			}
			qglEnd();
		}
	}
}

/**
 * @brief This routine takes all the given light mapped surfaces in the world and
 * blends them into the framebuffer.
 */
static void R_BlendLightmaps(void)
{
	int i;
	msurface_t *surf, *newdrawsurf = 0;

	/* don't bother if we're set to fullbright */
	if (r_fullbright->value)
		return;
	if (!rTiles[0]->lightdata)
		return;

	/* don't bother writing Z */
	qglDepthMask(0);

	/*
	 ** set the appropriate blending mode unless we're only looking at the
	 ** lightmaps.
	 */
	if (!gl_lightmap->value) {
		qglEnable(GL_BLEND);

		if (gl_saturatelighting->value)
			qglBlendFunc(GL_ONE, GL_ONE);
		else {
			if (gl_monolightmap->string[0] != '0') {
				switch (toupper(gl_monolightmap->string[0])) {
				case 'I':
					qglBlendFunc(GL_ZERO, GL_SRC_COLOR);
					break;
				case 'L':
					qglBlendFunc(GL_ZERO, GL_SRC_COLOR);
					break;
				case 'A':
				default:
					qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					break;
				}
			} else {
				qglBlendFunc(GL_ZERO, GL_SRC_COLOR);
			}
		}
	}

	if (currentmodel == rTiles[0])
		c_visible_lightmaps = 0;

	/* render static lightmaps first */
	for (i = 1; i < MAX_LIGHTMAPS; i++) {
		if (gl_lms.lightmap_surfaces[i]) {
			if (currentmodel == rTiles[0])
				c_visible_lightmaps++;
			GL_Bind(gl_state.lightmap_textures + i);

			for (surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain) {
				if (surf->polys)
					DrawGLPolyChain(surf->polys, 0, 0);
			}
		}
	}

	/* render dynamic lightmaps */
	if (gl_dynamic->value) {
		LM_InitBlock();

		GL_Bind(gl_state.lightmap_textures + 0);

		if (currentmodel == rTiles[0])
			c_visible_lightmaps++;

		newdrawsurf = gl_lms.lightmap_surfaces[0];

		for (surf = gl_lms.lightmap_surfaces[0]; surf != 0; surf = surf->lightmapchain) {
			int smax, tmax;
			byte *base;

			smax = (surf->extents[0] >> surf->lquant) + 1;
			tmax = (surf->extents[1] >> surf->lquant) + 1;

			if (LM_AllocBlock(smax, tmax, &surf->dlight_s, &surf->dlight_t)) {
				base = gl_lms.lightmap_buffer;
				base += (surf->dlight_t * BLOCK_WIDTH + surf->dlight_s) * LIGHTMAP_BYTES;

				R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
			} else {
				msurface_t *drawsurf;

				/* upload what we have so far */
				LM_UploadBlock(qtrue);

				/* draw all surfaces that use this lightmap */
				for (drawsurf = newdrawsurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain) {
					if (drawsurf->polys)
						DrawGLPolyChain(drawsurf->polys,
										(drawsurf->light_s - drawsurf->dlight_s) * (1.0 / BLOCK_WIDTH),
										(drawsurf->light_t - drawsurf->dlight_t) * (1.0 / BLOCK_HEIGHT));
				}

				newdrawsurf = drawsurf;

				/* clear the block */
				LM_InitBlock();

				/* try uploading the block now */
				if (!LM_AllocBlock(smax, tmax, &surf->dlight_s, &surf->dlight_t))
					ri.Sys_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed (dynamic)\n", smax, tmax);

				base = gl_lms.lightmap_buffer;
				base += (surf->dlight_t * BLOCK_WIDTH + surf->dlight_s) * LIGHTMAP_BYTES;

				R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
			}
		}

		/* draw remainder of dynamic lightmaps that haven't been uploaded yet */
		if (newdrawsurf)
			LM_UploadBlock(qtrue);

		for (surf = newdrawsurf; surf != 0; surf = surf->lightmapchain) {
			if (surf->polys)
				DrawGLPolyChain(surf->polys, (surf->light_s - surf->dlight_s) * (1.0 / BLOCK_WIDTH), (surf->light_t - surf->dlight_t) * (1.0 / BLOCK_HEIGHT));
		}
	}

	/* restore state */
	qglDisable(GL_BLEND);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask(1);
}

/**
 * @brief
 */
static void R_RenderBrushPoly(msurface_t * fa)
{
	int maps;
	image_t *image;
	qboolean is_dynamic = qfalse;

	c_brush_polys++;

	image = R_TextureAnimation(fa->texinfo);

	if (fa->flags & SURF_DRAWTURB) {
		GL_Bind(image->texnum);

		/* warp texture, no lightmaps */
		GL_TexEnv(GL_MODULATE);
		qglColor4f(gl_state.inverse_intensity, gl_state.inverse_intensity, gl_state.inverse_intensity, 1.0F);
		EmitWaterPolys(fa);
		GL_TexEnv(GL_REPLACE);
		return;
	} else {
		GL_Bind(image->texnum);
		GL_TexEnv(GL_REPLACE);
	}

	if (fa->texinfo->flags & SURF_FLOWING)
		DrawGLFlowingPoly(fa);
	else
		DrawGLPoly(fa->polys);

	/* check for lightmap modification */
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++) {
		if (r_newrefdef.lightstyles[fa->styles[maps]].white != fa->cached_light[maps])
			goto dynamic;
	}

	/* dynamic this frame or dynamic previously */
	if ((fa->dlightframe == r_framecount)) {
	  dynamic:
		if (gl_dynamic->value) {
			if (!(fa->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
				is_dynamic = qtrue;
		}
	}

	if (is_dynamic) {
		if ((fa->styles[maps] >= 32 || fa->styles[maps] == 0) && (fa->dlightframe != r_framecount)) {
			unsigned temp[34 * 34];
			int smax, tmax;

			smax = (fa->extents[0] >> fa->lquant) + 1;
			tmax = (fa->extents[1] >> fa->lquant) + 1;

			R_BuildLightMap(fa, (void *) temp, smax * 4);
			R_SetCacheState(fa);

			GL_Bind(gl_state.lightmap_textures + fa->lightmaptexturenum);

			qglTexSubImage2D(GL_TEXTURE_2D, 0, fa->light_s, fa->light_t, smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp);

			fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
			gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
		} else {
			fa->lightmapchain = gl_lms.lightmap_surfaces[0];
			gl_lms.lightmap_surfaces[0] = fa;
		}
	} else {
		fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
		gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
	}
}


/**
 * @brief Draw water surfaces and windows.
 * The BSP tree is waled front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void R_DrawAlphaSurfaces(void)
{
	msurface_t *s;
	float intens;

	/* go back to the world matrix */
	qglLoadMatrixf(r_world_matrix);

	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);

	/* the textures are prescaled up for a better lighting range, */
	/* so scale it back down */
	intens = gl_state.inverse_intensity;

	for (s = r_alpha_surfaces; s; s = s->texturechain) {
		GL_Bind(s->texinfo->image->texnum);
		c_brush_polys++;
		if (s->texinfo->flags & SURF_TRANS33)
			qglColor4f(intens, intens, intens, 0.33);
		else if (s->texinfo->flags & SURF_TRANS66)
			qglColor4f(intens, intens, intens, 0.66);
		else
			qglColor4f(intens, intens, intens, 1);
		if (s->flags & SURF_DRAWTURB)
			EmitWaterPolys(s);
		else if (s->texinfo->flags & SURF_FLOWING)
			DrawGLFlowingPoly(s);
		else
			DrawGLPoly(s->polys);
	}

	GL_TexEnv(GL_REPLACE);
	qglColor4f(1, 1, 1, 1);
	qglDisable(GL_BLEND);

	r_alpha_surfaces = NULL;
}

/**
 * @brief
 */
static void GL_RenderLightmappedPoly(msurface_t * surf)
{
	int i, nv = surf->polys->numverts;
	int map;
	float *v;
	image_t *image = R_TextureAnimation(surf->texinfo);
	qboolean is_dynamic = qfalse;
	unsigned lmtex = surf->lightmaptexturenum;
	glpoly_t *p;

	for (map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++) {
		if (r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map])
			goto dynamic;
	}

	/* dynamic this frame or dynamic previously */
	if ((surf->dlightframe == r_framecount)) {
	  dynamic:
		if (gl_dynamic->value) {
			if (!(surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
				is_dynamic = qtrue;
		}
	}

	if (is_dynamic) {
		unsigned temp[BLOCK_WIDTH * BLOCK_HEIGHT];
		int smax, tmax;

		if ((surf->styles[map] >= 32 || surf->styles[map] == 0) && (surf->dlightframe != r_framecount)) {
			smax = (surf->extents[0] >> surf->lquant) + 1;
			tmax = (surf->extents[1] >> surf->lquant) + 1;

			R_BuildLightMap(surf, (void *) temp, smax * 4);
			R_SetCacheState(surf);

			GL_MBind(gl_texture1, gl_state.lightmap_textures + surf->lightmaptexturenum);

			lmtex = surf->lightmaptexturenum;

			qglTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp);

		} else {
			smax = (surf->extents[0] >> surf->lquant) + 1;
			tmax = (surf->extents[1] >> surf->lquant) + 1;

			R_BuildLightMap(surf, (void *) temp, smax * 4);

			GL_MBind(gl_texture1, gl_state.lightmap_textures + 0);

			lmtex = 0;

			qglTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp);

		}

		c_brush_polys++;

		GL_MBind(gl_texture0, image->texnum);
		GL_MBind(gl_texture1, gl_state.lightmap_textures + lmtex);

		if (surf->texinfo->flags & SURF_FLOWING) {
			float scroll;

			scroll = -64 * ((r_newrefdef.time / 40.0) - (int) (r_newrefdef.time / 40.0));
			if (scroll == 0.0)
				scroll = -64.0;

			for (p = surf->polys; p; p = p->chain) {
				v = p->verts[0];
				qglBegin(GL_POLYGON);
				for (i = 0; i < nv; i++, v += VERTEXSIZE) {
					qglMTexCoord2fSGIS(gl_texture0, (v[3] + scroll), v[4]);
					qglMTexCoord2fSGIS(gl_texture1, v[5], v[6]);
					qglVertex3fv(v);
				}
				qglEnd();
			}
		} else {
			for (p = surf->polys; p; p = p->chain) {
				v = p->verts[0];
				qglBegin(GL_POLYGON);
				for (i = 0; i < nv; i++, v += VERTEXSIZE) {
					qglMTexCoord2fSGIS(gl_texture0, v[3], v[4]);
					qglMTexCoord2fSGIS(gl_texture1, v[5], v[6]);
					qglVertex3fv(v);
				}
				qglEnd();
			}
		}
	} else {
		c_brush_polys++;

		GL_MBind(gl_texture0, image->texnum);
		GL_MBind(gl_texture1, gl_state.lightmap_textures + lmtex);

		if (surf->texinfo->flags & SURF_FLOWING) {
			float scroll;

			scroll = -64 * ((r_newrefdef.time / 40.0) - (int) (r_newrefdef.time / 40.0));
			if (scroll == 0.0)
				scroll = -64.0;

			for (p = surf->polys; p; p = p->chain) {
				v = p->verts[0];
				qglBegin(GL_POLYGON);
				for (i = 0; i < nv; i++, v += VERTEXSIZE) {
					qglMTexCoord2fSGIS(gl_texture0, (v[3] + scroll), v[4]);
					qglMTexCoord2fSGIS(gl_texture1, v[5], v[6]);
					qglVertex3fv(v);
				}
				qglEnd();
			}
		} else {
			for (p = surf->polys; p; p = p->chain) {
				v = p->verts[0];
				qglBegin(GL_POLYGON);
				for (i = 0; i < nv; i++, v += VERTEXSIZE) {
					qglMTexCoord2fSGIS(gl_texture0, v[3], v[4]);
					qglMTexCoord2fSGIS(gl_texture1, v[5], v[6]);
					qglVertex3fv(v);
				}
				qglEnd();
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawInlineBModel(void)
{
	int i, k;
	cplane_t *pplane;
	float dot;
	msurface_t *psurf;
	dlight_t *lt;

	/* calculate dynamic lighting for bmodel */
	if (!gl_flashblend->value) {
		lt = r_newrefdef.dlights;
		for (k = 0; k < r_newrefdef.num_dlights; k++, lt++)
			R_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
	}

	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

	if (currententity->flags & RF_TRANSLUCENT) {
		qglEnable(GL_BLEND);
		qglColor4f(1, 1, 1, 0.25);
		GL_TexEnv(GL_MODULATE);
	}

	/* set r_alpha_surfaces = NULL to ensure psurf->texturechain terminates */
	r_alpha_surfaces = NULL;

	/* draw texture */
	for (i = 0; i < currentmodel->nummodelsurfaces; i++, psurf++) {
		/* find which side of the node we are on */
		pplane = psurf->plane;

		if (r_isometric->value)
			dot = -DotProduct(vpn, pplane->normal);
		else
			dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		/* draw the polygon */
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			/* add to the translucent chain */
			if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
				psurf->texturechain = r_alpha_surfaces;
				r_alpha_surfaces = psurf;
			} else if (qglMTexCoord2fSGIS && !(psurf->flags & SURF_DRAWTURB)) {
				GL_RenderLightmappedPoly(psurf);
			} else {
				GL_EnableMultitexture(qfalse);
				R_RenderBrushPoly(psurf);
				GL_EnableMultitexture(qtrue);
			}
		}
	}

	if (!(currententity->flags & RF_TRANSLUCENT)) {
		if (!qglMTexCoord2fSGIS)
			R_BlendLightmaps();
	} else {
		qglDisable(GL_BLEND);
		qglColor4f(1, 1, 1, 1);
		GL_TexEnv(GL_REPLACE);
	}
}

/**
 * @brief
 */
void R_DrawBrushModel(entity_t * e)
{
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

/*	Com_Printf( "Brush model %i!\n", currentmodel->nummodelsurfaces ); */

	if (currentmodel->nummodelsurfaces == 0)
		return;

	currententity = e;
	gl_state.currenttextures[0] = gl_state.currenttextures[1] = -1;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = qtrue;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	} else {
		rotated = qfalse;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	qglColor3f(1, 1, 1);
	memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	VectorSubtract(r_newrefdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t temp;
		vec3_t forward, right, up;

		FastVectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	qglPushMatrix();
	e->angles[0] = -e->angles[0];	/* stupid quake bug */
	e->angles[2] = -e->angles[2];	/* stupid quake bug */
	R_RotateForEntity(e);
	e->angles[0] = -e->angles[0];	/* stupid quake bug */
	e->angles[2] = -e->angles[2];	/* stupid quake bug */

	GL_EnableMultitexture(qtrue);
	GL_SelectTexture(gl_texture0);
	GL_TexEnv(GL_REPLACE);
	GL_SelectTexture(gl_texture1);
	GL_TexEnv(GL_MODULATE);

	R_DrawInlineBModel();
	GL_EnableMultitexture(qfalse);

	qglPopMatrix();
}


/*
=============================================================
WORLD MODEL
=============================================================
*/

/**
 * @brief
 * @sa R_DrawWorld
 */
static void R_RecursiveWorldNode(mnode_t * node)
{
	int c, side, sidebit;
	cplane_t *plane;
	msurface_t *surf;

/*	msurface_t	**mark; */
/*	mleaf_t		*pleaf; */
	float dot;
	image_t *image;

	if (node->contents == CONTENTS_SOLID)
		return;					/* solid */

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	/* if a leaf node, draw stuff */
	if (node->contents != -1)
		return;

	/* node is just a decision point, so go down the apropriate sides */
	/* find which side of the node we are on */
	plane = node->plane;

	if (r_isometric->value) {
		dot = -DotProduct(vpn, plane->normal);
	} else if (plane->type >= 3) {
		dot = DotProduct(modelorg, plane->normal) - plane->dist;
	} else {
		dot = modelorg[plane->type] - plane->dist;
	}

	if (dot >= 0) {
		side = 0;
		sidebit = 0;
	} else {
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	R_RecursiveWorldNode(node->children[side]);

	/* draw stuff */
	for (c = node->numsurfaces, surf = currentmodel->surfaces + node->firstsurface; c; c--, surf++) {
		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;			/* wrong side */

		/* add to the translucent chain */
		if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		} else {
			if (qglMTexCoord2fSGIS && !(surf->flags & SURF_DRAWTURB))
				GL_RenderLightmappedPoly(surf);
			else {
				/* the polygon is visible, so add it to the texture */
				/* sorted chain */
				/* FIXME: this is a hack for animation */
				image = R_TextureAnimation(surf->texinfo);
				surf->texturechain = image->texturechain;
				image->texturechain = surf;
			}
		}
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side]);
}


/**
 * @brief
 * @sa R_RecursiveWorldNode
 */
static void R_DrawWorld(mnode_t * nodes)
{
	entity_t ent;

	if (!r_drawworld->value)
		return;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	FastVectorCopy(r_newrefdef.vieworg, modelorg);

	/* auto cycle the world frame for texture animation */
	memset(&ent, 0, sizeof(ent));
	ent.as.frame = (int) (r_newrefdef.time * 2);
	currententity = &ent;

	gl_state.currenttextures[0] = gl_state.currenttextures[1] = -1;

	qglColor3f(1, 1, 1);
	memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	if (qglMTexCoord2fSGIS) {
		GL_EnableMultitexture(qtrue);

		GL_SelectTexture(gl_texture0);
		GL_TexEnv(GL_REPLACE);
		GL_SelectTexture(gl_texture1);

		if (gl_combine) {
			GL_TexEnv(gl_combine);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, intensity->value);
		} else {
			GL_TexEnv(GL_MODULATE);
		}

		R_RecursiveWorldNode(nodes);

		GL_EnableMultitexture(qfalse);
	} else {
		R_RecursiveWorldNode(nodes);
	}
}


/**
 * @brief
 */
static void R_FindModelNodes_r(mnode_t * node)
{
	if (!node->plane) {
		R_FindModelNodes_r(node->children[0]);
		R_FindModelNodes_r(node->children[1]);
	} else {
		R_DrawWorld(node);
	}
}


/**
 * @brief Draws the brushes for the current worldlevel
 * @sa cvar cl_worldlevel
 */
void R_DrawLevelBrushes(void)
{
	entity_t ent;
	int i, tile, mask;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	memset(&ent, 0, sizeof(ent));
	currententity = &ent;

	mask = 1 << (int) r_newrefdef.worldlevel;

	for (tile = 0; tile < rNumTiles; tile++) {
		currentmodel = rTiles[tile];

		for (i = 0; i < 256; i++) {
			if (i && !(i & mask))
				continue;

			if (!currentmodel->submodels[i].numfaces)
				continue;

			R_FindModelNodes_r(currentmodel->nodes + currentmodel->submodels[i].headnode);
		}
	}
}



/*
=============================================================================
LIGHTMAP ALLOCATION
=============================================================================
*/

/**
 * @brief
 */
static void LM_InitBlock(void)
{
	memset(gl_lms.allocated, 0, sizeof(gl_lms.allocated));
}

/**
 * @brief
 */
static void LM_UploadBlock(qboolean dynamic)
{
	int texture;
	int height = 0;

	if (dynamic)
		texture = 0;
	else
		texture = gl_lms.current_lightmap_texture;

	GL_Bind(gl_state.lightmap_textures + texture);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (dynamic) {
		int i;

		for (i = 0; i < BLOCK_WIDTH; i++) {
			if (gl_lms.allocated[i] > height)
				height = gl_lms.allocated[i];
		}

		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, height, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer);
	} else {
		qglTexImage2D(GL_TEXTURE_2D, 0, gl_lms.internal_format, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer);
		if (++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS)
			ri.Sys_Error(ERR_DROP, "LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n");
	}
}

/**
 * @brief returns a texture number and the position inside it
 */
static qboolean LM_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;

	best = BLOCK_HEIGHT;

	for (i = 0; i < BLOCK_WIDTH - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (gl_lms.allocated[i + j] >= best)
				break;
			if (gl_lms.allocated[i + j] > best2)
				best2 = gl_lms.allocated[i + j];
		}
		/* this is a valid spot */
		if (j == w) {
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
		return qfalse;

	for (i = 0; i < w; i++)
		gl_lms.allocated[*x + i] = best + h;

	return qtrue;
}

/**
 * @brief
 */
void GL_BuildPolygonFromSurface(msurface_t * fa, int shift[3])
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	int vertpage;
	float *vec;
	float s, t;
	glpoly_t *poly;
	vec3_t total;

	/* reconstruct the polygon */
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	VectorClear(total);

	/* draw texture */
	poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->image->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->image->height;

		VectorAdd(total, vec, total);
		VectorAdd(vec, shift, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		/* lightmap texture coordinates */
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s << fa->lquant;
		s += 1 << (fa->lquant - 1);
		s /= BLOCK_WIDTH << fa->lquant;	/*fa->texinfo->texture->width; */

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t << fa->lquant;
		t += 1 << (fa->lquant - 1);
		t /= BLOCK_HEIGHT << fa->lquant;	/*fa->texinfo->texture->height; */

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}

/**
 * @brief
 */
void GL_CreateSurfaceLightmap(msurface_t * surf)
{
	int smax, tmax;
	byte *base;

	if (surf->flags & SURF_DRAWTURB)
		return;

	smax = (surf->extents[0] >> surf->lquant) + 1;
	tmax = (surf->extents[1] >> surf->lquant) + 1;

	if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		LM_UploadBlock(qfalse);
		LM_InitBlock();
		if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
			ri.Sys_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax);
	}

	surf->lightmaptexturenum = gl_lms.current_lightmap_texture;

	base = gl_lms.lightmap_buffer;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	R_SetCacheState(surf);
	R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
}


/**
 * @brief
 */
void GL_BeginBuildingLightmaps(void)
{
	static lightstyle_t lightstyles[MAX_LIGHTSTYLES];
	int i;
	unsigned dummy[BLOCK_WIDTH * BLOCK_HEIGHT];

	memset(gl_lms.allocated, 0, sizeof(gl_lms.allocated));

	r_framecount = 1;			/* no dlightcache */

	GL_EnableMultitexture(qtrue);
	GL_SelectTexture(gl_texture1);

	/*
	 ** setup the base lightstyles so the lightmaps won't have to be regenerated
	 ** the first time they're seen
	 */
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}
	r_newrefdef.lightstyles = lightstyles;

	if (!gl_state.lightmap_textures)
		gl_state.lightmap_textures = TEXNUM_LIGHTMAPS;

	gl_lms.current_lightmap_texture = 1;

	/*
	 ** if mono lightmaps are enabled and we want to use alpha
	 ** blending (a,1-a) then we're likely running on a 3DLabs
	 ** Permedia2.  In a perfect world we'd use a GL_ALPHA lightmap
	 ** in order to conserve space and maximize bandwidth, however
	 ** this isn't a perfect world.
	 **
	 ** So we have to use alpha lightmaps, but stored in GL_RGBA format,
	 ** which means we only get 1/16th the color resolution we should when
	 ** using alpha lightmaps.  If we find another board that supports
	 ** only alpha lightmaps but that can at least support the GL_ALPHA
	 ** format then we should change this code to use real alpha maps.
	 */
	if (toupper(gl_monolightmap->string[0]) == 'A') {
		gl_lms.internal_format = gl_alpha_format;
		/* try to do hacked colored lighting with a blended texture */
	} else if (toupper(gl_monolightmap->string[0]) == 'C') {
		gl_lms.internal_format = gl_alpha_format;
	} else if (toupper(gl_monolightmap->string[0]) == 'I') {
		gl_lms.internal_format = GL_INTENSITY8;
	} else if (toupper(gl_monolightmap->string[0]) == 'L') {
		gl_lms.internal_format = GL_LUMINANCE8;
	} else {
		gl_lms.internal_format = gl_solid_format;
	}

	/* initialize the dynamic lightmap texture */
	GL_Bind(gl_state.lightmap_textures + 0);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D(GL_TEXTURE_2D, 0, gl_lms.internal_format, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, dummy);
}

/**
 * @brief
 */
void GL_EndBuildingLightmaps(void)
{
	LM_UploadBlock(qfalse);
	GL_EnableMultitexture(qfalse);
/*	ri.Con_Printf( PRINT_ALL, "lightmaps: %i\n", gl_lms.current_lightmap_texture ); */
}
