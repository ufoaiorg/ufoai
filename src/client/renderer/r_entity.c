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
#include "r_matrix.h"
#include "r_entity.h"
#include "r_mesh.h"
#include "r_mesh_anim.h"
#include "r_draw.h"

#define	MAX_ENTITIES	2048*2

static entity_t r_entities[MAX_ENTITIES];

/**
 * @brief setter for entity origin
 * @param[out] ent The entity to set the origin for
 * @param[in] origin The new origin for the given entity
 */
void R_EntitySetOrigin (entity_t *ent, const vec3_t origin)
{
	VectorCopy(origin, ent->origin);
}

/**
 * @brief Translates the origin of the given entity by the given offset vector
 * @param[in,out] ent The entity to translate
 * @param[in] offset The translation vector
 */
void R_EntityAddToOrigin (entity_t *ent, const vec3_t offset)
{
	VectorAdd(ent->origin, offset, ent->origin);
}

/**
 * @brief Draws the field marker entity is specified in cl_actor.c CL_AddTargeting
 * @sa CL_AddTargeting
 * @sa RF_BOX
 */
static void R_DrawBox (const entity_t * e)
{
	const vec4_t color = {e->color[0], e->color[1], e->color[2], e->alpha};

	glDisable(GL_TEXTURE_2D);

	R_Color(color);

	if (VectorNotEmpty(e->mins) && VectorNotEmpty(e->maxs)) {
		R_DrawBoundingBox(e->mins, e->maxs);
	} else {
#ifdef ANDROID
		// TODO: most probably incorrect
		vec_t points[3*4] = {	e->oldorigin[0], e->oldorigin[1], e->origin[2],
								e->oldorigin[0], e->origin[1], e->oldorigin[2],
								e->origin[0], e->origin[1], e->oldorigin[2],
								e->origin[0], e->oldorigin[1], e->origin[2],
							};
		glLineWidth(2.0f);
		glVertexPointer(3, GL_FLOAT, 0, points);
		glDrawArrays(GL_LINE_LOOP, 0, 4);
		glLineWidth(1.0f);
#else
		vec3_t upper, lower;
		const float dx = e->oldorigin[0] - e->origin[0];
		const float dy = e->oldorigin[1] - e->origin[1];
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		VectorCopy(e->origin, lower);
		VectorCopy(e->origin, upper);
		upper[2] = e->oldorigin[2];

		glLineWidth(2.0f);
		glBegin(GL_QUAD_STRIP);
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[0] += dx;
		upper[0] += dx;
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[1] += dy;
		upper[1] += dy;
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[0] -= dx;
		upper[0] -= dx;
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[1] -= dy;
		upper[1] -= dy;
		glVertex3fv(lower);
		glVertex3fv(upper);
		glEnd();

		glLineWidth(1.0f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	}

	glEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

static image_t *cellIndicator;

/**
 * @brief Draws a marker on the ground to indicate pathing CL_AddPathingBox
 * @sa CL_AddPathing
 * @sa RF_BOX
 */
static void R_DrawFloor (const entity_t * e)
{
	float dx;
	const vec4_t color = {e->color[0], e->color[1], e->color[2], e->alpha};
	const float size = 4.0;

	if (cellIndicator == NULL) {
		cellIndicator = R_FindImage("pics/sfx/cell", it_pic);
	}

	glDisable(GL_DEPTH_TEST);

	R_Color(color);
	R_BindTexture(cellIndicator->texnum);

	dx = PLAYER_WIDTH * 2;

	/* circle points */
	GLfloat points[4*3] = {	e->origin[0]-size, e->origin[1]+dx+size, e->origin[2],
							e->origin[0]+dx+size, e->origin[1]+dx+size, e->origin[2],
							e->origin[0]+dx+size, e->origin[1]-size, e->origin[2],
							e->origin[0]-size, e->origin[1]-size, e->origin[2] };
	GLfloat texcoords[4*2] = {	0.0, 1.0,
								1.0, 1.0,
								1.0, 0.0,
								0.0, 0.0 };
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, points);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glEnable(GL_DEPTH_TEST);

	R_Color(NULL);
}

/**
 * @brief Draws an arrow between two points
 * @sa CL_AddArrow
 * @sa RF_BOX
 */
static void R_DrawArrow (const entity_t * e)
{
	vec3_t upper, mid, lower;
	const vec4_t color = {e->color[0], e->color[1], e->color[2], e->alpha};

	VectorCopy(e->origin, upper);
	upper[0] += 2;

	VectorCopy(e->origin, mid);
	mid[1] += 2;

	VectorCopy(e->origin, lower);
	lower[2] += 2;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);

	R_Color(color);

	GLfloat points[3*4] = {	e->oldorigin[0], e->oldorigin[1], e->oldorigin[2],
							upper[0], upper[1], upper[2],
							mid[0], mid[1], mid[2],
							lower[0], lower[1], lower[2] };
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, points);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

