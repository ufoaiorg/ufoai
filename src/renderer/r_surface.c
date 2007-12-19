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
static model_t *currentmodel;

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
static void R_DrawPoly (const mBspSurface_t *surf, const float scroll)
{
	int i;
	float *v;
	mBspPoly_t *p = surf->polys;

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
			qglMultiTexCoord2f(GL_TEXTURE0_ARB, (v[3] + scroll), v[4]);
			qglMultiTexCoord2f(GL_TEXTURE1_ARB, v[5], v[6]);
			qglVertex3fv(v);
		}
		qglEnd();
	}
}

static void R_RenderBrushPoly (mBspSurface_t *surf)
{
	c_brush_polys++;

	R_Bind(surf->texinfo->image->texnum);

	if (surf->flags & SURF_WARP) {
		/* warp texture, no lightmaps */
		R_TexEnv(GL_MODULATE);
		R_DrawTurbSurface(surf);
		R_TexEnv(GL_REPLACE);
		return;
	}

	if (surf->texinfo->flags & SURF_FLOWING) {
		float scroll;
		scroll = -64 * ((refdef.time / 40.0) - (int) (refdef.time / 40.0));
		if (scroll == 0.0)
			scroll = -64.0;
		R_DrawPoly(surf, scroll);
	} else
		R_DrawPoly(surf, 0.0f);
}


/**
 * @brief Draw water surfaces and windows.
 * The BSP tree is waled front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void R_DrawAlphaSurfaces (mBspSurface_t *list)
{
	mBspSurface_t *surf;
	vec4_t color = {1, 1, 1, 1};

	/* go back to the world matrix */
	qglLoadMatrixf(r_world_matrix);

	R_EnableBlend(qtrue);
	qglDepthMask(GL_FALSE); /* disable depth writing */
	R_TexEnv(GL_MODULATE);

	for (surf = list; surf; surf = surf->texturechain) {
		R_Bind(surf->texinfo->image->texnum);
		c_brush_polys++;

		if (surf->texinfo->flags & SURF_TRANS33)
			color[3] = 0.33;
		else if (surf->texinfo->flags & SURF_TRANS66)
			color[3] = 0.66;
		else
			color[3] = 1.0;

		R_Color(color);

		if (surf->flags & SURF_WARP)
			R_DrawTurbSurface(surf);
		else if (surf->texinfo->flags & SURF_FLOWING) {
			float scroll;
			scroll = -64 * ((refdef.time / 40.0) - (int) (refdef.time / 40.0));
			if (scroll == 0.0)
				scroll = -64.0;
			R_DrawPoly(surf, scroll);
		} else
			R_DrawPoly(surf, 0.0f);
	}

	R_TexEnv(GL_REPLACE);
	qglDepthMask(GL_TRUE); /* reenable depth writing */
	R_ColorBlend(NULL);
}

/**
 * @brief Draw the lightmapped surface
 * @sa R_RenderBrushPoly
 * @sa R_DrawPolyChain
 * @sa R_DrawWorld
 */
static void R_DrawSurface (mBspSurface_t *surf)
{
	unsigned lmtex = surf->lightmaptexturenum;

	if (surf->texinfo->flags & SURF_ALPHATEST)
		R_EnableAlphaTest(qtrue);

	c_brush_polys++;

	R_MBind(GL_TEXTURE0_ARB, surf->texinfo->image->texnum);
	R_MBind(GL_TEXTURE1_ARB, r_state.lightmap_texnum + lmtex);

	if (surf->texinfo->flags & SURF_FLOWING) {
		float scroll;

		scroll = -64 * ((refdef.time / 40.0) - (int) (refdef.time / 40.0));
		if (scroll == 0.0)
			scroll = -64.0;

		R_DrawPolyChain(surf, scroll);
	} else {
		R_DrawPolyChain(surf, 0.0f);
	}

	R_EnableAlphaTest(qfalse);
}

#define BACKFACE_EPSILON    0.01

/**
 * @brief
 * @sa R_DrawBrushModel
 */
static void R_DrawInlineBrushModel (entity_t *e, model_t *mod)
{
	int i;
	cBspPlane_t *pplane;
	float dot;
	mBspSurface_t *psurf, *s;
	qboolean duplicate = qfalse;

	psurf = &mod->bsp.surfaces[mod->bsp.firstmodelsurface];

	if (e->flags & RF_TRANSLUCENT) {
		vec4_t color = {1, 1, 1, 0.25};
		R_ColorBlend(color);
		R_TexEnv(GL_MODULATE);
	}

	/* draw texture */
	for (i = 0; i < mod->bsp.nummodelsurfaces; i++, psurf++) {
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
			} else if (!(psurf->flags & SURF_WARP)) {
				R_DrawSurface(psurf);
			} else {
				R_EnableMultitexture(qfalse);
				R_RenderBrushPoly(psurf);
				R_EnableMultitexture(qtrue);
			}
		}
	}

	if (e->flags & RF_TRANSLUCENT) {
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

	if (currentmodel->bsp.nummodelsurfaces == 0)
		return;

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
	R_SelectTexture(GL_TEXTURE0_ARB);
	R_TexEnv(GL_REPLACE);
	R_SelectTexture(GL_TEXTURE1_ARB);
	R_TexEnv(GL_MODULATE);

	R_DrawInlineBrushModel(e, currentmodel);
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

	/* node is just a decision point, so go down the apropriate sides
	 * find which side of the node we are on */
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
			if (!(surf->flags & SURF_WARP))
				R_DrawSurface(surf);
		}
		/* add to the material chain if appropriate */
		if (surf->texinfo->image->material.flags & STAGE_RENDER) {
			surf->materialchain = r_material_surfaces;
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
	VectorCopy(refdef.vieworg, modelorg);

	r_state.currenttextures[0] = r_state.currenttextures[1] = -1;

	R_Color(NULL);
	memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	R_EnableMultitexture(qtrue);

	R_SelectTexture(GL_TEXTURE0_ARB);
	R_TexEnv(GL_REPLACE);
	R_SelectTexture(GL_TEXTURE1_ARB);

	R_TexEnv(r_config.envCombine);

	R_RecursiveWorldNode(nodes);
	R_EnableMultitexture(qfalse);
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
	int i, tile, mask;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!r_drawworld->integer)
		return;

	r_alpha_surfaces = NULL;
	r_material_surfaces = NULL;

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

void R_BuildPolygonFromSurface (mBspSurface_t *surf, int shift[3], model_t *mod)
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
		s -= surf->texturemins[0];
		s += surf->light_s << surf->lquant;
		s += 1 << (surf->lquant - 1);
		s /= BLOCK_WIDTH << surf->lquant;

		t = DotProduct(vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t -= surf->texturemins[1];
		t += surf->light_t << surf->lquant;
		t += 1 << (surf->lquant - 1);
		t /= BLOCK_HEIGHT << surf->lquant;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}
