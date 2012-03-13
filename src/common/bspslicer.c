/*
 * @file bspslicer.c
 * @brief Based on the BSP_tool from botman's Half-Life BSP utilities
 */

#include "bspslicer.h"
#include "../ports/system.h"
#include "../common/common.h"
#include "../shared/images.h"

#define DEPTH 3

static qboolean SL_CreatePNGFile (const char *filename, unsigned char *buffer, int width, int height)
{
	qFILE f;

	/* create the .bmp file... */
	if (FS_OpenFile(filename, &f, FILE_WRITE) == -1)
		return qfalse;

	R_WritePNG(&f, buffer, width, height);

	FS_CloseFile(&f);

	return qtrue;
}

#define IMAGE_BUFFER_SET_BLACK(buffer, offset) do {int i; for (i = 0; i < DEPTH; i++) {buffer[offset + i] = 0xFF; } } while(0);

static void SL_Bresenham (int x0, int y0, int x1, int y1, int width, int height, qboolean rotated, unsigned char *outputBuffer)
{
	int dy = y1 - y0;
	int dx = x1 - x0;
	int stepx, stepy;
	int offset;
	const int maxOffset = width * height * DEPTH;

	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}
	dy *= 2;
	dx *= 2;

	if (rotated)
		offset = x0 * DEPTH * height + y0;
	else
		offset = y0 * width * DEPTH + x0 * DEPTH;

	if (offset >= 0 && offset < maxOffset) {
		IMAGE_BUFFER_SET_BLACK(outputBuffer, offset);
	} else
		return;

	if (dx > dy) {
		int fraction = 2 * dy - (dx >> 1); /* same as 2*dy - dx */
		while (x0 != x1) {
			if (fraction >= 0) {
				y0 += stepy;
				fraction -= dx; /* same as fraction -= 2*dx */
			}
			x0 += stepx;
			fraction += dy; /* same as fraction -= 2*dy */

			if (rotated)
				offset = x0 * DEPTH * height + y0;
			else
				offset = y0 * width * DEPTH + x0 * DEPTH;

			if (offset >= 0 && offset < maxOffset) {
				IMAGE_BUFFER_SET_BLACK(outputBuffer, offset);
			} else
				return;
		}
	} else {
		int fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;

			if (rotated)
				offset = x0 * DEPTH * height + y0;
			else
				offset = y0 * width * DEPTH + x0 * DEPTH;

			if (offset >= 0 && offset < maxOffset) {
				IMAGE_BUFFER_SET_BLACK(outputBuffer, offset);
			} else
				return;
		}
	}
}

/**
 * @info the "vector" parameter should be NORMALIZED!!!
 */
static float SL_DistanceToIntersection (const vec3_t origin, const vec3_t vector, const vec3_t planeOrigin, const vec3_t planeNormal)
{
	const float d = -(DotProduct(planeNormal, planeOrigin));
	const float numerator = DotProduct(planeNormal, origin) + d;
	const float denominator = DotProduct(planeNormal, vector);

	if (fabs(denominator) < 0.00001)
		/* normal is orthogonal to vector, no intersection */
		return -1.0f;

	return -(numerator / denominator);
}

/**
 * @return @c true or false if vector intersects a plane...
 */
static qboolean SL_VectorIntersectPlane (const vec3_t origin, const vec3_t vector, const vec3_t planeOrigin, const vec3_t planeNormal, vec3_t intersectPoint)
{
	vec3_t v_temp;
	const float dist = SL_DistanceToIntersection(origin, vector, planeOrigin, planeNormal);
	if (dist < 0)
		return qfalse;

	VectorScale(vector, dist, v_temp);
	VectorAdd(origin, v_temp, intersectPoint);

	return qtrue;
}

/**
 * @brief slice the world in Z planes going from min_z to max_z by stepsize...
 */
