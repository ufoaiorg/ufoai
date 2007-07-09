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
void Mod_LoadAnims (mAliasModel_t * mod, void *buffer)
{
	char *text, *token;
	mAliasAnim_t *anim;
	int n;
	mdl_md2_t *md2;

	md2 = (mdl_md2_t *) mod->extraData;

	for (n = 0, text = buffer; text; n++)
		COM_Parse(&text);
	n /= 4;
	if (n > MAX_ANIMS)
		n = MAX_ANIMS;

	mod->animdata = (mAliasAnim_t *) ri.TagMalloc(ri.modelPool, n * sizeof(mAliasAnim_t), 0);
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
int Mod_FindTriangleWithEdge (mAliasNeighbors_t *neighbors, dtriangle_t *tris, int numtris, int triIndex, int edgeIndex)
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
void Mod_BuildTriangleNeighbors (mAliasNeighbors_t *neighbors, dtriangle_t *tris, int numtris)
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
