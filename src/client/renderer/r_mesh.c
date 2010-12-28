/**
 * @file r_mesh.c
 * @brief Mesh Model drawing code
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "r_draw.h"

static void R_TransformModelDirect (modelInfo_t * mi)
{
	/* translate and rotate */
	glTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);

	glRotatef(mi->angles[0], 0, 0, 1);
	glRotatef(mi->angles[1], 0, 1, 0);
	glRotatef(mi->angles[2], 1, 0, 0);

	/* scale by parameters */
	if (mi->scale)
		glScalef(mi->scale[0], mi->scale[1], mi->scale[2]);
	if (mi->center)
		glTranslatef(mi->center[0], mi->center[1], mi->center[2]);
}



/**
 * @brief Draws an animated, colored shell for the specified entity. Rather than
 * re-lerping or re-scaling the entity, the currently bound vertex arrays
 * are simply re-drawn using a small depth offset and varying texcoord delta.
 */
static void R_DrawMeshModelShell (const mAliasMesh_t *mesh, const vec4_t color)
{
	/* check whether rgb is set */
	if (!VectorNotEmpty(color))
		return;

	R_Color(color);

	R_BindTexture(r_envmaptextures[1]->texnum);

	R_EnableShell(qtrue);

	glDrawArrays(GL_TRIANGLES, 0, mesh->num_tris * 3);

	R_EnableShell(qfalse);

	R_Color(NULL);
}

/**
 * @brief Animated model render function
 * @see R_DrawAliasStatic
 */
static void R_DrawAliasFrameLerp (mAliasModel_t* mod, mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum, const vec4_t shellColor)
{
	R_FillArrayData(mod, mesh, backlerp, framenum, oldframenum, qfalse);

	R_EnableAnimation(mesh, backlerp, qtrue);

	glDrawArrays(GL_TRIANGLES, 0, mesh->num_tris * 3);

	R_DrawMeshModelShell(mesh, shellColor);

	R_EnableAnimation(NULL, 0.0, qfalse);

	R_CheckError();
}

/**
 * @brief Static model render function
 * @sa R_DrawAliasFrameLerp
 */
static void R_DrawAliasStatic (const mAliasMesh_t *mesh, const vec4_t shellColor)
{
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mesh->verts);
	R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mesh->normals);
	if (r_state.bumpmap_enabled || r_state.dynamic_lighting_enabled)
		R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, mesh->tangents);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mesh->texcoords);

	glDrawArrays(GL_TRIANGLES, 0, mesh->num_tris * 3);

	R_DrawMeshModelShell(mesh, shellColor);

	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	if (r_state.bumpmap_enabled || r_state.dynamic_lighting_enabled)
		R_BindDefaultArray(GL_TANGENT_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
}

/**
 * @brief Searches the tag data for the given name
 * @param[in] mod The model to search the tag data. Can be @c NULL
 * @param[in] tagName The name of the tag to get the matrix for. Might not be @c NULL
 * @return The tag matrix. In case the model has no tag data assigned, @c NULL is returned. The same
 * is true if the given tag name was not found in the assigned tag data.
 * @note Matrix format:
 * @li 0-11: rotation (forward, right, up)
 * @li 12-14: translation
 * @li 15:
 */
const float* R_GetTagMatrix (const model_t* mod, const char* tagName)
{
	int i;
	const dMD2tag_t *taghdr;
	const char *name;

	if (!mod)
		return NULL;

	taghdr = (const dMD2tag_t *) mod->alias.tagdata;

	/* no tag data found for this model */
	if (taghdr == NULL)
		return NULL;

	assert(tagName);

	/* find the right tag */
	name = (const char *) taghdr + taghdr->ofs_names;
	for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME) {
		if (Q_streq(name, tagName)) {
			/* found the tag (matrix) */
			const float *tag = (const float *) ((const byte *) taghdr + taghdr->ofs_tags);
			tag += i * 16 * taghdr->num_frames;
			return tag;
		}
	}
	return NULL;
}

