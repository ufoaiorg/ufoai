/**
 * @file r_mesh_md2.c
 * @brief MD2 Model drawing code
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "r_shader.h"
#include "r_error.h"

static vec3_t r_mesh_verts[MD2_MAX_VERTS];
static vec3_t r_mesh_norms[MD2_MAX_VERTS];

/**
 * @brief interpolates between two frames and origins
 */
static void R_DrawAliasFrameLerp (mdl_md2_t * md2, float backlerp, int framenum, int oldframenum)
{
	dAliasFrame_t *frame, *oldframe;
	dAliasTriangleVertex_t *v, *ov, *verts;
	int *order;
	int count;
	float frontlerp;
	vec3_t move;
	vec3_t frontv, backv;
	int i, j, mode, index_xyz;

	frame = (dAliasFrame_t *) ((byte *) md2 + md2->ofs_frames + framenum * md2->framesize);
	verts = v = frame->verts;

	oldframe = (dAliasFrame_t *) ((byte *) md2 + md2->ofs_frames + oldframenum * md2->framesize);
	ov = oldframe->verts;

	order = (int *) ((byte *) md2 + md2->ofs_glcmds);

	frontlerp = 1.0 - backlerp;

	for (i = 0; i < 3; i++)
		move[i] = backlerp * (oldframe->translate[i]) + frontlerp * frame->translate[i];

	for (i = 0; i < 3; i++) {
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	for (i = 0; i < md2->num_xyz; i++, v++, ov++) {
		VectorSet(r_mesh_verts[i],
				move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0],
				move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1],
				move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2]);

		if (r_state.lighting_enabled) {  /* and the norms */
			VectorSet(r_mesh_norms[i],
					backlerp * bytedirs[ov->lightnormalindex][0] + frontlerp * bytedirs[v->lightnormalindex][0],
					backlerp * bytedirs[ov->lightnormalindex][1] + frontlerp * bytedirs[v->lightnormalindex][1],
					backlerp * bytedirs[ov->lightnormalindex][2] + frontlerp * bytedirs[v->lightnormalindex][2]);
		}
	}

	i = j = 0;
	while (qtrue) {
		/* get the vertex count and primitive type */
		count = *order++;
		if (!count)
			break;				/* done */
		if (count < 0) {
			count = -count;
			mode = GL_TRIANGLE_FAN;
		} else {
			mode = GL_TRIANGLE_STRIP;
		}
		do {
			/* texture coordinates come from the draw list */
			memcpy(&r_state.texture_texunit.texcoords[j], order, sizeof(vec2_t));
			j += 2;

			index_xyz = order[2];
			order += 3;

			/* verts */
			memcpy(&r_state.vertex_array_3d[i], r_mesh_verts[index_xyz], sizeof(vec3_t));
			/* norms */
			memcpy(&r_state.normal_array[i], r_mesh_norms[index_xyz], sizeof(vec3_t));
			i += 3;
		} while (--count);

		qglDrawArrays(mode, 0, i / 3);
		i = j = 0;
		R_CheckError();
	}

	R_Color(NULL);
	R_CheckError();
}

/**
 * @brief Check if model is out of view
 */
