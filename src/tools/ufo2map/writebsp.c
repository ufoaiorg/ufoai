/**
 * @file writebsp.c
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

#include "bsp.h"

static int c_nofaces;
static int c_facenodes;

void U2M_ProgressBar (void (*func) (unsigned int cnt), unsigned int count, qboolean showProgress, const char *id)
{
	unsigned int i;
	int current, previous = 0;
	int	start = 0, end;

	if (showProgress) {
		start = time(NULL);
		fprintf(stdout, "%10s: ", id);
		fflush(stdout);
	}
	for (i = 0; i < count; i++) {
		if (showProgress) {
			current = (10 * i / count);
			if (current != previous) {
				previous = current;
				fprintf(stdout, "%i..", current * 10);
				fflush(stdout);
			}
		}

		func(i);
	}
	if (showProgress) {
		end = time(NULL);
		Com_Printf(" (time: %4is, count: %i)\n", end - start, count);
	}
}


/*
=========================================================
ONLY SAVE OUT PLANES THAT ARE ACTUALLY USED AS NODES
=========================================================
*/

static int planeused[MAX_MAP_PLANES];

/**
 * @brief There is no oportunity to discard planes, because all of the original
 * brushes will be saved in the map.
 */
void EmitPlanes (void)
{
	int i;
	dBspPlane_t *dp;
	plane_t *mp;
	int planetranslate[MAX_MAP_PLANES];

	mp = mapplanes;
	for (i = 0; i < nummapplanes; i++, mp++) {
		dp = &curTile->planes[curTile->numplanes];
		planetranslate[i] = curTile->numplanes;
		VectorCopy(mp->normal, dp->normal);
		dp->dist = mp->dist;
		dp->type = mp->type;
		curTile->numplanes++;
	}
}

static void EmitLeaf (node_t *node)
{
	dBspLeaf_t *leaf_p;
	int i, brushnum;
	bspbrush_t *b;

	/* emit a leaf */
	if (curTile->numleafs >= MAX_MAP_LEAFS)
		Sys_Error("MAX_MAP_LEAFS (%i)", curTile->numleafs);

	leaf_p = &curTile->leafs[curTile->numleafs];
	curTile->numleafs++;

	leaf_p->contentFlags = node->contentFlags;
	leaf_p->area = node->area;

	/* write bounding box info */
	VectorCopy((short)node->mins, leaf_p->mins);
	VectorCopy((short)node->maxs, leaf_p->maxs);

	/* write the leafbrushes */
	leaf_p->firstleafbrush = curTile->numleafbrushes;
	for (b = node->brushlist; b; b = b->next) {
		if (curTile->numleafbrushes >= MAX_MAP_LEAFBRUSHES)
			Sys_Error("MAX_MAP_LEAFBRUSHES (%i)", curTile->numleafbrushes);

		brushnum = b->original - mapbrushes;
		for (i = leaf_p->firstleafbrush; i < curTile->numleafbrushes; i++)
			if (curTile->leafbrushes[i] == brushnum)
				break;
		if (i == curTile->numleafbrushes) {
			curTile->leafbrushes[curTile->numleafbrushes] = brushnum;
			curTile->numleafbrushes++;
		}
	}
	leaf_p->numleafbrushes = curTile->numleafbrushes - leaf_p->firstleafbrush;
}


/**
 * @sa EmitDrawNode_r
 * @note Called for every node face
 */
static void EmitFace (face_t *f)
{
	dBspFace_t *df;
	int i, e;

	if (f->numpoints < 3) {
		return;		/* degenerated */
	}
	if (f->merged || f->split[0] || f->split[1]) {
		return;		/* not a final face */
	}

	if (curTile->numfaces >= MAX_MAP_FACES)
		Sys_Error("numfaces >= MAX_MAP_FACES (%i)", curTile->numfaces);
	df = &curTile->faces[curTile->numfaces];
	curTile->numfaces++;

	/* planenum is used by qlight, but not quake */
	df->planenum = f->planenum & (~1);
	df->side = f->planenum & 1;

	df->firstedge = curTile->numsurfedges;
	df->numedges = f->numpoints;
	df->texinfo = f->texinfo;
	for (i = 0; i < f->numpoints; i++) {
		e = GetEdge(f->vertexnums[i], f->vertexnums[(i + 1) % f->numpoints], f);
		if (curTile->numsurfedges >= MAX_MAP_SURFEDGES)
			Sys_Error("numsurfedges >= MAX_MAP_SURFEDGES (%i)", curTile->numsurfedges);
		curTile->surfedges[curTile->numsurfedges] = e;
		curTile->numsurfedges++;
	}
}

