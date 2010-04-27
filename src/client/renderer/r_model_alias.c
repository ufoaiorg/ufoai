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
void R_ModCalcNormalsAndTangents (mAliasMesh_t *mesh, size_t offset)
{
	/* count unique verts */
	int i, j;
	vec3_t triangleNormals[MD2_MAX_TRIANGLES];
	vec3_t triangleTangents[MD2_MAX_TRIANGLES];
	vec3_t triangleBitangents[MD2_MAX_TRIANGLES];
	mAliasVertex_t *vertexes = &mesh->vertexes[offset];
	mAliasCoord_t *stcoords = mesh->stcoords;
	const int32_t *indexArray = mesh->indexes;
	const int numVerts = mesh->num_verts;
	const int numIndexes = mesh->num_tris * 3;

#if 0
	vec3_t normals[MD2_MAX_VERTS];
	vec4_t tangents[MD2_MAX_VERTS];
	int numUniqueVerts = 0;
	int uniqueVerts[MD2_MAX_VERTS];
	int vertRemap[MD2_MAX_VERTS];

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
#endif

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

		VectorAdd(vertexes[indexArray[i + 0]].normal, triangleNormals[j], vertexes[indexArray[i + 0]].normal);
		VectorAdd(vertexes[indexArray[i + 1]].normal, triangleNormals[j], vertexes[indexArray[i + 1]].normal);
		VectorAdd(vertexes[indexArray[i + 2]].normal, triangleNormals[j], vertexes[indexArray[i + 2]].normal);
	}

	/* calculate per-triangle tangents and bitangents */
	for (i = 0, j = 0; i < numIndexes; i += 3, j++) {
		vec3_t dir1, dir2;
		vec2_t dir1uv, dir2uv;

		/* calculate the edge directions */
		VectorSubtract(vertexes[indexArray[i + 0]].point, vertexes[indexArray[i + 1]].point, dir1);
		VectorSubtract(vertexes[indexArray[i + 2]].point, vertexes[indexArray[i + 1]].point, dir2);
		Vector2Subtract(stcoords[indexArray[i + 0]], stcoords[indexArray[i + 1]], dir1uv);
		Vector2Subtract(stcoords[indexArray[i + 2]], stcoords[indexArray[i + 1]], dir2uv);

		if ((dir1uv[1] * dir2uv[0] - dir1uv[0] * dir2uv[1]) != 0.0) {
			const float frac = 1.0 / (dir1uv[1] * dir2uv[0] - dir1uv[0] * dir2uv[1]);
			vec3_t tmp1, tmp2;

			/* calculate tangent */
			VectorMul(-1.0 * dir2uv[1] * frac, dir1, tmp1);
			VectorMul(dir1uv[1] * frac, dir2, tmp2);
			VectorAdd(tmp1, tmp2, triangleTangents[j]);

			/* calculate bitangent */
			VectorMul(-1.0 * dir2uv[0] * frac, dir1, tmp1);
			VectorMul(dir1uv[0] * frac, dir2, tmp2);
			VectorAdd(tmp1, tmp2, triangleBitangents[j]);
		}

		/* normalize */
		VectorNormalize(triangleTangents[j]);
		VectorNormalize(triangleBitangents[j]);

		/* keep a running sum for averaging */
		VectorAdd(vertexes[indexArray[i + 0]].tangent, triangleTangents[j], vertexes[indexArray[i + 0]].tangent);
		VectorAdd(vertexes[indexArray[i + 1]].tangent, triangleTangents[j], vertexes[indexArray[i + 1]].tangent);
		VectorAdd(vertexes[indexArray[i + 2]].tangent, triangleTangents[j], vertexes[indexArray[i + 2]].tangent);
		VectorAdd(vertexes[indexArray[i + 0]].bitangent, triangleBitangents[j], vertexes[indexArray[i + 0]].bitangent);
		VectorAdd(vertexes[indexArray[i + 1]].bitangent, triangleBitangents[j], vertexes[indexArray[i + 1]].bitangent);
		VectorAdd(vertexes[indexArray[i + 2]].bitangent, triangleBitangents[j], vertexes[indexArray[i + 2]].bitangent);
	}


	/* average and orthogonalize tangents */
	for (i = 0; i < numVerts; i ++) {
		vec3_t v;

		/* normalization here does shared-vertex smoothing */
		VectorNormalize(vertexes[i].normal);
		VectorNormalize(vertexes[i].tangent);
		VectorNormalize(vertexes[i].bitangent);

		/* Grahm-Schmidt orthogonalization */
		VectorMul(DotProduct(vertexes[i].tangent, vertexes[i].normal), vertexes[i].normal, v);
		VectorSubtract(vertexes[i].tangent, v, vertexes[i].tangent);
		VectorNormalize(vertexes[i].tangent);

		//if (fabs(DotProduct(vertexes[i].tangent, vertexes[i].normal)) >= 0.001)
		//	Com_Printf("%f: %s\n", DotProduct(vertexes[i].tangent, vertexes[i].normal), mesh->name);

		/* calculate handedness */
		CrossProduct(vertexes[i].normal, vertexes[i].tangent, v);
		vertexes[i].tangent[3] = (DotProduct(v, vertexes[i].bitangent) < 0.0) ? -1.0 : 1.0;
	}

