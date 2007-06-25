/**
 * @file gl_model_alias.c
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

#include "gl_local.h"

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

/**
 * @brief
 */
extern void Mod_LoadTags (model_t * mod, void *buffer, int bufSize)
{
	dtag_t *pintag, *pheader;
	int version;
	int i, j, size;
	float *inmat, *outmat;
	int read;
	mdl_md2_t *md2;

	pintag = (dtag_t *) buffer;
	md2 = (mdl_md2_t *) mod->extraData;

	version = LittleLong(pintag->version);
	if (version != TAG_VERSION)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: %s has wrong version number (%i should be %i)", mod->tagname, version, TAG_VERSION);

	size = LittleLong(pintag->ofs_extractend);
	mod->tagdata = ri.TagMalloc(ri.modelPool, size, 0);
	pheader = mod->tagdata;

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dtag_t) / 4; i++)
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (pheader->num_tags <= 0)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: tag file %s has no tags", mod->tagname);

	if (pheader->num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: tag file %s has no frames", mod->tagname);

	/* load tag names */
	memcpy((char *) pheader + pheader->ofs_names, (char *) pintag + pheader->ofs_names, pheader->num_tags * MD2_MAX_SKINNAME);

	/* load tag matrices */
	inmat = (float *) ((byte *) pintag + pheader->ofs_tags);
	outmat = (float *) ((byte *) pheader + pheader->ofs_tags);

	if (bufSize != LittleLong(pheader->ofs_end))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: tagfile %s is broken - expected: %i, offsets tell us to read: %i\n",
			mod->tagname, bufSize, LittleLong(pheader->ofs_end));

	if (pheader->num_frames != md2->num_frames)
		ri.Con_Printf(PRINT_ALL, "Mod_LoadTags: found %i frames in %s but model has %i frames\n",
			pheader->num_frames, mod->tagname, md2->num_frames);

	if (pheader->ofs_names != 32)
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_name for tagfile %s\n", mod->tagname);
	if (pheader->ofs_tags != pheader->ofs_names + (pheader->num_tags * 64))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_tags for tagfile %s\n", mod->tagname);
	/* (4 * 3) * 4 bytes (int) */
	if (pheader->ofs_end != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 48))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_end for tagfile %s\n", mod->tagname);
	/* (4 * 4) * 4 bytes (int) */
	if (pheader->ofs_extractend != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 64))
		ri.Sys_Error(ERR_DROP, "Mod_LoadTags: invalid ofs_extractend for tagfile %s\n", mod->tagname);

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
 * @brief
 */
extern void Mod_LoadAnims (model_t * mod, void *buffer)
{
	char *text, *token;
	manim_t *anim;
	int n;
	mdl_md2_t *md2;

	md2 = (mdl_md2_t *) mod->extraData;

	for (n = 0, text = buffer; text; n++)
		COM_Parse(&text);
	n /= 4;
	if (n > MAX_ANIMS)
		n = MAX_ANIMS;

	mod->animdata = (manim_t*) ri.TagMalloc(ri.modelPool, n * sizeof(manim_t), 0);
	anim = mod->animdata;
	text = buffer;
	mod->numanims = 0;

	do {
		/* get the name */
		token = COM_Parse(&text);
		if (!text)
			break;
		Q_strncpyz(anim->name, token, MAX_ANIMNAME);

		/* get the start */
		token = COM_Parse(&text);
		if (!text)
			break;
		anim->from = atoi(token);
		if (anim->from < 0)
			ri.Sys_Error(ERR_DROP, "Mod_LoadAnims: negative start frame for %s", mod->animname);
		else if (anim->from > md2->num_frames)
			ri.Sys_Error(ERR_DROP, "Mod_LoadAnims: start frame is higher than models frame count (%i) (model: %s)", md2->num_frames, mod->animname);

		/* get the end */
		token = COM_Parse(&text);
		if (!text)
			break;
		anim->to = atoi(token);
		if (anim->to < 0)
			ri.Sys_Error(ERR_DROP, "Mod_LoadAnims: negative start frame for %s", mod->animname);
		else if (anim->to > md2->num_frames)
			ri.Sys_Error(ERR_DROP, "Mod_LoadAnims: end frame is higher than models frame count (%i) (model: %s)", md2->num_frames, mod->animname);

		/* get the fps */
		token = COM_Parse(&text);
		if (!text)
			break;
		anim->time = (atof(token) > 0.01) ? (1000.0 / atof(token)) : (1000.0 / 0.01);

		/* add it */
		mod->numanims++;
		anim++;
	} while (mod->numanims < MAX_ANIMS);
/*	ri.Con_Printf(PRINT_ALL, "anims: %i for model %s\n", mod->numanims, mod->name); */
}

/**
 * @brief
 */
extern int Mod_FindTriangleWithEdge (neighbors_t *neighbors, dtriangle_t *tris, int numtris, int triIndex, int edgeIndex)
{
	int			i, j, found = -1, foundj = 0;
	dtriangle_t	*current = &tris[triIndex];
	qboolean	dup = qfalse;

	for (i = 0; i < numtris; i++) {
		if (i == triIndex)
			continue;

		for (j = 0; j < 3; j++) {
			if (((current->index_xyz[edgeIndex] == tris[i].index_xyz[j]) &&
				 (current->index_xyz[(edgeIndex + 1) % 3] == tris[i].index_xyz[(j + 1) % 3])) ||
				 ((current->index_xyz[edgeIndex] == tris[i].index_xyz[(j + 1) % 3]) &&
				 (current->index_xyz[(edgeIndex + 1) % 3] == tris[i].index_xyz[j]))) {
				/* no edge for this model found yet? */
				if (found == -1) {
					found = i;
					foundj = j;
				} else
					dup = qtrue;	/* the three edges story */
			}
		}
	}

	/* normal edge, setup neighbour pointers */
	if (!dup && found != -1) {
		neighbors[found].n[foundj] = triIndex;
		return found;
	}

	/* naughty egde let no-one have the neighbour */
	return -1;
}

/**
 * @brief
 */
extern void Mod_BuildTriangleNeighbors (neighbors_t *neighbors, dtriangle_t *tris, int numtris)
{
	int		i, j;

	/* set neighbours to -1 */
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++)
			neighbors[i].n[j] = -1;
	}

	/* generate edges information (for shadow volumes) */
	/* NOTE: We do this with the original vertices not the reordered onces since reordering them
	* duplicates vertices and we only compare indices */
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
			if (neighbors[i].n[j] == -1)
				neighbors[i].n[j] = Mod_FindTriangleWithEdge(neighbors, tris, numtris, i, j);
		}
	}
}
