/**
 * @file
 * @brief md3 alias model loading
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
MD3 ALIAS MODELS
==============================================================================
*/

/**
 * @brief Load MD3 models from file.
 * @note Some Vic code here not fully used
 */
void R_ModLoadAliasMD3Model (model_t* mod, byte* buffer, int bufSize)
{
	const dmd3_t* md3 = (dmd3_t*)buffer;
	int version = LittleLong(md3->version);

	if (version != MD3_ALIAS_VERSION) {
		Com_Error(ERR_DROP, "%s has wrong version number (%i should be %i)",
				mod->name, version, MD3_ALIAS_VERSION);
	}

	mod->type = mod_alias_md3;
	/* byte swap the header fields and sanity check */
	mod->alias.num_frames = LittleLong(md3->num_frames);
	mod->alias.num_tags = LittleLong(md3->num_tags);
	mod->alias.num_meshes = LittleLong(md3->num_meshes);

	if (mod->alias.num_frames <= 0)
		Com_Error(ERR_DROP, "model %s has no frames", mod->name);
	else if (mod->alias.num_frames > MD3_MAX_FRAMES)
		Com_Error(ERR_DROP, "model %s has too many frames", mod->name);

	if (mod->alias.num_tags > MD3_MAX_TAGS)
		Com_Error(ERR_DROP, "model %s has too many tags", mod->name);
	else if (mod->alias.num_tags < 0)
		Com_Error(ERR_DROP, "model %s has invalid number of tags", mod->name);

	if (mod->alias.num_meshes <= 0)
		Com_Error(ERR_DROP, "model %s has no meshes", mod->name);
	else if (mod->alias.num_meshes > MD3_MAX_MESHES)
		Com_Error(ERR_DROP, "model %s has too many meshes", mod->name);

	/* load the frames */
	const dmd3frame_t* pinframe = (const dmd3frame_t*)((const byte*)md3 + LittleLong(md3->ofs_frames));
	mAliasFrame_t* poutframe = mod->alias.frames = Mem_PoolAllocTypeN(mAliasFrame_t, mod->alias.num_frames, vid_modelPool);

	mod->radius = 0;
	mod->modBox.setNegativeVolume();

	int i, j, l;
	for (i = 0; i < mod->alias.num_frames; i++, pinframe++, poutframe++) {
		for (j = 0; j < 3; j++) {
			poutframe->fBox.mins[j] = LittleFloat(pinframe->mins[j]);
			poutframe->fBox.maxs[j] = LittleFloat(pinframe->maxs[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		poutframe->radius = LittleFloat(pinframe->radius);
		mod->radius = std::max(mod->radius, poutframe->radius);
		mod->modBox.add(poutframe->fBox);
	}

	/* load the tags */
	if (mod->alias.num_tags) {
		const dmd3tag_t* pintag = (const dmd3tag_t*)((const byte*)md3 + LittleLong(md3->ofs_tags));
		mAliasTag_t* pouttag = mod->alias.tags = Mem_PoolAllocTypeN(mAliasTag_t, mod->alias.num_tags, vid_modelPool);

		/** @todo the tag loading is broken - the order is different in the md3 file */
		for (l = 0; l < mod->alias.num_tags; l++, pouttag++, pintag++) {
			mAliasTagOrientation_t* orient = pouttag->orient = Mem_PoolAllocTypeN(mAliasTagOrientation_t, mod->alias.num_frames, vid_modelPool);
			memcpy(pouttag->name, pintag->name, MD3_MAX_PATH);
			for (i = 0; i < mod->alias.num_frames; i++, orient++) {
				for (j = 0; j < 3; j++) {
					orient->origin[j] = LittleFloat(pintag->orient.origin[j]);
					orient->axis[0][j] = LittleFloat(pintag->orient.axis[0][j]);
					orient->axis[1][j] = LittleFloat(pintag->orient.axis[1][j]);
					orient->axis[2][j] = LittleFloat(pintag->orient.axis[2][j]);
				}
			}
		}
	}

	/* load the meshes */
	const dmd3mesh_t* pinmesh = (const dmd3mesh_t*)((const byte*)md3 + LittleLong(md3->ofs_meshes));
	mAliasMesh_t* poutmesh = mod->alias.meshes = Mem_PoolAllocTypeN(mAliasMesh_t, mod->alias.num_meshes, vid_modelPool);

	for (i = 0; i < mod->alias.num_meshes; i++, poutmesh++) {
		memcpy(poutmesh->name, pinmesh->name, MD3_MAX_PATH);

		if (strncmp(pinmesh->id, "IDP3", 4)) {
			Com_Error(ERR_DROP, "mesh %s in model %s has wrong id (%s should be %i)",
					poutmesh->name, mod->name, pinmesh->id, IDMD3HEADER);
		}

		poutmesh->num_tris = LittleLong(pinmesh->num_tris);
		poutmesh->num_skins = LittleLong(pinmesh->num_skins);
		poutmesh->num_verts = LittleLong(pinmesh->num_verts);

		if (poutmesh->num_skins <= 0)
			Com_Error(ERR_DROP, "mesh %i in model %s has no skins", i, mod->name);
		else if (poutmesh->num_skins > MD3_MAX_SHADERS)
			Com_Error(ERR_DROP, "mesh %i in model %s has too many skins", i, mod->name);

		if (poutmesh->num_tris <= 0)
			Com_Error(ERR_DROP, "mesh %i in model %s has no triangles", i, mod->name);
		else if (poutmesh->num_tris > MD3_MAX_TRIANGLES)
			Com_Error(ERR_DROP, "mesh %i in model %s has too many triangles", i, mod->name);

		if (poutmesh->num_verts <= 0)
			Com_Error(ERR_DROP, "mesh %i in model %s has no vertices", i, mod->name);
		else if (poutmesh->num_verts > MD3_MAX_VERTS)
			Com_Error(ERR_DROP, "mesh %i in model %s has too many vertices", i, mod->name);

		/* register all skins */
		const dmd3skin_t* pinskin = (const dmd3skin_t*)((const byte*)pinmesh + LittleLong(pinmesh->ofs_skins));
		poutmesh->skins = Mem_PoolAllocTypeN(mAliasSkin_t, poutmesh->num_skins, vid_modelPool);

		for (j = 0; j < poutmesh->num_skins; j++) {
			mAliasSkin_t* modelSkin = &poutmesh->skins[j];
			modelSkin->skin = R_AliasModelGetSkin(mod->name, pinskin->name);
			Q_strncpyz(modelSkin->name, modelSkin->skin->name, MODEL_MAX_PATH);
		}

		/* load the indexes */
		const int32_t* pinindex = (const int32_t*)((const byte*)pinmesh + LittleLong(pinmesh->ofs_tris));
		int32_t* poutindex = poutmesh->indexes = Mem_PoolAllocTypeN(int32_t, poutmesh->num_tris * 3, vid_modelPool);

		for (j = 0; j < poutmesh->num_tris; j++, pinindex += 3, poutindex += 3) {
			poutindex[0] = (int32_t)LittleLong(pinindex[0]);
			poutindex[1] = (int32_t)LittleLong(pinindex[1]);
			poutindex[2] = (int32_t)LittleLong(pinindex[2]);
		}

		/* load the texture coordinates */
		const dmd3coord_t* pincoord = (const dmd3coord_t*)((const byte*)pinmesh + LittleLong(pinmesh->ofs_tcs));
		poutmesh->stcoords = Mem_PoolAllocTypeN(mAliasCoord_t, poutmesh->num_verts, vid_modelPool);

		for (j = 0; j < poutmesh->num_verts; j++, pincoord++) {
			poutmesh->stcoords[j][0] = LittleFloat(pincoord->st[0]);
			poutmesh->stcoords[j][1] = LittleFloat(pincoord->st[1]);
		}

		/* load the vertexes and normals */
		const dmd3vertex_t* pinvert = (const dmd3vertex_t*)((const byte*)pinmesh + LittleLong(pinmesh->ofs_verts));
		mAliasVertex_t* poutvert = poutmesh->vertexes = Mem_PoolAllocTypeN(mAliasVertex_t, mod->alias.num_frames * poutmesh->num_verts, vid_modelPool);

		for (l = 0; l < mod->alias.num_frames; l++) {
			for (j = 0; j < poutmesh->num_verts; j++, pinvert++, poutvert++) {
				float lat, lng;

				poutvert->point[0] = LittleShort(pinvert->point[0]) * MD3_XYZ_SCALE;
				poutvert->point[1] = LittleShort(pinvert->point[1]) * MD3_XYZ_SCALE;
				poutvert->point[2] = LittleShort(pinvert->point[2]) * MD3_XYZ_SCALE;

				lat = (pinvert->norm >> 8) & 0xff;
				lng = (pinvert->norm & 0xff);

				lat *= M_PI / 128.0f;
				lng *= M_PI / 128.0f;

				poutvert->normal[0] = cos(lat) * sin(lng);
				poutvert->normal[1] = sin(lat) * sin(lng);
				poutvert->normal[2] = cos(lng);
			}
		}

		R_ModCalcUniqueNormalsAndTangents(poutmesh, mod->alias.num_frames, 0.5);

		pinmesh = (const dmd3mesh_t*)((const byte*)pinmesh + LittleLong(pinmesh->meshsize));

		R_ModLoadArrayData(&mod->alias, poutmesh, true);
	}
}
