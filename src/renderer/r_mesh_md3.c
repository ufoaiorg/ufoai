/**
 * @file r_mesh_md3.c
 * @brief MD3 Model drawing code
 * @note Currently only for static none-animated models
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
		float max, size;
		vec3_t mins, maxs, center;
		int i;
		mAliasFrame_t *frame = mi->model->alias.frames;

		/* get center and scale */
		for (max = 1.0, i = 0; i < 3; i++) {
			mins[i] = frame->translate[i];
			maxs[i] = mins[i] + frame->scale[i] * 255;
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
 * @sa R_DrawAliasModel
 */
void R_DrawModelDirect (modelInfo_t * mi, modelInfo_t * pmi, const char *tagname)
{
	int i;
	image_t *skin;
	mAliasMesh_t *mesh;

	/* register the model */
	mi->model = R_RegisterModelShort(mi->name);

	/* check if the model exists */
	if (!mi->model) {
		Com_Printf("No model given or not in md2 format\n");
		return;
	}

	skin = R_AliasModelState(mi->model, &mi->mesh, &mi->frame, &mi->oldframe, &mi->skin);

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
			dMD2tag_t *taghdr;
			char *name;
			float *tag;
			float interpolated[16];

			taghdr = (dMD2tag_t *) pmi->model->alias.tagdata;

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
	R_BindTexture(skin->texnum);

	if ((mi->color && mi->color[3] < 1.0f) || skin->has_alpha)
		R_EnableBlend(qtrue);

	/* draw the model */
	for (i = 0; i < mi->model->alias.num_meshes; i++) {
		mesh = &mi->model->alias.meshes[i];
		refdef.alias_count += mesh->num_tris;
		R_DrawAliasFrameLerp(&mi->model->alias, mesh, mi->backlerp, mi->frame, mi->oldframe);
	}

	if ((mi->color && mi->color[3] < 1.0f) || skin->has_alpha)
		R_EnableBlend(qfalse);

	qglDisable(GL_DEPTH_TEST);

	qglPopMatrix();

	R_Color(NULL);
}

/**
 * @sa R_DrawPtlModel
 */
void R_DrawModelParticle (modelInfo_t * mi)
{
	int i;
	image_t *skin;
	mAliasMesh_t *mesh;

	/* check if the model exists */
	if (!mi->model)
		return;

	skin = R_AliasModelState(mi->model, &mi->mesh, &mi->frame, &mi->oldframe, &mi->skin);

	R_Color(mi->color);

	/* draw all the triangles */
	qglPushMatrix();

	qglTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);
	qglRotatef(mi->angles[1], 0, 0, 1);
	qglRotatef(mi->angles[0], 0, 1, 0);
	qglRotatef(-mi->angles[2], 1, 0, 0);

	/* draw it */
	R_BindTexture(skin->texnum);

	/* draw the model */
	for (i = 0; i < mi->model->alias.num_meshes; i++) {
		mesh = &mi->model->alias.meshes[i];
		refdef.alias_count += mesh->num_tris;
		R_DrawAliasFrameLerp(&mi->model->alias, mesh, mi->backlerp, mi->frame, mi->oldframe);
	}

	qglPopMatrix();

	R_Color(NULL);
}

/**
 * @brief Check if model is out of view
 */
