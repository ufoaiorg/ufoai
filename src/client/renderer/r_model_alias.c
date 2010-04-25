/**
 * @file r_model_alias.c
 * @brief shared alias model loading code (md2, md3)
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
#include "../../shared/parse.h"

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

void R_ModLoadAnims (mAliasModel_t *mod, void *buffer)
{
	const char *text, *token;
	mAliasAnim_t *anim;
	int n;

	for (n = 0, text = buffer; text; n++)
		Com_Parse(&text);
	n /= 4;
	if (n > MAX_ANIMS)
		n = MAX_ANIMS;

	mod->animdata = (mAliasAnim_t *) Mem_PoolAlloc(n * sizeof(*mod->animdata), vid_modelPool, 0);
	anim = mod->animdata;
	text = buffer;
	mod->num_anims = 0;

	do {
		/* get the name */
		token = Com_Parse(&text);
		if (!text)
			break;
		Q_strncpyz(anim->name, token, sizeof(anim->name));

		/* get the start */
		token = Com_Parse(&text);
		if (!text)
			break;
		anim->from = atoi(token);
		if (anim->from < 0)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: negative start frame for %s", mod->animname);
		else if (anim->from > mod->num_frames)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: start frame is higher than models frame count (%i) (model: %s)",
					mod->num_frames, mod->animname);

		/* get the end */
		token = Com_Parse(&text);
		if (!text)
			break;
		anim->to = atoi(token);
		if (anim->to < 0)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: negative start frame for %s", mod->animname);
		else if (anim->to > mod->num_frames)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: end frame is higher than models frame count (%i) (model: %s)",
					mod->num_frames, mod->animname);

		/* get the fps */
		token = Com_Parse(&text);
		if (!text)
			break;
		anim->time = (atof(token) > 0.01) ? (1000.0 / atof(token)) : (1000.0 / 0.01);

		/* add it */
		mod->num_anims++;
		anim++;
	} while (mod->num_anims < MAX_ANIMS);
}

/**
 * @brief Calculates the normals and tangents of a mesh model
 * @param[in,out] mesh The model mesh data that is used to calculate the normals and tangents.
 * The calculated data is stored here, too.
 */
void R_ModCalcNormalsAndTangents (mAliasMesh_t *mesh)
{
	/* count unique verts */
	int numUniqueVerts = 0;
	int uniqueVerts[MD2_MAX_VERTS];
	int vertRemap[MD2_MAX_VERTS];
	int i, j;
	vec3_t triangleNormals[MD2_MAX_TRIANGLES];
	vec3_t triangleTangents[MD2_MAX_TRIANGLES];
	vec3_t triangleCotangents[MD2_MAX_TRIANGLES];
	vec3_t normals[MD2_MAX_VERTS];
	vec4_t tangents[MD2_MAX_VERTS];
	mAliasVertex_t *vertexes = mesh->vertexes;
	mAliasCoord_t *stcoords = mesh->stcoords;
	const int32_t *indexArray = mesh->indexes;
	const int numVerts = mesh->num_verts;
	const int numIndexes = mesh->num_tris * 3;

	/* figure out which verticies are shared between which triangles */
	for (i = 0; i < numVerts; i++) {
		qboolean found = qfalse;

		for (j = 0; j < numUniqueVerts; j++) {
			if (VectorCompare(vertexes[uniqueVerts[j]].point, vertexes[i].point)) {
				vertRemap[i] = j;
				found = qtrue;
				break;
			}
		}

		if (!found) {
			vertRemap[i] = numUniqueVerts;
			uniqueVerts[numUniqueVerts++] = i;
		}
	}

	/* calculate per-triangle surface normals */
	for (i = 0, j = 0; i < numIndexes; i += 3, j++) {
		vec3_t dir1, dir2;

		/* calculate two mostly perpendicular edge directions */
		VectorSubtract(vertexes[indexArray[i + 0]].point, vertexes[indexArray[i + 1]].point, dir1);
		VectorSubtract(vertexes[indexArray[i + 2]].point, vertexes[indexArray[i + 1]].point, dir2);

		/* we have two edge directions, we can calculate a third vector from
		 * them, which is the direction of the surface normal */
		CrossProduct(dir1, dir2, triangleNormals[j]);
		VectorNormalize(triangleNormals[j]);
	}

	/* calculate per-triangle tangents and cotangents */
	for (i = 0, j = 0; i < numIndexes; i += 3, j++) {
		vec3_t v1, v2, v3;
		vec2_t w1, w2, w3;
		float x1, x2, y1, y2, z1, z2, s1, s2, t1, t2, r;

		/* vertex coordinates */
		VectorCopy(vertexes[indexArray[i + 0]].point, v1);
		VectorCopy(vertexes[indexArray[i + 1]].point, v2);
		VectorCopy(vertexes[indexArray[i + 2]].point, v3);

		/* texture coordinates */
		Vector2Copy(stcoords[indexArray[i + 0]], w1);
		Vector2Copy(stcoords[indexArray[i + 1]], w2);
		Vector2Copy(stcoords[indexArray[i + 2]], w3);

		/* triangle edge directions */
		x1 = v2[0] - v1[0];
		x2 = v3[0] - v1[0];
		y1 = v2[1] - v1[1];
		y2 = v3[1] - v1[1];
		z1 = v2[2] - v1[2];
		z2 = v3[2] - v1[2];

		/* texture coordinate edge directions */
		s1 = w2[0] - w1[0];
		s2 = w3[0] - w1[0];
		t1 = w2[1] - w1[1];
		t2 = w3[1] - w1[1];

		/* calculate tangent and cotangent */
		r = 1.0F / (s1 * t2 - s2 * t1);
		VectorSet(triangleTangents[j], (t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
		VectorSet(triangleCotangents[j], (s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);
	}

	/* average all triangle normals and tangents */
	for (i = 0; i < numUniqueVerts; i++) {
		vec3_t normal, tangent, cotangent, v;
		float handedness;
		int k;

		VectorClear(normal);
		VectorClear(tangent);
		VectorClear(cotangent);

		for (j = 0, k = 0; j < numIndexes; j += 3, k++) {
			if (vertRemap[indexArray[j + 0]] == i || vertRemap[indexArray[j + 1]] == i || vertRemap[indexArray[j + 2]]
					== i) {
				VectorAdd(normal, triangleNormals[k], normal);
				VectorAdd(tangent, triangleTangents[k], tangent);
				VectorAdd(cotangent, triangleCotangents[k], cotangent);
			}
		}

		VectorNormalize(normal);

		/* calculate handedness */
		CrossProduct(normal, tangent, v);
		if (DotProduct(v, cotangent) < 0.0)
			handedness = -1.0;
		else
			handedness = 1.0;

		/* Grahm-Schmidt orthogonalization */
		VectorSubtract(tangent, normal, v);
		VectorMul(DotProduct(tangent, normal), v, tangent);
		VectorNormalize(tangent);

		/* copy normalized results to arrays */
		VectorCopy(normal, normals[i]);
		VectorCopy(tangent, tangents[i]);
		tangents[i][3] = handedness;
	}

	/* copy normals and tangents back */
	for (i = 0; i < numVerts; i++) {
		VectorCopy(normals[vertRemap[i]], vertexes[i].normal);
		Vector4Copy(tangents[vertRemap[i]], vertexes[i].tangent);
	}
}
