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

#define SMOOTH_THRESH 0.5

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
 * @brief Tries to load a mdx file that contains the normals and the tangets for a model.
 * @sa R_ModCalcNormalsAndTangents
 * @sa R_ModCalcUniqueNormalsAndTangents
 * @param mod The model to load the mdx file for
 */
void R_ModLoadMDX (model_t *mod)
{
	int i;
	for (i = 0; i < mod->alias.num_meshes; i++) {
		mAliasMesh_t *mesh = &mod->alias.meshes[i];
		char mdxFileName[MAX_QPATH];
		byte *buffer = NULL, *buf;
		const float *dataBuf;

		Com_StripExtension(mod->name, mdxFileName, sizeof(mdxFileName));
		Com_DefaultExtension(mdxFileName, sizeof(mdxFileName), ".mdx");

		if (FS_LoadFile(mdxFileName, &buffer) == -1)
			continue;

		buf = buffer;
		buffer += strlen(IDMDXHEADER) * sizeof(char);
		buffer += sizeof(uint32_t);
		dataBuf = (const float *)buffer;

		for (i = 0; i < mesh->num_verts; i++) {
			mAliasVertex_t *v = &mesh->vertexes[i];
			int j;
			for (j = 0; j < 3; j++) {
				v->normal[j] = LittleFloat(*dataBuf);
				dataBuf++;
			}
			for (j = 0; j < 3; j++) {
				v->tangent[j] = LittleFloat(*dataBuf);
				dataBuf++;
			}
		}

		FS_FreeFile(buf);
	}
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
	vec3_t bitangents[MD2_MAX_TRIANGLES * 3];
	mAliasVertex_t *vertexes = &mesh->vertexes[offset];
	mAliasCoord_t *stcoords = mesh->stcoords;
	const int32_t *indexArray = mesh->indexes;
	const int numVerts = mesh->num_verts;
	const int numIndexes = mesh->num_tris * 3;

	for (i = 0; i < numVerts; i++) {
		VectorClear(vertexes[i].normal);
		Vector4Clear(vertexes[i].tangent);
		VectorClear(bitangents[i]);
	}

	/* calculate per-triangle tangent space basis*/
	for (i = 0, j = 0; i < numIndexes; i += 3, j++) {
		vec3_t dir1, dir2;
		vec2_t dir1uv, dir2uv;

		/* calculate two mostly perpendicular edge directions */
		VectorSubtract(vertexes[indexArray[i + 0]].point, vertexes[indexArray[i + 1]].point, dir1);
		VectorSubtract(vertexes[indexArray[i + 2]].point, vertexes[indexArray[i + 1]].point, dir2);
		Vector2Subtract(stcoords[indexArray[i + 0]], stcoords[indexArray[i + 1]], dir1uv);
		Vector2Subtract(stcoords[indexArray[i + 2]], stcoords[indexArray[i + 1]], dir2uv);

		/* we have two edge directions, we can calculate a third vector from
		 * them, which is the direction of the surface normal */
		CrossProduct(dir1, dir2, triangleNormals[j]);
		VectorNormalize(triangleNormals[j]);

		VectorAdd(vertexes[indexArray[i + 0]].normal, triangleNormals[j], vertexes[indexArray[i + 0]].normal);
		VectorAdd(vertexes[indexArray[i + 1]].normal, triangleNormals[j], vertexes[indexArray[i + 1]].normal);
		VectorAdd(vertexes[indexArray[i + 2]].normal, triangleNormals[j], vertexes[indexArray[i + 2]].normal);

		/* calculate per-triangle tangents and bitangents */
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
		VectorAdd(bitangents[indexArray[i + 0]], triangleBitangents[j], bitangents[indexArray[i + 0]]);
		VectorAdd(bitangents[indexArray[i + 1]], triangleBitangents[j], bitangents[indexArray[i + 1]]);
		VectorAdd(bitangents[indexArray[i + 2]], triangleBitangents[j], bitangents[indexArray[i + 2]]);
	}

	/* average and orthogonalize tangents */
	for (i = 0; i < numVerts; i++) {
		vec3_t v;

		/* normalization here does shared-vertex smoothing */
		VectorNormalize(vertexes[i].normal);
		VectorNormalize(vertexes[i].tangent);
		VectorNormalize(bitangents[i]);

		/* Grahm-Schmidt orthogonalization */
		VectorMul(DotProduct(vertexes[i].tangent, vertexes[i].normal), vertexes[i].normal, v);
		VectorSubtract(vertexes[i].tangent, v, vertexes[i].tangent);
		VectorNormalize(vertexes[i].tangent);

#ifdef PARANOID
		if (fabs(DotProduct(vertexes[i].tangent, vertexes[i].normal)) >= 0.001)
			Com_Printf("Non-orthogonal basis! normal * tangent = %f (in %s)\n", DotProduct(vertexes[i].tangent, vertexes[i].normal), mesh->name);
#endif

		/* calculate handedness */
		CrossProduct(vertexes[i].normal, vertexes[i].tangent, v);
		vertexes[i].tangent[3] = (DotProduct(v, bitangents[i]) < 0.0) ? -1.0 : 1.0;
	}
}

