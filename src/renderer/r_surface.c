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

static vec3_t modelorg;			/* relative to viewpoint */

mBspSurface_t *r_alpha_surfaces;
mBspSurface_t *r_material_surfaces;

/*
=============================================================
BRUSH MODELS
=============================================================
*/

/**
 * @param[in] scroll != 0 for SURF_FLOWING
 */
static void R_DrawPoly (const mBspSurface_t * fa, const float scroll)
{
	int i;
	float *v;
	mBspPoly_t *p = fa->polys;

	qglBegin(GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		qglTexCoord2f(v[3] + scroll, v[4]);
		qglVertex3fv(v);
	}
	qglEnd();
}

/**
 * @brief Used for flowing and non-flowing surfaces
 * @param[in] scroll != 0 for SURF_FLOWING
 * @sa R_DrawSurface
 */
static void R_DrawPolyChain (const mBspSurface_t *surf, const float scroll)
{
	int i, nv = surf->polys->numverts;
	float *v;
	mBspPoly_t *p;

	assert(surf->polys);
	for (p = surf->polys; p; p = p->chain) {
		v = p->verts[0];
		qglBegin(GL_POLYGON);
		for (i = 0; i < nv; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fARB(gl_texture0, (v[3] + scroll), v[4]);
			qglMultiTexCoord2fARB(gl_texture1, v[5], v[6]);
			qglVertex3fv(v);
		}
		qglEnd();
	}
}

static void R_RenderBrushPoly (mBspSurface_t * fa)
{
	c_brush_polys++;

	if (fa->flags & SURF_DRAWTURB) {
		vec4_t color = {r_state.inverse_intensity, r_state.inverse_intensity, r_state.inverse_intensity, 1.0f};
		R_Bind(fa->texinfo->image->texnum);

		/* warp texture, no lightmaps */
		R_TexEnv(GL_MODULATE);
		R_Color(color);
		R_DrawTurbSurface(fa);
		R_TexEnv(GL_REPLACE);
		return;
	} else {
		R_Bind(fa->texinfo->image->texnum);
		R_TexEnv(GL_REPLACE);
	}

	if (fa->texinfo->flags & SURF_FLOWING) {
		float scroll;
		scroll = -64 * ((refdef.time / 40.0) - (int) (refdef.time / 40.0));
		if (scroll == 0.0)
			scroll = -64.0;
		R_DrawPoly(fa, scroll);
	} else
		R_DrawPoly(fa, 0.0f);

	fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
	gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
}


/**
 * @brief Draw water surfaces and windows.
 * The BSP tree is waled front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void R_DrawAlphaSurfaces (mBspSurface_t *list)
{
	mBspSurface_t *s;
	float intens;
	vec4_t color = {1, 1, 1, 1};

	/* go back to the world matrix */
	qglLoadMatrixf(r_world_matrix);

	RSTATE_ENABLE_BLEND
	qglDepthMask(GL_FALSE); /* disable depth writing */
	R_TexEnv(GL_MODULATE);

	/* the textures are prescaled up for a better lighting range,
	 * so scale it back down */
	intens = r_state.inverse_intensity;

	for (s = list; s; s = s->texturechain) {
		R_Bind(s->texinfo->image->texnum);
		c_brush_polys++;

		if (s->texinfo->flags & SURF_TRANS33)
			color[3] = 0.33;
		else if (s->texinfo->flags & SURF_TRANS66)
			color[3] = 0.66;
		else
			color[3] = 1.0;

		R_Color(color);

		if (s->flags & SURF_DRAWTURB)
			R_DrawTurbSurface(s);
		else if (s->texinfo->flags & SURF_FLOWING) {
			float scroll;
			scroll = -64 * ((refdef.time / 40.0) - (int) (refdef.time / 40.0));
			if (scroll == 0.0)
				scroll = -64.0;
			R_DrawPoly(s, scroll);
		} else
			R_DrawPoly(s, 0.0f);
	}

	R_TexEnv(GL_REPLACE);
	qglDepthMask(GL_TRUE); /* reenable depth writing */
	R_ColorBlend(NULL);
}

/**
 * @brief Draw the lightmapped surface
 * @note If multitexturing is not supported this function will only draw the
 * surface without any lightmap
 * @sa R_RenderBrushPoly
 * @sa R_DrawPolyChain
 * @sa R_DrawWorld
 */
