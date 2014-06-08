/**
 * @file
 * @brief Pi constants and degrees/radians conversion.
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846  /* matches value in gcc v2 math.h */
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923 /* pi/2 */
#endif

const double c_pi = M_PI;
const double c_half_pi = M_PI_2;
const double c_2pi = 2 * M_PI;
const double c_inv_2pi = 1 / c_2pi;

const double c_DEG2RADMULT = M_PI / 180.0;
const double c_RAD2DEGMULT = 180.0 / M_PI;

inline double radians_to_degrees (double radians)
{
	return radians * c_RAD2DEGMULT;
}
inline double degrees_to_radians (double degrees)
{
	return degrees * c_DEG2RADMULT;
}