static image_t *selectedActorIndicator;
static image_t *actorIndicator;

/**
 * @brief Draws shadow and highlight effects for the entities (actors)
 * @note The origins are already transformed
 */
static void R_DrawEntityEffects (void)
{
	int i;

	R_EnableBlend(qtrue);

	if (actorIndicator == NULL) {
		selectedActorIndicator = R_FindImage("pics/sfx/actor_selected", it_pic);
		actorIndicator = R_FindImage("pics/sfx/actor", it_pic);
	}

	for (i = 0; i < refdef.numEntities; i++) {
		const entity_t *e = &r_entities[i];

		if (e->flags <= RF_BOX)
			continue;

		glPushMatrix();
		glMultMatrixf(e->transform.matrix);

		if (r_shadows->integer && (e->flags & (RF_SHADOW | RF_BLOOD))) {
			if (e->flags & RF_SHADOW) {
				R_BindTexture(shadow->texnum);
			} else {
				assert(e->deathTexture);
				R_BindTexture(e->deathTexture->texnum);
			}

			GLfloat points[3*4] = {	-18.0, 14.0, -28.5,
									10.0, 14.0, -28.5,
									10.0, -14.0, -28.5,
									-18.0, -14.0, -28.5 };
			GLfloat texcoords[2*4] = {	0.0, 1.0,
										1.0, 1.0,
										1.0, 0.0,
										0.0, 0.0 };
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, points);
			glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}


		if (e->flags & RF_ACTOR) {
			const float size = 15.0;
			int texnum;
			/* draw the circles for team-members and allied troops */
			vec4_t color = {1, 1, 1, 1};
			glDisable(GL_DEPTH_TEST);

			if (e->flags & RF_MEMBER)
				Vector4Set(color, 0, 1, 0, 0.5);
			else if (e->flags & RF_ALLIED)
				Vector4Set(color, 0, 1, 0.5, 0.5);
			else if (e->flags & RF_NEUTRAL)
				Vector4Set(color, 1, 1, 0, 0.5);
			else
				Vector4Set(color, 1, 0, 0, 0.5);

			if (e->flags & RF_SELECTED)
				texnum = selectedActorIndicator->texnum;
			else
				texnum = actorIndicator->texnum;

			R_BindTexture(texnum);
			R_Color(color);

			/* circle points */
			GLfloat points[3*4] = {	-size, size, -27.5,
									size, size, -27.5,
									size, -size, -27.5,
									-size, -size, -27.5 };
			GLfloat texcoords[2*4] = {	0.0, 1.0,
										1.0, 1.0,
										1.0, 0.0,
										0.0, 0.0 };
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, points);
			glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			glEnable(GL_DEPTH_TEST);
			R_Color(NULL);
		}
		glPopMatrix();
	}

}

/**
 * @brief Draws a list of brush model entities (inline models)
 * @param[in] ents The entity list to render (brush model entities)
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
 * @brief Draws the list of entities
 * @param[in,out] ents The list of entities that are going to get rendered
 * @sa R_DrawEntities
 */
static void R_DrawMeshEntities (entity_t *ents)
{
	entity_t *e;

	e = ents;

	while (e) {
		switch (e->model->type) {
		case mod_alias_dpm:
		case mod_alias_md2:
		case mod_alias_md3:
		case mod_obj:
			R_DrawAliasModel(e);
			break;
		default:
			break;
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

	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		R_EnableLighting(r_state.world_program, qtrue);
	}
	R_DrawMeshEntities(ents);
	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		R_EnableLighting(NULL, qfalse);
	}
}

/**
 * Merge sort merge helper function.
 */
