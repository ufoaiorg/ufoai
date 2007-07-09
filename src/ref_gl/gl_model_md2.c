/**
 * @file gl_model_md2.c
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

#include "gl_local.h"

/*
==============================================================================
MD2 ALIAS MODELS
==============================================================================
*/

/**
 * @brief
 */
static void Mod_LoadTags (model_t * mod, void *buffer, int bufSize)
{
	dtag_t *pintag, *pheader;
	int version;
	int i, j, size;
	float *inmat, *outmat;
	int read;
	mdl_md2_t *md2;

	pintag = (dtag_t *) buffer;
	md2 = (mdl_md2_t *) mod->alias.extraData;

	version = LittleLong(pintag->version);
	if (version != TAG_VERSION)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: %s has wrong version number (%i should be %i)", mod->alias.tagname, version, TAG_VERSION);

	size = LittleLong(pintag->ofs_extractend);
	mod->alias.tagdata = ri.TagMalloc(ri.modelPool, size, 0);
	pheader = mod->alias.tagdata;

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dtag_t) / 4; i++)
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (pheader->num_tags <= 0)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: tag file %s has no tags", mod->alias.tagname);

	if (pheader->num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: tag file %s has no frames", mod->alias.tagname);

	/* load tag names */
	memcpy((char *) pheader + pheader->ofs_names, (char *) pintag + pheader->ofs_names, pheader->num_tags * MD2_MAX_SKINNAME);

	/* load tag matrices */
	inmat = (float *) ((byte *) pintag + pheader->ofs_tags);
	outmat = (float *) ((byte *) pheader + pheader->ofs_tags);

	if (bufSize != pheader->ofs_end)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: tagfile %s is broken - expected: %i, offsets tell us to read: %i\n",
			mod->alias.tagname, bufSize, pheader->ofs_end);

	if (pheader->num_frames != md2->num_frames)
		ri.Con_Printf(PRINT_ALL, "Mod_LoadTags: found %i frames in %s but model has %i frames\n",
			pheader->num_frames, mod->alias.tagname, md2->num_frames);

	if (pheader->ofs_names != 32)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_name for tagfile %s\n", mod->alias.tagname);
	if (pheader->ofs_tags != pheader->ofs_names + (pheader->num_tags * 64))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_tags for tagfile %s\n", mod->alias.tagname);
	/* (4 * 3) * 4 bytes (int) */
	if (pheader->ofs_end != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 48))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_end for tagfile %s\n", mod->alias.tagname);
	/* (4 * 4) * 4 bytes (int) */
	if (pheader->ofs_extractend != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 64))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_extractend for tagfile %s\n", mod->alias.tagname);

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
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: read: %i expected: %i - tags: %i, frames: %i (should be %i)",
			read, size, pheader->num_tags, pheader->num_frames, md2->num_frames);
}

/**
 * @brief Load MD2 models from file.
 */
