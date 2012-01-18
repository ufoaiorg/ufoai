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
#include "r_state.h"

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

void R_ModLoadAnims (mAliasModel_t *mod, const char *buffer)
{
	const char *text, *token;
	mAliasAnim_t *anim;
	int n;

	/* count the animations */
	n = Com_CountTokensInBuffer(buffer);

	if ((n % 4) != 0)
		Com_Error(ERR_DROP, "invalid syntax: %s", mod->animname);

	/* each animation definition is made out of 4 tokens */
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
	} while (mod->num_anims < n);
}


/**
 * @brief Calculates a per-vertex tangentspace basis and stores it in GL arrays attached to the mesh
 * @param mesh The mesh to calculate normals for
 * @param framenum The animation frame to calculate normals for
 * @param translate The frame translation for the given animation frame
 * @param backlerp Whether to store the results in the GL arrays for the previous keyframe or the next keyframe
 * @sa R_ModCalcUniqueNormalsAndTangents
 */
static void R_ModCalcNormalsAndTangents (mAliasMesh_t *mesh, int framenum, const vec3_t translate, qboolean backlerp)
{
	int i, j;
	mAliasVertex_t *vertexes = &mesh->vertexes[framenum * mesh->num_verts];
	mAliasCoord_t *stcoords = mesh->stcoords;
	const int numIndexes = mesh->num_tris * 3;
	const int32_t *indexArray = mesh->indexes;
	vec3_t triangleNormals[MAX_ALIAS_TRIS];
	vec3_t triangleTangents[MAX_ALIAS_TRIS];
	vec3_t triangleBitangents[MAX_ALIAS_TRIS];
	float *texcoords, *verts, *normals, *tangents;

	/* set up array pointers for either the previous keyframe or the next keyframe */
	texcoords = mesh->texcoords;
	if (backlerp) {
		verts = mesh->verts;
		normals = mesh->normals;
		tangents = mesh->tangents;
	} else {
		verts = mesh->next_verts;
		normals = mesh->next_normals;
		tangents = mesh->next_tangents;
	}

	/* calculate per-triangle surface normals and tangents*/
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
		/* normalize */
		VectorNormalizeFast(triangleNormals[j]);

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

			/* normalize */
			VectorNormalizeFast(triangleTangents[j]);
			VectorNormalizeFast(triangleBitangents[j]);
		} else {
			VectorClear(triangleTangents[j]);
			VectorClear(triangleBitangents[j]);
		}
	}

	/* for each vertex */
	for (i = 0; i < mesh->num_verts; i++) {
		vec3_t n, b, v;
		vec4_t t;
		const int len = mesh->revIndexes[i].length;
		const int32_t *list = mesh->revIndexes[i].list;

		VectorClear(n);
		VectorClear(t);
		VectorClear(b);

		/* for each vertex that got mapped to this one (ie. for each triangle this vertex is a part of) */
		for (j = 0; j < len; j++) {
			const int32_t idx = list[j] / 3;
			VectorAdd(n, triangleNormals[idx], n);
			VectorAdd(t, triangleTangents[idx], t);
			VectorAdd(b, triangleBitangents[idx], b);
		}

		/* normalization here does shared-vertex smoothing */
		VectorNormalizeFast(n);
		VectorNormalizeFast(t);
		VectorNormalizeFast(b);

		/* Grahm-Schmidt orthogonalization */
		Orthogonalize(t, n);

		/* calculate handedness */
		CrossProduct(n, t, v);
		t[3] = (DotProduct(v, b) < 0.0) ? -1.0 : 1.0;

		/* copy this vertex's info to all the right places in the arrays */
		for (j = 0; j < len; j++) {
			const int32_t idx = list[j];
			const int meshIndex = mesh->indexes[list[j]];
			Vector2Copy(stcoords[meshIndex], (texcoords + (2 * idx)));
			VectorAdd(vertexes[meshIndex].point, translate, (verts + (3 * idx)));
			VectorCopy(n, (normals + (3 * idx)));
			Vector4Copy(t, (tangents + (4 * idx)));
		}
	}
}

