/**
 * @file
 * @brief Pseudoinstanced grass generation and rendering
 */

/*
Copyright (C) 2013 UFO: Alien Invasion.

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
#include "r_grass.h"

#define GRASS_MARK -956 /* arbitrary number from top of my head -- Sandro */

#define TRIS_PER_CLUMP 10
#define MAX_CLUMPS 2046

#define MAX_CLUMP_TRIS (TRIS_PER_CLUMP * MAX_CLUMPS)

static int clumpTriangleCount = 0;

/* gfv -- grass fragment vertex */
vec3_t gfv_pos[MAX_CLUMP_TRIS * 3];
vec2_t gfv_texcoord[MAX_CLUMP_TRIS * 3];

void R_ClearGrass ()
{
	clumpTriangleCount = 0;
}

static void R_PlantGrass (const vec3_t pos, const vec3_t normal)
{
	if (clumpTriangleCount + TRIS_PER_CLUMP >= MAX_CLUMP_TRIS) {
		return;
	}

	vec3_t rot[3]; /* rotation matrix for the plant */
#define xt (rot[0])
#define yt (rot[1])
#define zt (rot[2])

#if 0
	/* horisontal planting */
	VectorSet(xt, 1, 0, 0);
	VectorSet(zt, 0, 0, 1);
#else
	/* normal-based planting */

	/* we can calculate the downslope vector and use it instead;
	 * a bit more of math, but call allow us to create plants that interact with the slope in various ways */
	VectorCopy(normal, zt);
	VectorSet(xt, zt[2], 0.0, -zt[0]); /* short-circuit CrossProduct(yaxis, normal, xt) since it degenerates to a simple shuffle */
	VectorNormalizeFast(xt);
#endif

	/* generate geometry */
#if 0
	/* prebuilt mesh or debug grass marker */

	/* randomly rotate plant around the base */
	RotatePointAroundVector(yt, zt, xt, frand() * 360);
	CrossProduct(yt, zt, xt);

	/* randomly mirror the plant, too */
	if (rand() & 1)
		VectorInverse(yt);

	/* marker */
	vec_t *ptr = gfv_pos[clumpTriangleCount * 3];
	VectorMA(pos, 1, zt, ptr);
	VectorMA(ptr, 16, yt, ptr + 3);
	VectorMA(ptr, 16, xt, ptr + 6);

	vec_t *texc = gfv_texcoord[clumpTriangleCount * 3];
	Vector2Set(texc, 0, 0);
	Vector2Set(texc + 2, 1, 0);
	Vector2Set(texc + 4, 0, 1);

	clumpTriangleCount++;
#else
	/* programmatically generated clump */
	CrossProduct(zt, xt, yt);
	for (int i = 0; i <TRIS_PER_CLUMP; i += 2) {
		vec3_t sdir, tdir;
		vec2_t sprrot;
		vec3_t tmp;

		sprrot[0] = frand() * 360;
		sprrot[1] = frand() * 60 + 15;

		PolarToVec(sprrot, tmp);
		VectorRotate(rot, tmp, tdir);

		sprrot[0] += 90;
		sprrot[1] = 0;

		PolarToVec(sprrot, tmp);
		VectorRotate(rot, tmp, sdir);
#if 0
		/* debug marker */
		vec_t *ptr = gfv_pos[clumpTriangleCount * 3];
		VectorCopy(pos, ptr);
		VectorMA(ptr, 16, sdir, ptr + 3);
		VectorMA(ptr, 16, tdir, ptr + 6);

		vec_t *texc = gfv_texcoord[clumpTriangleCount * 3];
		Vector2Set(texc, 0, 0);
		Vector2Set(texc + 2, 1, 0);
		Vector2Set(texc + 4, 0, 1);

		clumpTriangleCount++;
#else
		/* billboard sprite */
		vec_t *ptr = gfv_pos[clumpTriangleCount * 3];
		VectorCopy(pos, ptr);
		VectorMA(pos, -24, sdir, ptr); /* quad vertex 0 */
		VectorMA(ptr, 32, tdir, ptr + 3); /* quad vertex 1 */
		VectorMA(ptr + 3, 48, sdir, ptr + 6); /* quad vertex 2 */

		VectorCopy(ptr, ptr + 9); /* quad vertex 0 */
		VectorCopy(ptr + 6, ptr + 12); /* quad vertex 2 */
		VectorMA(ptr + 6, -32, tdir, ptr + 15); /* quad vertex 3 */

		vec_t *texc = gfv_texcoord[clumpTriangleCount * 3];
		Vector2Set(texc, 0, 1);
		Vector2Set(texc + 2, 0, 0);
		Vector2Set(texc + 4, 1, 0);

		Vector2Set(texc + 6, 0, 1);
		Vector2Set(texc + 8, 1, 0);
		Vector2Set(texc + 12, 1, 1);

		clumpTriangleCount += 2;
#endif
	}
#endif

#undef xt
#undef yt
#undef zt
}