static entity_t* R_MergeSortMerge (entity_t *a, entity_t *b)
{
	entity_t *c;

	if (a == NULL)
		return b;

	if (b == NULL)
		return a;

	if (a->distanceFromViewOrigin > b->distanceFromViewOrigin) {
		c = a;
		c->next = R_MergeSortMerge(a->next, b);
	} else {
		c = b;
		c->next = R_MergeSortMerge(a, b->next);
	}

	return c;
}

/**
 * @brief Merge sort for the entity list
 *
 * @note We can't use in-place merging of the entities array because there are references
 * in this array that must stay intact (references for tagged models e.g.).
 *
 * @return the first entity to render
 */
static entity_t* R_MergeSortEntList (entity_t *c)
{
	entity_t *a, *b;

	if (c == NULL || c->next == NULL)
		return c;

	a = c;
	b = c->next;
	while (b != NULL && b->next != NULL) {
		c = c->next;
		b = b->next->next;
	}
	b = c->next;
	c->next = NULL;

	return R_MergeSortMerge(R_MergeSortEntList(a), R_MergeSortEntList(b));
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawBlendMeshEntities (entity_t *ents)
{
	if (!ents)
		return;

	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		R_EnableLighting(r_state.world_program, qtrue);
	}
	R_EnableBlend(qtrue);

	R_DrawMeshEntities(R_MergeSortEntList(ents));

	R_EnableBlend(qfalse);
	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		R_EnableLighting(NULL, qfalse);
	}
}

/**
 * @brief Draw replacement model (e.g. when model wasn't found)
 * @sa R_DrawNullEntities
 */
static void R_DrawNullModel (const entity_t *e)
{
	int i;

	R_EnableTexture(&texunit_diffuse, qfalse);

	glPushMatrix();
	glMultMatrixf(e->transform.matrix);

#ifdef ANDROID
	vec_t points[3*6] = { 0, 0, -16 };
	for (i = 0; i <= 4; i++) {
		points[i*3+3] = 16 * cos(i * (M_PI / 2));
		points[i*3+4] = 16 * sin(i * (M_PI / 2));
		points[i*3+5] = 0;
	}
	glVertexPointer(3, GL_FLOAT, 0, points);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 6);

	vec_t points2[3*6] = { 0, 0, 16 };
	for (i = 4; i >= 0; i--) {
		points2[i*3+3] = 16 * cos(i * (M_PI / 2));
		points2[i*3+4] = 16 * sin(i * (M_PI / 2));
		points2[i*3+5] = 0;
	}
	glVertexPointer(3, GL_FLOAT, 0, points2);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
#else
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, -16);
	for (i = 0; i <= 4; i++)
		glVertex3f(16 * cos(i * (M_PI / 2)), 16 * sin(i * (M_PI / 2)), 0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, 16);
	for (i = 4; i >= 0; i--)
		glVertex3f(16 * cos(i * (M_PI / 2)), 16 * sin(i * (M_PI / 2)), 0);
	glEnd();
#endif
	glPopMatrix();

	R_EnableTexture(&texunit_diffuse, qtrue);
}

