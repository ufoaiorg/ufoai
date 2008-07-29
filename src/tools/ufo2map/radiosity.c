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
entity_t *face_entity[MAX_MAP_FACES];
patch_t patches[MAX_PATCHES];
unsigned num_patches;

static vec3_t radiosity[MAX_PATCHES];		/**< light leaving a patch */
static vec3_t illumination[MAX_PATCHES];	/**< light arriving at a patch */

vec3_t face_offset[MAX_MAP_FACES];		/**< for rotating bmodels */
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
		backplanes[i].dist = -curTile->planes[i].dist;
		VectorSubtract(vec3_origin, curTile->planes[i].normal, backplanes[i].normal);
	}
}

/*
===================================================================
TRANSFER SCALES
===================================================================
*/

static inline int PointInLeafnum (const vec3_t point)
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
	const int num = PointInLeafnum(point);
	assert(num >= 0);
	assert(num < MAX_MAP_LEAFS);
	return &curTile->leafs[num];
}


static int total_transfer;
static void MakeTransfers (unsigned int i)
{
	unsigned int			j;
	vec3_t		delta;
	vec_t		dist, scale;
	float		trans;
	int			itrans;
	patch_t		*patch, *patch2;
	float		total;
	dBspPlane_t	plane;
	vec3_t		origin;
	float		transfers[MAX_PATCHES], *all_transfers;
	int			s;
	int			itotal;

	patch = patches + i;
	total = 0;

	VectorCopy(patch->origin, origin);
	plane = *patch->plane;

	/* find out which patch2s will collect light */
	/* from patch */

	all_transfers = transfers;
	patch->numtransfers = 0;
	for (j = 0, patch2 = patches; j < num_patches; j++, patch2++) {
		transfers[j] = 0;

		if (j == i)
			continue;

		/* calculate vector */
		VectorSubtract(patch2->origin, origin, delta);
		dist = VectorNormalize(delta);
		if (!dist)
			continue;	/* should never happen */

		/* reletive angles */
		scale = DotProduct(delta, plane.normal);
		scale *= -DotProduct(delta, patch2->plane->normal);
		if (scale <= 0)
			continue;

		trans = scale * patch2->area / (dist * dist);

		if (trans < 0.1)
			continue;

		/* check exact transfer */
		if (TR_TestLine(patch->origin, patch2->origin, TL_FLAG_NONE))
			continue;

#if 0
		if (trans < 0)
			trans = 0;		/* rounding errors... */
#endif

		transfers[j] = trans;
		if (trans > 0) {
			total += trans;
			patch->numtransfers++;
		}
	}

	/* copy the transfers out and normalize */
	/* total should be somewhere near PI if everything went right */
	/* because partial occlusion isn't accounted for, and nearby */
	/* patches have underestimated form factors, it will usually */
	/* be higher than PI */
	if (patch->numtransfers) {
		transfer_t *t;

		if (patch->numtransfers < 0 || patch->numtransfers > MAX_PATCHES)
			Sys_Error("Weird numtransfers");
		s = patch->numtransfers * sizeof(transfer_t);
		patch->transfers = malloc(s);
		if (!patch->transfers)
			Sys_Error("Memory allocation failure");

		/* normalize all transfers so all of the light */
		/* is transfered to the surroundings */
		t = patch->transfers;
		itotal = 0;
		for (j = 0; j < num_patches; j++) {
			if (transfers[j] <= 0)
				continue;
			itrans = transfers[j] * 0x10000 / total;
			itotal += itrans;
			t->transfer = itrans;
			t->patch = j;
			t++;
		}
	}

	/* don't bother locking around this.  not that important. */
	total_transfer += patch->numtransfers;
}


static void FreeTransfers (void)
{
	unsigned int i;

	for (i = 0; i < num_patches; i++) {
		if (patches[i].transfers)
			free(patches[i].transfers);
		patches[i].transfers = NULL;
	}
}

static float CollectLight (void)
{
	unsigned int i, j;
	patch_t	*patch;
	vec_t	total;

	total = 0;

	for (i = 0, patch = patches; i < num_patches; i++, patch++) {
		for (j = 0; j < 3; j++) {
			patch->totallight[j] += illumination[i][j] / patch->area;
			radiosity[i][j] = illumination[i][j] * patch->reflectivity[j];
		}

		total += radiosity[i][0] + radiosity[i][1] + radiosity[i][2];
		VectorClear(illumination[i]);
	}

	return total;
}


/**
 * @brief Send light out to other patches
 */
static void ShootLight (unsigned int patchnum)
{
	int			k, l;
	transfer_t	*trans;
	int			num;
	patch_t		*patch;
	vec3_t		send;

	/* this is the amount of light we are distributing */
	/* prescale it so that multiplying by the 16 bit */
	/* transfer values gives a proper output value */
	for (k = 0; k < 3; k++)
		send[k] = radiosity[patchnum][k] / 0x10000;
	patch = &patches[patchnum];

	trans = patch->transfers;
	num = patch->numtransfers;

	for (k = 0; k < num; k++, trans++) {
		for (l = 0; l < 3; l++)
			illumination[trans->patch][l] += send[l] * trans->transfer;
	}
}

static void BounceLight (void)
{
	unsigned int i, j;

	for (i = 0; i < num_patches; i++) {
		const patch_t *p = &patches[i];
		for (j = 0; j < 3; j++) {
			radiosity[i][j] = p->samplelight[j] * p->reflectivity[j] * p->area;
		}
	}

	for (i = 0; i < config.numbounce; i++) {
		float added;
		char buf[12];

		snprintf(buf, sizeof(buf), " %i LGHTBNCE", i);
		RunThreadsOn(ShootLight, num_patches, qtrue, buf);
		added = CollectLight();

		Sys_FPrintf(SYS_VRB, "bounce:%i added:%f\n", i, added);
	}
}



/*============================================================== */

static void CheckPatches (void)
{
	unsigned int i;
	patch_t *patch;

	for (i = 0; i < num_patches; i++) {
		patch = &patches[i];
		if (patch->totallight[0] < 0 || patch->totallight[1] < 0 || patch->totallight[2] < 0)
			Sys_Error("negative patch totallight\n");
	}
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
	RunThreadsOn(BuildFacelights, curTile->numfaces, qtrue, "FACELIGHTS");

	if (config.numbounce > 0) {
		/* build transfer lists */
		RunThreadsOn(MakeTransfers, num_patches, qtrue, "TRANSFERS");
		Sys_FPrintf(SYS_VRB, "transfer lists: %5.1f megs\n",
			(float)total_transfer * sizeof(transfer_t) / (1024 * 1024));

		/* spread light around */
		BounceLight();

		FreeTransfers();

		CheckPatches();
	}

	/* blend bounced light into direct light and save */
	LinkPlaneFaces();

	RunThreadsOn(FinalLightFace, curTile->numfaces, qtrue, "FINALLIGHT");
	CloseTracingNodes();
}
