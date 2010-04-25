/**
 * @file r_model_md2.c
 * @brief md2 alias model loading
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

/*
==============================================================================
MD2 ALIAS MODELS
==============================================================================
*/

#if 0
/**
 * @brief Calculates the normals of an md2 model
 */
static void R_CalcAliasNormals (const int numIndexes, int32_t *indexArray, const int numVerts, mAliasMesh_t *mesh)
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
		VectorCopy(v1, vertexes[indexArray[i + 0]].point);
		VectorCopy(v2, vertexes[indexArray[i + 1]].point);
		VectorCopy(v3, vertexes[indexArray[i + 2]].point);

		/* texture coordinates */
		Vector2Copy(w1, stcoords[indexArray[i + 0]]);
		Vector2Copy(w2, stcoords[indexArray[i + 1]]);
		Vector2Copy(w3, stcoords[indexArray[i + 2]]);

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
#endif

static void R_ModLoadTags (model_t * mod, void *buffer, int bufSize)
{
	dMD2tag_t *pintag, *pheader;
	int version;
	int i, j, size;
	float *inmat, *outmat;
	int read;

	pintag = (dMD2tag_t *) buffer;

	version = LittleLong(pintag->version);
	if (version != TAG_VERSION)
		Com_Error(ERR_FATAL, "R_ModLoadTags: %s has wrong version number (%i should be %i)",
				mod->alias.tagname, version, TAG_VERSION);

	size = LittleLong(pintag->ofs_extractend);
	mod->alias.tagdata = Mem_PoolAlloc(size, vid_modelPool, 0);
	pheader = mod->alias.tagdata;

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dMD2tag_t) / 4; i++)
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (pheader->num_tags <= 0)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tag file %s has no tags", mod->alias.tagname);

	if (pheader->num_frames <= 0)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tag file %s has no frames", mod->alias.tagname);

	/* load tag names */
	memcpy((char *) pheader + pheader->ofs_names, (char *) pintag + pheader->ofs_names, pheader->num_tags * MD2_MAX_SKINNAME);

	/* load tag matrices */
	inmat = (float *) ((byte *) pintag + pheader->ofs_tags);
	outmat = (float *) ((byte *) pheader + pheader->ofs_tags);

	if (bufSize != pheader->ofs_end)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tagfile %s is broken - expected: %i, offsets tell us to read: %i",
			mod->alias.tagname, bufSize, pheader->ofs_end);

	if (pheader->num_frames != mod->alias.num_frames)
		Com_Printf("R_ModLoadTags: found %i frames in %s but model has %i frames\n",
			pheader->num_frames, mod->alias.tagname, mod->alias.num_frames);

	if (pheader->ofs_names != 32)
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_name for tagfile %s", mod->alias.tagname);
	if (pheader->ofs_tags != pheader->ofs_names + (pheader->num_tags * 64))
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_tags for tagfile %s", mod->alias.tagname);
	/* (4 * 3) * 4 bytes (int) */
	if (pheader->ofs_end != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 48))
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_end for tagfile %s", mod->alias.tagname);
	/* (4 * 4) * 4 bytes (int) */
	if (pheader->ofs_extractend != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 64))
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_extractend for tagfile %s", mod->alias.tagname);

	for (i = 0; i < pheader->num_tags * pheader->num_frames; i++) {
		for (j = 0; j < 4; j++) {
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = 0.0;
		}
		outmat--;
		*outmat++ = 1.0;
	}

	read = (byte *)outmat - (byte *)pheader;
	if (read != size)
		Com_Error(ERR_FATAL, "R_ModLoadTags: read: %i expected: %i - tags: %i, frames: %i (should be %i)",
			read, size, pheader->num_tags, pheader->num_frames, mod->alias.num_frames);
}