/**
 * @brief Tries to load a mdx file that contains the normals and the tangents for a model.
 * @sa R_ModCalcNormalsAndTangents
 * @sa R_ModCalcUniqueNormalsAndTangents
 * @param mod The model to load the mdx file for
 */
qboolean R_ModLoadMDX (model_t *mod)
{
	int i;
	for (i = 0; i < mod->alias.num_meshes; i++) {
		mAliasMesh_t *mesh = &mod->alias.meshes[i];
		char mdxFileName[MAX_QPATH];
		byte *buffer = NULL, *buf;
		const int32_t *intbuf;
		uint32_t version;
		int sharedTris[MAX_ALIAS_VERTS];

		Com_StripExtension(mod->name, mdxFileName, sizeof(mdxFileName));
		Com_DefaultExtension(mdxFileName, sizeof(mdxFileName), ".mdx");

		if (FS_LoadFile(mdxFileName, &buffer) == -1)
			return qfalse;

		buf = buffer;
		if (strncmp((const char *) buf, IDMDXHEADER, strlen(IDMDXHEADER)))
			Com_Error(ERR_DROP, "No mdx file buffer given");
		buffer += strlen(IDMDXHEADER) * sizeof(char);
		version = LittleLong(*(uint32_t*) buffer);
		if (version != MDX_VERSION)
			Com_Error(ERR_DROP, "Invalid version of the mdx file, expected %i, found %i",
					MDX_VERSION, version);
		buffer += sizeof(uint32_t);

		intbuf = (const int32_t *) buffer;

		mesh->num_verts = LittleLong(*intbuf);
		if (mesh->num_verts <= 0 || mesh->num_verts > MAX_ALIAS_VERTS)
			Com_Error(ERR_DROP, "mdx file for %s has to many (or no) vertices: %i", mod->name, mesh->num_verts);
		intbuf++;
		mesh->num_indexes = LittleLong(*intbuf);
		intbuf++;

		mesh->indexes = (int32_t *)Mem_PoolAlloc(sizeof(int32_t) * mesh->num_indexes, vid_modelPool, 0);
		mesh->revIndexes = (mIndexList_t *)Mem_PoolAlloc(sizeof(mIndexList_t) * mesh->num_verts, vid_modelPool, 0);
		mesh->vertexes = (mAliasVertex_t *)Mem_PoolAlloc(sizeof(mAliasVertex_t) * mod->alias.num_frames * mesh->num_verts, vid_modelPool, 0);

		/* load index that maps triangle verts to Vertex objects */
		for (i = 0; i < mesh->num_indexes; i++) {
			mesh->indexes[i] = LittleLong(*intbuf);
			intbuf++;
		}

		for (i = 0; i < mesh->num_verts; i++)
			sharedTris[i] = 0;

		/* set up reverse-index that maps Vertex objects to a list of triangle verts */
		for (i = 0; i < mesh->num_indexes; i++)
			sharedTris[mesh->indexes[i]]++;

		for (i = 0; i < mesh->num_verts; i++) {
			mesh->revIndexes[i].length = 0;
			mesh->revIndexes[i].list = (int32_t *)Mem_PoolAlloc(sizeof(int32_t) * sharedTris[i], vid_modelPool, 0);
		}

		for (i = 0; i < mesh->num_indexes; i++)
			mesh->revIndexes[mesh->indexes[i]].list[mesh->revIndexes[mesh->indexes[i]].length++] = i;

		FS_FreeFile(buf);
	}

	return qtrue;
}

/**
 * @brief Calculates normals and tangents for all frames and does vertex merging based on smoothness
 * @param mesh The mesh to calculate normals for
 * @param nFrames How many frames the mesh has
 * @param smoothness How aggressively should normals be smoothed; value is compared with dotproduct of vectors to decide if they should be merged
 * @sa R_ModCalcNormalsAndTangents
 */
