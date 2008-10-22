/**
 * @file radiosity.c
 * @note every surface must be divided into at least two patches each axis
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

#include "radiosity.h"
#include "bsp.h"
#include "../../common/tracing.h"

patch_t *face_patches[MAX_MAP_FACES];
patch_t patches[MAX_PATCHES];
unsigned num_patches;

dBspPlane_t backplanes[MAX_MAP_PLANES];

/*
===================================================================
MISC
===================================================================
*/

/**
 * @sa MakePatchForFace
 */
static void MakeBackplanes (void)
{
	int i;

	for (i = 0; i < curTile->numplanes; i++) {
		backplanes[i].dist = PLANE_X;
		backplanes[i].dist = -curTile->planes[i].dist;
		VectorSubtract(vec3_origin, curTile->planes[i].normal, backplanes[i].normal);
	}
}

/*
===================================================================
TRANSFER SCALES
===================================================================
*/

static inline int Rad_PointInLeafnum (const vec3_t point)
{
	int nodenum;

	nodenum = 0;
	while (nodenum >= 0) {
		const dBspNode_t *node = &curTile->nodes[nodenum];
		const dBspPlane_t *plane = &curTile->planes[node->planenum];
		const vec_t dist = DotProduct(point, plane->normal) - plane->dist;
		if (dist > 0)
			nodenum = node->children[0];
		else
			nodenum = node->children[1];
	}

	return -nodenum - 1;
}


dBspLeaf_t *Rad_PointInLeaf (const vec3_t point)
{
	const int num = Rad_PointInLeafnum(point);
	assert(num >= 0);
	assert(num < MAX_MAP_LEAFS);
	return &curTile->leafs[num];
}

void RadWorld (void)
{
	if (curTile->numnodes == 0 || curTile->numfaces == 0)
		Sys_Error("Empty map");

	/* initialize light data */
	curTile->lightdata[config.compile_for_day][0] = config.lightquant;
	curTile->lightdatasize[config.compile_for_day] = 1;

	MakeBackplanes();

	MakeTracingNodes(LEVEL_LASTVISIBLE + 1);

	/* turn each face into a single patch */
	MakePatches();

	/* subdivide patches to a maximum dimension */
	SubdividePatches(num_patches);

	/* create directlights out of patches and lights */
	CreateDirectLights();

	/* build per-vertex normals for phong shading */
	BuildVertexNormals();

	/* build initial facelights */
	RunThreadsOn(BuildFacelights, curTile->numfaces, config.verbosity >= VERB_NORMAL, "FACELIGHTS");

	/* finalize it and write it out */
	RunThreadsOn(FinalLightFace, curTile->numfaces, config.verbosity >= VERB_NORMAL, "FINALLIGHT");
	CloseTracingNodes();
}
