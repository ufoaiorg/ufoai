/**
 * @file
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

#pragma once

#include "ufotypes.h"
#include "defines.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846  /* matches value in gcc v2 math.h */
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923 /* pi/2 */
#endif

#define	EQUAL_EPSILON	0.001

bool Q_IsPowerOfTwo(int i);

/* Compare floats with custom epsilon error */
#define EQUAL2(a,b,epsilon) (fabs((a)-(b))<epsilon)

/* microsoft's fabs seems to be ungodly slow... */
#define Q_ftol(f) (long) (f)

#define torad (M_PI/180.0f)
#define todeg (180.0f/M_PI)

/* angle indexes */
#define PITCH  0   /* rotation around y axis - up / down (-90 up to 90 degree) */
#define YAW    1   /* rotation around z axis - left / right (0 up to 360 degree) */
#define ROLL   2   /* rotation around x axis - fall over */

/* axis */
#define	AXIS_FORWARD	0
#define	AXIS_RIGHT		1
#define	AXIS_UP			2

/* earth map data */
/* values of sinus and cosinus of earth inclination (23,5 degrees) for faster day and night calculations */
#define SIN_ALPHA   0.39875
#define COS_ALPHA   0.91706
#define HIGH_LAT    +1.0
#define LOW_LAT     -1.0
#define CENTER_LAT  0.0
#define SIZE_LAT    2.0

/**
 * @brief Number of angles from a position (2-dimensional)
 * @sa dvecs (in q_shared.c) for a description of its use.
 * @sa AngleToDV.
 * @sa BASE_DIRECTIONS
 */
#define DIRECTIONS 8

/**
 * @brief Number of direct connected fields for a position
 * @sa DIRECTIONS.
 */
#define BASE_DIRECTIONS			4			/* Only the standard N,S,E,W directions */

/* game/g_ai.c, game/g_spawn.c, common/routing.c, ufo2map/routing.c, client/cl_actor.c, common/cmodel.c, shared/typedefs.h */
#define PATHFINDING_DIRECTIONS	40			/* total number of directions */
#define CORE_DIRECTIONS			8			/* The standard N,S,E,W directions plus diagonals. */
#define FLYING_DIRECTIONS		16			/* starting number of directions available only to fliers */

extern const vec4_t dvecs[PATHFINDING_DIRECTIONS];
extern const float dvecsn[CORE_DIRECTIONS][2];
extern const float directionAngles[CORE_DIRECTIONS];

extern const byte dvright[CORE_DIRECTIONS];
extern const byte dvleft[CORE_DIRECTIONS];

/** @brief Map boundary is +/- MAX_WORLD_WIDTH - to get into the positive area we add the
 * possible max negative value and divide by the size of a grid unit field */
#define VecToPos(v, p) (                  \
	(p)[0] = ((int)(v)[0] + MAX_WORLD_WIDTH) / UNIT_SIZE,  \
	(p)[1] = ((int)(v)[1] + MAX_WORLD_WIDTH) / UNIT_SIZE,  \
	(p)[2] = std::min((PATHFINDING_HEIGHT - 1), ((int)(v)[2] / UNIT_HEIGHT)) \
)
/** @brief Pos boundary size is +/- 128 - to get into the positive area we add
 * the possible max negative value and multiply with the grid unit size to get
 * back the vector coordinates - now go into the middle of the grid field
 * by adding the half of the grid unit size to this value
 * @sa PATHFINDING_WIDTH */
#define PosToVec(p, v) ( \
	(v)[0] = ((int)(p)[0] - GRID_WIDTH) * UNIT_SIZE   + UNIT_SIZE   / 2, \
	(v)[1] = ((int)(p)[1] - GRID_WIDTH) * UNIT_SIZE   + UNIT_SIZE   / 2, \
	(v)[2] =  (int)(p)[2]               * UNIT_HEIGHT + UNIT_HEIGHT / 2  \
)

#include "vector.h"
#include "line.h"
#include "aabb.h"

class GridBox {
public:
	static const GridBox EMPTY;