/**
 * @brief Compute scale and center for a model info data structure
 * @param[in] boxSize The size the model should fit into
 * @param[in,out] mi The model info that contains the model that should be scaled
 * @param[out] scale The scale vector
 * @param[out] center The center of the model (center of the model's bounding box)
 * @note The scale and center vectors are parameters here because the @c modelInfo_t
 * struct only holds pointers to the vectors.
 * @todo Take the rotation info from @c modelInfo_t into account
 */
void R_ModelAutoScale (const vec2_t boxSize, modelInfo_t *mi, vec3_t scale, vec3_t center)
{
	const float width = mi->model->maxs[0] - mi->model->mins[0];
	const float height = mi->model->maxs[2] - mi->model->mins[2];
	const float factorX = boxSize[0] / width;
	const float factorY = boxSize[1] / height;
	const float size = min(factorX, factorY);

	/* get center */
	VectorCenterFromMinsMaxs(mi->model->mins, mi->model->maxs, center);
	VectorNegate(center, center);
	VectorSet(scale, size, size, size);

	mi->center = center;
	mi->scale = scale;
}

/**
 * @brief Draws a model in 2d mode (for rendering model data from the ui)
 * @param[in,out] mi All the needed model information to render the model
 * @param[in,out] pmi The model information of the parent model. This is used
 * in those cases, where the model that should get rendered here is placed relativly
 * to an already existing model in the world.
 * @param[in] tagname If a parent model is given, a @c tagname is given in most cases, too. It's used
 * to transform the model location relative to the parent model location again. E.g. a
 * @c tagname of tag_rweapon will transform the location to the right hand of an actor.
 * @sa R_DrawAliasModel
 */
void R_DrawModelDirect (modelInfo_t * mi, modelInfo_t * pmi, const char *tagname)
{
	image_t *skin;
	mAliasMesh_t *mesh;

	if (!mi->name || mi->name[0] == '\0')
		return;

	/* register the model */
	mi->model = R_RegisterModelShort(mi->name);

	/* check if the model exists */
	if (!mi->model) {
		Com_Printf("No model found for '%s'\n", mi->name);
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

		/* tag transformation */
		if (tagname) {
			const float *tag = R_GetTagMatrix(pmi->model, tagname);
			if (tag) {
				float interpolated[16];
				animState_t as;
				dMD2tag_t *taghdr = (dMD2tag_t *) pmi->model->alias.tagdata;

				/* do interpolation */
				as.frame = pmi->frame;
				as.oldframe = pmi->oldframe;
				as.backlerp = pmi->backlerp;
				R_InterpolateTransform(&as, taghdr->num_frames, tag, interpolated);

				/* transform */
				glMultMatrixf(interpolated);
				R_CheckError();
			}
		}
	}

	/* transform */
	R_TransformModelDirect(mi);

	/* we have to reenable this here - we are in 2d mode here already */
	glEnable(GL_DEPTH_TEST);

	/* draw it */
	R_BindTexture(skin->texnum);

	/* draw the model */
	mesh = &mi->model->alias.meshes[0];
	refdef.aliasCount += mesh->num_tris;
	if (mi->model->alias.num_frames == 1)
		R_DrawAliasStatic(mesh, vec4_origin);
	else
		R_DrawAliasFrameLerp(&mi->model->alias, mesh, mi->backlerp, mi->frame, mi->oldframe, vec4_origin);

	/* show model bounding box */
	if (r_showbox->integer)
		R_DrawBoundingBox(mi->model->alias.frames[mi->frame].mins, mi->model->alias.frames[mi->frame].maxs);

	glDisable(GL_DEPTH_TEST);

	glPopMatrix();

	R_Color(NULL);
}

/**
 * @brief Renders a particle model for the battlescape
 * @param[in,out] mi The model information that is used to render the particle model.
 * @sa R_DrawPtlModel
 */
