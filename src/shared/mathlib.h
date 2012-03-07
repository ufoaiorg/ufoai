/**
 * @file mathlib.h
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

#ifndef __MATHLIB__
#define __MATHLIB__

#include "ufotypes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846  /* matches value in gcc v2 math.h */
#endif

#define	EQUAL_EPSILON	0.001

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef byte pos_t;
typedef pos_t pos3_t[3];
typedef pos_t pos4_t[4];

extern const vec2_t vec2_origin;
extern const vec3_t vec3_origin;
extern const vec4_t vec4_origin;
extern const vec4_t color_white;

qboolean Q_IsPowerOfTwo(int i);

/* Used to compare floats when rounding errors could occur  */
#ifndef equal
#define equal(a,b) (fabs((a)-(b))<0.0000000001)
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/* microsoft's fabs seems to be ungodly slow... */
#define Q_ftol(f) (long) (f)

#define torad (M_PI/180.0f)
#define todeg (180.0f/M_PI)

/* angle indexes */
#define PITCH  0   /* rotation around y axis - up / down (-90 up to 90 degree) */
#define YAW    1   /* rotation around z axis - left / right (0 up to 360 degree) */
#define ROLL   2   /* rotation around x axis - fall over */

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
	(p)[2] =  min((PATHFINDING_HEIGHT - 1), ((int)(v)[2] / UNIT_HEIGHT)) \
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

/** @brief Returns the distance between two 3-dimensional vectors */
#define DotProduct(x,y)         ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,dest)   ((dest)[0]=(a)[0]-(b)[0],(dest)[1]=(a)[1]-(b)[1],(dest)[2]=(a)[2]-(b)[2])
#define Vector2Subtract(a,b,dest)   ((dest)[0]=(a)[0]-(b)[0],(dest)[1]=(a)[1]-(b)[1])
#define VectorAdd(a,b,dest)        ((dest)[0]=(a)[0]+(b)[0],(dest)[1]=(a)[1]+(b)[1],(dest)[2]=(a)[2]+(b)[2])
#define VectorMul(scalar,b,dest)       ((dest)[0]=(scalar)*(b)[0],(dest)[1]=(scalar)*(b)[1],(dest)[2]=(scalar)*(b)[2])
#define Vector2Mul(scalar,b,dest)      ((c)[0]=(scalar)*(b)[0],(dest)[1]=(scalar)*(b)[1])
#define VectorDiv(in,scalar,out)    VectorScale((in),(1.0f/(scalar)),(out))
#define VectorCopy(src,dest)        ((dest)[0]=(src)[0],(dest)[1]=(src)[1],(dest)[2]=(src)[2])
#define Vector2Copy(src,dest)       ((dest)[0]=(src)[0],(dest)[1]=(src)[1])
#define Vector4Copy(src,dest)       ((dest)[0]=(src)[0],(dest)[1]=(src)[1],(dest)[2]=(src)[2],(dest)[3]=(src)[3])
#define Vector2Clear(a)            ((a)[0]=(a)[1]=0)
#define VectorClear(a)          ((a)[0]=(a)[1]=(a)[2]=0)
#define VectorInside(vec,mins,maxs) (vec[0] >= mins[0] && vec[0] <= maxs[0] && vec[1] >= mins[1] && vec[1] <= maxs[1] && vec[2] >= mins[2] && vec[2] <= maxs[2])
#define Vector4Clear(a)          ((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#define VectorNegate(src,dest)       ((dest)[0]=-(src)[0],(dest)[1]=-(src)[1],(dest)[2]=-(src)[2])
#define VectorSet(v, x, y, z)   ((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define VectorSum(a)            ((a)[0]+(a)[1]+(a)[2])
#define Vector2Set(v, x, y)     ((v)[0]=(x), (v)[1]=(y))
#define Vector4Set(v, r, g, b, a)   ((v)[0]=(r), (v)[1]=(g), (v)[2]=(b), (v)[3]=(a))
#define VectorCompare(a,b)      ((a)[0]==(b)[0]?(a)[1]==(b)[1]?(a)[2]==(b)[2]?1:0:0:0)
#define VectorEqual(a,b)      (equal((a)[0],(b)[0])?equal((a)[1],(b)[1])?equal((a)[2],(b)[2])?1:0:0:0)
#define Vector2Compare(a,b)     ((a)[0]==(b)[0]?(a)[1]==(b)[1]?1:0:0)
#define Vector2Equal(a,b)      (equal((a)[0],(b)[0])?equal((a)[1],(b)[1])?1:0:0)
#define VectorDistSqr(a,b)      (((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])+((b)[2]-(a)[2])*((b)[2]-(a)[2]))
#define VectorDist(a,b)         (sqrt(((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])+((b)[2]-(a)[2])*((b)[2]-(a)[2])))
#define Vector2Dist(a,b)         (sqrt(((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])))
#define VectorLengthSqr(a)      ((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2])
#define VectorNotEmpty(a)           (!VectorEmpty((a)))
#define VectorEmpty(a)           (VectorEqual((a), vec3_origin))
#define Vector2Empty(a)			 (Vector2Equal((a), vec2_origin))
#define Vector2NotEmpty(a)		    (!Vector2Empty((a)))
#define Vector4NotEmpty(a)          (VectorNotEmpty(a) || !equal((a)[3],0.0))
#define LinearInterpolation(a, b, x, y)   ((y)=(a)[1] + ((((x) - (a)[0]) * ((b)[1] - (a)[1])) / ((b)[0] - (a)[0])))
#define VectorScale(in,scale,out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorInterpolation(p1,p2,frac,mid)	((mid)[0]=(p1)[0]+(frac)*((p2)[0]-(p1)[0]),(mid)[1]=(p1)[1]+(frac)*((p2)[1]-(p1)[1]),(mid)[2]=(p1)[2]+(frac)*((p2)[2]-(p1)[2]))

