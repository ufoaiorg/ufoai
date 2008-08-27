/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __MATHLIB__
#define __MATHLIB__

/* mathlib.h */
#include <math.h>

#ifdef __cplusplus

/* start declarations of functions defined in C library. */
extern "C" {

#endif

	typedef float vec_t;
	typedef vec_t vec3_t[3];
	typedef vec_t vec5_t[5];
	typedef vec_t vec4_t[4];

	extern const vec3_t vec3_origin;

	extern const vec3_t g_vec3_axis_x;
	extern const vec3_t g_vec3_axis_y;
	extern const vec3_t g_vec3_axis_z;

#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorIncrement(a,b) ((b)[0]+=(a)[0],(b)[1]+=(a)[1],(b)[2]+=(a)[2])
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorSet(v, a, b, c) ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c))
#define VectorScale(a,b,c) ((c)[0]=(b)*(a)[0],(c)[1]=(b)*(a)[1],(c)[2]=(b)*(a)[2])
#define VectorMid(a,b,c) ((c)[0]=((a)[0]+(b)[0])*0.5f,(c)[1]=((a)[1]+(b)[1])*0.5f,(c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorNegate(a,b) ((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define CrossProduct(a,b,c) ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],(c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],(c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0])
#define VectorClear(x) ((x)[0]=(x)[1]=(x)[2]=0)

	vec_t VectorLength(const vec3_t v);

	void VectorMA(const vec3_t va, vec_t scale, const vec3_t vb, vec3_t vc);

	/*!
	aabb_t -  "axis-aligned" bounding box...
	  origin: centre of bounding box...
	  extents: +/- extents of box from origin...
	*/
	typedef struct aabb_s {
		vec3_t origin;
		vec3_t extents;
	} aabb_t;

	/*! Extend AABB to include point. */
	void aabb_extend_by_point(aabb_t* aabb, const vec3_t point);
	/*! Extend AABB to include aabb_src. */
	void aabb_extend_by_aabb(aabb_t* aabb, const aabb_t* aabb_src);

	/*! Calculate the corners of the aabb. */
	void aabb_corners(const aabb_t* aabb, vec3_t corners[8]);

#ifdef __cplusplus
}
#endif

#endif /* __MATHLIB__ */