static void R_ModLoadAliasMD2Mesh (model_t *mod, const dMD2Model_t *md2, int bufSize)
{
	int i, j;
	const dMD2Triangle_t *pintri;
	const dMD2Coord_t *pincoord;
	mAliasMesh_t *outMesh;
	mAliasFrame_t *outFrame, *outFrameTmp;
	mAliasVertex_t *outVertex;
	mAliasCoord_t *outCoord;
	int32_t tempIndex[MD2_MAX_TRIANGLES * 3];
	int32_t tempSTIndex[MD2_MAX_TRIANGLES * 3];
	int indRemap[MD2_MAX_TRIANGLES * 3];
	int32_t *outIndex;
	int frameSize, numIndexes, numVerts;
	int version;
	double isw, ish;
	const char *md2Path;
	size_t size;

	/* sanity checks */
	version = LittleLong(md2->version);
	if (version != MD2_ALIAS_VERSION)
		Com_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, version, MD2_ALIAS_VERSION);

	if (bufSize != LittleLong(md2->ofs_end))
		Com_Error(ERR_DROP, "model %s broken offset values (%i, %i)", mod->name, bufSize, LittleLong(md2->ofs_end));

	mod->alias.num_meshes++;
	size = sizeof(mAliasMesh_t) * mod->alias.num_meshes;

	if (mod->alias.meshes == NULL)
		mod->alias.meshes = outMesh = Mem_PoolAlloc(size, vid_modelPool, 0);
	else {
		mod->alias.meshes = Mem_ReAlloc(mod->alias.meshes, size);
		outMesh = &mod->alias.meshes[mod->alias.num_meshes - 1];
	}

	Q_strncpyz(outMesh->name, mod->name, sizeof(outMesh->name));
	outMesh->num_verts = LittleLong(md2->num_verts);
	if (outMesh->num_verts <= 0 || outMesh->num_verts >= MD2_MAX_VERTS)
		Com_Error(ERR_DROP, "model %s has too many (or no) vertices (%i/%i)",
			mod->name, outMesh->num_verts, MD2_MAX_VERTS);
	outMesh->num_tris = LittleLong(md2->num_tris);
	if (outMesh->num_tris <= 0 || outMesh->num_tris >= MD2_MAX_TRIANGLES)
		Com_Error(ERR_DROP, "model %s has too many (or no) triangles", mod->name);
	frameSize = LittleLong(md2->framesize);

	if (mod->alias.num_meshes == 1) {
		/* load the skins */
		outMesh->num_skins = LittleLong(md2->num_skins);
		if (outMesh->num_skins < 0 || outMesh->num_skins >= MD2_MAX_SKINS)
			Com_Error(ERR_DROP, "Could not load model '%s' - invalid num_skins value: %i\n", mod->name, outMesh->num_skins);

		outMesh->skins = Mem_PoolAlloc(sizeof(mAliasSkin_t) * outMesh->num_skins, vid_modelPool, 0);
		md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
		for (i = 0; i < outMesh->num_skins; i++) {
			/* arisian: bookmark */
			outMesh->skins[i].skin = R_AliasModelGetSkin(mod, md2Path + i * MD2_MAX_SKINNAME);
			Q_strncpyz(outMesh->skins[i].name, outMesh->skins[i].skin->name, sizeof(outMesh->skins[i].name));
		}

		outMesh->skinWidth = LittleLong(md2->skinwidth);
		outMesh->skinHeight = LittleLong(md2->skinheight);

		if (outMesh->skinHeight <= 0 || outMesh->skinWidth <= 0)
			Com_Error(ERR_DROP, "model %s has invalid skin dimensions '%d x %d'",
					mod->name, outMesh->skinHeight, outMesh->skinWidth);
	} else {
		/* skin data must be the same for the lod meshes */
		outMesh->num_skins = mod->alias.meshes[0].num_skins;
		outMesh->skins = mod->alias.meshes[0].skins;
		outMesh->skinWidth = mod->alias.meshes[0].skinWidth;
		outMesh->skinHeight = mod->alias.meshes[0].skinHeight;
	}

	isw = 1.0 / (double)outMesh->skinWidth;
	ish = 1.0 / (double)outMesh->skinHeight;

	/* load triangle lists */
	pintri = (const dMD2Triangle_t *) ((const byte *) md2 + LittleLong(md2->ofs_tris));
	pincoord = (const dMD2Coord_t *) ((const byte *) md2 + LittleLong(md2->ofs_st));

	for (i = 0; i < outMesh->num_tris; i++) {
		for (j = 0; j < 3; j++) {
			tempIndex[i * 3 + j] = (int32_t)LittleShort(pintri[i].index_verts[j]);
			tempSTIndex[i * 3 + j] = (int32_t)LittleShort(pintri[i].index_st[j]);
		}
	}

	/* build list of unique vertices */
	numIndexes = outMesh->num_tris * 3;
	numVerts = 0;
	outMesh->indexes = outIndex = Mem_PoolAlloc(sizeof(int32_t) * numIndexes, vid_modelPool, 0);

	for (i = 0; i < numIndexes; i++)
		indRemap[i] = -1;

	for (i = 0; i < numIndexes; i++) {
		if (indRemap[i] != -1)
			continue;

		/* remap duplicates */
		for (j = i + 1; j < numIndexes; j++) {
			if (tempIndex[j] != tempIndex[i])
				continue;
			if (pincoord[tempSTIndex[j]].s != pincoord[tempSTIndex[i]].s
			 || pincoord[tempSTIndex[j]].t != pincoord[tempSTIndex[i]].t)
				continue;

			indRemap[j] = i;
			outIndex[j] = numVerts;
		}

		/* add unique vertex */
		indRemap[i] = i;
		outIndex[i] = numVerts++;
	}
	outMesh->num_verts = numVerts;

	if (outMesh->num_verts >= 4096)
		Com_Printf("model %s has more than 4096 verts\n", mod->name);

	if (outMesh->num_verts <= 0 || outMesh->num_verts >= 8192)
		Com_Error(ERR_DROP, "R_ModLoadAliasMD2Mesh: invalid amount of verts for model '%s' (verts: %i, tris: %i)\n",
			mod->name, outMesh->num_verts, outMesh->num_tris);

	for (i = 0; i < numIndexes; i++) {
		if (indRemap[i] == i)
			continue;

		outIndex[i] = outIndex[indRemap[i]];
	}

	outMesh->stcoords = outCoord = Mem_PoolAlloc(sizeof(mAliasCoord_t) * outMesh->num_verts, vid_modelPool, 0);
	for (j = 0; j < numIndexes; j++) {
		outCoord[outIndex[j]][0] = (float)(((double)LittleShort(pincoord[tempSTIndex[indRemap[j]]].s) + 0.5) * isw);
		outCoord[outIndex[j]][1] = (float)(((double)LittleShort(pincoord[tempSTIndex[indRemap[j]]].t) + 0.5) * isw);
	}

	/* load the frames */
	outFrameTmp = outFrame = Mem_PoolAlloc(sizeof(mAliasFrame_t) * mod->alias.num_frames, vid_modelPool, 0);
	outMesh->vertexes = outVertex = Mem_PoolAlloc(sizeof(mAliasVertex_t) * mod->alias.num_frames * outMesh->num_verts, vid_modelPool, 0);
	if (mod->alias.num_meshes == 1)
		mod->alias.frames = outFrame;
	else if (mod->alias.num_frames != LittleLong(md2->num_frames))
		Com_Error(ERR_DROP, "R_ModLoadAliasMD2Mesh: invalid amount of frames for lod model for '%s'\n", mod->name);

	for (i = 0; i < mod->alias.num_frames; i++, outFrame++, outVertex += numVerts) {
		const dMD2Frame_t *pinframe = (const dMD2Frame_t *) ((const byte *) md2 + LittleLong(md2->ofs_frames) + i * frameSize);

		for (j = 0; j < 3; j++)
			outFrame->scale[j] = LittleFloat(pinframe->scale[j]);

		if (mod->alias.num_meshes == 1) {
			for (j = 0; j < 3; j++)
				outFrame->translate[j] = LittleFloat(pinframe->translate[j]);

			VectorCopy(outFrame->translate, outFrame->mins);
			VectorMA(outFrame->translate, 255, outFrame->scale, outFrame->maxs);

			AddPointToBounds(outFrame->mins, mod->mins, mod->maxs);
			AddPointToBounds(outFrame->maxs, mod->mins, mod->maxs);
		}

		for (j = 0; j < numIndexes; j++) {
			outVertex[outIndex[j]].point[0] = (int16_t)pinframe->verts[tempIndex[indRemap[j]]].v[0] * outFrame->scale[0];
			outVertex[outIndex[j]].point[1] = (int16_t)pinframe->verts[tempIndex[indRemap[j]]].v[1] * outFrame->scale[1];
			outVertex[outIndex[j]].point[2] = (int16_t)pinframe->verts[tempIndex[indRemap[j]]].v[2] * outFrame->scale[2];
		}

#if 0
		/* Calculate normals */
		R_CalcAliasNormals(numIndexes, outIndex, numVerts, outMesh);
#endif
	}

	if (mod->alias.num_meshes > 1)
		Mem_Free(outFrameTmp);
}