void R_ModCalcUniqueNormalsAndTangents (mAliasMesh_t *mesh, int nFrames, float smoothness)
{
	int i, j;
	vec3_t triangleNormals[MAX_ALIAS_TRIS];
	vec3_t triangleTangents[MAX_ALIAS_TRIS];
	vec3_t triangleBitangents[MAX_ALIAS_TRIS];
	const mAliasVertex_t *vertexes = mesh->vertexes;
	mAliasCoord_t *stcoords = mesh->stcoords;
	mAliasVertex_t *newVertexes;
	mAliasComplexVertex_t tmpVertexes[MAX_ALIAS_VERTS];
	vec3_t tmpBitangents[MAX_ALIAS_VERTS];
	mAliasCoord_t *newStcoords;
	const int numIndexes = mesh->num_tris * 3;
	const int32_t *indexArray = mesh->indexes;
	int32_t *newIndexArray;
	int indRemap[MAX_ALIAS_VERTS];
	int sharedTris[MAX_ALIAS_VERTS];
	int numVerts = 0;

	if (numIndexes >= MAX_ALIAS_VERTS)
		Com_Error(ERR_DROP, "model %s has too many tris", mesh->name);

	newIndexArray = (int32_t *)Mem_PoolAlloc(sizeof(int32_t) * numIndexes, vid_modelPool, 0);

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
		} else {
			const float frac = 1.0 / (0.00001);
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
		VectorNormalizeFast(triangleNormals[j]);
		VectorNormalizeFast(triangleTangents[j]);
		VectorNormalizeFast(triangleBitangents[j]);

		Orthogonalize(triangleTangents[j], triangleBitangents[j]);
	}

	/* do smoothing */
	for (i = 0; i < numIndexes; i++) {
		const int idx = (i - i % 3) / 3;
		VectorCopy(triangleNormals[idx], tmpVertexes[i].normal);
		VectorCopy(triangleTangents[idx], tmpVertexes[i].tangent);
		VectorCopy(triangleBitangents[idx], tmpBitangents[i]);

		for (j = 0; j < numIndexes; j++) {
			const int idx2 = (j - j % 3) / 3;
			/* don't add a vertex with itself */
			if (j == i)
				continue;

			/* only average normals if vertices have the same position
			 * and the normals aren't too far apart to start with */
			if (VectorEqual(vertexes[indexArray[i]].point, vertexes[indexArray[j]].point)
					&& DotProduct(triangleNormals[idx], triangleNormals[idx2]) > smoothness) {
				/* average the normals */
				VectorAdd(tmpVertexes[i].normal, triangleNormals[idx2], tmpVertexes[i].normal);

				/* if the tangents match as well, average them too.
				 * Note that having matching normals without matching tangents happens
				 * when the order of vertices in two triangles sharing the vertex
				 * in question is different.  This happens quite frequently if the
				 * modeler does not go out of their way to avoid it. */

				if (Vector2Equal(stcoords[indexArray[i]], stcoords[indexArray[j]])
						&& DotProduct(triangleTangents[idx], triangleTangents[idx2]) > smoothness
						&& DotProduct(triangleBitangents[idx], triangleBitangents[idx2]) > smoothness) {
					/* average the tangents */
					VectorAdd(tmpVertexes[i].tangent, triangleTangents[idx2], tmpVertexes[i].tangent);
					VectorAdd(tmpBitangents[i], triangleBitangents[idx2], tmpBitangents[i]);
				}
			}
		}

		VectorNormalizeFast(tmpVertexes[i].normal);
		VectorNormalizeFast(tmpVertexes[i].tangent);
		VectorNormalizeFast(tmpBitangents[i]);
	}

	/* assume all vertices are unique until proven otherwise */
	for (i = 0; i < numIndexes; i++)
		indRemap[i] = -1;

	/* merge vertices that have become identical */
	for (i = 0; i < numIndexes; i++) {
		vec3_t n, b, t, v;
		if (indRemap[i] != -1)
			continue;

		for (j = i + 1; j < numIndexes; j++) {
			if (Vector2Equal(stcoords[indexArray[i]], stcoords[indexArray[j]])
					&& VectorEqual(vertexes[indexArray[i]].point, vertexes[indexArray[j]].point)
					&& (DotProduct(tmpVertexes[i].normal, tmpVertexes[j].normal) > smoothness)
					&& (DotProduct(tmpVertexes[i].tangent, tmpVertexes[j].tangent) > smoothness)) {
				indRemap[j] = i;
				newIndexArray[j] = numVerts;
			}
		}

		VectorCopy(tmpVertexes[i].normal, n);
		VectorCopy(tmpVertexes[i].tangent, t);
		VectorCopy(tmpBitangents[i], b);

		/* normalization here does shared-vertex smoothing */
		VectorNormalizeFast(n);
		VectorNormalizeFast(t);
		VectorNormalizeFast(b);

		/* Grahm-Schmidt orthogonalization */
		VectorMul(DotProduct(t, n), n, v);
		VectorSubtract(t, v, t);
		VectorNormalizeFast(t);

		/* calculate handedness */
		CrossProduct(n, t, v);
		tmpVertexes[i].tangent[3] = (DotProduct(v, b) < 0.0) ? -1.0 : 1.0;
		VectorCopy(n, tmpVertexes[i].normal);
		VectorCopy(t, tmpVertexes[i].tangent);

		newIndexArray[i] = numVerts++;
		indRemap[i] = i;
	}

	for (i = 0; i < numVerts; i++)
		sharedTris[i] = 0;

	for (i = 0; i < numIndexes; i++)
		sharedTris[newIndexArray[i]]++;

	/* set up reverse-index that maps Vertex objects to a list of triangle verts */
	mesh->revIndexes = (mIndexList_t *)Mem_PoolAlloc(sizeof(mIndexList_t) * numVerts, vid_modelPool, 0);
	for (i = 0; i < numVerts; i++) {
		mesh->revIndexes[i].length = 0;
		mesh->revIndexes[i].list = (int32_t *)Mem_PoolAlloc(sizeof(int32_t) * sharedTris[i], vid_modelPool, 0);
	}

	/* merge identical vertexes, storing only unique ones */
	newVertexes = (mAliasVertex_t *)Mem_PoolAlloc(sizeof(mAliasVertex_t) * numVerts * nFrames, vid_modelPool, 0);
	newStcoords = (mAliasCoord_t *)Mem_PoolAlloc(sizeof(mAliasCoord_t) * numVerts, vid_modelPool, 0);
	for (i = 0; i < numIndexes; i++) {
		const int idx = indexArray[indRemap[i]];
		const int idx2 = newIndexArray[i];

		/* add vertex to new vertex array */
		VectorCopy(vertexes[idx].point, newVertexes[idx2].point);
		Vector2Copy(stcoords[idx], newStcoords[idx2]);
		mesh->revIndexes[idx2].list[mesh->revIndexes[idx2].length++] = i;
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
}

