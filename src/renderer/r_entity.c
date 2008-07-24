/**
 * @file r_entity.c
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
#include "r_entity.h"
#include "r_mesh_anim.h"
#include "r_error.h"

int r_numEntities;
static entity_t r_entities[MAX_ENTITIES];

#define HIGHLIGHT_START_Z 22
#define HIGHTLIGHT_SIZE 18
static const vec3_t r_highlightVertices[HIGHTLIGHT_SIZE] = {
	{4, 4, HIGHLIGHT_START_Z + 0},
	{0, 0, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{4, 4, HIGHLIGHT_START_Z + 0},
	{0, 0, HIGHLIGHT_START_Z + 16},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{4, 4, HIGHLIGHT_START_Z + 0},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{8, 8, HIGHLIGHT_START_Z + 16},
	{4, 4, HIGHLIGHT_START_Z + 0},
	{8, 8, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{0, 0, HIGHLIGHT_START_Z + 16},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{8, 8, HIGHLIGHT_START_Z + 16},
};
/**
 * @brief Used to draw actor highlights over the actors
 * @sa RF_HIGHLIGHT
 */
static inline void R_DrawHighlight (const entity_t * e)
{
	qglDisable(GL_TEXTURE_2D);
	R_Color(NULL);
	memcpy(r_state.vertex_array_3d, r_highlightVertices, sizeof(r_highlightVertices));
	qglDrawArrays(GL_TRIANGLES, 0, HIGHTLIGHT_SIZE);
	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief Compute the bouding box for an entity out of the mins, maxs
 * @sa R_EntityDrawBBox
 */
void R_EntityComputeBoundingBox (const vec3_t mins, const vec3_t maxs, vec4_t bbox[8])
{
	vec4_t tmp;
	int i;

	/* compute a full bounding box */
	for (i = 0; i < 8; i++) {
		tmp[0] = (i & 1) ? mins[0] : maxs[0];
		tmp[1] = (i & 2) ? mins[1] : maxs[1];
		tmp[2] = (i & 4) ? mins[2] : maxs[2];
		tmp[3] = 1.0;

		Vector4Copy(tmp, bbox[i]);
	}
}

/**
 * @brief Draws the model bounding box
 * @sa R_EntityComputeBoundingBox
 */
void R_EntityDrawBBox (vec4_t bbox[8])
{
	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw top and sides */
	qglBegin(GL_TRIANGLE_STRIP);
	qglVertex3fv(bbox[2]);
	qglVertex3fv(bbox[1]);
	qglVertex3fv(bbox[0]);
	qglVertex3fv(bbox[1]);
	qglVertex3fv(bbox[4]);
	qglVertex3fv(bbox[5]);
	qglVertex3fv(bbox[1]);
	qglVertex3fv(bbox[7]);
	qglVertex3fv(bbox[3]);
	qglVertex3fv(bbox[2]);
	qglVertex3fv(bbox[7]);
	qglVertex3fv(bbox[6]);
	qglVertex3fv(bbox[2]);
	qglVertex3fv(bbox[4]);
	qglVertex3fv(bbox[0]);
	qglEnd();

	/* Draw bottom */
	qglBegin(GL_TRIANGLE_STRIP);
	qglVertex3fv(bbox[4]);
	qglVertex3fv(bbox[6]);
	qglVertex3fv(bbox[7]);
	qglEnd();

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

/**
 * @brief Draws the field marker entity is specified in cl_actor.c CL_AddTargeting
 * @sa CL_AddTargeting
 * @sa RF_BOX
 */
static void R_DrawBox (const entity_t * e)
{
	vec3_t upper, lower;
	float dx, dy;
	const vec4_t color = {e->angles[0], e->angles[1], e->angles[2], e->alpha};

	qglDisable(GL_TEXTURE_2D);
	if (!r_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	qglEnable(GL_LINE_SMOOTH);

	R_Color(color);

	if (VectorNotEmpty(e->mins) && VectorNotEmpty(e->maxs)) {
		vec4_t bbox[8];
		R_EntityComputeBoundingBox(e->mins, e->maxs, bbox);
		R_EntityDrawBBox(bbox);
	} else {
		VectorCopy(e->origin, lower);
		VectorCopy(e->origin, upper);
		upper[2] = e->oldorigin[2];

		dx = e->oldorigin[0] - e->origin[0];
		dy = e->oldorigin[1] - e->origin[1];

		qglBegin(GL_QUAD_STRIP);
		qglVertex3fv(lower);
		qglVertex3fv(upper);
		lower[0] += dx;
		upper[0] += dx;
		qglVertex3fv(lower);
		qglVertex3fv(upper);
		lower[1] += dy;
		upper[1] += dy;
		qglVertex3fv(lower);
		qglVertex3fv(upper);
		lower[0] -= dx;
		upper[0] -= dx;
		qglVertex3fv(lower);
		qglVertex3fv(upper);
		lower[1] -= dy;
		upper[1] -= dy;
		qglVertex3fv(lower);
		qglVertex3fv(upper);
		qglEnd();
	}

	qglDisable(GL_LINE_SMOOTH);
	if (!r_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	qglEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

/**
 * @brief Draws a marker on the ground to indicate pathing CL_AddPathingBox
 * @sa CL_AddPathing
 * @sa RF_BOX
 */
static void R_DrawFloor (const entity_t * e)
{
	vec3_t upper, lower;
	float dx, dy;
	const vec4_t color = {e->angles[0], e->angles[1], e->angles[2], e->alpha};

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_LINE_SMOOTH);

	R_Color(color);

	VectorCopy(e->origin, lower);
	VectorCopy(e->origin, upper);

	dx = PLAYER_WIDTH * 2;
	dy = e->oldorigin[2];

	upper[2] += dy;

	qglBegin(GL_QUAD_STRIP);
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[0] += dx;
	upper[0] += dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[1] += dx;
	upper[1] += dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[0] -= dx;
	upper[0] -= dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[1] -= dx;
	upper[1] -= dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	qglEnd();

	lower[2] += dy;
	upper[1] += dx;

	qglBegin(GL_QUAD_STRIP);
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	lower[0] += dx;
	upper[0] += dx;
	qglVertex3fv(lower);
	qglVertex3fv(upper);
	qglEnd();
	qglDisable(GL_LINE_SMOOTH);

	qglEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

/**
 * @brief Draws shadow and highlight effects for the entities (actors)
 * @note The origins are already transformed
 */
static void R_DrawEntityEffects (void)
{
	int i;

	for (i = 0; i < r_numEntities; i++) {
		const entity_t *e = &r_entities[i];

		if (e->flags <= RF_BOX)
			continue;

		qglPushMatrix();
		qglMultMatrixf(e->transform.matrix);

		/* draw a highlight icon over this entity */
		if (e->flags & RF_HIGHLIGHT)
			R_DrawHighlight(e);

		if (r_shadows->integer && (e->flags & (RF_SHADOW | RF_BLOOD))) {
			/** @todo Shouldn't we get the texture type from the team-definition somehow? */
			if (e->flags & RF_SHADOW)
				R_BindTexture(shadow->texnum);
			else
				R_BindTexture(blood[e->state % STATE_DEAD]->texnum);

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

			R_CheckError();
		}

		if (e->flags & (RF_SELECTED | RF_ALLIED | RF_MEMBER)) {
			/* draw the circles for team-members and allied troops */
			vec4_t color = {1, 1, 1, 1};
			qglDisable(GL_DEPTH_TEST);
			qglDisable(GL_TEXTURE_2D);
			qglEnable(GL_LINE_SMOOTH);

			if (e->flags & RF_MEMBER) {
				if (e->flags & RF_SELECTED)
					Vector4Set(color, 0, 1, 0, 1);
				else
					Vector4Set(color, 0, 1, 0, 0.3);
			} else if (e->flags & RF_ALLIED)
				Vector4Set(color, 0, 0.5, 1, 0.3);
			else
				Vector4Set(color, 0, 1, 0, 1);

			R_Color(color);

#if 0
			qglLineWidth(10.0f);
			qglLineStipple(2, 0x00FF);
			qglEnable(GL_LINE_STIPPLE);
#endif

			/* circle points */
			qglBegin(GL_LINE_STRIP);
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
			R_CheckError();

#if 0
			qglLineWidth(1.0f);
			qglDisable(GL_LINE_STIPPLE);
#endif
			qglDisable(GL_LINE_SMOOTH);
			qglEnable(GL_TEXTURE_2D);
			qglEnable(GL_DEPTH_TEST);
		}
		qglPopMatrix();
	}

	R_Color(NULL);
}

/**
 * @sa R_DrawEntities
 * @sa R_DrawBrushModel
 */
static void R_DrawBspEntities (const entity_t *ents)
{
	const entity_t *e;

	if (!ents)
		return;

	e = ents;

	while (e) {
		R_DrawBrushModel(e);
		e = e->next;
	}
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawMeshEntities (const entity_t *ents)
{
	const entity_t *e;

	e = ents;

	while (e) {
		if (e->flags & RF_BOX) {
			R_DrawBox(e);
		} else if (e->flags & RF_PATH) {
			R_DrawFloor(e);
		} else {
			switch (e->model->type) {
			case mod_alias_dpm:
			case mod_alias_md2:
			case mod_alias_md3:
				R_DrawAliasModel(e);
				break;
			default:
				break;
			}
		}
		e = e->next;
	}
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawOpaqueMeshEntities (entity_t *ents)
{
	if (!ents)
		return;

	if (!(refdef.rdflags & RDF_NOWORLDMODEL))
		R_EnableLighting(qtrue);
	R_DrawMeshEntities(ents);
	if (!(refdef.rdflags & RDF_NOWORLDMODEL))
		R_EnableLighting(qfalse);
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawBlendMeshEntities (entity_t *ents)
{
	if (!ents)
		return;

	R_EnableBlend(qtrue);
	R_DrawMeshEntities(ents);
	R_EnableBlend(qfalse);
}

/**
 * @brief Draw replacement model (e.g. when model wasn't found)
 * @sa R_DrawNullEntities
 */
static void R_DrawNullModel (const entity_t *e)
{
	int i;

	qglPushMatrix();
	qglMultMatrixf(e->transform.matrix);

	qglDisable(GL_TEXTURE_2D);

	qglBegin(GL_TRIANGLE_FAN);
	qglVertex3f(0, 0, -16);
	for (i = 0; i <= 4; i++)
		qglVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	qglEnd();

	qglBegin(GL_TRIANGLE_FAN);
	qglVertex3f(0, 0, 16);
	for (i = 4; i >= 0; i--)
		qglVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	qglEnd();

	qglPopMatrix();

	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief Draw entities which models couldn't be loaded
 */
static void R_DrawNullEntities (const entity_t *ents)
{
	const entity_t *e;

	if (!ents)
		return;

	e = ents;

	while (e) {
		R_DrawNullModel(e);
		e = e->next;
	}
}

/**
 * @brief Calculates transformation matrix for the model and its tags
 * @note The transformation matrix is only calculated once
 */
static float *R_CalcTransform (entity_t * e)
{
	vec3_t angles;
	transform_t *t;
	float *mp;
	float mt[16], mc[16];
	int i;

	/* check if this entity is already transformed */
	t = &e->transform;

	if (t->processing)
		Sys_Error("Ring in entity transformations!\n");

	if (t->done)
		return t->matrix;

	/* process this matrix */
	t->processing = qtrue;
	mp = NULL;

	/* do parent object transformations first */
	if (e->tagent) {
		/* parent transformation */
		mp = R_CalcTransform(e->tagent);

		/* tag transformation */
		if (e->tagent->model && e->tagent->model->alias.tagdata) {
			dMD2tag_t *taghdr = (dMD2tag_t *) e->tagent->model->alias.tagdata;
			/* find the right tag */
			const char *name = (char *) taghdr + taghdr->ofs_names;
			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME) {
				if (!strcmp(name, e->tagname)) {
					float interpolated[16];
					/* found the tag (matrix) */
					const float *tag = (const float *) ((const byte *) taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					R_InterpolateTransform(&e->tagent->as, taghdr->num_frames, tag, interpolated);

					/* transform */
					GLMatrixMultiply(mp, interpolated, mt);
					mp = mt;

					break;
				}
			}
		}
	}

	/* fill in edge values */
	mc[3] = mc[7] = mc[11] = 0.0;
	mc[15] = 1.0;

	/* add rotation */
	VectorCopy(e->angles, angles);
	AngleVectors(angles, &mc[0], &mc[4], &mc[8]);

	/* add translation */
	mc[12] = e->origin[0];
	mc[13] = e->origin[1];
	mc[14] = e->origin[2];

	/* flip an axis */
	VectorScale(&mc[4], -1, &mc[4]);

	/* combine transformations */
	if (mp)
		GLMatrixMultiply(mp, mc, t->matrix);
	else
		memcpy(t->matrix, mc, sizeof(float) * 16);

	/* we're done */
	t->done = qtrue;
	t->processing = qfalse;

	return t->matrix;
}

/**
 * @brief Draw entities like models and cursor box
 * @sa R_RenderFrame
 */
void R_DrawEntities (void)
{
	int i;
	entity_t **chain;
	entity_t *r_bsp_entities, *r_opaque_mesh_entities;
	entity_t *r_blend_mesh_entities, *r_null_entities;
	image_t *skin;

	if (!r_drawentities->integer)
		return;

	r_bsp_entities = r_opaque_mesh_entities =
		r_blend_mesh_entities = r_null_entities = NULL;

	for (i = 0; i < r_numEntities; i++) {
		entity_t *e = &r_entities[i];
		R_CalcTransform(e);

		if (!e->model) {
			if (e->flags & RF_BOX || e->flags & RF_PATH)
				chain = &r_blend_mesh_entities;
			else
				chain = &r_null_entities;
		} else {
			switch (e->model->type) {
			case mod_bsp_submodel:
				chain = &r_bsp_entities;
				break;
			case mod_alias_dpm:
			case mod_alias_md2:
			case mod_alias_md3:
				skin = R_AliasModelState(e->model, &e->as.mesh, &e->as.frame, &e->as.oldframe, &e->skinnum);
				if (skin == NULL) {
					Com_Printf("Model '%s' is broken\n", e->model->name);
					continue;
				}
				if (skin->has_alpha || e->flags & RF_TRANSLUCENT)
					chain = &r_blend_mesh_entities;
				else
					chain = &r_opaque_mesh_entities;
				break;
			default:
				Sys_Error("Unknown model type in R_DrawEntities entity chain: %i", e->model->type);
				break;
			}
		}
		e->next = *chain;
		*chain = e;
	}

	R_DrawBspEntities(r_bsp_entities);
	R_DrawOpaqueMeshEntities(r_opaque_mesh_entities);
	R_DrawBlendMeshEntities(r_blend_mesh_entities);
	R_Color(NULL);
	R_DrawNullEntities(r_null_entities);

	R_EnableBlend(qtrue);
	R_DrawEntityEffects();
	R_EnableBlend(qfalse);
}

/**
 * @brief Get the next free entry in the entity list (the last one)
 * @note This can't overflow, because R_AddEntity checks the bounds
 * @sa R_AddEntity
 */
entity_t *R_GetFreeEntity (void)
{
	assert(r_numEntities < MAX_ENTITIES);
	return &r_entities[r_numEntities];
}

/**
 * @brief Returns a specific entity from the list
 */
entity_t *R_GetEntity (int id)
{
	if (id < 0 || id >= r_numEntities)
		return NULL;
	return &r_entities[id];
}

/**
 * @sa R_GetFreeEntity
 */
void R_AddEntity (entity_t * ent)
{
	if (r_numEntities >= MAX_ENTITIES) {
		Com_Printf("R_AddEntity: MAX_ENTITIES exceeded\n");
		return;
	}

	r_entities[r_numEntities++] = *ent;
}