static void SL_SliceTheWorld (const TR_TILE_TYPE *tile, const vec3_t mins, const vec3_t maxs, float stepsize, int scale, qboolean singleContour, qboolean multipleContour)
{
	int sliceIndex;
	vec3_t zDistance = { 0.0f, 0.0f, 0.0f };
	const vec3_t zNormal = { 0.0f, 0.0f, 1.0f };
	vec3_t v[2];
	vec3_t vTemp, intersectPoint;
	float lineX1, lineY1, lineX2, lineY2;
	qboolean first, both;
	char filename[MAX_QPATH];
	const float minX = mins[0];
	const float maxX = maxs[0];
	const float minY = mins[1];
	const float maxY = maxs[1];
	const float minZ = mins[2];
	const float maxZ = maxs[2];

	const int numberOfSlices = (int) ((maxZ - minZ) / stepsize);
	int width;
	int height;
	qboolean rotated = qfalse;
	unsigned char *pictureBuffer;

	lineX1 = lineY1 = lineX2 = lineY2 = 0.0f;

	width = (int) (maxX - minX);
	height = (int) (maxY - minY);

	/* is the map higher than it is wide? */
	if (height > width) {
		rotated = qtrue; /* rotate the map 90 degrees */
		width = (int) (maxY - minY); /* swap the width and height */
		height = (int) (maxX - minX);
	}

	height /= scale;
	width /= scale;

	pictureBuffer = (unsigned char *) Mem_Alloc(width * height * DEPTH);

	for (sliceIndex = 0; sliceIndex < numberOfSlices; sliceIndex++) {
		int j;
		const float zHeight = minZ + (sliceIndex * stepsize);
		zDistance[2] = zHeight;

		/* if not doing a contour map then clear the buffer each pass... */
		if (!singleContour && !multipleContour)
			memset(pictureBuffer, 0, width * height * DEPTH);

		/* loop through all the faces of the BSP file... */
		for (j = 0; j < tile->nummodels; j++) {
			const dBspModel_t *model = &tile->models[j];
			int faceIndex;

			for (faceIndex = 0; faceIndex < model->numfaces; faceIndex++) {
				const dBspSurface_t *face = &tile->faces[model->firstface + faceIndex];
				int edgeIndex;

				first = qtrue;
				both = qfalse;

				/* for each face, loop though all of the edges, getting the vertexes... */
				for (edgeIndex = 0; edgeIndex < face->numedges;
						edgeIndex++) {
					/* get the coordinates of the vertex of this edge... */
					int edge = tile->surfedges[face->firstedge + edgeIndex];

					if (edge < 0) {
						edge = -edge;
						const dBspEdge_t* e = &tile->edges[edge];
						v[0][0] = tile->vertexes[e->v[1]].point[0];
						v[0][1] = tile->vertexes[e->v[1]].point[1];
						v[0][2] = tile->vertexes[e->v[1]].point[2];
					} else {
						const dBspEdge_t* e = &tile->edges[edge];
						v[0][0] = tile->vertexes[e->v[0]].point[0];
						v[0][1] = tile->vertexes[e->v[0]].point[1];
						v[0][2] = tile->vertexes[e->v[0]].point[2];
					}

					/* get the coordinates of the vertex of the next edge... */
					edge = tile->surfedges[face->firstedge
							+ ((edgeIndex + 1) % face->numedges)];

					if (edge < 0) {
						edge = -edge;
						const dBspEdge_t* e = &tile->edges[edge];
						v[1][0] = tile->vertexes[e->v[1]].point[0];
						v[1][1] = tile->vertexes[e->v[1]].point[1];
						v[1][2] = tile->vertexes[e->v[1]].point[2];
					} else {
						const dBspEdge_t* e = &tile->edges[edge];
						v[1][0] = tile->vertexes[e->v[0]].point[0];
						v[1][1] = tile->vertexes[e->v[0]].point[1];
						v[1][2] = tile->vertexes[e->v[0]].point[2];
					}

					/* vector from v[0] to v[1] intersect the Z plane? */
					if (((v[0][2] < zHeight) && (v[1][2] > zHeight))
							|| ((v[1][2] < zHeight) && (v[0][2] > zHeight))) {
						/* create a normalized vector between the 2 vertexes... */
						VectorSubtract(v[1], v[0], vTemp);
						VectorNormalize(vTemp);

						/* find the point where the vector intersects the Z plane... */
						SL_VectorIntersectPlane(v[0], vTemp, zDistance,
								zNormal, intersectPoint);

						/* is this the first or second Z plane intersection point? */
						if (first) {
							first = qfalse;
							lineX1 = intersectPoint[0];
							lineY1 = intersectPoint[1];
						} else {
							both = qtrue;
							lineX2 = intersectPoint[0];
							lineY2 = intersectPoint[1];
						}
					}

					/* move v[1] to v[0] */
					VectorCopy(v[1], v[0]);
				}

				/* did we find 2 points that intersected the Z plane? */
				if (both) {
					/* offset by min_x to start at 0 */
					lineX1 -= minX;
					lineX2 -= minX;
					/* offset by min_y to start at 0 */
					lineY1 -= minY;
					lineY2 -= minY;

					lineX1 = lineX1 / scale;
					lineY1 = lineY1 / scale;
					lineX2 = lineX2 / scale;
					lineY2 = lineY2 / scale;

					/* are these points within the bounding box of the world? */
					if (lineX1 >= 0 && lineX1 < width && lineY1 >= 0 && lineY1 < height
							&& lineX2 >= 0 && lineX2 < width && lineY2 >= 0 && lineY2 < height) {
						const int x1 = (int) lineX1;
						const int y1 = (int) lineY1;
						const int x2 = (int) lineX2;
						const int y2 = (int) lineY2;

						if (rotated)
							SL_Bresenham(x1, height - y1, x2, height - y2, width, height, rotated, pictureBuffer);
						else
							SL_Bresenham(x1, y1, x2, y2, width, height, rotated, pictureBuffer);
					}
				}
			}
		}

		if (!singleContour) {
			Com_sprintf(filename, sizeof(filename), "out%d.png", sliceIndex + 1);
			Com_Printf("creating %s...\n", filename);
			SL_CreatePNGFile(filename, pictureBuffer, width, height);
		}
	}

	if (singleContour) {
		Q_strncpyz(filename, "out.png", sizeof(filename));
		Com_Printf("creating %s...\n", filename);
		SL_CreatePNGFile(filename, pictureBuffer, width, height);
	}

	Mem_Free(pictureBuffer);
}

/**
 * @param[in] thickness the thickness of the brushes to render to the 2d map
 */
void SL_BSPSlice (const TR_TILE_TYPE *model, float thickness, int scale, qboolean singleContour, qboolean multipleContour)
{
	int i;
	/** @todo remove these values once the mins/maxs calculation works */
	vec3_t mins = {-MAX_WORLD_WIDTH, -MAX_WORLD_WIDTH, 0};
	vec3_t maxs = {MAX_WORLD_WIDTH, MAX_WORLD_WIDTH, (PATHFINDING_HEIGHT - 1) * UNIT_HEIGHT};

	if (model == NULL)
		return;

	for (i = 0; i < model->nummodels; i++) {
		const dBspModel_t *d = &model->models[i];
		AddPointToBounds(d->mins, mins, maxs);
		AddPointToBounds(d->maxs, mins, maxs);
	}

	/* don't start or end on exact multiples of the Z slice thickness
	 * (if you do, it causes "weirdness" in the plane intersect function) */

	/* offset a tiny bit */
	mins[2] = (float) floor(mins[2]) + 0.01f;
	maxs[2] = (float) floor(maxs[2]) + 0.01f;

	SL_SliceTheWorld(model, mins, maxs, thickness, scale, singleContour, multipleContour);
}
