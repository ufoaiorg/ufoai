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

#include <cmath>

extern const vec2_t vec2_origin;
extern const vec3_t vec3_origin;
extern const vec4_t vec4_origin;
extern const vec4_t color_white;
extern const pos3_t pos3_origin;

/* Used to compare floats when rounding errors could occur  */
#ifndef EQUAL
#define EQUAL(a,b) (fabsf((a)-(b))<0.0000000001f)
#endif

#define Vector2FromInt(x, y) { static_cast<float>(x), static_cast<float>(y) }
#define Vector3FromInt(x, y, z) { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) }

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
#define VectorCompare(a,b)      ((a)[0]==(b)[0]?(a)[1]==(b)[1]?(a)[2]==(b)[2]?true:false:false:false)
#define VectorEqualEpsilon(a,b,epsilon)      (EQUAL2((a)[0],(b)[0],epsilon)?EQUAL2((a)[1],(b)[1],epsilon)?EQUAL2((a)[2],(b)[2],epsilon)?true:false:false:false)
#define VectorEqual(a,b)      (EQUAL((a)[0],(b)[0])?EQUAL((a)[1],(b)[1])?EQUAL((a)[2],(b)[2])?true:false:false:false)
#define Vector2Compare(a,b)     ((a)[0]==(b)[0]?(a)[1]==(b)[1]?true:false:false)
#define Vector2Equal(a,b)      (EQUAL((a)[0],(b)[0])?EQUAL((a)[1],(b)[1])?true:false:false)
#define VectorDistSqr(a,b)      (((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])+((b)[2]-(a)[2])*((b)[2]-(a)[2]))
#define VectorDist(a,b)         (sqrtf(((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])+((b)[2]-(a)[2])*((b)[2]-(a)[2])))
#define Vector2Dist(a,b)         (sqrtf(((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])))
#define VectorLengthSqr(a)      ((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2])
#define VectorNotEmpty(a)           (!VectorEmpty((a)))
#define VectorEmpty(a)           (VectorEqual((a), vec3_origin))
#define Vector2Empty(a)			 (Vector2Equal((a), vec2_origin))
#define Vector2NotEmpty(a)		    (!Vector2Empty((a)))
#define Vector4NotEmpty(a)          (VectorNotEmpty(a) || !EQUAL((a)[3],0.0f))
#define VectorIntZero(a)          ((a)[0] == 0 && (a)[1] == 0 && (a)[2] == 0)
#define LinearInterpolation(a, b, x, y)   ((y)=(a)[1] + ((((x) - (a)[0]) * ((b)[1] - (a)[1])) / ((b)[0] - (a)[0])))
#define VectorScale(in,scale,out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorInterpolation(p1,p2,frac,mid)	((mid)[0]=(p1)[0]+(frac)*((p2)[0]-(p1)[0]),(mid)[1]=(p1)[1]+(frac)*((p2)[1]-(p1)[1]),(mid)[2]=(p1)[2]+(frac)*((p2)[2]-(p1)[2]))
#define VectorAbs(a)  (a[0] = fabsf(a[0]), a[1] = fabsf(a[1]), a[2] = fabsf(a[2]))
