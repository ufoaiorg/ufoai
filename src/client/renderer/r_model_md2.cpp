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

static void R_ModLoadTags (model_t * mod, void *buffer, int bufSize)
{
	dMD2tag_t *pintag;
	int version;
	int i, j;
	float *inmat;
	dMD2tag_t pheader;
	mAliasTag_t *pouttag;

	pintag = (dMD2tag_t *) buffer;

	version = LittleLong(pintag->version);
	if (version != TAG_VERSION)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tag has wrong version number (%i should be %i)", version, TAG_VERSION);

	/* byte swap the header fields and sanity check */
	pheader.ident = LittleLong(pintag->ident);
	pheader.version = LittleLong(pintag->version);
	pheader.num_tags = LittleLong(pintag->num_tags);
	pheader.num_frames = LittleLong(pintag->num_frames);
	pheader.ofs_names = LittleLong(pintag->ofs_names);
	pheader.ofs_tags = LittleLong(pintag->ofs_tags);
	pheader.ofs_end = LittleLong(pintag->ofs_end);
	pheader.ofs_extractend = LittleLong(pintag->ofs_extractend);

	if (pheader.num_tags <= 0)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tag file for %s has no tags", mod->name);

	if (pheader.num_frames <= 0)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tag file for %s has no frames", mod->name);

	/* load tag matrices */
	inmat = (float *) ((byte *) pintag + pheader.ofs_tags);

	if (bufSize != pheader.ofs_end)
		Com_Error(ERR_FATAL, "R_ModLoadTags: tagfile %s is broken - expected: %i, offsets tell us to read: %i",
			mod->name, bufSize, pheader.ofs_end);

	if (pheader.num_frames != mod->alias.num_frames)
		Com_Printf("R_ModLoadTags: found %i frames in %s but model has %i frames\n",
			pheader.num_frames, mod->name, mod->alias.num_frames);

	if (pheader.ofs_names != 32)
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_name in tagfile for %s", mod->name);
	if (pheader.ofs_tags != pheader.ofs_names + (pheader.num_tags * 64))
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_tags for tagfile %s", mod->name);
	/* (4 * 3) * 4 bytes (int) */
	if (pheader.ofs_end != pheader.ofs_tags + (pheader.num_tags * pheader.num_frames * 48))
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_end for tagfile %s", mod->name);
	/* (4 * 4) * 4 bytes (int) */
	if (pheader.ofs_extractend != pheader.ofs_tags + (pheader.num_tags * pheader.num_frames * 64))
		Com_Error(ERR_FATAL, "R_ModLoadTags: invalid ofs_extractend for tagfile %s", mod->name);

	mod->alias.num_tags = pheader.num_tags;
	pouttag = mod->alias.tags = (mAliasTag_t *)Mem_PoolAlloc(sizeof(mAliasTag_t) * mod->alias.num_tags, vid_modelPool, 0);

	for (j = 0; j < pheader.num_tags; j++, pouttag++) {
		mAliasTagOrientation_t *pouttagorient = mod->alias.tags[j].orient = (mAliasTagOrientation_t *)Mem_PoolAlloc(sizeof(mAliasTagOrientation_t) * mod->alias.num_frames, vid_modelPool, 0);
		memcpy(pouttag->name, (char *) pintag + pheader.ofs_names + j * MD2_MAX_SKINNAME, sizeof(pouttag->name));
		for (i = 0; i < pheader.num_frames; i++, pouttagorient++) {
			int k;
			for (k = 0; k < 3; k++) {
				pouttagorient->axis[k][0] = LittleFloat(*inmat++);
				pouttagorient->axis[k][1] = LittleFloat(*inmat++);
				pouttagorient->axis[k][2] = LittleFloat(*inmat++);
			}
			VectorSet(pouttagorient->origin, LittleFloat(*inmat++), LittleFloat(*inmat++), LittleFloat(*inmat++));
		}
	}
}

/**
 * @brief Assume that the indexArray is *NOT* filled, and load data for the model accordingly
 */
