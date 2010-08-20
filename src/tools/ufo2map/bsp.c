/**
 * @file bsp.c
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

#include "map.h"
#include "bsp.h"

int entity_num;

/**
 * @note used as a shortcut so the tile being processed does not need to be repeatedly passed between functions.
 */
dMapTile_t *curTile;

/**
 * @sa ProcessModels
 */
static void ProcessWorldModel (void)
{
	entity_t *e = &entities[0];

	brush_start = e->firstbrush;
	brush_end = brush_start + e->numbrushes;

	ClearBounds(worldMins, worldMaxs);

	/* This is set so the Emit* functions in writebsp.c work correctly. */
	curTile->nummodels = NUM_REGULAR_MODELS;

	/* process levels */
	RunSingleThreadOn(ProcessLevel, NUM_REGULAR_MODELS, config.verbosity >= VERB_NORMAL, "LEVEL");

	/* calculate routing */
	DoRouting();
}


/**
 * @sa ProcessModels
 */
static void ProcessSubModel (int entityNum)
{
	const entity_t *e;
	int start, end;
	tree_t *tree;
	bspbrush_t *list;
	vec3_t mins, maxs;

	BeginModel(entityNum);

	e = &entities[entityNum];
#if 0
	Com_Printf("Processing entity: %i into model %i (%s:%s)\n", entityNum, curTile->nummodels, e->epairs->key, e->epairs->value);
#endif

	start = e->firstbrush;
	end = start + e->numbrushes;

	mins[0] = mins[1] = mins[2] = -MAX_WORLD_WIDTH;
	maxs[0] = maxs[1] = maxs[2] = MAX_WORLD_WIDTH;

	/* every level (-1) */
	list = MakeBspBrushList(start, end, -1, mins, maxs);
	if (!config.nocsg)
		list = ChopBrushes(list);
	tree = BrushBSP(list, mins, maxs);
	assert(tree);
	assert(tree->headnode);
	if (tree->headnode->planenum == PLANENUM_LEAF)
		Sys_Error("No head node bmodel of %s (%i)\n", ValueForKey(e, "classname"), entityNum);
	MakeTreePortals(tree);
	MarkVisibleSides(tree, start, end);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);
	curTile->models[curTile->nummodels].headnode = WriteBSP(tree->headnode);
	FreeTree(tree);

	EndModel();
}


/**
 * @sa ProcessWorldModel
 * @sa ProcessSubModel
 */
void ProcessModels (const char *filename)
{
	BeginBSPFile();

	for (entity_num = 0; entity_num < num_entities; entity_num++) {
		if (!entities[entity_num].numbrushes)
			continue;

		Verb_Printf(VERB_EXTRA, "############### model %i ###############\n", curTile->nummodels);

		if (entity_num == 0)
			ProcessWorldModel();
		else
			ProcessSubModel(entity_num);

		if (!config.verboseentities)
			config.verbose = qfalse;	/* don't bother printing submodels */
	}

	EndBSPFile(filename);
}
