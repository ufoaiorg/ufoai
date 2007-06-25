/**
 * @file gl_mesh_md2.c
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

#include "gl_local.h"

#define NUMVERTEXNORMALS	162

static float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

static vec4_t s_lerped[MD2_MAX_VERTS];
static float normalArray[MD2_MAX_VERTS * 3];

/**
 * @brief
 */
void GL_LerpVerts (int nverts, dtrivertx_t * v, dtrivertx_t * ov, float *lerp, float move[3], float frontv[3], float backv[3])
{
	int i;

	for (i = 0; i < nverts; i++, v++, ov++, lerp += 4) {
		lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0];
		lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1];
		lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2];
	}
}

/**
 * @brief interpolates between two frames and origins
 * FIXME: batch lerp all vertexes
 */
extern void R_DrawAliasFrameLerp (mdl_md2_t * paliashdr, float backlerp, int framenum, int oldframenum)
{
	daliasframe_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	int *order;
	int count;
	float frontlerp;
	vec3_t move;
	vec3_t frontv, backv;
	int i;
	float *lerp;
	float *na;
	float *oldNormal, *newNormal;

	frame = (daliasframe_t *) ((byte *) paliashdr + paliashdr->ofs_frames + framenum * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *) ((byte *) paliashdr + paliashdr->ofs_frames + oldframenum * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((byte *) paliashdr + paliashdr->ofs_glcmds);

	frontlerp = 1.0 - backlerp;

	for (i = 0; i < 3; i++)
		move[i] = backlerp * (oldframe->translate[i]) + frontlerp * frame->translate[i];

	for (i = 0; i < 3; i++) {
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	lerp = s_lerped[0];

	GL_LerpVerts(paliashdr->num_xyz, v, ov, lerp, move, frontv, backv);

	/* setup vertex array */
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglVertexPointer(3, GL_FLOAT, 16, s_lerped);	/* padded for SIMD */

	/* setup normal array */
	qglEnableClientState(GL_NORMAL_ARRAY);
	qglNormalPointer(GL_FLOAT, 0, normalArray);

	na = normalArray;
	if (backlerp == 0.0)
		for (i = 0; i < paliashdr->num_xyz; v++, i++) {
			/* get current normals */
			newNormal = r_avertexnormals[v->lightnormalindex];

			*na++ = newNormal[0];
			*na++ = newNormal[1];
			*na++ = newNormal[2];
	} else
		for (i = 0; i < paliashdr->num_xyz; v++, ov++, i++) {
			/* normalize interpolated normals? */
			/* no: probably too much to compute */
			oldNormal = r_avertexnormals[ov->lightnormalindex];
			newNormal = r_avertexnormals[v->lightnormalindex];

			*na++ = backlerp * oldNormal[0] + frontlerp * newNormal[0];
			*na++ = backlerp * oldNormal[1] + frontlerp * newNormal[1];
			*na++ = backlerp * oldNormal[2] + frontlerp * newNormal[2];
		}

	if (qglLockArraysEXT != 0)
		qglLockArraysEXT(0, paliashdr->num_xyz);

	while (1) {
		/* get the vertex count and primitive type */
		count = *order++;
		if (!count)
			break;				/* done */
		if (count < 0) {
			count = -count;
			qglBegin(GL_TRIANGLE_FAN);
		} else {
			qglBegin(GL_TRIANGLE_STRIP);
		}
		do {
			/* texture coordinates come from the draw list */
			qglTexCoord2f(((float *) order)[0], ((float *) order)[1]);
			order += 2;

			qglArrayElement(*order++);
		} while (--count);

		qglEnd();
	}

	if (qglUnlockArraysEXT != 0)
		qglUnlockArraysEXT();
}

/**
 * @brief
 */
static qboolean R_CullAliasMD2Model (vec4_t bbox[8], entity_t * e)
{
	int i, p, mask, f, aggregatemask = ~0;
	vec3_t mins, maxs;
	mdl_md2_t *paliashdr;
	vec3_t thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	float dp;

	assert(currentmodel->type == mod_alias_md2);
	paliashdr = (mdl_md2_t *) currentmodel->extraData;

	pframe = (daliasframe_t *) ((byte *) paliashdr + paliashdr->ofs_frames + e->as.frame * paliashdr->framesize);

	poldframe = (daliasframe_t *) ((byte *) paliashdr + paliashdr->ofs_frames + e->as.oldframe * paliashdr->framesize);

	/*
	 ** compute axially aligned mins and maxs
	 */
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

	/*
	** compute a full bounding box
	*/
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
	/*
	** transform the bounding box
	*/
	for (i = 0; i < 8; i++) {
		vec4_t tmp;

		Vector4Copy(bbox[i], tmp);
		GLVectorTransform(trafo[e - r_newrefdef.entities].matrix, tmp, bbox[i]);
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
 * @brief
 * @sa R_DrawAliasMD3Model
 */
void R_DrawAliasMD2FrameLerp (entity_t * e)
{
	qboolean lightfixed;
	vec4_t bbox[8];
	mdl_md2_t *paliashdr;
	image_t *skin;

	/* check if model is out of fov */
	if (R_CullAliasMD2Model(bbox, e))
		return;

	assert(currentmodel->type == mod_alias_md2);
	paliashdr = (mdl_md2_t *) currentmodel->extraData;

	/* check animations */
	if ((e->as.frame >= paliashdr->num_frames) || (e->as.frame < 0)) {
		ri.Con_Printf(PRINT_ALL, "R_DrawAliasMD2Model %s: no such frame %d\n", currentmodel->name, e->as.frame);
		e->as.frame = 0;
	}
	if ((e->as.oldframe >= paliashdr->num_frames) || (e->as.oldframe < 0)) {
		ri.Con_Printf(PRINT_ALL, "R_DrawAliasMD2Model %s: no such oldframe %d\n", currentmodel->name, e->as.oldframe);
		e->as.oldframe = 0;
	}

	if (!r_lerpmodels->integer)
		e->as.backlerp = 0;

	/* select skin */
	if (e->skin)
		skin = e->skin;			/* custom player skin */
	else {
		if (e->skinnum >= MD2_MAX_SKINS)
			skin = currentmodel->skins[0];
		else {
			skin = currentmodel->skins[e->skinnum];
			if (!skin)
				skin = currentmodel->skins[0];
		}
	}
	if (!skin)
		skin = r_notexture;		/* fallback... */
	else if (skin->has_alpha && !(e->flags & RF_TRANSLUCENT)) {
		/* it will be drawn in the next entity render pass */
		/* for the translucent entities */
		e->flags |= RF_TRANSLUCENT;
		if (!e->alpha)
			e->alpha = 1.0;
		return;
	}

	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	/* set-up lighting */
	lightfixed = e->flags & RF_LIGHTFIXED ? qtrue : qfalse;
	if (lightfixed)
		R_EnableLights(lightfixed, e->lightcolor, e->lightparam, e->lightambient);
	else
		R_EnableLights(lightfixed, trafo[e - r_newrefdef.entities].matrix, e->lightparam, NULL);

	/* FIXME: Does not work */
	/* IR goggles override color */
	if (r_newrefdef.rdflags & RDF_IRGOGGLES)
		qglColor4f(1.0, 0.0, 0.0, e->alpha);
	else
		qglColor4f(1.0, 1.0, 1.0, e->alpha);

	qglPushMatrix();

	qglMultMatrixf(trafo[e - r_newrefdef.entities].matrix);

	/* draw it */
	GL_Bind(skin->texnum);

	if (gl_combine) {
		GL_TexEnv(gl_combine);
		qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, gl_intensity->value);
	} else {
		GL_TexEnv(GL_MODULATE);
	}

	if ((e->flags & RF_TRANSLUCENT))
		GLSTATE_ENABLE_BLEND

	qglEnable(GL_DEPTH_TEST);
	qglEnable(GL_CULL_FACE);

	R_DrawAliasFrameLerp(paliashdr, e->as.backlerp, e->as.frame, e->as.oldframe);

	GL_TexEnv(GL_REPLACE);
	qglDisable(GL_LIGHTING);

	if ((e->flags & RF_TRANSLUCENT) || (skin && skin->has_alpha))
		GLSTATE_DISABLE_BLEND

	if (gl_shadows->integer == 1 && (e->flags & RF_SHADOW)) {
		if (!(e->flags & RF_TRANSLUCENT))
			qglDepthMask(GL_FALSE);
		GLSTATE_ENABLE_BLEND

		qglColor4f(1, 1, 1, 1);
		GL_Bind(shadow->texnum);
		qglBegin(GL_QUADS);

		qglTexCoord2f(0.0, 1.0);
		qglVertex3f(-18.0, 14.0, -28.5);
		qglTexCoord2f(1.0, 1.0);
		qglVertex3f(10.0, 14.0, -28.5);
		qglTexCoord2f(1.0, 0.0);
		qglVertex3f(10.0, -14.0, -28.5);
		qglTexCoord2f(0.0, 0.0);
		qglVertex3f(-18.0, -14.0, -28.5);

		qglEnd();

		GLSTATE_DISABLE_BLEND
		if (!(e->flags & RF_TRANSLUCENT))
			qglDepthMask(GL_TRUE);
	} else if (gl_shadows->integer == 2 && (e->flags & RF_SHADOW)) {
		R_DrawShadowVolume(e);
	}

	if (gl_fog->value && r_newrefdef.fog)
		qglDisable(GL_FOG);

	qglDisable(GL_CULL_FACE);
	qglDisable(GL_DEPTH_TEST);

	/* draw the circles for team-members and allied troops */
	if (e->flags & (RF_SELECTED | RF_ALLIED | RF_MEMBER)) {
		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_LINE_SMOOTH);
		GLSTATE_ENABLE_BLEND

		if (e->flags & RF_MEMBER) {
			if (e->flags & RF_SELECTED)
				qglColor4f(0, 1, 0, 1);
			else
				qglColor4f(0, 1, 0, 0.3);
		} else if (e->flags & RF_ALLIED) {
			if (e->flags & RF_SELECTED)
				qglColor4f(0, 0.5, 1, 1);
			else
				qglColor4f(0, 0.5, 1, 0.3);
		} else
			qglColor4f(0, 1, 0, 1);

		qglBegin(GL_LINE_STRIP);

		/* circle points */
		qglVertex3f(10.0, 0.0, -27.0);
		qglVertex3f(7.0, -7.0, -27.0);
		qglVertex3f(0.0, -10.0, -27.0);
		qglVertex3f(-7.0, -7.0, -27.0);
		qglVertex3f(-10.0, 0.0, -27.0);
		qglVertex3f(-7.0, 7.0, -27.0);
		qglVertex3f(0.0, 10.0, -27.0);
		qglVertex3f(7.0, 7.0, -27.0);
		qglVertex3f(10.0, 0.0, -27.0);

		qglEnd();

		GLSTATE_DISABLE_BLEND
		qglDisable(GL_LINE_SMOOTH);
		qglEnable(GL_TEXTURE_2D);
	}

	qglEnable(GL_DEPTH_TEST);
	qglEnable(GL_CULL_FACE);

	qglPopMatrix();

	/* show model bounding box */
	Mod_DrawModelBBox(bbox, e);

	if (gl_fog->value && r_newrefdef.fog)
		qglEnable(GL_FOG);

	qglColor4f(1, 1, 1, 1);
}

/**
 * @brief
 */
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
		mdl_md2_t *paliashdr;
		daliasframe_t *pframe;

		float max, size;
		vec3_t mins, maxs, center;
		int i;

		/* get model data */
		assert(mi->model->type == mod_alias_md2);
		paliashdr = (mdl_md2_t *) mi->model->extraData;
		pframe = (daliasframe_t *) ((byte *) paliashdr + paliashdr->ofs_frames);

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
 * @brief
 * @sa R_DrawAliasMD2Model
 * @sa R_DrawAliasMD3Model
 */
void R_DrawModelDirect (modelInfo_t * mi, modelInfo_t * pmi, const char *tagname)
{
	int i;
	mdl_md2_t *paliashdr;
	image_t *skin;

	/* register the model */
	mi->model = R_RegisterModelShort(mi->name);

	/* check if the model exists */
	if (!mi->model || mi->model->type != mod_alias_md2) {
		ri.Con_Printf(PRINT_ALL, "No model given or not in md2 format\n");
		return;
	}

	paliashdr = (mdl_md2_t *) mi->model->extraData;

	/* check animations */
	if ((mi->frame >= paliashdr->num_frames) || (mi->frame < 0)) {
		ri.Con_Printf(PRINT_ALL, "R_DrawModelDirect %s: no such frame %d\n", mi->model->name, mi->frame);
		mi->frame = 0;
	}
	if ((mi->oldframe >= paliashdr->num_frames) || (mi->oldframe < 0)) {
		ri.Con_Printf(PRINT_ALL, "R_DrawModelDirect %s: no such oldframe %d\n", mi->model->name, mi->oldframe);
		mi->oldframe = 0;
	}

	if (!r_lerpmodels->value)
		mi->backlerp = 0;

	/* select skin */
	if (mi->skin >= 0 && mi->skin < paliashdr->num_skins)
		skin = mi->model->skins[mi->skin];
	else
		skin = mi->model->skins[0];

	if (!skin)
		skin = r_notexture;		/* fallback... */

	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	/* draw all the triangles */
	qglPushMatrix();
	qglScalef(vid.rx, vid.ry, (vid.rx + vid.ry) / 2);

	if (mi->color[3])
		qglColor4fv(mi->color);
	else
		qglColor4f(1, 1, 1, 1);

	if (pmi) {
		/* register the parent model */
		pmi->model = R_RegisterModelShort(pmi->name);

		/* transform */
		R_TransformModelDirect(pmi);

		/* tag trafo */
		if (tagname && pmi->model && pmi->model->tagdata) {
			animState_t as;
			dtag_t *taghdr;
			char *name;
			float *tag;
			float interpolated[16];

			taghdr = (dtag_t *) pmi->model->tagdata;

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
					break;
				}
		}
	}

	/* transform */
	R_TransformModelDirect(mi);

	/* draw it */
	GL_Bind(skin->texnum);

	qglEnable(GL_DEPTH_TEST);
	qglEnable(GL_CULL_FACE);

	if (gl_combine) {
		GL_TexEnv(gl_combine);
		qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, gl_intensity->value);
	} else {
		GL_TexEnv(GL_MODULATE);
	}

	if ((mi->color[3] && mi->color[3] < 1.0f) || (skin && skin->has_alpha))
		GLSTATE_ENABLE_BLEND

	/* draw the model */
	R_DrawAliasFrameLerp(paliashdr, mi->backlerp, mi->frame, mi->oldframe);

	GL_TexEnv(GL_MODULATE);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_DEPTH_TEST);

	if ((mi->color[3] && mi->color[3] < 1.0f) || (skin && skin->has_alpha))
		GLSTATE_DISABLE_BLEND

	qglPopMatrix();

	qglColor4f(1, 1, 1, 1);
}