void R_DrawModelParticle (modelInfo_t * mi)
{
	image_t *skin;
	mAliasMesh_t *mesh;

	/* check if the model exists */
	if (!mi->model)
		return;

	skin = R_AliasModelState(mi->model, &mi->mesh, &mi->frame, &mi->oldframe, &mi->skin);
	if (skin == NULL) {
		Com_Printf("Model '%s' is broken\n", mi->name);
		return;
	}

	R_Color(mi->color);

	glPushMatrix();

	glTranslatef(mi->origin[0], mi->origin[1], mi->origin[2]);
	glRotatef(mi->angles[YAW], 0, 0, 1);
	glRotatef(mi->angles[PITCH], 0, 1, 0);
	glRotatef(-mi->angles[ROLL], 1, 0, 0);

	/* draw it */
	R_BindTexture(skin->texnum);

	/* draw the model */
	mesh = &mi->model->alias.meshes[0];
	refdef.aliasCount += mesh->num_tris;
	if (mi->model->alias.num_frames == 1)
		R_DrawAliasStatic(mesh, vec4_origin);
	else
		R_DrawAliasFrameLerp(&mi->model->alias, mesh, mi->backlerp, mi->frame, mi->oldframe, vec4_origin);

	/* show model bounding box */
	if (r_showbox->integer)
		R_DrawBoundingBox(mi->model->alias.frames[mi->frame].mins, mi->model->alias.frames[mi->frame].maxs);

	glPopMatrix();

	R_Color(NULL);
}

/**
 * @brief Checks whether a model is visible in the current scene
 * @param[in] e The entity to check
 * @return @c false if visible, @c true if the entity is outside the current view
 */
qboolean R_CullMeshModel (const entity_t *e)
{
	int i;
	uint32_t aggregatemask;
	vec3_t mins, maxs, origin, angles;
	vec3_t vectors[3];
	vec4_t bbox[8];

	/* this is an approximation of the origin of the tagged model - we are
	 * using the origin of the parent entity to check the culling for the model
	 * that is placed relative to the tag */
	if (e->tagent) {
		VectorCopy(e->tagent->origin, origin);
		VectorCopy(e->tagent->angles, angles);
	} else {
		VectorCopy(e->origin, origin);
		VectorCopy(e->angles, angles);
	}

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

	/* rotate the bounding box */
	angles[YAW] = -angles[YAW];
	AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

	/* compute translated and rotate bounding box */
	for (i = 0; i < 8; i++) {
		vec3_t tmp;

		tmp[0] = (i & 1) ? mins[0] : maxs[0];
		tmp[1] = (i & 2) ? mins[1] : maxs[1];
		tmp[2] = (i & 4) ? mins[2] : maxs[2];

		bbox[i][0] = DotProduct(vectors[0], tmp);
		bbox[i][1] = -DotProduct(vectors[1], tmp);
		bbox[i][2] = DotProduct(vectors[2], tmp);

		VectorAdd(origin, bbox[i], bbox[i]);
	}

	/* compute a full bounding box */
	aggregatemask = ~0;

	for (i = 0; i < 8; i++) {
		int mask = 0;
		int j;
		const int size = lengthof(r_locals.frustum);

		for (j = 0; j < size; j++) {
			const cBspPlane_t *bspPlane = &r_locals.frustum[j];
			/* get the distance between the frustum normal vector and the
			 * current vector of the bounding box */
			const float f = DotProduct(bspPlane->normal, bbox[i]);
			if (f - bspPlane->dist < 0)
				mask |= (1 << j);
		}

		aggregatemask &= mask;
	}

	if (aggregatemask)
		return qtrue;

	return qfalse;
}

/**
 * @brief Searches an appropriate level-of-detail mesh for the given model
 * @param origin The origin the model should be placed to in the world
 * @param mod The model where we are searching an appropriate level-of-detail mesh for
 * @return The mesh to render
 */