/**
 * @brief Adds new meshes to md2 models for different level of detail meshes
 * @param mod The model to load the lod models for
 * @note We support three different levels here
 */
static void R_ModLoadLevelOfDetailData (model_t* mod)
{
	char base[MAX_QPATH];
	int i;

	Com_StripExtension(mod->name, base, sizeof(base));

	for (i = 1; i <= 3; i++) {
		if (FS_CheckFile("%s-lod%02i.md2", base, i) != -1) {
			byte *buf;
			int bufSize;
			char fileName[MAX_QPATH];
			const dMD2Model_t *md2;

			Com_Printf("found lod model for %s\n", mod->name);

			Com_sprintf(fileName, sizeof(fileName), "%s-lod%02i.md2", base, i);
			/* load the file */
			bufSize = FS_LoadFile(fileName, &buf);
			if (!buf)
				return;

			/* get the disk data */
			md2 = (const dMD2Model_t *) buf;

			R_ModLoadAliasMD2Mesh(mod, md2, bufSize);

			FS_FreeFile(buf);
		}
	}
}

/**
 * @brief Load MD2 models from file.
 */
void R_ModLoadAliasMD2Model (model_t *mod, byte *buffer, int bufSize)
{
	dMD2Model_t *md2;
	int size;
	byte *tagbuf = NULL, *animbuf = NULL;
	size_t l;

	/* get the disk data */
	md2 = (dMD2Model_t *) buffer;

	/* only one mesh for md2 models */
	mod->alias.num_frames = LittleLong(md2->num_frames);
	if (mod->alias.num_frames <= 0 || mod->alias.num_frames >= MD2_MAX_FRAMES)
		Com_Error(ERR_DROP, "model %s has too many (or no) frames", mod->name);

	/* fixed values */
	mod->type = mod_alias_md2;

	ClearBounds(mod->mins, mod->maxs);

	R_ModLoadAliasMD2Mesh(mod, md2, bufSize);

	/* load the tags */
	Q_strncpyz(mod->alias.tagname, mod->name, sizeof(mod->alias.tagname));
	/* strip model extension and set the extension to tag */
	l = strlen(mod->alias.tagname) - 4;
	strcpy(&(mod->alias.tagname[l]), ".tag");

	/* try to load the tag file */
	if (FS_CheckFile("%s", mod->alias.tagname) != -1) {
		/* load the tags */
		size = FS_LoadFile(mod->alias.tagname, &tagbuf);
		R_ModLoadTags(mod, tagbuf, size);
		FS_FreeFile(tagbuf);
	}

	/* load the animations */
	Q_strncpyz(mod->alias.animname, mod->name, sizeof(mod->alias.animname));
	l = strlen(mod->alias.animname) - 4;
	strcpy(&(mod->alias.animname[l]), ".anm");

	/* try to load the animation file */
	if (FS_CheckFile("%s", mod->alias.animname) != -1) {
		/* load the tags */
		FS_LoadFile(mod->alias.animname, &animbuf);
		R_ModLoadAnims(&mod->alias, animbuf);
		FS_FreeFile(animbuf);
	}

	R_ModLoadLevelOfDetailData(mod);

	R_ModLoadArrayDataForStaticModel(&mod->alias, mod->alias.meshes);
}
