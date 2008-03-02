/**
 * @file r_model_dpm.c
 * @brief dpm alias model loading
 */

/*
Copyright (C) 1997-2001 Darkplaces

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

/**
 * @brief Load dpm models from file.
 * @todo Load this into mAliasModel_t structure
 */
void R_ModLoadAliasDPMModel (model_t *mod, void *buffer, int bufSize)
{
	mAliasMesh_t *outMesh;
	mAliasFrame_t *outFrame;
	mAliasVertex_t *outVertex;
	mAliasCoord_t *outCoord;
	int32_t *outIndex, *index;
	int num;
	int i, j, k;
	dpmbone_t *bone;
	dpmbonepose_t *bonepose;
	dpmmesh_t *mesh;
	dpmbonevert_t *bonevert;
	dpmvertex_t *vert;
	dpmframe_t *frame;
	dpmheader_t *dpm;

	/* fixed values */
	mod->type = mod_alias_dpm;

	/* get the disk data */
	dpm = (dpmheader_t *) buffer;

	dpm->type = BigLong(dpm->type);
	if (dpm->type != 2)
		Com_Error(ERR_DROP, "%s has wrong type number (%i should be 2)", mod->name, dpm->type);

	mod->mins[0] = BigFloat(dpm->mins[0]);
	mod->mins[1] = BigFloat(dpm->mins[1]);
	mod->mins[2] = BigFloat(dpm->mins[2]);
	mod->maxs[0] = BigFloat(dpm->maxs[0]);
	mod->maxs[1] = BigFloat(dpm->maxs[1]);
	mod->maxs[2] = BigFloat(dpm->maxs[2]);
	/*mod->yawradius = BigFloat(dpm->yawradius);*/
	mod->radius = BigFloat(dpm->allradius);

	mod->alias.num_bones = BigLong(dpm->num_bones);
	mod->alias.num_meshes = BigLong(dpm->num_meshs);
	mod->alias.num_frames = BigLong(dpm->num_frames);
	dpm->ofs_bones = BigLong(dpm->ofs_bones);
	dpm->ofs_meshs = BigLong(dpm->ofs_meshs);
	dpm->ofs_frames = BigLong(dpm->ofs_frames);
	for (i = 0, bone = (dpmbone_t *)((byte *)dpm + dpm->ofs_bones); i < mod->alias.num_bones; i++, bone++) {
		bone->parent = BigLong(bone->parent);
		bone->flags = BigLong(bone->flags);
	}

	mod->alias.meshes = outMesh = Mem_PoolAlloc(sizeof(mAliasMesh_t) * mod->alias.num_meshes, vid_modelPool, 0);
	for (i = 0, mesh = (dpmmesh_t *)((byte *)dpm + dpm->ofs_meshs); i < mod->alias.num_meshes; i++, mesh++, outMesh++) {
		outMesh->num_verts = BigLong(mesh->num_verts);
		outMesh->num_tris = BigLong(mesh->num_tris);
		mesh->ofs_verts = BigLong(mesh->ofs_verts);
		mesh->ofs_texcoords = BigLong(mesh->ofs_texcoords);
		mesh->ofs_indices = BigLong(mesh->ofs_indices);
		mesh->ofs_groupids = BigLong(mesh->ofs_groupids);
		outMesh->skins = Mem_PoolAlloc(sizeof(mAliasSkin_t), vid_modelPool, 0);
		outMesh->skins[0].skin = R_AliasModelGetSkin(mod, mesh->shadername);
		Q_strncpyz(outMesh->skins[0].name, outMesh->skins[0].skin->name, sizeof(outMesh->skins[0].name));

		for (j = 0, vert = (dpmvertex_t *)((byte *)dpm + mesh->ofs_verts); j < outMesh->num_verts; j++) {
			vert->numbones = BigLong(vert->numbones);
			for (k = 0, bonevert = (dpmbonevert_t *)(vert + 1); k < vert->numbones; k++, bonevert++) {
				bonevert->origin[0] = BigFloat(bonevert->origin[0]);
				bonevert->origin[1] = BigFloat(bonevert->origin[1]);
				bonevert->origin[2] = BigFloat(bonevert->origin[2]);
				bonevert->influence = BigFloat(bonevert->influence);
				bonevert->normal[0] = BigFloat(bonevert->normal[0]);
				bonevert->normal[1] = BigFloat(bonevert->normal[1]);
				bonevert->normal[2] = BigFloat(bonevert->normal[2]);
				bonevert->bonenum = BigLong(bonevert->bonenum);
			}
			vert = (dpmvertex_t *)bonevert;
		}

		outMesh->stcoords = Mem_PoolAlloc(sizeof(mAliasCoord_t) * outMesh->num_verts, vid_modelPool, 0);
		for (num = 0, index = (int32_t *)((byte *)dpm + mesh->ofs_texcoords); num < outMesh->num_verts; num++, index += 2) {
			outMesh->stcoords[j][0] = BigLong(index[0]);
			outMesh->stcoords[j][1] = BigLong(index[0]);
		}

		outMesh->indexes = outIndex = Mem_PoolAlloc(sizeof(int32_t) * outMesh->num_tris * 3, vid_modelPool, 0);
		for (num = 0, index = (int32_t *)((byte *)dpm + mesh->ofs_indices); num < outMesh->num_tris; num++, index += 3) {
			outIndex[0] = BigLong(index[0]);
			outIndex[1] = BigLong(index[2]);
			outIndex[2] = BigLong(index[3]);
		}

		for (num = 0, index = (int32_t *)((byte *)dpm + mesh->ofs_groupids); num < outMesh->num_tris; num++, index++)
			index[0] = BigLong(index[0]);
	}

	mod->alias.frames = outFrame = Mem_PoolAlloc(sizeof(mAliasFrame_t) * mod->alias.num_frames, vid_modelPool, 0);
	for (i = 0, frame = (dpmframe_t *)((byte *)dpm + dpm->ofs_frames); i < mod->alias.num_frames; i++, frame++, outFrame++) {
		outFrame->mins[0] = BigFloat(frame->mins[0]);
		outFrame->mins[1] = BigFloat(frame->mins[1]);
		outFrame->mins[2] = BigFloat(frame->mins[2]);
		outFrame->maxs[0] = BigFloat(frame->maxs[0]);
		outFrame->maxs[1] = BigFloat(frame->maxs[1]);
		outFrame->maxs[2] = BigFloat(frame->maxs[2]);
		/*outFrame->yawradius = BigFloat(frame->yawradius);*/
		outFrame->radius = BigFloat(frame->allradius);
		frame->ofs_bonepositions = BigLong(frame->ofs_bonepositions);
		for (j = 0, bonepose = (dpmbonepose_t *)((byte *)dpm + frame->ofs_bonepositions); j < mod->alias.num_bones; j++, bonepose++) {
			bonepose->matrix[0][0] = BigFloat(bonepose->matrix[0][0]);
			bonepose->matrix[0][1] = BigFloat(bonepose->matrix[0][1]);
			bonepose->matrix[0][2] = BigFloat(bonepose->matrix[0][2]);
			bonepose->matrix[0][3] = BigFloat(bonepose->matrix[0][3]);
			bonepose->matrix[1][0] = BigFloat(bonepose->matrix[1][0]);
			bonepose->matrix[1][1] = BigFloat(bonepose->matrix[1][1]);
			bonepose->matrix[1][2] = BigFloat(bonepose->matrix[1][2]);
			bonepose->matrix[1][3] = BigFloat(bonepose->matrix[1][3]);
			bonepose->matrix[2][0] = BigFloat(bonepose->matrix[2][0]);
			bonepose->matrix[2][1] = BigFloat(bonepose->matrix[2][1]);
			bonepose->matrix[2][2] = BigFloat(bonepose->matrix[2][2]);
			bonepose->matrix[2][3] = BigFloat(bonepose->matrix[2][3]);
		}
	}
}