static qboolean R_CullAliasMD2Model (vec4_t bbox[8], entity_t * e)
{
	int i, p, mask, f, aggregatemask = ~0;
	vec3_t mins, maxs;
	mdl_md2_t *md2;
	vec3_t thismins, oldmins, thismaxs, oldmaxs;
	dAliasFrame_t *pframe, *poldframe;
	float dp;

	assert(e->model->type == mod_alias_md2);
	md2 = (mdl_md2_t *) e->model->alias.extraData;

	pframe = (dAliasFrame_t *) ((byte *) md2 + md2->ofs_frames + e->as.frame * md2->framesize);

	poldframe = (dAliasFrame_t *) ((byte *) md2 + md2->ofs_frames + e->as.oldframe * md2->framesize);

	/* compute axially aligned mins and maxs */
	if (pframe == poldframe) {
		for (i = 0; i < 3; i++) {
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	} else {
		for (i = 0; i < 3; i++) {
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

			oldmins[i] = poldframe->translate[i];
			oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

			if (thismins[i] < oldmins[i])
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if (thismaxs[i] > oldmaxs[i])
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

	/* compute a full bounding box */
	for (i = 0; i < 8; i++) {
		vec3_t tmp;

		if (i & 1)
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if (i & 2)
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if (i & 4)
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy(tmp, bbox[i]);
		bbox[i][3] = 1.0;
	}
	/* transform the bounding box */
	for (i = 0; i < 8; i++) {
		vec4_t tmp;

		Vector4Copy(bbox[i], tmp);
		GLVectorTransform(e->transform.matrix, tmp, bbox[i]);
	}

	/* cull */
	for (p = 0; p < 8; p++) {
		mask = 0;

		for (f = 0; f < 4; f++) {
			dp = DotProduct(frustum[f].normal, bbox[p]);

			if ((dp - frustum[f].dist) < 0)
				mask |= (1 << f);
		}
		aggregatemask &= mask;
	}

	if (aggregatemask)
		return qtrue;

	return qfalse;
}

/**
 * @sa R_DrawAliasMD2Model
 */
void R_DrawAliasMD2Model (entity_t * e)
{
	mdl_md2_t *md2;
	image_t *skin;
	vec4_t color = {1, 1, 1, 1};
	vec4_t bbox[8];
	vec3_t tmp;

	/* check if model is out of fov */
	if (R_CullAliasMD2Model(bbox, e))
		return;

	md2 = (mdl_md2_t *) e->model->alias.extraData;

	/* check animations */
	if ((e->as.frame >= md2->num_frames) || (e->as.frame < 0)) {
		Com_Printf("R_DrawAliasMD2Model %s: no such frame %d\n", e->model->name, e->as.frame);
		e->as.frame = 0;
	}
	if ((e->as.oldframe >= md2->num_frames) || (e->as.oldframe < 0)) {
		Com_Printf("R_DrawAliasMD2Model %s: no such oldframe %d\n", e->model->name, e->as.oldframe);
		e->as.oldframe = 0;
	}

	/* select skin */
	if (e->skin)
		skin = e->skin;			/* custom player skin */
	else {
		if (e->skinnum >= MD2_MAX_SKINS)
			skin = e->model->alias.skins_img[0];
		else {
			skin = e->model->alias.skins_img[e->skinnum];
			if (!skin)
				skin = e->model->alias.skins_img[0];
		}
	}

	if (skin->has_alpha && !(e->flags & RF_TRANSLUCENT)) {
		/* it will be drawn in the next entity render pass
		 * for the translucent entities */
		e->flags |= RF_TRANSLUCENT;
		if (!e->alpha)
			e->alpha = 1.0;
		return;
	}

	/* locate the proper data */
	c_alias_polys += md2->num_tris;

	/* resolve lighting for coloring and shadow position */
	GLVectorTransform(e->transform.matrix, e->origin, tmp);
	R_LightPoint(tmp);

	qglPushMatrix();
	qglMultMatrixf(e->transform.matrix);

	VectorCopy(r_lightmap_sample.color, color);
	color[3] = r_state.blend_enabled ? 0.25 : 1.0;

	/* FIXME: Does not work */
	/* IR goggles override color */
	if (refdef.rdflags & RDF_IRGOGGLES)
		color[1] = color[2] = 0.0;

	if (r_state.lighting_enabled)
		R_ShaderFragmentParameter(0, color);
	else
		R_Color(color);

	/* draw it */
	R_Bind(skin->texnum);

	R_DrawAliasFrameLerp(md2, e->as.backlerp, e->as.frame, e->as.oldframe);

	/* show model bounding box */
	R_ModDrawModelBBox(bbox, e);

	qglPopMatrix();

	R_Color(NULL);
}

static void R_TransformModelDirect (modelInfo_t * mi)
{
	/* translate and rotate */
	qglTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);

	qglRotatef(mi->angles[0], 0, 0, 1);
	qglRotatef(mi->angles[1], 0, 1, 0);
	qglRotatef(mi->angles[2], 1, 0, 0);

	if (mi->scale) {
		/* scale by parameters */
		qglScalef(mi->scale[0], mi->scale[1], mi->scale[2]);
		if (mi->center)
			qglTranslatef(-mi->center[0], -mi->center[1], -mi->center[2]);
	} else if (mi->center) {
		/* autoscale */
		mdl_md2_t *md2;
		dAliasFrame_t *pframe;

		float max, size;
		vec3_t mins, maxs, center;
		int i;

		/* get model data */
		assert(mi->model->type == mod_alias_md2);
		md2 = (mdl_md2_t *) mi->model->alias.extraData;
		pframe = (dAliasFrame_t *) ((byte *) md2 + md2->ofs_frames);

		/* get center and scale */
		for (max = 1.0, i = 0; i < 3; i++) {
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
			center[i] = -(mins[i] + maxs[i]) / 2;
			size = maxs[i] - mins[i];
			if (size > max)
				max = size;
		}
		size = (mi->center[0] < mi->center[1] ? mi->center[0] : mi->center[1]) / max;
		qglScalef(size, size, size);
		qglTranslatef(center[0], center[1], center[2]);
	}
}

/**
 * @sa R_DrawAliasMD2Model
 * @sa R_DrawAliasMD3Model
 */
void R_DrawModelDirect (modelInfo_t * mi, modelInfo_t * pmi, const char *tagname)
{
	int i;
	mdl_md2_t *md2;
	image_t *skin;

	/* register the model */
	mi->model = R_RegisterModelShort(mi->name);

	/* check if the model exists */
	if (!mi->model || mi->model->type != mod_alias_md2) {
		Com_Printf("No model given or not in md2 format\n");
		return;
	}

	md2 = (mdl_md2_t *) mi->model->alias.extraData;

	/* check animations */
	if ((mi->frame >= md2->num_frames) || (mi->frame < 0)) {
		Com_Printf("R_DrawModelDirect %s: no such frame %d\n", mi->model->name, mi->frame);
		mi->frame = 0;
	}
	if ((mi->oldframe >= md2->num_frames) || (mi->oldframe < 0)) {
		Com_Printf("R_DrawModelDirect %s: no such oldframe %d\n", mi->model->name, mi->oldframe);
		mi->oldframe = 0;
	}

	/* select skin */
	if (mi->skin >= 0 && mi->skin < md2->num_skins)
		skin = mi->model->alias.skins_img[mi->skin];
	else
		skin = mi->model->alias.skins_img[0];

	if (!skin)
		skin = r_notexture;		/* fallback... */

	/* locate the proper data */
	c_alias_polys += md2->num_tris;

	/* draw all the triangles */
	qglPushMatrix();
	qglScalef(viddef.rx, viddef.ry, (viddef.rx + viddef.ry) / 2);

	R_Color(mi->color);

	if (pmi) {
		/* register the parent model */
		pmi->model = R_RegisterModelShort(pmi->name);

		/* transform */
		R_TransformModelDirect(pmi);

		/* tag trafo */
		if (tagname && pmi->model && pmi->model->alias.tagdata) {
			animState_t as;
			dtag_t *taghdr;
			char *name;
			float *tag;
			float interpolated[16];

			taghdr = (dtag_t *) pmi->model->alias.tagdata;

			/* find the right tag */
			name = (char *) taghdr + taghdr->ofs_names;
			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME)
				if (!Q_strcmp(name, tagname)) {
					/* found the tag (matrix) */
					tag = (float *) ((byte *) taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					as.frame = pmi->frame;
					as.oldframe = pmi->oldframe;
					as.backlerp = pmi->backlerp;
					R_InterpolateTransform(&as, taghdr->num_frames, tag, interpolated);

					/* transform */
					qglMultMatrixf(interpolated);
					R_CheckError();
					break;
				}
		}
	}

	/* transform */
	R_TransformModelDirect(mi);

	/* we have to reenable this here - we are in 2d mode here already */
	qglEnable(GL_DEPTH_TEST);

	/* draw it */
	R_Bind(skin->texnum);

	if ((mi->color && mi->color[3] < 1.0f) || (skin && skin->has_alpha))
		R_EnableBlend(qtrue);

	/* draw the model */
	R_DrawAliasFrameLerp(md2, mi->backlerp, mi->frame, mi->oldframe);

	if ((mi->color && mi->color[3] < 1.0f) || (skin && skin->has_alpha))
		R_EnableBlend(qfalse);

	qglDisable(GL_DEPTH_TEST);

	qglPopMatrix();

	R_Color(NULL);
}

void R_DrawModelParticle (modelInfo_t * mi)
{
	mdl_md2_t *md2;
	image_t *skin;

	/* check if the model exists */
	if (!mi->model || mi->model->type != mod_alias_md2)
		return;

	md2 = (mdl_md2_t *) mi->model->alias.extraData;

	/* check animations */
	if ((mi->frame >= md2->num_frames) || (mi->frame < 0)) {
		Com_Printf("R_DrawModelParticle %s: no such frame %d\n", mi->model->name, mi->frame);
		mi->frame = 0;
	}
	if ((mi->oldframe >= md2->num_frames) || (mi->oldframe < 0)) {
		Com_Printf("R_DrawModelParticle %s: no such oldframe %d\n", mi->model->name, mi->oldframe);
		mi->oldframe = 0;
	}

	/* select skin */
	if (mi->skin >= md2->num_skins) {
		Com_Printf("R_DrawModelParticle %s: no such skin %i (found %i skins)\n",
			mi->model->name, mi->skin, md2->num_skins);
		mi->skin = 0;
	}
	skin = mi->model->alias.skins_img[mi->skin];
	if (!skin)
		skin = r_notexture;		/* fallback... */

	/* locate the proper data */
	c_alias_polys += md2->num_tris;

	R_Color(mi->color);

	/* draw all the triangles */
	qglPushMatrix();

	qglTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);
	qglRotatef(mi->angles[1], 0, 0, 1);
	qglRotatef(mi->angles[0], 0, 1, 0);
	qglRotatef(-mi->angles[2], 1, 0, 0);

	/* draw it */
	R_Bind(skin->texnum);

	/* draw the model */
	R_DrawAliasFrameLerp(md2, mi->backlerp, mi->frame, mi->oldframe);

	qglPopMatrix();

	R_Color(NULL);
}
