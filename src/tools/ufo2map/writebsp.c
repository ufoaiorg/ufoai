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
		dp = &dplanes[numplanes];
		planetranslate[i] = numplanes;
		VectorCopy(mp->normal, dp->normal);
		dp->dist = mp->dist;
		dp->type = mp->type;
		numplanes++;
	}
}

static void EmitLeaf (node_t *node)
{
	dBspLeaf_t *leaf_p;
	int i, brushnum;
	bspbrush_t *b;

	/* emit a leaf */
	if (numleafs >= MAX_MAP_LEAFS)
		Sys_Error("MAX_MAP_LEAFS (%i)", numleafs);

	leaf_p = &dleafs[numleafs];
	numleafs++;

	leaf_p->contentFlags = node->contentFlags;
	leaf_p->area = node->area;

	/* write bounding box info */
	VectorCopy((short)node->mins, leaf_p->mins);
	VectorCopy((short)node->maxs, leaf_p->maxs);

	/* write the leafbrushes */
	leaf_p->firstleafbrush = numleafbrushes;
	for (b = node->brushlist; b; b = b->next) {
		if (numleafbrushes >= MAX_MAP_LEAFBRUSHES)
			Sys_Error("MAX_MAP_LEAFBRUSHES (%i)", numleafbrushes);

		brushnum = b->original - mapbrushes;
		for (i = leaf_p->firstleafbrush; i < numleafbrushes; i++)
			if (dleafbrushes[i] == brushnum)
				break;
		if (i == numleafbrushes) {
			dleafbrushes[numleafbrushes] = brushnum;
			numleafbrushes++;
		}
	}
	leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;
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

	if (numfaces >= MAX_MAP_FACES)
		Sys_Error("numfaces >= MAX_MAP_FACES (%i)", numfaces);
	df = &dfaces[numfaces];
	numfaces++;

	/* planenum is used by qlight, but not quake */
	df->planenum = f->planenum & (~1);
	df->side = f->planenum & 1;

	df->firstedge = numsurfedges;
	df->numedges = f->numpoints;
	df->texinfo = f->texinfo;
	for (i = 0; i < f->numpoints; i++) {
		e = GetEdge(f->vertexnums[i], f->vertexnums[(i + 1) % f->numpoints], f);
		if (numsurfedges >= MAX_MAP_SURFEDGES)
			Sys_Error("numsurfedges >= MAX_MAP_SURFEDGES (%i)", numsurfedges);
		dsurfedges[numsurfedges] = e;
		numsurfedges++;
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
		return -numleafs;
	}

	/* emit a node	 */
	if (numnodes >= MAX_MAP_NODES)
		Sys_Error("MAX_MAP_NODES (%i)", numnodes);
	n = &dnodes[numnodes];
	numnodes++;

	VectorCopy((short)node->mins, n->mins);
	VectorCopy((short)node->maxs, n->maxs);

	planeused[node->planenum]++;
	planeused[node->planenum ^ 1]++;

	if (node->planenum & 1)
		Sys_Error("EmitDrawNode_r: odd planenum");
	n->planenum = node->planenum;
	n->firstface = numfaces;

	if (!node->faces)
		c_nofaces++;
	else
		c_facenodes++;

	for (f = node->faces; f; f = f->next)
		EmitFace(f);

	n->numfaces = numfaces - n->firstface;

	/* recursively output the other nodes */
	for (i = 0; i < 2; i++) {
		if (node->children[i]->planenum == PLANENUM_LEAF) {
			n->children[i] = -(numleafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			n->children[i] = numnodes;
			EmitDrawNode_r(node->children[i]);
		}
	}

	return n - dnodes;
}

void WriteBSP (node_t *headnode)
{
	int oldfaces;

	c_nofaces = 0;
	c_facenodes = 0;

	Sys_FPrintf(SYS_VRB, "--- WriteBSP ---\n");

	oldfaces = numfaces;
	dmodels[nummodels].headnode = EmitDrawNode_r(headnode);

	Sys_FPrintf(SYS_VRB, "%5i nodes with faces\n", c_facenodes);
	Sys_FPrintf(SYS_VRB, "%5i nodes without faces\n", c_nofaces);
	Sys_FPrintf(SYS_VRB, "%5i faces\n", numfaces-oldfaces);
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
static void EmitBrushes (void)
{
	int i, j, bnum, s, x;
	dBspBrush_t *db;
	mapbrush_t *b;
	dBspBrushSide_t *cp;
	vec3_t normal;
	vec_t dist;
	int planenum;

	numbrushsides = 0;
	numbrushes = nummapbrushes;

	for (bnum = 0; bnum < nummapbrushes; bnum++) {
		b = &mapbrushes[bnum];
		db = &dbrushes[bnum];

		db->contentFlags = b->contentFlags;
		db->firstside = numbrushsides;
		db->numsides = b->numsides;
		for (j = 0; j < b->numsides; j++) {
			if (numbrushsides == MAX_MAP_BRUSHSIDES)
				Sys_Error("MAX_MAP_BRUSHSIDES (%i)", numbrushsides);
			cp = &dbrushsides[numbrushsides];
			numbrushsides++;
			cp->planenum = b->original_sides[j].planenum;
			cp->texinfo = b->original_sides[j].texinfo;
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
					if (numbrushsides >= MAX_MAP_BRUSHSIDES)
						Sys_Error("MAX_MAP_BRUSHSIDES (%i)", numbrushsides);

					dbrushsides[numbrushsides].planenum = planenum;
					dbrushsides[numbrushsides].texinfo = dbrushsides[numbrushsides - 1].texinfo;
					numbrushsides++;
					db->numsides++;
				}
			}
	}
}

/**
 * @sa EndBSPFile
 */
void BeginBSPFile (void)
{
	/* these values may actually be initialized */
	/* if the file existed when loaded, so clear them explicitly */
	nummodels = 0;
	numfaces = 0;
	numbrushsides = 0;
	numvertexes = 0;
	numleafbrushes = 0;
	numsurfedges = 0;
	numnodes = 0;

	/* edge 0 is not used, because 0 can't be negated */
	numedges = 1;

	/* leave vertex 0 as an error */
	numvertexes = 1;

	/* leave leaf 0 as an error */
	numleafs = 1;
	dleafs[0].contentFlags = CONTENTS_SOLID;
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

	if (nummodels == MAX_MAP_MODELS)
		Sys_Error("MAX_MAP_MODELS (%i)", nummodels);
	mod = &dmodels[nummodels];

	mod->firstface = numfaces;

	firstmodeledge = numedges;
	firstmodelface = numfaces;

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

	mod = &dmodels[nummodels];
	mod->numfaces = numfaces - mod->firstface;

	nummodels++;
}