/**
 * @brief
 */
void R_DrawModelParticle (modelInfo_t * mi)
{
	mdl_md2_t *paliashdr;
	image_t *skin;

	/* check if the model exists */
	if (!mi->model || mi->model->type != mod_alias_md2)
		return;

	paliashdr = (mdl_md2_t *) mi->model->extraData;

	/* check animations */
	if ((mi->frame >= paliashdr->num_frames) || (mi->frame < 0)) {
		ri.Con_Printf(PRINT_ALL, "R_DrawModelParticle %s: no such frame %d\n", mi->model->name, mi->frame);
		mi->frame = 0;
	}
	if ((mi->oldframe >= paliashdr->num_frames) || (mi->oldframe < 0)) {
		ri.Con_Printf(PRINT_ALL, "R_DrawModelParticle %s: no such oldframe %d\n", mi->model->name, mi->oldframe);
		mi->oldframe = 0;
	}

	if (!r_lerpmodels->value)
		mi->backlerp = 0;

	/* select skin */
	if (mi->skin >= paliashdr->num_skins) {
		ri.Con_Printf(PRINT_ALL, "R_DrawModelParticle %s: no such skin %i (found %i skins)\n",
			mi->model->name, mi->skin, paliashdr->num_skins);
		mi->skin = 0;
	}
	skin = mi->model->skins[mi->skin];
	if (!skin)
		skin = r_notexture;		/* fallback... */

	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	if (mi->color[3])
		qglColor4fv(mi->color);
	else
		qglColor4f(1, 1, 1, 1);

	/* draw all the triangles */
	qglPushMatrix();

	qglTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);
	qglRotatef(mi->angles[1], 0, 0, 1);
	qglRotatef(mi->angles[0], 0, 1, 0);
	qglRotatef(-mi->angles[2], 1, 0, 0);

	/* draw it */
	GL_Bind(skin->texnum);

	qglEnable(GL_DEPTH_TEST);
	qglEnable(GL_CULL_FACE);

	if (gl_combine) {
		GL_TexEnv(gl_combine);
		qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, gl_intensity->value);
	} else {
		GL_TexEnv(GL_MODULATE);
	}

	if ((mi->color[3] && mi->color[3] < 1.0f) || (skin && skin->has_alpha))
		GLSTATE_ENABLE_BLEND

	/* draw the model */
	R_DrawAliasFrameLerp(paliashdr, mi->backlerp, mi->frame, mi->oldframe);

	GL_TexEnv(GL_REPLACE);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_DEPTH_TEST);

	if ((mi->color[3] && mi->color[3] < 1.0f) || (skin && skin->has_alpha))
		GLSTATE_DISABLE_BLEND

	qglPopMatrix();

	qglColor4f(1, 1, 1, 1);
}