static mAliasMesh_t* R_GetLevelOfDetailForModel (const vec3_t origin, const mAliasModel_t* mod)
{
	if (mod->num_meshes == 1 || (refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		return &mod->meshes[0];
	} else {
		vec3_t dist;
		vec_t length;

		/* get distance, set lod if available */
		VectorSubtract(refdef.viewOrigin, origin, dist);
		length = VectorLength(dist);
		if (mod->num_meshes > 3 && length > 700) {
			return &mod->meshes[3];
		} if (mod->num_meshes > 2 && length > 600) {
			return &mod->meshes[2];
		} else if (length > 500) {
			return &mod->meshes[1];
		}

		return &mod->meshes[0];
	}
}

static void R_DrawAliasModelBuffer (entity_t *e)
{
	mAliasModel_t *mod = (mAliasModel_t *)&e->model->alias;
	mAliasMesh_t* lodMesh;

	glPushMatrix();
	glMultMatrixf(e->transform.matrix);

	if (VectorNotEmpty(e->scale))
		glScalef(e->scale[0], e->scale[1], e->scale[2]);

	R_ResetArrayState();

	/** @todo what about the origin of a tagged model here? */
	lodMesh = R_GetLevelOfDetailForModel(e->origin, mod);
	refdef.aliasCount += lodMesh->num_tris;
	if (mod->num_frames == 1)
		R_DrawAliasStatic(lodMesh, e->shell);
	else
		R_DrawAliasFrameLerp(mod, lodMesh, e->as.backlerp, e->as.frame, e->as.oldframe, e->shell);

	glPopMatrix();
}

/**
 * @brief Draw a model from the battlescape entity list
 * @sa R_DrawEntities
 */
void R_DrawAliasModel (entity_t *e)
{
	mAliasModel_t *mod = (mAliasModel_t *)&e->model->alias;
	/* the values are sane here already - see R_DrawEntities */
	const image_t *skin = mod->meshes[e->as.mesh].skins[e->skinnum].skin;
	int i;
	float g;
	vec4_t color = {0.8, 0.8, 0.8, 1.0};

	/* IR goggles override color for entities that are affected */
	if (refdef.rendererFlags & RDF_IRGOGGLES && e->flags & RF_IRGOGGLES)
		Vector4Set(e->shell, 1.0, 0.3, 0.3, 1.0);

	if (e->flags & RF_PULSE) {  /* and then adding in a pulse */
		const float f = 1.0 + sin((refdef.time + (e->model->alias.meshes[0].num_tris)) * 6.0) * 0.33;
		VectorScale(color, 1.0 + f, color);
	}

	g = 0.0;
	/* find brightest component */
	for (i = 0; i < 3; i++) {
		if (color[i] > g)  /* keep it */
			g = color[i];
	}

	/* scale it back to 1.0 */
	if (g > 1.0)
		VectorScale(color, 1.0 / g, color);

	R_Color(color);

	assert(skin->texnum > 0);
	R_BindTexture(skin->texnum);

	R_EnableGlowMap(skin->glowmap, qtrue);

	R_UpdateLightList(e);
	R_EnableDynamicLights(e, qtrue);

	if (skin->normalmap)
		R_EnableBumpmap(skin->normalmap, qtrue);

	if (skin->specularmap)
		R_EnableSpecularMap(skin->specularmap, qtrue);

	if (skin->roughnessmap)
		R_EnableRoughnessMap(skin->roughnessmap, qtrue);

	R_DrawAliasModelBuffer(e);

	if (r_state.specularmap_enabled)
		R_EnableSpecularMap(NULL, qfalse);

	if (r_state.roughnessmap_enabled)
		R_EnableRoughnessMap(NULL, qfalse);

	R_EnableDynamicLights(NULL, qfalse);

	if (r_state.glowmap_enabled)
		R_EnableGlowMap(NULL, qfalse);

	if (r_state.bumpmap_enabled)
		R_EnableBumpmap(NULL, qfalse);

	/* show model bounding box */
	if (r_showbox->integer)
		R_DrawBoundingBox(mod->frames[e->as.frame].mins, mod->frames[e->as.frame].maxs);

	R_Color(NULL);
}
