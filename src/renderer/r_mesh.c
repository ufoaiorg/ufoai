/**
 * @file r_mesh.c
 * @brief Mesh Model drawing code
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
#include "r_light.h"
#include "r_mesh.h"
#include "r_mesh_anim.h"
#include "r_error.h"

static void R_TransformModelDirect (modelInfo_t * mi)
{
	/* translate and rotate */
	glTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);

	glRotatef(mi->angles[0], 0, 0, 1);
	glRotatef(mi->angles[1], 0, 1, 0);
	glRotatef(mi->angles[2], 1, 0, 0);

	if (mi->scale) {
		/* scale by parameters */
		glScalef(mi->scale[0], mi->scale[1], mi->scale[2]);
		if (mi->center)
			glTranslatef(-mi->center[0], -mi->center[1], -mi->center[2]);
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
		glScalef(size, size, size);
		glTranslatef(center[0], center[1], center[2]);
	}
}

/**
 * @sa R_DrawAliasModel
 */
void R_DrawModelDirect (modelInfo_t * mi, modelInfo_t * pmi, const char *tagname)
{
	int i;
	image_t *skin;

	/* register the model */
	mi->model = R_RegisterModelShort(mi->name);

	/* check if the model exists */
	if (!mi->model) {
		Com_Printf("No model given '%s'\n", mi->name);
		return;
	}

	skin = R_AliasModelState(mi->model, &mi->mesh, &mi->frame, &mi->oldframe, &mi->skin);
	if (skin == NULL) {
		Com_Printf("Model '%s' is broken\n", mi->name);
		return;
	}

	glPushMatrix();
	glScalef(viddef.rx, viddef.ry, (viddef.rx + viddef.ry) / 2);

	R_Color(mi->color);

	if (pmi) {
		/* register the parent model */
		pmi->model = R_RegisterModelShort(pmi->name);

		/* transform - the next transform for the child model will be relative from the
		 * parent model location now */
		R_TransformModelDirect(pmi);

		/* tag trafo */
		if (tagname && pmi->model && pmi->model->alias.tagdata) {
			dMD2tag_t *taghdr = (dMD2tag_t *) pmi->model->alias.tagdata;
			/* find the right tag */
			const char *name = (const char *) taghdr + taghdr->ofs_names;

			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME) {
				if (!Q_strcmp(name, tagname)) {
					float interpolated[16];
					animState_t as;
					/* found the tag (matrix) */
					const float *tag = (const float *) ((const byte *) taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					as.frame = pmi->frame;
					as.oldframe = pmi->oldframe;
					as.backlerp = pmi->backlerp;
					R_InterpolateTransform(&as, taghdr->num_frames, tag, interpolated);

					/* transform */
					glMultMatrixf(interpolated);
					R_CheckError();
					break;
				}
			}
		}
	}

	/* transform */
	R_TransformModelDirect(mi);

	/* we have to reenable this here - we are in 2d mode here already */
	glEnable(GL_DEPTH_TEST);

	/* draw it */
	R_BindTexture(skin->texnum);

	if ((mi->color && mi->color[3] < 1.0f) || skin->has_alpha)
		R_EnableBlend(qtrue);

	/* draw the model */
	for (i = 0; i < mi->model->alias.num_meshes; i++) {
		const mAliasMesh_t *mesh = &mi->model->alias.meshes[i];
		refdef.alias_count += mesh->num_tris;
		R_DrawAliasFrameLerp(&mi->model->alias, mesh, mi->backlerp, mi->frame, mi->oldframe);
	}

	if ((mi->color && mi->color[3] < 1.0f) || skin->has_alpha)
		R_EnableBlend(qfalse);

	glDisable(GL_DEPTH_TEST);

	glPopMatrix();

	R_Color(NULL);
}

/**
 * @sa R_DrawPtlModel
 */
void R_DrawModelParticle (modelInfo_t * mi)
{
	int i;
	image_t *skin;

	/* check if the model exists */
	if (!mi->model)
		return;

	skin = R_AliasModelState(mi->model, &mi->mesh, &mi->frame, &mi->oldframe, &mi->skin);
	if (skin == NULL) {
		Com_Printf("Model '%s' is broken\n", mi->name);
		return;
	}

	R_Color(mi->color);

	/* draw all the triangles */
	glPushMatrix();

	glTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);
	glRotatef(mi->angles[1], 0, 0, 1);
	glRotatef(mi->angles[0], 0, 1, 0);
	glRotatef(-mi->angles[2], 1, 0, 0);

	/* draw it */
	R_BindTexture(skin->texnum);

	/* draw the model */
	for (i = 0; i < mi->model->alias.num_meshes; i++) {
		const mAliasMesh_t *mesh = &mi->model->alias.meshes[i];
		refdef.alias_count += mesh->num_tris;
		R_DrawAliasFrameLerp(&mi->model->alias, mesh, mi->backlerp, mi->frame, mi->oldframe);
	}

	glPopMatrix();

	R_Color(NULL);
}