static void R_DrawSurface (mBspSurface_t * surf)
{
	unsigned lmtex = surf->lightmaptexturenum;

	/* no multitexturing supported - draw the poly now and blend the lightmap
	 * later in R_DrawWorld */
	if (!qglMultiTexCoord2fARB) {
		R_RenderBrushPoly(surf);
		return;
	}

	if (surf->texinfo->flags & SURF_ALPHATEST)
		RSTATE_ENABLE_ALPHATEST

	c_brush_polys++;

	R_MBind(gl_texture0, surf->texinfo->image->texnum);
	R_MBind(gl_texture1, r_state.lightmap_texnum + lmtex);

	if (surf->texinfo->flags & SURF_FLOWING) {
		float scroll;

		scroll = -64 * ((refdef.time / 40.0) - (int) (refdef.time / 40.0));
		if (scroll == 0.0)
			scroll = -64.0;

		R_DrawPolyChain(surf, scroll);
	} else {
		R_DrawPolyChain(surf, 0.0f);
	}

	RSTATE_DISABLE_ALPHATEST
}

#define BACKFACE_EPSILON    0.01

/**
 * @brief
 * @sa R_DrawBrushModel
 */
static void R_DrawInlineBrushModel (void)
{
	int i;
	cBspPlane_t *pplane;
	float dot;
	mBspSurface_t *psurf, *s;
	qboolean duplicate = qfalse;

	psurf = &currentmodel->bsp.surfaces[currentmodel->bsp.firstmodelsurface];

	if (currententity->flags & RF_TRANSLUCENT) {
		vec4_t color = {1, 1, 1, 0.25};
		R_ColorBlend(color);
		R_TexEnv(GL_MODULATE);
	}

	/* draw texture */
	for (i = 0; i < currentmodel->bsp.nummodelsurfaces; i++, psurf++) {
		/* find which side of the node we are on */
		pplane = psurf->plane;

		if (r_isometric->integer)
			dot = -DotProduct(vpn, pplane->normal);
		else
			dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		/* draw the polygon */
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			/* add to the translucent chain */
			if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
				/* if bmodel is used by multiple entities, adding surface
				 * to linked list more than once would result in an infinite loop */
				for (s = r_alpha_surfaces; s; s = s->texturechain)
					if (s == psurf) {
						duplicate = qtrue;
						break;
					}
				/* Don't allow surface to be added twice (fixes hang) */
				if (!duplicate) {
					psurf->texturechain = r_alpha_surfaces;
					r_alpha_surfaces = psurf;
				}
			} else if (!(psurf->flags & SURF_DRAWTURB)) {
				R_DrawSurface(psurf);
			} else {
				R_EnableMultitexture(qfalse);
				R_RenderBrushPoly(psurf);
				R_EnableMultitexture(qtrue);
			}
		}
	}

	if (!(currententity->flags & RF_TRANSLUCENT)) {
		if (!qglMultiTexCoord2fARB)
			R_BlendLightmaps();
	} else {
		R_ColorBlend(NULL);
		R_TexEnv(GL_REPLACE);
	}
}

/**
 * @brief Draws a brush model
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (entity_t * e)
{
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

/*	Com_DPrintf(DEBUG_RENDERER, "Brush model %i!\n", currentmodel->bsp.nummodelsurfaces);*/

	if (currentmodel->bsp.nummodelsurfaces == 0)
		return;

	currententity = e;
	r_state.currenttextures[0] = r_state.currenttextures[1] = -1;

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

	R_Color(NULL);
	memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	VectorSubtract(refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
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

	R_EnableMultitexture(qtrue);
	R_SelectTexture(gl_texture0);
	R_TexEnv(GL_REPLACE);
	R_SelectTexture(gl_texture1);
	R_TexEnv(GL_MODULATE);

	R_DrawInlineBrushModel();
	R_EnableMultitexture(qfalse);

	qglPopMatrix();
	R_CheckError();
}


/*
=============================================================
WORLD MODEL
=============================================================
*/

/**
 * @sa R_DrawWorld
 */