static void R_DrawSpecialEntities (const entity_t *ents)
{
	const entity_t *e;

	if (!ents)
		return;

	e = ents;

	R_EnableBlend(qtrue);

	while (e) {
		if (e->flags & RF_BOX) {
			R_DrawBox(e);
		} else if (e->flags & RF_PATH) {
			R_DrawFloor(e);
		} else if (e->flags & RF_ARROW) {
			R_DrawArrow(e);
		}
		e = e->next;
	}

	R_EnableBlend(qfalse);
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
	transform_t *t;
	float *mp;
	float mt[16], mc[16];

	/* check if this entity is already transformed */
	t = &e->transform;

	if (t->processing)
		Com_Error(ERR_DROP, "Ring in entity transformations!");

	if (t->done)
		return t->matrix;

	/* process this matrix */
	t->processing = qtrue;
	mp = NULL;

	/* do parent object transformations first */
	if (e->tagent) {
		/* tag transformation */
		const float *tag = R_GetTagMatrix(e->tagent->model, e->tagname);
		if (tag) {
			const dMD2tag_t *taghdr = (const dMD2tag_t *) e->tagent->model->alias.tagdata;
			float interpolated[16];

			/* parent transformation */
			mp = R_CalcTransform(e->tagent);

			/* do interpolation */
			R_InterpolateTransform(&e->tagent->as, taghdr->num_frames, tag, interpolated);

			/* transform */
			GLMatrixMultiply(mp, interpolated, mt);
			mp = mt;
		}
	}

	GLMatrixAssemble(e->origin, e->angles, mc);

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
 * @brief Perform a frustum cull check for a given entity
 * @param[in,out] e The entity to perform the frustum cull check for
 * @return @c false if visible, @c true is the origin of the entity is outside the
 * current frustum view
 */
static qboolean R_CullEntity (entity_t *e)
{
	if (refdef.rendererFlags & RDF_NOWORLDMODEL)
		return qfalse;

	if (r_nocull->integer)
		return qfalse;

	if (!e->model)  /* don't bother culling null model ents */
		return qfalse;

	if (e->model->type == mod_bsp_submodel)
		return R_CullBspModel(e);
	else
		return R_CullMeshModel(e);
}

/**
 * @brief Primary entry point for drawing all entities.
 * @sa R_RenderFrame
 */
void R_DrawEntities (void)
{
	int i;
	entity_t **chain;
	entity_t *r_bsp_entities, *r_opaque_mesh_entities;
	entity_t *r_blend_mesh_entities, *r_null_entities;
	entity_t *r_special_entities;

	if (!r_drawentities->integer)
		return;

	r_bsp_entities = r_opaque_mesh_entities = r_special_entities =
		r_blend_mesh_entities = r_null_entities = NULL;

	for (i = 0; i < refdef.numEntities; i++) {
		entity_t *e = &r_entities[i];

		/* frustum cull check */
		if (R_CullEntity(e))
			continue;

		R_CalcTransform(e);

		if (!e->model) {
			if (e->flags & RF_BOX || e->flags & RF_PATH || e->flags & RF_ARROW)
				chain = &r_special_entities;
			else
				chain = &r_null_entities;
		} else {
			const image_t *skin;
			switch (e->model->type) {
			case mod_bsp_submodel:
				chain = &r_bsp_entities;
				break;
			case mod_alias_dpm:
			case mod_alias_md2:
			case mod_alias_md3:
			case mod_obj:
				skin = R_AliasModelState(e->model, &e->as.mesh, &e->as.frame, &e->as.oldframe, &e->skinnum);
				if (skin == NULL || skin->texnum == 0)
					Com_Error(ERR_DROP, "Model '%s' has no skin assigned", e->model->name);
				if (skin->has_alpha || e->flags & RF_TRANSLUCENT)
					chain = &r_blend_mesh_entities;
				else
					chain = &r_opaque_mesh_entities;
				break;
			default:
				Com_Error(ERR_DROP, "Unknown model type in R_DrawEntities entity chain: %i (%s)",
						e->model->type, e->model->name);
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
	R_DrawSpecialEntities(r_special_entities);
	R_DrawNullEntities(r_null_entities);
	R_DrawEntityEffects();
}

/**
 * @brief Get the next free entry in the entity list (the last one)
 * @note This can't overflow, because R_AddEntity checks the bounds
 * @sa R_AddEntity
 */
entity_t *R_GetFreeEntity (void)
{
	if (refdef.numEntities >= MAX_ENTITIES)
		Com_Error(ERR_DROP, "R_GetFreeEntity: MAX_ENTITIES exceeded");
	return &r_entities[refdef.numEntities];
}

/**
 * @brief Returns a specific entity from the list
 */
entity_t *R_GetEntity (int id)
{
	if (id < 0 || id >= refdef.numEntities)
		return NULL;
	return &r_entities[id];
}

/**
 * @brief Adds a copy of the specified entity to the list of all known render entities.
 * @sa R_GetFreeEntity
 * @return The position of the entity in the render entity array or @c -1 in case the entity wasn't added.
 */
int R_AddEntity (const entity_t *ent)
{
	if (refdef.numEntities >= MAX_ENTITIES)
		Com_Error(ERR_DROP, "R_AddEntity: MAX_ENTITIES exceeded");

	/* don't add the bsp tiles from random map assemblies */
	if (ent->model && ent->model->type == mod_bsp)
		return -1;

	r_entities[refdef.numEntities] = *ent;
	r_entities[refdef.numEntities].distanceFromViewOrigin = VectorDist(ent->origin, refdef.viewOrigin);

	refdef.numEntities++;

	return refdef.numEntities - 1;
}