	/*==================
	 *		ctors
	 *==================*/
	GridBox() {
		VectorCopy(vec3_origin, mins);
		VectorCopy(vec3_origin, maxs);
	}
	GridBox(const ipos3_t mini, const ipos3_t maxi) {
		VectorCopy(mini, mins);
		VectorCopy(maxi, maxs);
	}
	GridBox(const pos3_t mini, const pos3_t maxi) {
		VectorCopy(mini, mins);
		VectorCopy(maxi, maxs);
	}
#if 1
	GridBox(const AABB& aabb) {
		VecToPos(aabb.getMins(), mins);
		VecToPos(aabb.getMaxs(), maxs);
	}
#endif
	/*==================
	 *		setters
	 *==================*/
	inline void set(const pos3_t mini, const pos3_t maxi) {
		VectorCopy(mini, mins);
		VectorCopy(maxi, maxs);
	}
	inline void set(const AABB& aabb) {
		VecToPos(aabb.getMins(), mins);
		VecToPos(aabb.getMaxs(), maxs);
	}

	/**
	 * @brief Set the box correctly if the maxs value is the upper corner of a cell.
	 * VecToPos considers the upper bounds of a cell as belonging to the next cell.
	 * If this is not compensated, boxes derived from a map bounding box have one
	 * surplus cell in each direction.
	 */
	inline void setFromMapBounds(const vec3_t mini, const vec3_t maxi) {
		VecToPos(mini, mins);
		VecToPos(maxi, maxs);
		maxs[0]--;
		maxs[1]--;
		maxs[2]--;
	}

	/*==================
	 *		getters
	 *==================*/
	inline pos_t getMinX () const {
		return mins[0];
	}
	inline pos_t getMinY () const {
		return mins[1];
	}
	inline pos_t getMinZ () const {
		return mins[2];
	}
	inline pos_t getMaxX () const {
		return maxs[0];
	}
	inline pos_t getMaxY () const {
		return maxs[1];
	}
	inline pos_t getMaxZ () const {
		return maxs[2];
	}

	/*==================
	 *		checkers
	 *==================*/
	inline bool isZero() const {
		return VectorIntZero(mins) && VectorIntZero(maxs);
	}

	/*==================
	 *	manipulators
	 *==================*/
	/** @brief expand the box in four directions, but clip them to the maximum boundaries
	 * @note this is pretty much nonsense with the current setting of PATHFINDING_WIDTH
	 * and the data type of pos3_t, but who knows the future... */
	inline void expandXY(const int byVal) {
		mins[0] = std::max(mins[0] - byVal, 0);
		mins[1] = std::max(mins[1] - byVal, 0);
		maxs[0] = std::min(maxs[0] + byVal, PATHFINDING_WIDTH - 1);
		maxs[1] = std::min(maxs[1] + byVal, PATHFINDING_WIDTH - 1);
	}
	inline void addOneZ () {
		maxs[2] = std::min(maxs[2] + 1, PATHFINDING_HEIGHT - 1);
	}
	inline void clipToMaxBoundaries() {
		return;	/* do nothing, see above */
	}

	/*==================
	 *		data
	 *==================*/
	pos3_t mins;
	pos3_t maxs;
};

/** @brief  The direction vector tells us where the actor came from (in his previous step).
 * The pathing table holds about a million of these dvecs, so we save quite some memory by squeezing
 * three pieces of information into a short value:
 * - the direction he took to get here
 * - the z-level he came from
 * - *how* he moved. There are three special crouching conditions
 * -- autocrouch: standing upright in the previous cell, now crouch and stay crouched
 * -- autocrouched: being already autocrouched in the previous cell, stand up if you get the chance
 * -- autodive: we can stand in both cells, but there is just a small hole in the wall between them
 */
typedef short dvec_t;
#define DV_FLAGS_BIT_SHIFT	4	/**< This is the bit shift needed to store the 'how' component of a DV value */
#define DV_DIR_BIT_SHIFT	8	/**< This is the bit shift needed to store the dir component of a DV value */
#define DV_Z_BIT_MASK		0x0007	/**< The mask to retrieve the z component of a  DV value */
#define DV_FLAGS_BIT_MASK	0x00F0	/**< The mask to retrieve the 'how' component of a  DV value */
#define DV_DIR_BIT_MASK		0xFF00	/**< The mask to retrieve the dir component of a  DV value */