#if 0
	/* average all triangle normals and tangents */
	for (i = 0; i < numUniqueVerts; i++) {
		vec3_t normal, tangent, bitangent, v;
		float handedness;
		int k;

		VectorClear(normal);
		VectorClear(tangent);
		VectorClear(bitangent);

		for (j = 0, k = 0; j < numIndexes; j += 3, k++) {
			if (vertRemap[indexArray[j + 0]] == i || vertRemap[indexArray[j + 1]] == i || vertRemap[indexArray[j + 2]]
					== i) {

				if ( VectorLengthSqr(normal) > 0 && (DotProduct(triangleNormals[k], normal) <= 0)){
					//Com_Printf("%f, %f, %f dot %f, %f, %f = %f\n", normal[0], normal[1], normal[2], triangleNormals[k][0], triangleNormals[k][1], triangleNormals[k][2], DotProduct(triangleNormals[k], normal));
				}
				if ( VectorLengthSqr(tangent) > 0 && (DotProduct(triangleTangents[k], tangent) <= 0)){
					//Com_Printf("%f, %f, %f dot %f, %f, %f\n", tangent[0], tangent[1], tangent[2], triangleTangents[k][0], triangleTangents[k][1], triangleTangents[k][2]);
				}
				VectorAdd(normal, triangleNormals[k], normal);
				VectorAdd(tangent, triangleTangents[k], tangent);
				VectorAdd(bitangent, triangleBitangents[k], bitangent);

			}
		}

		VectorNormalize(normal);
		VectorNormalize(tangent);
		VectorNormalize(bitangent);

		/* calculate handedness */
		CrossProduct(normal, tangent, v);
		if (DotProduct(v, bitangent) < 0.0)
			handedness = -1.0;
		else
			handedness = 1.0;

		/* Grahm-Schmidt orthogonalization */
		VectorMul(DotProduct(tangent, normal), normal, v);
		VectorSubtract(tangent, v, tangent);
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

#ifdef PARANOID
		if (VectorLengthSqr(vertexes[i].normal) == 0)
			Com_Printf("%s: normals[%d]=[%f, %f, %f]\n", mesh->name, i,
					vertexes[i].normal[0], vertexes[i].normal[1], vertexes[i].normal[2]);
#endif
	}
#endif
}

image_t* R_AliasModelGetSkin (const model_t* mod, const char *skin)
{
	/* arisian: bookmark */
	if (skin[0] != '.')
		return R_FindImage(skin, it_skin);
	else {
		char path[MAX_QPATH];
		char *slash, *end;

		Q_strncpyz(path, mod->name, sizeof(path));
		end = path;
		while ((slash = strchr(end, '/')) != 0)
			end = slash + 1;
		strcpy(end, skin + 1);
		return R_FindImage(path, it_skin);
	}
}

image_t* R_AliasModelState (const model_t *mod, int *mesh, int *frame, int *oldFrame, int *skin)
{
	/* check animations */
	if ((*frame >= mod->alias.num_frames) || *frame < 0) {
		Com_Printf("R_AliasModelState %s: no such frame %d (# %i)\n", mod->name, *frame, mod->alias.num_frames);
		*frame = 0;
	}
	if ((*oldFrame >= mod->alias.num_frames) || *oldFrame < 0) {
		Com_Printf("R_AliasModelState %s: no such oldframe %d (# %i)\n", mod->name, *oldFrame, mod->alias.num_frames);
		*oldFrame = 0;
	}

	if (*mesh < 0 || *mesh >= mod->alias.num_meshes)
		*mesh = 0;

	if (!mod->alias.meshes)
		return NULL;

	/* use default skin - this is never null - but maybe the placeholder texture */
	if (*skin < 0 || *skin >= mod->alias.meshes[*mesh].num_skins)
		*skin = 0;

	if (!mod->alias.meshes[*mesh].num_skins)
		Com_Error(ERR_DROP, "Model with no skins");

	if (mod->alias.meshes[*mesh].skins[*skin].skin->texnum <= 0)
		Com_Error(ERR_DROP, "Texture is already freed and no longer uploaded, texnum is invalid for model %s", mod->name);

	return mod->alias.meshes[*mesh].skins[*skin].skin;
}