static void R_ModLoadAliasMD2MeshUnindexed (model_t *mod, const dMD2Model_t *md2, int bufSize, qboolean loadNormals)
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
	int frameSize, numVerts;
	double isw;
	const char *md2Path;
	int md2Verts;

	outMesh = &mod->alias.meshes[mod->alias.num_meshes - 1];

	Q_strncpyz(outMesh->name, mod->name, sizeof(outMesh->name));
	md2Verts = LittleLong(md2->num_verts);
	if (md2Verts <= 0 || md2Verts >= MD2_MAX_VERTS)
		Com_Error(ERR_DROP, "model %s has too many (or no) vertices (%i/%i)",
			mod->name, md2Verts, MD2_MAX_VERTS);
	outMesh->num_tris = LittleLong(md2->num_tris);
	if (outMesh->num_tris <= 0 || outMesh->num_tris >= MD2_MAX_TRIANGLES)
		Com_Error(ERR_DROP, "model %s has too many (or no) triangles (%i/%i)",
			mod->name, outMesh->num_tris, MD2_MAX_TRIANGLES);
	frameSize = LittleLong(md2->framesize);
	outMesh->num_verts = md2Verts;

	if (mod->alias.num_meshes == 1) {
		if (R_UseActorSkin() && Q_strstart(outMesh->name, "models/soldiers/")) {
			image_t *defaultSkin;
			md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
			defaultSkin = R_AliasModelGetSkin(mod->name, md2Path);
			R_LoadActorSkinsFromModel(outMesh, defaultSkin);

			/** @todo Should we check skin image versus this size? */
			outMesh->skinWidth = LittleLong(md2->skinwidth);
			outMesh->skinHeight = LittleLong(md2->skinheight);
		} else {
			/* load the skins */
			outMesh->num_skins = LittleLong(md2->num_skins);
			if (outMesh->num_skins < 0 || outMesh->num_skins >= MD2_MAX_SKINS)
				Com_Error(ERR_DROP, "Could not load model '%s' - invalid num_skins value: %i", mod->name, outMesh->num_skins);

			outMesh->skins = (mAliasSkin_t *)Mem_PoolAlloc(sizeof(mAliasSkin_t) * outMesh->num_skins, vid_modelPool, 0);
			md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
			for (i = 0; i < outMesh->num_skins; i++) {
				outMesh->skins[i].skin = R_AliasModelGetSkin(mod->name, md2Path + i * MD2_MAX_SKINNAME);
				Q_strncpyz(outMesh->skins[i].name, outMesh->skins[i].skin->name, sizeof(outMesh->skins[i].name));
			}

			outMesh->skinWidth = LittleLong(md2->skinwidth);
			outMesh->skinHeight = LittleLong(md2->skinheight);
		}

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
	outMesh->num_indexes = outMesh->num_tris * 3;
	numVerts = 0;
	outMesh->indexes = outIndex = (int32_t *)Mem_PoolAlloc(sizeof(int32_t) * outMesh->num_indexes, vid_modelPool, 0);

	for (i = 0; i < outMesh->num_indexes; i++)
		indRemap[i] = -1;

	for (i = 0; i < outMesh->num_indexes; i++) {
		if (indRemap[i] != -1)
			continue;

		/* remap duplicates */
		for (j = i + 1; j < outMesh->num_indexes; j++) {
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

	if (numVerts >= 4096)
		Com_Printf("model %s has more than 4096 verts (original verts: %i and tris: %i)\n",
				mod->name, outMesh->num_verts, outMesh->num_tris);

	outMesh->num_verts = numVerts;
	if (outMesh->num_verts <= 0 || outMesh->num_verts >= MAX_ALIAS_VERTS)
		Com_Error(ERR_DROP, "R_ModLoadAliasMD2Mesh: invalid amount of verts for model '%s' (verts: %i, tris: %i)\n",
			mod->name, outMesh->num_verts, outMesh->num_tris);

	for (i = 0; i < outMesh->num_indexes; i++) {
		if (indRemap[i] == i)
			continue;

		outIndex[i] = outIndex[indRemap[i]];
	}

	outMesh->stcoords = outCoord = (mAliasCoord_t *)Mem_PoolAlloc(sizeof(mAliasCoord_t) * outMesh->num_verts, vid_modelPool, 0);
	for (j = 0; j < outMesh->num_indexes; j++) {
		outCoord[outIndex[j]][0] = (float)(((double)LittleShort(pincoord[tempSTIndex[indRemap[j]]].s) + 0.5) * isw);
		outCoord[outIndex[j]][1] = (float)(((double)LittleShort(pincoord[tempSTIndex[indRemap[j]]].t) + 0.5) * isw);
	}

	/* load the frames */
	outFrameTmp = outFrame = (mAliasFrame_t *)Mem_PoolAlloc(sizeof(mAliasFrame_t) * mod->alias.num_frames, vid_modelPool, 0);
	outMesh->vertexes = outVertex = (mAliasVertex_t *)Mem_PoolAlloc(sizeof(mAliasVertex_t) * mod->alias.num_frames * outMesh->num_verts, vid_modelPool, 0);
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

		for (j = 0; j < outMesh->num_indexes; j++) {
			const int index = tempIndex[indRemap[j]];
			const dMD2TriangleVertex_t *v = &pinframe->verts[index];
			float* ov = outVertex[outIndex[j]].point;
			ov[0] = (int16_t)v->v[0] * outFrame->scale[0];
			ov[1] = (int16_t)v->v[1] * outFrame->scale[1];
			ov[2] = (int16_t)v->v[2] * outFrame->scale[2];
		}
	}

	/* Calculate normals and tangents */
	if (loadNormals)
		R_ModCalcUniqueNormalsAndTangents(outMesh, mod->alias.num_frames, 0.5);

	if (mod->alias.num_meshes > 1)
		Mem_Free(outFrameTmp);
}

/**
 * @brief Assume that the indexArray is already filled, and load data for the model accordingly
 */
static void R_ModLoadAliasMD2MeshIndexed (model_t *mod, const dMD2Model_t *md2, int bufSize)
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
	int32_t *outIndex;
	int frameSize, numIndexes, numVerts;
	double isw;
	const char *md2Path;
	int md2Verts;

	outMesh = &mod->alias.meshes[mod->alias.num_meshes - 1];

	Q_strncpyz(outMesh->name, mod->name, sizeof(outMesh->name));
	md2Verts = LittleLong(md2->num_verts);
	if (md2Verts <= 0 || md2Verts >= MD2_MAX_VERTS)
		Com_Error(ERR_DROP, "model %s has too many (or no) vertices (%i/%i)",
			mod->name, md2Verts, MD2_MAX_VERTS);
	outMesh->num_tris = LittleLong(md2->num_tris);
	if (outMesh->num_tris <= 0 || outMesh->num_tris >= MD2_MAX_TRIANGLES)
		Com_Error(ERR_DROP, "model %s has too many (or no) triangles (%i/%i)",
			mod->name, outMesh->num_tris, MD2_MAX_TRIANGLES);
	frameSize = LittleLong(md2->framesize);

	if (outMesh->num_verts >= 4096)
		Com_Printf("model %s has more than 4096 verts\n", mod->name);

	if (outMesh->num_verts <= 0 || outMesh->num_verts >= MAX_ALIAS_VERTS)
		Com_Error(ERR_DROP, "R_ModLoadAliasMD2Mesh: invalid amount of verts for model '%s' (verts: %i, tris: %i)",
			mod->name, outMesh->num_verts, outMesh->num_tris);

	if (mod->alias.num_meshes == 1) {
		/* load the skins */
		outMesh->num_skins = LittleLong(md2->num_skins);
		if (outMesh->num_skins < 0 || outMesh->num_skins >= MD2_MAX_SKINS)
			Com_Error(ERR_DROP, "Could not load model '%s' - invalid num_skins value: %i\n", mod->name, outMesh->num_skins);

		outMesh->skins = (mAliasSkin_t *)Mem_PoolAlloc(sizeof(mAliasSkin_t) * outMesh->num_skins, vid_modelPool, 0);
		md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
		for (i = 0; i < outMesh->num_skins; i++) {
			outMesh->skins[i].skin = R_AliasModelGetSkin(mod->name, md2Path + i * MD2_MAX_SKINNAME);
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
	numVerts = outMesh->num_verts;

	if (numIndexes != outMesh->num_indexes)
		Com_Error(ERR_DROP, "mdx and model file differ: %s", mod->name);

	outMesh->stcoords = outCoord = (mAliasCoord_t *)Mem_PoolAlloc(sizeof(mAliasCoord_t) * outMesh->num_verts, vid_modelPool, 0);
	outIndex = outMesh->indexes;
	for (j = 0; j < outMesh->num_indexes; j++) {
		const int32_t index = outIndex[j];
		outCoord[index][0] = (float)(((double)LittleShort(pincoord[tempSTIndex[j]].s) + 0.5) * isw);
		outCoord[index][1] = (float)(((double)LittleShort(pincoord[tempSTIndex[j]].t) + 0.5) * isw);
	}

	/* load the frames */
	outFrameTmp = outFrame = (mAliasFrame_t *)Mem_PoolAlloc(sizeof(mAliasFrame_t) * mod->alias.num_frames, vid_modelPool, 0);
	outVertex = outMesh->vertexes;
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

		for (j = 0; j < outMesh->num_indexes; j++) {
			const int index = tempIndex[j];
			const dMD2TriangleVertex_t *v = &pinframe->verts[index];
			float* ov = outVertex[outIndex[j]].point;
			ov[0] = (int16_t)v->v[0] * outFrame->scale[0];
			ov[1] = (int16_t)v->v[1] * outFrame->scale[1];
			ov[2] = (int16_t)v->v[2] * outFrame->scale[2];
		}
	}

	if (mod->alias.num_meshes > 1)
		Mem_Free(outFrameTmp);
}

/**
 * @brief See if the model has an MDX file, and then load the model data appropriately for either case
 */
static void R_ModLoadAliasMD2Mesh (model_t *mod, const dMD2Model_t *md2, int bufSize, qboolean loadNormals)
{
	int version;
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
		mod->alias.meshes = (mAliasMesh_t *)Mem_PoolAlloc(size, vid_modelPool, 0);
	else {
		mod->alias.meshes = (mAliasMesh_t *)Mem_ReAlloc(mod->alias.meshes, size);
	}

	if (loadNormals) {
		/* try to load normals and tangents */
		if (R_ModLoadMDX(mod)) {
			R_ModLoadAliasMD2MeshIndexed(mod, md2, bufSize);
		} else {
			/* compute normals and tangents */
			R_ModLoadAliasMD2MeshUnindexed(mod, md2, bufSize, qtrue);
		}
	} else {
		/* don't load normals and tangents */
		R_ModLoadAliasMD2MeshUnindexed(mod, md2, bufSize, qfalse);
	}
}

/**
 * @brief Adds new meshes to md2 models for different level of detail meshes
 * @param mod The model to load the lod models for
 * @param loadNormals If true, load vertex normals
 * @note We support three different levels here
 */
static void R_ModLoadLevelOfDetailData (model_t* mod, qboolean loadNormals)
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

			R_ModLoadAliasMD2Mesh(mod, md2, bufSize, loadNormals);

			FS_FreeFile(buf);
		}
	}
}