/**
 * @brief Writes the draw nodes
 * @note Called after a drawing hull is completed
 */
static int EmitDrawNode_r (node_t *node)
{
	dBspNode_t *n;
	face_t *f;
	int i;

	if (node->planenum == PLANENUM_LEAF) {
		EmitLeaf(node);
		return -curTile->numleafs;
	}

	/* emit a node	 */
	if (curTile->numnodes >= MAX_MAP_NODES)
		Sys_Error("MAX_MAP_NODES (%i)", curTile->numnodes);
	n = &curTile->nodes[curTile->numnodes];
	curTile->numnodes++;

	VectorCopy((short)node->mins, n->mins);
	VectorCopy((short)node->maxs, n->maxs);

	planeused[node->planenum]++;
	planeused[node->planenum ^ 1]++;

	if (node->planenum & 1)
		Sys_Error("EmitDrawNode_r: odd planenum");
	n->planenum = node->planenum;
	n->firstface = curTile->numfaces;

	if (!node->faces)
		c_nofaces++;
	else
		c_facenodes++;

	for (f = node->faces; f; f = f->next)
		EmitFace(f);

	n->numfaces = curTile->numfaces - n->firstface;

	/* recursively output the other nodes */
	for (i = 0; i < 2; i++) {
		if (node->children[i]->planenum == PLANENUM_LEAF) {
			n->children[i] = -(curTile->numleafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			n->children[i] = curTile->numnodes;
			EmitDrawNode_r(node->children[i]);
		}
	}

	return n - curTile->nodes;
}


/**
 * @brief copies working data for a bsp tree into the structures used to create the bsp file.
 * @param[in] headnode the top-most node in this bsp tree
 * @return the index to the head node created.
 */
int WriteBSP (node_t *headnode)
{
	int oldfaces, emittedHeadnode;

	c_nofaces = 0;
	c_facenodes = 0;

	Sys_FPrintf(SYS_VRB, "--- WriteBSP ---\n");

	oldfaces = curTile->numfaces;
	emittedHeadnode = EmitDrawNode_r(headnode);

	Sys_FPrintf(SYS_VRB, "%5i nodes with faces\n", c_facenodes);
	Sys_FPrintf(SYS_VRB, "%5i nodes without faces\n", c_nofaces);
	Sys_FPrintf(SYS_VRB, "%5i faces\n", curTile->numfaces-oldfaces);

	return emittedHeadnode;
}

/**
 * @brief Set the model numbers for SOLID_BSP entities like func_door or func_breakable
 */
void SetModelNumbers (void)
{
	int i;
	char value[10];

	/* 0 is the world - start at 1 */
	int models = 1;
	for (i = 1; i < num_entities; i++) {
		if (entities[i].numbrushes) {
			Com_sprintf(value, sizeof(value), "*%i", models);
			models++;
			SetKeyValue(&entities[i], "model", value);
		}
	}
}

/**
 * @note routing.c - commented out
 */
void EmitBrushes (void)
{
	int i, j, bnum, s, x;
	vec3_t normal;
	vec_t dist;
	int planenum;

	curTile->numbrushsides = 0;
	curTile->numbrushes = nummapbrushes;
	/* Clear out curTile->brushes */
	memset(curTile->brushes, 0, MAX_MAP_BRUSHES * sizeof(cBspBrush_t));

	for (bnum = 0; bnum < nummapbrushes; bnum++) {
		mapbrush_t *b = &mapbrushes[bnum];
		dBspBrush_t *db = &curTile->dbrushes[bnum];
		cBspBrush_t *cb = &curTile->brushes[bnum];

		db->contentFlags = b->contentFlags;
		db->firstbrushside = curTile->numbrushsides;
		db->numsides = b->numsides;
		cb->contentFlags = b->contentFlags;
		cb->firstbrushside = curTile->numbrushsides;
		cb->numsides = b->numsides;
		cb->checkcount = 0;
		for (j = 0; j < b->numsides; j++) {
			if (curTile->numbrushsides == MAX_MAP_BRUSHSIDES) {
				Sys_Error("MAX_MAP_BRUSHSIDES (%i)", curTile->numbrushsides);
			} else {
				dBspBrushSide_t *cp = &curTile->brushsides[curTile->numbrushsides];
				curTile->numbrushsides++;
				cp->planenum = b->original_sides[j].planenum;
				cp->texinfo = b->original_sides[j].texinfo;
			}
		}

		/* add any axis planes not contained in the brush to bevel off corners */
		for (x = 0; x < 3; x++)
			for (s = -1; s <= 1; s += 2) {
				/* add the plane */
				VectorCopy(vec3_origin, normal);
				normal[x] = (float)s;
				if (s == -1)
					dist = -b->mins[x];
				else
					dist = b->maxs[x];
				planenum = FindFloatPlane(normal, dist);
				for (i = 0; i < b->numsides; i++)
					if (b->original_sides[i].planenum == planenum)
						break;
				if (i == b->numsides) {
					if (curTile->numbrushsides >= MAX_MAP_BRUSHSIDES)
						Sys_Error("MAX_MAP_BRUSHSIDES (%i)", curTile->numbrushsides);

					curTile->brushsides[curTile->numbrushsides].planenum = planenum;
					curTile->brushsides[curTile->numbrushsides].texinfo = curTile->brushsides[curTile->numbrushsides - 1].texinfo;
					curTile->numbrushsides++;
					db->numsides++;
					cb->numsides++;
				}
			}
	}
}

/**
 * @sa EndBSPFile
 */
void BeginBSPFile (void)
{
	/* Create this shortcut to mapTiles[0] */
	curTile = &mapTiles[0];
	/* Set the number of tiles to 1. */
	numTiles = 1;

	/* these values may actually be initialized */
	/* if the file existed when loaded, so clear them explicitly */
	curTile->nummodels = 0;
	curTile->numfaces = 0;
	curTile->numbrushsides = 0;
	curTile->numvertexes = 0;
	curTile->numleafbrushes = 0;
	curTile->numsurfedges = 0;
	curTile->numnodes = 0;

	/* edge 0 is not used, because 0 can't be negated */
	curTile->numedges = 1;

	/* leave vertex 0 as an error */
	curTile->numvertexes = 1;

	/* leave leaf 0 as an error */
	curTile->numleafs = 1;
	curTile->leafs[0].contentFlags = CONTENTS_SOLID;
}


/**
 * @sa BeginBSPFile
 */
void EndBSPFile (const char *filename)
{
	EmitBrushes();
	EmitPlanes();
	UnparseEntities();

	/* write the map */
	Com_Printf("Writing %s\n", filename);
	WriteBSPFile(filename);
}

extern int firstmodeledge;
extern int firstmodelface;

/**
 * @sa EndModel
 */
void BeginModel (int entityNum)
{
	dBspModel_t *mod;
	int start, end;
	mapbrush_t *b;
	int j;
	entity_t *e;
	vec3_t mins, maxs;

	if (curTile->nummodels == MAX_MAP_MODELS)
		Sys_Error("MAX_MAP_MODELS (%i)", curTile->nummodels);
	mod = &curTile->models[curTile->nummodels];

	mod->firstface = curTile->numfaces;

	firstmodeledge = curTile->numedges;
	firstmodelface = curTile->numfaces;

	/* bound the brushes */
	e = &entities[entityNum];

	start = e->firstbrush;
	end = start + e->numbrushes;
	ClearBounds(mins, maxs);

	for (j = start; j < end; j++) {
		b = &mapbrushes[j];
		/* not a real brush (origin brush) - e.g. func_door */
		if (!b->numsides)
			continue;
		AddPointToBounds(b->mins, mins, maxs);
		AddPointToBounds(b->maxs, mins, maxs);
	}

	VectorCopy(mins, mod->mins);
	VectorCopy(maxs, mod->maxs);
}


/**
 * @sa BeginModel
 */
void EndModel (void)
{
	dBspModel_t *mod;

	mod = &curTile->models[curTile->nummodels];
	mod->numfaces = curTile->numfaces - mod->firstface;

	curTile->nummodels++;
}