static qboolean R_CullAliasModel (vec4_t bbox[8], const entity_t * e)
{
	int i, p, mask, f, aggregatemask = ~0;
	vec3_t mins, maxs;
	mAliasModel_t *md2;
	vec3_t thismins, oldmins, thismaxs, oldmaxs;
	mAliasFrame_t *frame, *oldframe;
	float dp;

	assert(e->model->type == mod_alias_md2);
	md2 = &e->model->alias;

	frame = md2->frames + e->as.frame;
	oldframe = md2->frames + e->as.oldframe;

	/* compute axially aligned mins and maxs */
	if (frame == oldframe) {
		for (i = 0; i < 3; i++) {
			mins[i] = frame->translate[i];
			maxs[i] = mins[i] + frame->scale[i] * 255;
		}
	} else {
		for (i = 0; i < 3; i++) {
			thismins[i] = frame->translate[i];
			thismaxs[i] = thismins[i] + frame->scale[i] * 255;

			oldmins[i] = oldframe->translate[i];
			oldmaxs[i] = oldmins[i] + oldframe->scale[i] * 255;

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

static vec3_t r_mesh_verts[MD3_MAX_VERTS];
static vec3_t r_mesh_norms[MD3_MAX_VERTS];

void R_DrawAliasFrameLerp (const mAliasModel_t* mod, const mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum)
{
	int i, vertind, coordind;
	mAliasFrame_t *frame, *oldframe;
	mAliasVertex_t *v, *ov;
	vec3_t move;
	const float frontlerp = 1.0 - backlerp;

	frame = mod->frames + framenum;
	oldframe = mod->frames + oldframenum;

	for (i = 0; i < 3; i++)
		move[i] = backlerp * oldframe->translate[i] + frontlerp * frame->translate[i];

	v = &mesh->vertexes[framenum * mesh->num_verts];
	ov = &mesh->vertexes[oldframenum * mesh->num_verts];

	for (i = 0; i < mesh->num_verts; i++, v++, ov++) {  /* lerp the verts */
		VectorSet(r_mesh_verts[i],
				move[0] + ov->point[0] * backlerp + v->point[0] * frontlerp,
				move[1] + ov->point[1] * backlerp + v->point[1] * frontlerp,
				move[2] + ov->point[2] * backlerp + v->point[2] * frontlerp);

		if (r_state.lighting_enabled) {  /* and the norms */
			VectorSet(r_mesh_norms[i],
					v->normal[0] + (ov->normal[0] - v->normal[0]) * backlerp,
					v->normal[1] + (ov->normal[1] - v->normal[1]) * backlerp,
					v->normal[2] + (ov->normal[2] - v->normal[2]) * backlerp);
		}
	}

	vertind = coordind = 0;

	for (i = 0; i < mesh->num_tris; i++) {  /* draw the tris */
		memcpy(&r_state.texcoord_array[coordind + 0], &mesh->stcoords[mesh->indexes[3 * i + 0]][0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 0], &r_mesh_verts[mesh->indexes[3 * i + 0]], sizeof(vec3_t));

		memcpy(&r_state.texcoord_array[coordind + 2], &mesh->stcoords[mesh->indexes[3 * i + 1]][0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 3], &r_mesh_verts[mesh->indexes[3 * i + 1]], sizeof(vec3_t));

		memcpy(&r_state.texcoord_array[coordind + 4], &mesh->stcoords[mesh->indexes[3 * i + 2]][0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 6], &r_mesh_verts[mesh->indexes[3 * i + 2]], sizeof(vec3_t));

		if (r_state.lighting_enabled) {  /* normal vectors for lighting */
			memcpy(&r_state.normal_array[vertind + 0], &r_mesh_norms[mesh->indexes[3 * i + 0]], sizeof(vec3_t));
			memcpy(&r_state.normal_array[vertind + 3], &r_mesh_norms[mesh->indexes[3 * i + 1]], sizeof(vec3_t));
			memcpy(&r_state.normal_array[vertind + 6], &r_mesh_norms[mesh->indexes[3 * i + 2]], sizeof(vec3_t));
		}

		coordind += 6;
		vertind += 9;
	}

	qglDrawArrays(GL_TRIANGLES, 0, mesh->num_tris * 3);

	R_CheckError();
}

/**
 * @sa R_DrawAliasMD2Model
 * @todo Implement the md2 renderer effect (e.g. RF_HIGHLIGHT)
 */
void R_DrawAliasModel (entity_t *e)
{
	mAliasModel_t *mod;
	int i;
	image_t *skin;
	vec4_t color = {1, 1, 1, 1};
	vec4_t bbox[8];
	vec4_t tmp;
#if 0
	/* check if model is out of fov */
	if (R_CullAliasModel(bbox, e))
		return;
#endif
	mod = (mAliasModel_t *)&e->model->alias;

	qglPushMatrix();

	qglMultMatrixf(e->transform.matrix);

	if (!(refdef.rdflags & RDF_NOWORLDMODEL)) {
		R_LightPoint(tmp);

		VectorCopy(r_lightmap_sample.color, color);
		color[3] = r_state.blend_enabled ? 0.25 : 1.0;

		/* IR goggles override color
		* FIXME RF_SHADOW is used here for actors - don't highlight misc_models */
		if (refdef.rdflags & RDF_IRGOGGLES && e->flags & RF_SHADOW)
			color[1] = color[2] = 0.0;

		if (r_state.lighting_enabled)
			R_ShaderFragmentParameter(0, color);
		else
			R_Color(color);
	}

	skin = R_AliasModelState(e->model, &e->as.mesh, &e->as.frame, &e->as.oldframe, &e->skinnum);

	R_BindTexture(skin->texnum);

	for (i = 0; i < mod->num_meshes; i++) {
		/* locate the proper data */
		refdef.alias_count += mod->meshes[i].num_tris;
		R_DrawAliasFrameLerp(mod, &mod->meshes[i], e->as.backlerp, e->as.frame, e->as.oldframe);
	}

	/* show model bounding box */
	R_ModDrawModelBBox(bbox, e);

	qglPopMatrix();

	R_Color(NULL);
}