/**
 * @todo Use R_ModLoadMDX
 * @param mesh
 * @param nFrames
 */
void R_ModCalcUniqueNormalsAndTangents (mAliasMesh_t *mesh, int nFrames)
{
	/* count unique verts */
	int i, j;
	vec3_t triangleNormals[MD2_MAX_TRIANGLES];
	vec3_t triangleTangents[MD2_MAX_TRIANGLES];
	float triangleTangentsH[MD2_MAX_TRIANGLES];
	vec3_t triangleBitangents[MD2_MAX_TRIANGLES];
	const mAliasVertex_t *vertexes = mesh->vertexes;
	mAliasCoord_t *stcoords = mesh->stcoords;
	mAliasVertex_t *newVertexes;
	mAliasCoord_t *newStcoords;
	const int numIndexes = mesh->num_tris * 3;
	const int32_t *indexArray = mesh->indexes;
	int32_t *newIndexArray;
	int indRemap[MD2_MAX_TRIANGLES * 3];
	int numVerts = 0;

	newIndexArray = Mem_PoolAlloc(sizeof(int32_t) * numIndexes, vid_modelPool, 0);

	/* calculate per-triangle surface normals */
	for (i = 0, j = 0; i < numIndexes; i += 3, j++) {
		vec3_t dir1, dir2;
		vec2_t dir1uv, dir2uv;

		/* calculate two mostly perpendicular edge directions */
		VectorSubtract(vertexes[indexArray[i + 0]].point, vertexes[indexArray[i + 1]].point, dir1);
		VectorSubtract(vertexes[indexArray[i + 2]].point, vertexes[indexArray[i + 1]].point, dir2);
		Vector2Subtract(stcoords[indexArray[i + 0]], stcoords[indexArray[i + 1]], dir1uv);
		Vector2Subtract(stcoords[indexArray[i + 2]], stcoords[indexArray[i + 1]], dir2uv);

		/* we have two edge directions, we can calculate a third vector from
		 * them, which is the direction of the surface normal */
		CrossProduct(dir1, dir2, triangleNormals[j]);

		/* then we use the texture coordinates to calculate a tangent space */
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
		VectorNormalize(triangleNormals[j]);
		VectorNormalize(triangleTangents[j]);
		VectorNormalize(triangleBitangents[j]);
	}

	/* assume all verticies are unique until proven otherwise */
	for (i = 0; i < numIndexes; i++)
		indRemap[i] = -1;

	for (i = 0; i < numIndexes; i++) {
		vec3_t n, t, b, v;
		const int idx = (i - i % 3) / 3;

		if (indRemap[i] != -1)
			continue;

		VectorCopy(triangleNormals[idx], n);
		VectorCopy(triangleTangents[idx], t);
		VectorCopy(triangleBitangents[idx], b);

		for (j = i + 1; j < numIndexes; j++) {
			const int idx2 = (j - j % 3) / 3;
			/* only average normals if verticies have the same position and texcoord, and the normals aren't too far appart to start with */
			if (VectorCompare(vertexes[indexArray[i]].point, vertexes[indexArray[j]].point)
					&& Vector2Compare(stcoords[indexArray[i]], stcoords[indexArray[j]])
					&& DotProduct(triangleNormals[idx], triangleNormals[idx2]) > SMOOTH_THRESH) {
				VectorAdd(n, triangleNormals[idx2], n);
				VectorAdd(t, triangleTangents[idx2], t);
				VectorAdd(b, triangleBitangents[idx2], b);

				/* remap the verticies we're combining  */
				indRemap[j] = i;
				newIndexArray[j] = numVerts;
			}
		}

		/* normalization here does shared-vertex smoothing */
		VectorNormalize(n);
		VectorNormalize(t);
		VectorNormalize(b);

		/* Grahm-Schmidt orthogonalization */
		VectorMul(DotProduct(t, n), n, v);
		VectorSubtract(t, v, t);
		VectorNormalize(t);

		/* calculate handedness */
		CrossProduct(n, t, v);
		triangleTangentsH[idx] = (DotProduct(v, b) < 0.0) ? -1.0 : 1.0;
		VectorCopy(n, triangleNormals[idx]);
		VectorCopy(t, triangleTangents[idx]);

		/* update indexArray */
		newIndexArray[i] = numVerts++;

		indRemap[i] = i;
	}

	/* merge identical vertexes, storing only unique ones */
	newVertexes = Mem_PoolAlloc(sizeof(mAliasVertex_t) * numVerts * nFrames, vid_modelPool, 0);
	newStcoords = Mem_PoolAlloc(sizeof(mAliasCoord_t) * numVerts, vid_modelPool, 0);
	for (i = 0; i < numIndexes; i++) {
		const int idx = indexArray[indRemap[i]];
		const int idx2 = newIndexArray[i];
		const int idx3 = (i - i % 3) / 3;

		/* add vertex to new vertex array */
		VectorCopy(vertexes[idx].point, newVertexes[idx2].point);
		Vector2Copy(stcoords[idx], newStcoords[idx2]);

		VectorCopy(triangleNormals[idx3], newVertexes[idx2].normal);
		VectorCopy(triangleTangents[idx3], newVertexes[idx2].tangent);
		newVertexes[idx2].tangent[3] = triangleTangentsH[idx3];

#ifdef PARANOID
		/* various checks for badly formed vectors */
		if (fabs(DotProduct(newVertexes[idx2].tangent, newVertexes[idx2].normal)) > 0.01)
			Com_Printf("Non-orthogonal basis! normal * tangent = %f (in %s)\n", DotProduct(newVertexes[idx2].tangent, newVertexes[idx2].normal), mesh->name);
		if (fabs(DotProduct(newVertexes[idx2].normal, newVertexes[idx2].normal)) < 0.9)
			Com_Printf("Non-unit normal! normal * normal = %f (in %s)\n", DotProduct(newVertexes[idx2].normal, newVertexes[idx2].normal), mesh->name);
		if (fabs(DotProduct(newVertexes[idx2].tangent, newVertexes[idx2].tangent)) < 0.9)
			Com_Printf("Non-unit tangent! tangent * tangent = %f (in %s)\n", DotProduct(newVertexes[idx2].tangent, newVertexes[idx2].tangent), mesh->name);
#endif
	}

	/* copy over the points from successive frames */
	for (i = 1; i < nFrames; i++) {
		for (j = 0; j < numIndexes; j++) {
			const int idx = indexArray[indRemap[j]] + (mesh->num_verts * i);
			const int idx2 = newIndexArray[j] + (numVerts * i);

			VectorCopy(vertexes[idx].point, newVertexes[idx2].point);
		}
	}

	/* copy new arrays back into original mesh */
	Mem_Free(mesh->stcoords);
	Mem_Free(mesh->indexes);
	Mem_Free(mesh->vertexes);

	mesh->num_verts = numVerts;
	mesh->vertexes = newVertexes;
	mesh->stcoords = newStcoords;
	mesh->indexes = newIndexArray;

	/* do tangent space calculation for successive frames */
	for (i = 1; i < nFrames; i++) {
		R_ModCalcNormalsAndTangents(mesh, numVerts * i);
	}
}