/**
 * @brief Load MD2 models from file.
 */
void R_ModLoadAliasMD2Model (model_t *mod, byte *buffer, int bufSize, qboolean loadNormals)
{
	dMD2Model_t *md2;
	byte *tagbuf = NULL;
	char tagname[MAX_QPATH];

	/* get the disk data */
	md2 = (dMD2Model_t *) buffer;

	/* only one mesh for md2 models */
	mod->alias.num_frames = LittleLong(md2->num_frames);
	if (mod->alias.num_frames <= 0 || mod->alias.num_frames >= MD2_MAX_FRAMES)
		Com_Error(ERR_DROP, "model %s has too many (or no) frames", mod->name);

	/* fixed values */
	mod->type = mod_alias_md2;

	ClearBounds(mod->mins, mod->maxs);

	R_ModLoadAliasMD2Mesh(mod, md2, bufSize, loadNormals);

	/* load the tags */
	Com_StripExtension(mod->name, tagname, sizeof(tagname));
	Com_DefaultExtension(tagname, sizeof(tagname), ".tag");

	/* try to load the tag file */
	if (FS_CheckFile("%s", tagname) != -1) {
		/* load the tags */
		const int size = FS_LoadFile(tagname, &tagbuf);
		R_ModLoadTags(mod, tagbuf, size);
		FS_FreeFile(tagbuf);
	}

	R_ModLoadLevelOfDetailData(mod, loadNormals);

	R_ModLoadArrayData(&mod->alias, mod->alias.meshes, loadNormals);
}