/** @brief  The direction vector tells us where the actor came from (in his previous step).
 * The pathing table holds about a million of these dvecs, so we save quite some memory by squeezing
 * three informations into a short value:
 * - the direction where he came from
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
qboolean VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp);
vec_t VectorLength(const vec3_t v);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t VectorNormalize(vec3_t v);    /* returns vector length */
void VectorNormalizeFast(vec3_t v);
vec_t VectorNormalize2(const vec3_t v, vec3_t out);
void VectorInverse(vec3_t v);
void VectorMidpoint(const vec3_t point1, const vec3_t point2, vec3_t midpoint);
int Q_log2(int val);

double GetDistanceOnGlobe(const vec2_t pos1, const vec2_t pos2);

void VectorCenterFromMinsMaxs(const vec3_t mins, const vec3_t maxs, vec3_t center);
void VectorCalcMinsMaxs(const vec3_t center, const vec3_t size, vec3_t mins, vec3_t maxs);
float VectorAngleBetween(const vec3_t vec1, const vec3_t vec2);

void VecToAngles(const vec3_t vec, vec3_t angles);

void Print2Vector(const vec2_t v, const char *text);
void Print3Vector(const vec3_t v, const char *text);

void VecToPolar(const vec3_t v, vec2_t a);
void PolarToVec(const vec2_t a, vec3_t v);

void CalculateMinsMaxs(const vec3_t angles, const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec3_t absmin, vec3_t absmax);

void VectorCreateRotationMatrix(const vec3_t angles, vec3_t matrix[3]);
void VectorRotatePoint(vec3_t point, vec3_t matrix[3]);

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);

float LerpAngle(float a1, float a2, float frac);

qboolean FrustumVis(const vec3_t origin, int dir, const vec3_t point);

void PerpendicularVector(vec3_t dst, const vec3_t src);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

float frand(void);              /* 0 to 1 */
float crand(void);              /* -1 to 1 */
void gaussrand(float *gauss1, float *gauss2);   /* -inf to +inf, median 0, stdev 1 */

vec_t Q_rint(const vec_t in);
vec_t ColorNormalize(const vec3_t in, vec3_t out);

void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent, vec3_t binormal);

void Orthogonalize(vec3_t v1, const vec3_t v2);
void MatrixTranspose(const vec3_t m[3], vec3_t t[3]);

qboolean RayIntersectAABB(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs);

#endif
