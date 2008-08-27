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

/* mathlib.c -- math primitives */
#include "mathlib.h"
/* we use memcpy and memset */
#include <memory.h>

const vec3_t vec3_origin = {0.0f,0.0f,0.0f};

const vec3_t g_vec3_axis_x = { 1, 0, 0, };
const vec3_t g_vec3_axis_y = { 0, 1, 0, };
const vec3_t g_vec3_axis_z = { 0, 0, 1, };

vec_t VectorLength(const vec3_t v) {
	int		i;
	float	length;

	length = 0.0f;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = (float)sqrt (length);

	return length;
}

void VectorMA(const vec3_t va, vec_t scale, const vec3_t vb, vec3_t vc) {
	vc[0] = va[0] + scale*vb[0];
	vc[1] = va[1] + scale*vb[1];
	vc[2] = va[2] + scale*vb[2];
}
