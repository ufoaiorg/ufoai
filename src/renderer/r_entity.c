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
#include "r_error.h"

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
 * @brief Draws the field marker entity is specified in cl_actor.c CL_AddTargeting
 * @sa CL_AddTargeting
 * @sa RF_BOX
 */
void R_DrawBox (const entity_t * e)
{
	vec3_t upper, lower;
	float dx, dy;
	const vec4_t color = {e->angles[0], e->angles[1], e->angles[2], e->alpha};

	qglDisable(GL_TEXTURE_2D);
	if (!r_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	qglEnable(GL_LINE_SMOOTH);

	R_Color(color);

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

	qglDisable(GL_LINE_SMOOTH);
	if (!r_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	qglEnable(GL_TEXTURE_2D);
}

/**
 * @brief Draws shadow and highlight effects for the entities (actors)
 * @note The origins are already transformed
 */
static void R_DrawEntityEffects (void)
{
	int i;
	entity_t *e;

	for (i = 0; i < r_numEntities; i++) {
		e = &r_entities[i];

		if (!e->flags)
			continue;

		qglPushMatrix();
		qglMultMatrixf(e->transform.matrix);

		/* draw a highlight icon over this entity */
		if (e->flags & RF_HIGHLIGHT)
			R_DrawHighlight(e);

		if (r_shadows->integer && (e->flags & (RF_SHADOW | RF_BLOOD))) {
			if (e->flags & RF_SHADOW)
				R_Bind(shadow->texnum);
			else
				R_Bind(blood->texnum);

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
			} else if (e->flags & RF_ALLIED) {
				if (e->flags & RF_SELECTED)
					Vector4Set(color, 0, 0.5, 1, 1);
				else
					Vector4Set(color, 0, 0.5, 1, 0.3);
			} else
				Vector4Set(color, 0, 1, 0, 1);

			R_Color(color);

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

			qglDisable(GL_LINE_SMOOTH);
			qglEnable(GL_TEXTURE_2D);
			qglEnable(GL_DEPTH_TEST);
		}
		qglPopMatrix();
	}

	R_Color(NULL);
}

static void R_DrawBspEntities (entity_t *ents)
{
	entity_t *e;

	if (!ents)
		return;

	e = ents;

	while (e) {
		R_DrawBrushModel(e);
		e = e->next;
	}

	R_DisableEffects();
}

static void R_DrawMeshEntities (entity_t *ents)
{
	entity_t *e;

	e = ents;

	while (e) {
		if (e->flags & RF_BOX) {
			R_DrawBox(e);
		} else {
			switch (e->model->type) {
			case mod_alias_md2:
				R_DrawAliasMD2Model(e);
				break;
			case mod_alias_md3:
				R_DrawAliasMD3Model(e);
				break;
			default:
				break;
			}
		}
		e = e->next;
	}
}

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

static void R_DrawAlphaMeshEntities (entity_t *ents)
{
	if (!ents)
		return;

	R_EnableBlend(qtrue);
	R_DrawMeshEntities(ents);
	R_EnableBlend(qfalse);
}

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
			dtag_t *taghdr;
			const char *name;
			float *tag;
			float interpolated[16];

			taghdr = (dtag_t *) e->tagent->model->alias.tagdata;

			/* find the right tag */
			name = (char *) taghdr + taghdr->ofs_names;
			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME)
				if (!strcmp(name, e->tagname)) {
					/* found the tag (matrix) */
					tag = (float *) ((byte *) taghdr + taghdr->ofs_tags);
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

	/* fill in edge values */
	mc[3] = mc[7] = mc[11] = 0.0;
	mc[15] = 1.0;

	/* add rotation */
	VectorCopy(e->angles, angles);
/*	angles[YAW] = -angles[YAW]; */

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

void R_DrawEntities (void)
{
	entity_t *e, **chain;
	int i;
	entity_t *r_bsp_entities, *r_opaque_mesh_entities;
	entity_t *r_alpha_mesh_entities, *r_null_entities;

	if (!r_drawentities->integer)
		return;

	r_bsp_entities = r_opaque_mesh_entities =
		r_alpha_mesh_entities = r_null_entities = NULL;

	for (i = 0; i < r_numEntities; i++) {
		e = &r_entities[i];
		R_CalcTransform(e);

		if (!e->model) {
			if (e->flags != RF_BOX)
				chain = &r_null_entities;
			else if (e->flags & RF_BOX)
				chain = &r_alpha_mesh_entities;
			else
				continue;
		} else {
			switch (e->model->type) {
			case mod_brush:
				chain = &r_bsp_entities;
				break;
			default:
				if (e->flags & RF_TRANSLUCENT)
					chain = &r_alpha_mesh_entities;
				else
					chain = &r_opaque_mesh_entities;
				break;
			}
		}
		e->next = *chain;
		*chain = e;
	}

	R_DrawBspEntities(r_bsp_entities);
	R_DrawOpaqueMeshEntities(r_opaque_mesh_entities);
	R_DrawAlphaMeshEntities(r_alpha_mesh_entities);
	R_Color(NULL);
	R_DrawNullEntities(r_null_entities);

	R_EnableBlend(qtrue);
	R_DrawEntityEffects();
	R_EnableBlend(qfalse);
}