void R_GenerateGrass ()
{
	float density = 1.0f; /* count of clumps per 32x32 unit tile */
	double area = 0.0; /* float does not provide enough mantissa precision for this; also note that area is doubled */

	Com_Printf("Planting grass ...\n");

	/* 1st pass: walk through all brushes and determine which surfaces to use and their total area */
	for (int tile = 0; tile < r_numMapTiles; tile++) {
		/* ignore weaponclip, actorclip and stepon */
		for (int i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			const mBspModel_t *const bspModel = &r_mapTiles[tile]->bsp;
			const mBspHeader_t *const header = &bspModel->submodels[i];

			if (!header->numfaces)
				continue;

			for (int j = 0; j < header->numfaces; j++) {
				mBspSurface_t *const surf = &bspModel->surfaces[header->firstface + j];
				const cBspPlane_t *const plane = surf->plane;

				if (surf->frame == GRASS_MARK)
					continue; /* already processed it */

				if (!(surf->texinfo->flags & SURF_FOLIAGE))
					continue; /* not tagged as overgrown */

				if (surf->firstTriangle < 0 || surf->numTriangles == 0)
					continue; /* no geometry for this surface, skip it */

				/* reject way too inclined or downward facing planes */
				if (plane->normal[2] < 0.5f) /* cutoff angle is 60 degrees */
					continue;

				/* walk triangle list and sum areas */
				double surfArea = 0.0;

				for (int k = 0; k < surf->numTriangles; k++) {
					int vofs = (k + surf->firstTriangle) * 3;
					int indo = bspModel->indexes[vofs]  & 0xffff;
					int inda = bspModel->indexes[vofs + 1]  & 0xffff;
					int indb = bspModel->indexes[vofs + 2] & 0xffff;
					vec3_t vo, va, vb;
					vec3_t cross;

					/* calculate barycentric origin and coordinate axes */
					VectorCopy(&bspModel->verts[indo * 3], vo);
					VectorSubtract(&bspModel->verts[inda * 3], vo, va);
					VectorSubtract(&bspModel->verts[indb * 3], vo, vb);

					/* calculate area */
					CrossProduct(va, vb, cross);
					surfArea += VectorLength(cross);
				}

				if (surfArea < 80.0)
					continue; /* skip tiny surfaces */

				area += surfArea;
				surf->frame = GRASS_MARK;
			}
		}
	}
	Com_Printf("Total grassy area is %7.0f units (%i cells)\n", area / 2, (int)(area / 2048));

	double areaPerClump = area / MAX_CLUMPS;
	if (areaPerClump < 2048 / (density * density))
		areaPerClump = 2048 / (density * density);

	/* 2nd pass: actually plant the grass */
	/** @todo sometimes rounding error makes it to plant an extra clump which may overrun the vertex array, thus causing problems with OpenGL driver, should be fixed */
	double clumpsToPlant = 0.0;
	int planted =  0;

	for (int tile = 0; tile < r_numMapTiles; tile++) {
		/* ignore weaponclip, actorclip and stepon */
		for (int i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			const mBspModel_t *const bspModel = &r_mapTiles[tile]->bsp;
			const mBspHeader_t *const header = &bspModel->submodels[i];

			if (!header->numfaces)
				continue;

			for (int j = 0; j < header->numfaces; j++) {
				mBspSurface_t *const surf = &bspModel->surfaces[header->firstface + j];
				const cBspPlane_t *const plane = surf->plane;

				if (surf->frame != GRASS_MARK)
					continue; /* not suitable for grass or got grass already generated */

				surf->frame--; /* screen from any future stumblings upon it */

				/* walk triangle list and plant grass */
				for (int k = 0; k < surf->numTriangles; k++) {
					int vofs = (k + surf->firstTriangle) * 3;
					int indo = bspModel->indexes[vofs]  & 0xffff;
					int inda = bspModel->indexes[vofs + 1]  & 0xffff;
					int indb = bspModel->indexes[vofs + 2] & 0xffff;
					vec3_t vo, va, vb;
					vec3_t cross;

					/* calculate barycentric origin and coordinate axes */
					VectorCopy(&bspModel->verts[indo * 3], vo);
					VectorSubtract(&bspModel->verts[inda * 3], vo, va);
					VectorSubtract(&bspModel->verts[indb * 3], vo, vb);

					/* calculate area and convert it to clump count */
					CrossProduct(va, vb, cross);
					clumpsToPlant += VectorLength(cross) / areaPerClump;

					while (clumpsToPlant >= 1.0) {
						/* generate random point within a triangle using barycentic coordinates */
						 float u = frand();
						 float v = frand();

						 /* if barycentric coordinates are outside of triangle, rotate them 180 deg by flipping both coords
						   * explanation: they just got on other half of parallelogram defined by va and vb, which half complements this triangle to said parallelogram
						   */
						 if (u + v > 1.0f) {
							u = 1.0f - u;
							v = 1.0f - v;
						 }

						 vec3_t pos;

						 VectorMA(vo, u, va, pos);
						 VectorMA(pos, v, vb, pos);

						 R_PlantGrass(pos, plane->normal);

						 clumpsToPlant -= 1.0;
						 planted++;
					}
				}
			}
		}
	}

	Com_Printf("Planted %i clumps of grass\n", planted);
}

void R_DrawGrass ()
{
	if (clumpTriangleCount <= 0)
		return;

	R_BindTexture(R_FindImage("models/objects/vegi/plants2/plant_skin3", it_pic)->texnum);

	R_EnableAlphaTest(true);

	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, gfv_texcoord);
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, gfv_pos);
	glDrawArrays(GL_TRIANGLES, 0, clumpTriangleCount * 3);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	R_EnableAlphaTest(false);

	refdef.batchCount++;
}