image_t* R_AliasModelGetSkin (const model_t* mod, const char *skin)
{
	if (skin[0] != '.')
		return R_FindImage(skin, it_skin);
	else {
		char path[MAX_QPATH];
		Com_ReplaceFilename(mod->name, skin, path, sizeof(path));
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
	vec4_t r_mesh_tangents[MD3_MAX_VERTS];
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
			Vector4Set(r_mesh_tangents[i],
					v->tangent[0] + (ov->tangent[0] - v->tangent[0]) * backlerp,
					v->tangent[1] + (ov->tangent[1] - v->tangent[1]) * backlerp,
					v->tangent[2] + (ov->tangent[2] - v->tangent[2]) * backlerp,
					v->tangent[3] + (ov->tangent[3] - v->tangent[3]) * backlerp);
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
				Vector4Copy(r_mesh_tangents[meshIndex], tangent_array);

			texcoord_array += 2;
			vertex_array_3d += 3;
			normal_array += 3;
			tangent_array += 4;
		}
	}
}

/**
 * @brief Loads array data for models with only one frame. Only called once at loading time.
 */
void R_ModLoadArrayDataForStaticModel (const mAliasModel_t *mod, mAliasMesh_t *mesh)
{
	const int v = mesh->num_tris * 3 * 3;
	const int t = mesh->num_tris * 3 * 4;
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
	mesh->tangents = (float *)Mem_PoolAlloc(sizeof(float) * t, vid_modelPool, 0);
	mesh->texcoords = (float *)Mem_PoolAlloc(sizeof(float) * st, vid_modelPool, 0);

	memcpy(mesh->verts, r_state.vertex_array_3d, sizeof(float) * v);
	memcpy(mesh->normals, r_state.normal_array, sizeof(float) * v);
	memcpy(mesh->tangents, r_state.tangent_array, sizeof(float) * t);
	memcpy(mesh->texcoords, texunit_diffuse.texcoord_array, sizeof(float) * st);
}