image_t* R_AliasModelGetSkin (const char *modelFileName, const char *skin)
{
	image_t* result;
	if (skin[0] != '.')
		result = R_FindImage(skin, it_skin);
	else {
		char path[MAX_QPATH];
		Com_ReplaceFilename(modelFileName, skin + 1, path, sizeof(path));
		result = R_FindImage(path, it_skin);
	}
	return result;
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
		Com_Error(ERR_DROP, "Texture is already freed and no longer uploaded, texnum is invalid for model %s",
				mod->name);

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
 *
 * @note If GLSL programs are enabled, the actual interpolation will be done on the GPU, but
 * this function is still needed to fill the GL arrays for the keyframes
 */
void R_FillArrayData (mAliasModel_t* mod, mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum, qboolean prerender)
{
	const mAliasFrame_t *frame, *oldframe;
	vec3_t move;
	const float frontlerp = 1.0 - backlerp;
	vec3_t r_mesh_verts[MAX_ALIAS_VERTS];
	vec_t *texcoord_array, *vertex_array_3d;

	frame = mod->frames + framenum;
	oldframe = mod->frames + oldframenum;

	/* try to do keyframe-interpolation on the GPU if possible*/
	if (r_state.lighting_enabled) {
		/* we only need to change the array data if we've switched to a new keyframe */
		if (mod->curFrame != framenum) {
			/* if we're rendering frames in order, the "next" keyframe from the previous
			 * time through will be our "previous" keyframe now, so we can swap pointers
			 * instead of generating it again from scratch */
			if (mod->curFrame == oldframenum) {
				vec_t *tmp1 = mesh->verts;
				vec_t *tmp2 = mesh->normals;
				vec_t *tmp3 = mesh->tangents;

				mesh->verts = mesh->next_verts;
				mesh->next_verts = tmp1;

				mesh->normals = mesh->next_normals;
				mesh->next_normals = tmp2;

				mesh->tangents = mesh->next_tangents;
				mesh->next_tangents = tmp3;

				/* if we're alternating between two keyframes, we don't need to generate
				 * anything; otherwise, generate the "next" keyframe*/
				if (mod->oldFrame != framenum)
					R_ModCalcNormalsAndTangents(mesh, framenum, frame->translate, qfalse);
			} else {
				/* if we're starting a new animation or otherwise not rendering keyframes
				 * in order, we need to fill the arrays for both keyframes */
				R_ModCalcNormalsAndTangents(mesh, oldframenum, oldframe->translate, qtrue);
				R_ModCalcNormalsAndTangents(mesh, framenum, frame->translate, qfalse);
			}
			/* keep track of which keyframes are currently stored in our arrays */
			mod->oldFrame = oldframenum;
			mod->curFrame = framenum;
		}
	} else { /* otherwise, we have to do it on the CPU */
		const mAliasVertex_t *v, *ov;
		int i;
		assert(mesh->num_verts < lengthof(r_mesh_verts));
		v = &mesh->vertexes[framenum * mesh->num_verts];
		ov = &mesh->vertexes[oldframenum * mesh->num_verts];

		if (prerender)
			R_ModCalcNormalsAndTangents(mesh, 0, oldframe->translate, qtrue);

		for (i = 0; i < 3; i++)
			move[i] = backlerp * oldframe->translate[i] + frontlerp * frame->translate[i];

		for (i = 0; i < mesh->num_verts; i++, v++, ov++) {  /* lerp the verts */
			VectorSet(r_mesh_verts[i],
					move[0] + ov->point[0] * backlerp + v->point[0] * frontlerp,
					move[1] + ov->point[1] * backlerp + v->point[1] * frontlerp,
					move[2] + ov->point[2] * backlerp + v->point[2] * frontlerp);
		}

		R_ReallocateStateArrays(mesh->num_tris * 3);
		R_ReallocateTexunitArray(&texunit_diffuse, mesh->num_tris * 3);
		texcoord_array = texunit_diffuse.texcoord_array;
		vertex_array_3d = r_state.vertex_array_3d;

		/** @todo damn slow - optimize this */
		for (i = 0; i < mesh->num_tris; i++) {  /* draw the tris */
			int j;
			for (j = 0; j < 3; j++) {
				const int arrayIndex = 3 * i + j;
				const int meshIndex = mesh->indexes[arrayIndex];
				Vector2Copy(mesh->stcoords[meshIndex], texcoord_array);
				VectorCopy(r_mesh_verts[meshIndex], vertex_array_3d);

				texcoord_array += 2;
				vertex_array_3d += 3;
			}
		}
	}
}

/**
 * @brief Allocates data arrays for animated models. Only called once at loading time.
 */
void R_ModLoadArrayData (mAliasModel_t *mod, mAliasMesh_t *mesh, qboolean loadNormals)
{
	const int v = mesh->num_tris * 3 * 3;
	const int t = mesh->num_tris * 3 * 4;
	const int st = mesh->num_tris * 3 * 2;

	assert(mesh->verts == NULL);
	assert(mesh->texcoords == NULL);
	assert(mesh->normals == NULL);
	assert(mesh->tangents == NULL);
	assert(mesh->next_verts == NULL);
	assert(mesh->next_normals == NULL);
	assert(mesh->next_tangents == NULL);

	mesh->verts = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->normals = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
	mesh->tangents = (float *)Mem_PoolAlloc(sizeof(float) * t, vid_modelPool, 0);
	mesh->texcoords = (float *)Mem_PoolAlloc(sizeof(float) * st, vid_modelPool, 0);
	if (mod->num_frames == 1) {
		R_FillArrayData(mod, mesh, 0.0, 0, 0, loadNormals);
	} else {
		mesh->next_verts = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
		mesh->next_normals = (float *)Mem_PoolAlloc(sizeof(float) * v, vid_modelPool, 0);
		mesh->next_tangents = (float *)Mem_PoolAlloc(sizeof(float) * t, vid_modelPool, 0);

		mod->curFrame = -1;
		mod->oldFrame = -1;
	}
}