static void R_RecursiveWorldNode (mBspNode_t * node)
{
	int c, side, sidebit;
	cBspPlane_t *plane;
	mBspSurface_t *surf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;					/* solid */

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	/* if a leaf node, draw stuff */
	if (node->contents != LEAFNODE)
		return;

	/* node is just a decision point, so go down the apropriate sides */
	/* find which side of the node we are on */
	plane = node->plane;

	if (r_isometric->integer) {
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
	for (c = node->numsurfaces, surf = currentmodel->bsp.surfaces + node->firstsurface; c; c--, surf++) {
		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;			/* wrong side */

		if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
			/* add to the translucent chain */
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		} else {
			if (!(surf->flags & SURF_DRAWTURB))
				R_DrawSurface(surf);
		}
		/* add to the material chain if appropriate */
		if (surf->texinfo->image->material.flags & STAGE_RENDER) {
			surf->material_next = r_material_surfaces;
			r_material_surfaces = surf;
		}
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side]);
}


/**
 * @sa R_RecursiveWorldNode
 * @sa R_DrawLevelBrushes
 * @todo Batch all the static surfaces from each of the 8 levels in a surface list
 * and only recurse down when the level changed
 */
static void R_DrawWorld (mBspNode_t * nodes)
{
	entity_t ent;

	VectorCopy(refdef.vieworg, modelorg);

	/* auto cycle the world frame for texture animation */
	memset(&ent, 0, sizeof(ent));
	ent.as.frame = (int) (refdef.time * 2);
	currententity = &ent;

	r_state.currenttextures[0] = r_state.currenttextures[1] = -1;

	R_Color(NULL);
	memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	if (qglMultiTexCoord2fARB) {
		R_EnableMultitexture(qtrue);

		R_SelectTexture(gl_texture0);
		R_TexEnv(GL_REPLACE);
		R_SelectTexture(gl_texture1);

		if (r_config.envCombine) {
			R_TexEnv(r_config.envCombine);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, r_intensity->value);
		} else {
			R_TexEnv(GL_MODULATE);
		}

		R_RecursiveWorldNode(nodes);
		R_EnableMultitexture(qfalse);
	} else {
		R_RecursiveWorldNode(nodes);
		R_BlendLightmaps();
	}
}


/**
 * @sa R_DrawLevelBrushes
 */
static void R_FindModelNodes_r (mBspNode_t * node)
{
	if (!node->plane) {
		R_FindModelNodes_r(node->children[0]);
		R_FindModelNodes_r(node->children[1]);
	} else {
		R_DrawWorld(node);
	}
}


/**
 * @brief Draws the brushes for the current worldlevel and hide other levels
 * @sa cvar cl_worldlevel
 * @sa R_FindModelNodes_r
 */
void R_DrawLevelBrushes (void)
{
	entity_t ent;
	int i, tile, mask;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!r_drawworld->integer)
		return;

	r_alpha_surfaces = NULL;
	r_material_surfaces = NULL;

	memset(&ent, 0, sizeof(ent));
	currententity = &ent;

	mask = 1 << refdef.worldlevel;

	for (tile = 0; tile < rNumTiles; tile++) {
		currentmodel = rTiles[tile];

		/* don't draw weapon-, actorclip and stepon */
		/* @note Change this to 258 to see the actorclip brushes in-game */
		for (i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			/* check the worldlevel flags */
			if (i && !(i & mask))
				continue;

			if (!currentmodel->bsp.submodels[i].numfaces)
				continue;

			R_FindModelNodes_r(currentmodel->bsp.nodes + currentmodel->bsp.submodels[i].headnode);
		}
	}
}

void R_BuildPolygonFromSurface (mBspSurface_t * fa, int shift[3])
{
	int i, lindex, lnumverts;
	mBspEdge_t *pedges, *r_pedge;
	int vertpage;
	float *vec;
	float s, t;
	mBspPoly_t *poly;
	vec3_t total;

	/* reconstruct the polygon */
	pedges = currentmodel->bsp.edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	VectorClear(total);

	/* draw texture */
	poly = VID_TagAlloc(vid_modelPool, sizeof(mBspPoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float), 0);
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->bsp.surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = currentmodel->bsp.vertexes[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = currentmodel->bsp.vertexes[r_pedge->v[1]].position;
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
		s /= BLOCK_WIDTH << fa->lquant;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t << fa->lquant;
		t += 1 << (fa->lquant - 1);
		t /= BLOCK_HEIGHT << fa->lquant;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}