extern void Mod_LoadAliasMD2Model (model_t * mod, void *buffer, int bufSize)
{
	int i, j;
	size_t l;
	mdl_md2_t *pinmodel, *pheader;
	dstvert_t *pinst, *poutst;
	dtriangle_t *pintri, *pouttri;
	dAliasFrame_t *pinframe, *poutframe;
	int *pincmd, *poutcmd;
	int version, size;

	byte *tagbuf = NULL, *animbuf = NULL;

	pinmodel = (mdl_md2_t *) buffer;

	version = LittleLong(pinmodel->version);
	if (version != MD2_ALIAS_VERSION)
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, version, MD2_ALIAS_VERSION);

	pheader = ri.TagMalloc(ri.modelPool, LittleLong(pinmodel->ofs_end), 0);
	mod->alias.extraData = pheader;

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(mdl_md2_t) / 4; i++) /* FIXME */
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (bufSize != pheader->ofs_end)
		ri.Sys_Error(ERR_DROP, "model %s broken offset values", mod->name);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		ri.Sys_Error(ERR_DROP, "model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);
	else if (pheader->skinheight <= 0 || pheader->skinwidth <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has invalid skin dimensions '%d x %d'", mod->name, pheader->skinheight, pheader->skinwidth);

	if (pheader->num_xyz <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no vertices", mod->name);
	else if (pheader->num_xyz > MD2_MAX_VERTS)
		ri.Sys_Error(ERR_DROP, "model %s has too many vertices", mod->name);

	if (pheader->num_st <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no st vertices", mod->name);

	if (pheader->num_tris <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no triangles", mod->name);
	else if (pheader->num_tris > MD2_MAX_TRIANGLES)
		ri.Sys_Error(ERR_DROP, "model %s has too many triangles", mod->name);

	if (pheader->num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no frames", mod->name);
	else if (pheader->num_frames > MD2_MAX_FRAMES)
		ri.Sys_Error(ERR_DROP, "model %s has too many frames", mod->name);

	/* load base s and t vertices (not used in gl version) */
	pinst = (dstvert_t *) ((byte *) pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *) ((byte *) pheader + pheader->ofs_st);

	for (i = 0; i < pheader->num_st; i++) {
		poutst[i].s = LittleShort(pinst[i].s);
		poutst[i].t = LittleShort(pinst[i].t);
	}

	/* load triangle lists */
	pintri = (dtriangle_t *) ((byte *) pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *) ((byte *) pheader + pheader->ofs_tris);

	for (i = 0; i < pheader->num_tris; i++) {
		for (j = 0; j < 3; j++) {
			pouttri[i].index_xyz[j] = LittleShort(pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort(pintri[i].index_st[j]);
		}
	}

	/* load the frames */
	for (i = 0; i < pheader->num_frames; i++) {
		pinframe = (dAliasFrame_t *) ((byte *) pinmodel + pheader->ofs_frames + i * pheader->framesize);
		poutframe = (dAliasFrame_t *) ((byte *) pheader + pheader->ofs_frames + i * pheader->framesize);

		memcpy(poutframe->name, pinframe->name, sizeof(poutframe->name));
		for (j = 0; j < 3; j++) {
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}
		/* verts are all 8 bit, so no swapping needed */
		memcpy(poutframe->verts, pinframe->verts, pheader->num_xyz * sizeof(dtrivertx_t));
	}

	mod->type = mod_alias_md2;

	/* load the glcmds */
	pincmd = (int *) ((byte *) pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *) ((byte *) pheader + pheader->ofs_glcmds);
	for (i = 0; i < pheader->num_glcmds; i++)
		poutcmd[i] = LittleLong(pincmd[i]);

	/* copy skin names */
	memcpy((char *) pheader + pheader->ofs_skins, (char *) pinmodel + pheader->ofs_skins, pheader->num_skins * MD2_MAX_SKINNAME);

	mod->mins[0] = -UNIT_SIZE;
	mod->mins[1] = -UNIT_SIZE;
	mod->mins[2] = -UNIT_SIZE;
	mod->maxs[0] = UNIT_SIZE;
	mod->maxs[1] = UNIT_SIZE;
	mod->maxs[2] = UNIT_SIZE;

	/* load the tags */
	Q_strncpyz(mod->alias.tagname, mod->name, sizeof(mod->alias.tagname));
	/* strip model extension and set the extension to tag */
	l = strlen(mod->alias.tagname) - 4;
	strcpy(&(mod->alias.tagname[l]), ".tag");

	/* try to load the tag file */
	if (ri.FS_CheckFile(mod->alias.tagname) != -1) {
		/* load the tags */
		size = ri.FS_LoadFile(mod->alias.tagname, (void **) &tagbuf);
		Mod_LoadTags(mod, tagbuf, size);
		ri.FS_FreeFile(tagbuf);
	}

	/* load the animations */
	Q_strncpyz(mod->alias.animname, mod->name, sizeof(mod->alias.animname));
	l = strlen(mod->alias.animname) - 4;
	strcpy(&(mod->alias.animname[l]), ".anm");

	/* try to load the animation file */
	if (ri.FS_CheckFile(mod->alias.animname) != -1) {
		/* load the tags */
		ri.FS_LoadFile(mod->alias.animname, (void **) &animbuf);
		Mod_LoadAnims(&mod->alias, animbuf);
		ri.FS_FreeFile(animbuf);
	}

	/* find neighbours */
	mod->alias.neighbors = ri.TagMalloc(ri.modelPool, pheader->num_tris * sizeof(mAliasNeighbors_t), 0);
	Mod_BuildTriangleNeighbors(mod->alias.neighbors, pouttri, pheader->num_tris);
}
