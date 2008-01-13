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
#include "r_error.h"

static vec3_t r_mesh_verts[MD3_MAX_VERTS];
static vec3_t r_mesh_norms[MD3_MAX_VERTS];

static void R_DrawAliasMD3FrameLerp (const entity_t* e, const mAliasModel_t *md3, const mAliasMesh_t *mesh)
{
	int i, j, vertind, coordind;
	mAliasFrame_t *frame, *oldframe;
	vec3_t move, delta, vectors[3];
	mAliasVertex_t *v, *ov;
	float frontlerp;
	vec3_t tempangle;
	mAliasVertex_t *tri;
	mAliasCoord_t *texcoords;

	frontlerp = 1.0 - e->as.backlerp;

	frame = md3->frames + e->as.frame;
	oldframe = md3->frames + e->as.oldframe;

	VectorSubtract(e->oldorigin, e->origin, delta);
	VectorCopy(e->angles, tempangle);
	tempangle[YAW] -= 90;
	AngleVectors(tempangle, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]);	/* forward */
	move[1] = -DotProduct(delta, vectors[1]);	/* left */
	move[2] = DotProduct(delta, vectors[2]);	/* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++)
		move[i] = e->as.backlerp * move[i] + frontlerp * frame->translate[i];

	v = mesh->vertexes + e->as.frame * mesh->num_verts;
	ov = mesh->vertexes + e->as.oldframe * mesh->num_verts;

	for (i = 0; i < mesh->num_verts; i++, v++, ov++) {  /* lerp the verts */
		VectorSet(r_mesh_verts[i],
				move[0] + ov->point[0] * e->as.backlerp + v->point[0] * frontlerp,
				move[1] + ov->point[1] * e->as.backlerp + v->point[1] * frontlerp,
				move[2] + ov->point[2] * e->as.backlerp + v->point[2] * frontlerp);

		if (r_state.lighting_enabled) {  /* and the norms */
			VectorSet(r_mesh_norms[i],
					v->normal[0] + (ov->normal[0] - v->normal[0]) * e->as.backlerp,
					v->normal[1] + (ov->normal[1] - v->normal[1]) * e->as.backlerp,
					v->normal[2] + (ov->normal[2] - v->normal[2]) * e->as.backlerp);
		}
	}

	tri = mesh->vertexes;
	texcoords = mesh->stcoords;

	vertind = coordind = 0;

	for (j = 0; j < mesh->num_tris; j++, tri++) {  /* draw the tris */
		memcpy(&r_state.texcoord_array[coordind + 0], &mesh->stcoords[mesh->indexes[3 * j + 0]].st[0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 0], &r_mesh_verts[mesh->indexes[3 * j + 0]], sizeof(vec3_t));

		memcpy(&r_state.texcoord_array[coordind + 2], &mesh->stcoords[mesh->indexes[3 * j + 1]].st[0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 3], &r_mesh_verts[mesh->indexes[3 * j + 1]], sizeof(vec3_t));

		memcpy(&r_state.texcoord_array[coordind + 4], &mesh->stcoords[mesh->indexes[3 * j + 2]].st[0], sizeof(vec2_t));
		memcpy(&r_state.vertex_array_3d[vertind + 6], &r_mesh_verts[mesh->indexes[3 * j + 2]], sizeof(vec3_t));

		if (r_state.lighting_enabled) {  /* normal vectors for lighting */
			memcpy(&r_state.normal_array[vertind + 0], &r_mesh_norms[mesh->indexes[3 * j + 0]], sizeof(vec3_t));
			memcpy(&r_state.normal_array[vertind + 3], &r_mesh_norms[mesh->indexes[3 * j + 1]], sizeof(vec3_t));
			memcpy(&r_state.normal_array[vertind + 6], &r_mesh_norms[mesh->indexes[3 * j + 2]], sizeof(vec3_t));
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
void R_DrawAliasMD3Model (entity_t *e)
{
	mAliasModel_t *md3;
	image_t *skin;
	int i;

	assert(e->model->type == mod_alias_md3);

	md3 = (mAliasModel_t *)&e->model->alias;

	qglPushMatrix();

	qglMultMatrixf(e->transform.matrix);

	skin = R_AliasModelState(e->model, &e->as.mesh, &e->as.frame, &e->as.oldframe, &e->skinnum);

	R_BindTexture(skin->texnum);

	for (i = 0; i < md3->num_meshes; i++) {
		/* locate the proper data */
		c_alias_polys += md3->meshes[i].num_tris;
		R_DrawAliasMD3FrameLerp(e, md3, &md3->meshes[i]);
	}

	qglPopMatrix();

	R_Color(NULL);
}