/**
 * @brief Converts the model data into the opengl arrays
 * @param mod The model to convert
 * @param mesh The particular mesh of the model to convert
 * @param backlerp The linear back interpolation when loading the data
 * @param framenum The frame number of the mesh to load (if animated)
 * @param oldframenum The old frame number (used to interpolate)
 * @param prerender If this is @c true, all data is filled to the arrays. If @c false, then
 * e.g. the normals are only filled to the arrays if the lighting is activated.
 */
void R_FillArrayData (const mAliasModel_t* mod, const mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum, qboolean prerender)
{
	int i, j;
	const mAliasFrame_t *frame, *oldframe;
	const mAliasVertex_t *v, *ov;
	vec3_t move;
	const float frontlerp = 1.0 - backlerp;
	vec3_t r_mesh_verts[MD3_MAX_VERTS];
	vec3_t r_mesh_norms[MD3_MAX_VERTS];
	vec3_t r_mesh_tangents[MD3_MAX_VERTS];
	float *texcoord_array, *vertex_array_3d, *normal_array, *tangent_array;

	frame = mod->frames + framenum;
	oldframe = mod->frames + oldframenum;

	for (i = 0; i < 3; i++)
		move[i] = backlerp * oldframe->translate[i] + frontlerp * frame->translate[i];

	v = &mesh->vertexes[framenum * mesh->num_verts];
	ov = &mesh->vertexes[oldframenum * mesh->num_verts];

	assert(mesh->num_verts < lengthof(r_mesh_verts));

	for (i = 0; i < mesh->num_verts; i++, v++, ov++) {  /* lerp the verts */
		VectorSet(r_mesh_verts[i],
				move[0] + ov->point[0] * backlerp + v->point[0] * frontlerp,
				move[1] + ov->point[1] * backlerp + v->point[1] * frontlerp,
				move[2] + ov->point[2] * backlerp + v->point[2] * frontlerp);

		if (r_state.lighting_enabled || prerender) {  /* and the norms */
			VectorSet(r_mesh_norms[i],
					v->normal[0] + (ov->normal[0] - v->normal[0]) * backlerp,
					v->normal[1] + (ov->normal[1] - v->normal[1]) * backlerp,
					v->normal[2] + (ov->normal[2] - v->normal[2]) * backlerp);
		}
		if (r_state.bumpmap_enabled || prerender) {  /* and the tangents */
			VectorSet(r_mesh_tangents[i],
					v->tangent[0] + (ov->tangent[0] - v->tangent[0]) * backlerp,
					v->tangent[1] + (ov->tangent[1] - v->tangent[1]) * backlerp,
					v->tangent[2] + (ov->tangent[2] - v->tangent[2]) * backlerp);
		}
	}

	texcoord_array = texunit_diffuse.texcoord_array;
	vertex_array_3d = r_state.vertex_array_3d;
	normal_array = r_state.normal_array;
	tangent_array = r_state.tangent_array;

	/** @todo damn slow - optimize this */
	for (i = 0; i < mesh->num_tris; i++) {  /* draw the tris */
		for (j = 0; j < 3; j++) {
			const int arrayIndex = 3 * i + j;
			const int meshIndex = mesh->indexes[arrayIndex];
			Vector2Copy(mesh->stcoords[meshIndex], texcoord_array);
			VectorCopy(r_mesh_verts[meshIndex], vertex_array_3d);

			/* normal vectors for lighting */
			if (r_state.lighting_enabled || prerender)
				VectorCopy(r_mesh_norms[meshIndex], normal_array);

			/* tangent vectors for bump mapping */
			if (r_state.bumpmap_enabled || prerender)
				VectorCopy(r_mesh_tangents[meshIndex], tangent_array);

			texcoord_array += 2;
			vertex_array_3d += 3;
			normal_array += 3;
			tangent_array += 3;
		}
	}
}

/**
 * @brief Loads array data for models with only one frame. Only called once at loading time.
 */
void R_ModLoadArrayDataForStaticModel (const mAliasModel_t *mod, mAliasMesh_t *mesh)
{
	const int v = mesh->num_tris * 3 * 3;
	const int st = mesh->num_tris * 3 * 2;

	if (mod->num_frames != 1)
		return;

	assert(mesh->verts == NULL);
	assert(mesh->texcoords == NULL);
	assert(mesh->normals == NULL);
	assert(mesh->tangents == NULL);

	R_FillArrayData(mod, mesh, 0.0, 0, 0, qtrue);

	mesh->verts = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->normals = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->tangents = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->texcoords = (float *)Mem_PoolAlloc(sizeof(float) * st, vid_modelPool, 0);

	memcpy(mesh->verts, r_state.vertex_array_3d, sizeof(float) * v);
	memcpy(mesh->normals, r_state.normal_array, sizeof(float) * v);
	memcpy(mesh->tangents, r_state.tangent_array, sizeof(float) * v);
	memcpy(mesh->texcoords, texunit_diffuse.texcoord_array, sizeof(float) * st);
}