#define DV_FLAG_AUTOCROUCH		0x01
#define DV_FLAG_AUTOCROUCHED	0x02
#define DV_FLAG_AUTODIVE		0x04

#define makeDV(dir, z)				(((dir) << DV_DIR_BIT_SHIFT) | ((z) & DV_Z_BIT_MASK))
#define setDVz(dv, z)				(((dv) & (~DV_Z_BIT_MASK)) | ((z) & DV_Z_BIT_MASK))
#define getDVdir(dv)				((dv) >> DV_DIR_BIT_SHIFT)
#define getDVflags(dv)				(((dv) & DV_FLAGS_BIT_MASK) >> DV_FLAGS_BIT_SHIFT)
#define getDVz(dv)					((dv) & DV_Z_BIT_MASK)

#define PosAddDV(p, crouch, dv)     ((p)[0]+=dvecs[getDVdir(dv)][0], (p)[1]+=dvecs[getDVdir(dv)][1], (p)[2]=getDVz(dv), (crouch)+=dvecs[getDVdir(dv)][3])
#define PosSubDV(p, crouch, dv)     ((p)[0]-=dvecs[getDVdir(dv)][0], (p)[1]-=dvecs[getDVdir(dv)][1], (p)[2]=getDVz(dv), (crouch)-=dvecs[getDVdir(dv)][3])

int AngleToDir(int angle);
#define AngleToDV(x)	(AngleToDir(x) << DV_DIR_BIT_SHIFT)

void VectorMA(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t outVector);
void VectorClampMA(vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc);
void VectorMix(const vec3_t v1, const vec3_t v2, const float mix, vec3_t out);

void MatrixMultiply(const vec3_t a[3], const vec3_t b[3], vec3_t c[3]);
void GLMatrixAssemble(const vec3_t origin, const vec3_t angles, float* matrix);
void GLMatrixMultiply(const float a[16], const float b[16], float c[16]);
void GLVectorTransform(const float m[16], const vec4_t in, vec4_t out);
void GLPositionTransform(const float m[16], const vec3_t in, vec3_t out);
void VectorRotate(vec3_t m[3], const vec3_t va, vec3_t vb);

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);
int VectorCompareEps(const vec3_t v1, const vec3_t v2, float epsilon);
bool VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp);
vec_t VectorLength(const vec3_t v);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t VectorNormalize(vec3_t v);    /* returns vector length */
void VectorNormalizeFast(vec3_t v);
vec_t VectorNormalize2(const vec3_t v, vec3_t out);
void VectorInverse(vec3_t v);
void VectorMidpoint(const vec3_t point1, const vec3_t point2, vec3_t midpoint);
int Q_log2(int val);

double GetDistanceOnGlobe(const vec2_t pos1, const vec2_t pos2);

void VectorCalcMinsMaxs(const vec3_t center, const vec3_t size, vec3_t mins, vec3_t maxs);
float VectorAngleBetween(const vec3_t vec1, const vec3_t vec2);

void VecToAngles(const vec3_t vec, vec3_t angles);

void VecToPolar(const vec3_t v, vec2_t a);
void PolarToVec(const vec2_t a, vec3_t v);

void CalculateMinsMaxs(const vec3_t angles, const AABB& relBox, const vec3_t origin, AABB& absBox);

void VectorCreateRotationMatrix(const vec3_t angles, vec3_t matrix[3]);
void VectorRotatePoint(vec3_t point, vec3_t matrix[3]);

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);

float LerpAngle(float a1, float a2, float frac);

bool FrustumVis(const vec3_t origin, int dir, const vec3_t point);

void PerpendicularVector(vec3_t dst, const vec3_t src);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

float frand(void);              /* 0 to 1 */
float crand(void);              /* -1 to 1 */
void gaussrand(float* gauss1, float* gauss2);   /* -inf to +inf, median 0, stdev 1 */

vec_t Q_rint(const vec_t in);
vec_t ColorNormalize(const vec3_t in, vec3_t out);

void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent, vec3_t binormal);

void Orthogonalize(vec3_t v1, const vec3_t v2);
void MatrixTranspose(const vec3_t m[3], vec3_t t[3]);

bool RayIntersectAABB(const vec3_t start, const vec3_t end, const AABB& aabb);