qboolean R_CullMeshModel (entity_t *e)
{
	int i;
	int aggregatemask;
	vec3_t mins, maxs, origin;
	vec4_t bbox[8];

	if (e->tagent)
		VectorCopy(e->tagent->origin, origin);
	else
		VectorCopy(e->origin, origin);

	/* determine scaled mins/maxs */
	for (i = 0; i < 3; i++) {
		if (e->scale[i]) {
			mins[i] = e->model->mins[i] * e->scale[i];
			maxs[i] = e->model->maxs[i] * e->scale[i];
		} else {
			mins[i] = e->model->mins[i];
			maxs[i] = e->model->maxs[i];
		}
	}

	/* compute translated bounding box */
	for (i = 0; i < 8; i++) {
		vec3_t tmp;

		tmp[0] = (i & 1) ? mins[0] : maxs[0];
		tmp[1] = (i & 2) ? mins[1] : maxs[1];
		tmp[2] = (i & 4) ? mins[2] : maxs[2];

		VectorAdd(origin, tmp, bbox[i]);
	}

	/* compute a full bounding box */
	aggregatemask = ~0;

	for (i = 0; i < 8; i++) {
		int mask = 0;
		int j;

		for (j = 0; j < 4; j++) {
			/* get the distance between the frustom normal vector and the
			 * current vector of the bounding box */
			const float f = DotProduct(r_locals.frustum[j].normal, bbox[i]);
			if ((f - r_locals.frustum[j].dist) < 0)
				mask |= (1 << j);
		}

		aggregatemask &= mask;
	}

	if (aggregatemask)
		return qtrue;

	return qfalse;
}

void R_DrawAliasFrameLerp (const mAliasModel_t* mod, const mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum)
{
	int i, vertind, coordind;
	const mAliasFrame_t *frame, *oldframe;
	const mAliasVertex_t *v, *ov;
	vec3_t move;
	const float frontlerp = 1.0 - backlerp;
	vec3_t r_mesh_verts[MD3_MAX_VERTS];
	vec3_t r_mesh_norms[MD3_MAX_VERTS];

	frame = mod->frames + framenum;
	oldframe = mod->frames + oldframenum;

	for (i = 0; i < 3; i++)
		move[i] = backlerp * oldframe->translate[i] + frontlerp * frame->translate[i];

	v = &mesh->vertexes[framenum * mesh->num_verts];
	ov = &mesh->vertexes[oldframenum * mesh->num_verts];

	assert(mesh->num_verts < MD3_MAX_VERTS);

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
		memcpy(&texunit_diffuse.texcoord_array[coordind + 0], &mesh->stcoords[mesh->indexes[3 * i + 0]][0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 0], &r_mesh_verts[mesh->indexes[3 * i + 0]], sizeof(vec3_t));

		memcpy(&texunit_diffuse.texcoord_array[coordind + 2], &mesh->stcoords[mesh->indexes[3 * i + 1]][0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 3], &r_mesh_verts[mesh->indexes[3 * i + 1]], sizeof(vec3_t));

		memcpy(&texunit_diffuse.texcoord_array[coordind + 4], &mesh->stcoords[mesh->indexes[3 * i + 2]][0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 6], &r_mesh_verts[mesh->indexes[3 * i + 2]], sizeof(vec3_t));

		if (r_state.lighting_enabled) {  /* normal vectors for lighting */
			memcpy(&r_state.normal_array[vertind + 0], &r_mesh_norms[mesh->indexes[3 * i + 0]], sizeof(vec3_t));
			memcpy(&r_state.normal_array[vertind + 3], &r_mesh_norms[mesh->indexes[3 * i + 1]], sizeof(vec3_t));
			memcpy(&r_state.normal_array[vertind + 6], &r_mesh_norms[mesh->indexes[3 * i + 2]], sizeof(vec3_t));
		}

		coordind += 6;
		vertind += 9;
	}

	glDrawArrays(GL_TRIANGLES, 0, mesh->num_tris * 3);

	R_CheckError();
}

/**
 * @brief Draw the models in the entity list
 * @note this is only called in ca_active or ca_sequence mode
 * @sa R_DrawEntities
 */
void R_DrawAliasModel (const entity_t *e)
{
	const mAliasModel_t *mod;
	int i;

	mod = (mAliasModel_t *)&e->model->alias;

	glPushMatrix();

	glMultMatrixf(e->transform.matrix);

	if (VectorNotEmpty(e->scale))
		glScalef(e->scale[0], e->scale[1], e->scale[2]);

	/* resolve lighting for coloring */
	if (!(refdef.rdflags & RDF_NOWORLDMODEL)) {
		vec4_t color = {1, 1, 1, 1};

		/* tagged models have an origin relative to the parent entity - so we
		 * have to transform them */
		if (e->tagent) {
			vec4_t tmp;
			GLVectorTransform(e->transform.matrix, e->origin, tmp);
			R_LightPoint(tmp);
		} else {
			R_LightPoint(e->origin);
		}

		/* resolve the color, starting with the lighting result */
		VectorCopy(r_lightmap_sample.color, color);

		if (e->flags & RF_GLOW) {  /* and then adding in a pulse */
			const float f = 1.0 + sin((refdef.time + (e - R_GetEntity(0))) * 6.0);
			VectorScale(color, 1.0 + f * 0.33, color);
		}

		/* IR goggles override color
		 * don't highlight all misc_models, only actors */
		if (refdef.rdflags & RDF_IRGOGGLES && e->flags & RF_ACTOR)
			color[1] = color[2] = 0.0;

		R_Color(color);
	}

	/* the values are sane here already - see R_DrawEntities */
	R_BindTexture(mod->meshes[e->as.mesh].skins[e->skinnum].skin->texnum);

	for (i = 0; i < mod->num_meshes; i++) {
		/* locate the proper data */
		refdef.alias_count += mod->meshes[i].num_tris;
		R_DrawAliasFrameLerp(mod, &mod->meshes[i], e->as.backlerp, e->as.frame, e->as.oldframe);
	}

	/* show model bounding box */
	if (r_showbox->integer) {
		vec3_t bbox[8];
		R_EntityComputeBoundingBox(mod->frames[e->as.frame].mins, mod->frames[e->as.frame].maxs, bbox);
		R_EntityDrawBBox(bbox);
	}

	glPopMatrix();

	R_Color(NULL);
}
