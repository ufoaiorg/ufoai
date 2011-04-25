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

#ifndef INCLUDED_TEXTURELIB_H
#define INCLUDED_TEXTURELIB_H

#include "debugging/debugging.h"
#include "math/Vector3.h"
#include "math/matrix.h"
#include "math/Plane3.h"

typedef Vector3 Colour3;
typedef unsigned int GLuint;
class LoadImageCallback;
class Image;

enum ProjectionAxis
{
	eProjectionAxisX = 0, eProjectionAxisY = 1, eProjectionAxisZ = 2
};

// describes a GL texture
class GLTexture
{
	private:

		/**
		 * @brief This function does the actual processing of raw RGBA data into a GL texture.
		 * @note It will also resample to power-of-two dimensions, generate the mipmaps and adjust gamma.
		 */
		void LoadTextureRGBA (Image* image);

		const std::string name;

	public:
		GLTexture (const LoadImageCallback& load, const std::string& name);

		void realise ();
		void unrealise ();

		const std::string& getName () const;

		const LoadImageCallback& load;
		std::size_t width, height;
		GLuint texture_number; // gl bind number
		Colour3 color; // for flat shade mode
		int surfaceFlags, contentFlags, value;
		bool hasAlpha;
};

inline Matrix4 matrix4_rotation_for_vector3 (const Vector3& x, const Vector3& y, const Vector3& z)
{
	return Matrix4(x.x(), x.y(), x.z(), 0, y.x(), y.y(), y.z(), 0, z.x(), z.y(), z.z(), 0, 0, 0, 0, 1);
}

inline Matrix4 matrix4_swap_axes (const Vector3& from, const Vector3& to)
{
	if (from.x() != 0 && to.y() != 0) {
		return matrix4_rotation_for_vector3(to, from, g_vector3_axis_z);
	}

	if (from.x() != 0 && to.z() != 0) {
		return matrix4_rotation_for_vector3(to, g_vector3_axis_y, from);
	}

	if (from.y() != 0 && to.z() != 0) {
		return matrix4_rotation_for_vector3(g_vector3_axis_x, to, from);
	}

	if (from.y() != 0 && to.x() != 0) {
		return matrix4_rotation_for_vector3(from, to, g_vector3_axis_z);
	}

	if (from.z() != 0 && to.x() != 0) {
		return matrix4_rotation_for_vector3(from, g_vector3_axis_y, to);
	}

	if (from.z() != 0 && to.y() != 0) {
		return matrix4_rotation_for_vector3(g_vector3_axis_x, from, to);
	}

	ERROR_MESSAGE("unhandled axis swap case");

	return Matrix4::getIdentity();
}

inline Matrix4 matrix4_reflection_for_plane (const Plane3& plane)
{
	return Matrix4::byColumns(1 - (2 * plane.normal().x() * plane.normal().x()), -2 * plane.normal().x()
			* plane.normal().y(), -2 * plane.normal().x() * plane.normal().z(), 0, -2 * plane.normal().y()
			* plane.normal().x(), 1 - (2 * plane.normal().y() * plane.normal().y()), -2 * plane.normal().y()
			* plane.normal().z(), 0, -2 * plane.normal().z() * plane.normal().x(), -2 * plane.normal().z()
			* plane.normal().y(), 1 - (2 * plane.normal().z() * plane.normal().z()), 0, -2 * plane.dist()
			* plane.normal().x(), -2 * plane.dist() * plane.normal().y(), -2 * plane.dist() * plane.normal().z(), 1);
}

inline Matrix4 matrix4_reflection_for_plane45 (const Plane3& plane, const Vector3& from, const Vector3& to)
{
	Vector3 first = from;
	Vector3 second = to;

	if ((from.dot(plane.normal()) > 0) == (to.dot(plane.normal()) > 0)) {
		first = -first;
		second = -second;
	}

	Matrix4 swap = matrix4_swap_axes(first, second);

	//Matrix4 tmp = matrix4_reflection_for_plane(plane);

	swap.tx() = -(-2 * plane.normal().x() * plane.dist());
	swap.ty() = -(-2 * plane.normal().y() * plane.dist());
	swap.tz() = -(-2 * plane.normal().z() * plane.dist());

	return swap;
}

const float ProjectionAxisEpsilon = static_cast<float> (0.0001);

inline bool projectionaxis_better (float axis, float other)
{
	return fabs(axis) > fabs(other) + ProjectionAxisEpsilon;
}

/// \brief Texture axis precedence: Z > X > Y
inline ProjectionAxis projectionaxis_for_normal (const Vector3& normal)
{
	return (projectionaxis_better(normal[eProjectionAxisY], normal[eProjectionAxisX])) ? (projectionaxis_better(
			normal[eProjectionAxisY], normal[eProjectionAxisZ])) ? eProjectionAxisY : eProjectionAxisZ
			: (projectionaxis_better(normal[eProjectionAxisX], normal[eProjectionAxisZ])) ? eProjectionAxisX
					: eProjectionAxisZ;
}

/*!
 \brief Construct a transform from XYZ space to ST space (3d to 2d).
 This will be one of three axis-aligned spaces, depending on the surface normal.
 NOTE: could also be done by swapping values.
 */
inline void Normal_GetTransform (const Vector3& normal, Matrix4& transform)
{
	switch (projectionaxis_for_normal(normal)) {
	case eProjectionAxisZ:
		transform[0] = 1;
		transform[1] = 0;
		transform[2] = 0;

		transform[4] = 0;
		transform[5] = 1;
		transform[6] = 0;

		transform[8] = 0;
		transform[9] = 0;
		transform[10] = 1;
		break;
	case eProjectionAxisY:
		transform[0] = 1;
		transform[1] = 0;
		transform[2] = 0;

		transform[4] = 0;
		transform[5] = 0;
		transform[6] = -1;

		transform[8] = 0;
		transform[9] = 1;
		transform[10] = 0;
		break;
	case eProjectionAxisX:
		transform[0] = 0;
		transform[1] = 0;
		transform[2] = 1;

		transform[4] = 1;
		transform[5] = 0;
		transform[6] = 0;

		transform[8] = 0;
		transform[9] = 1;
		transform[10] = 0;
		break;
	}
	transform[3] = transform[7] = transform[11] = transform[12] = transform[13] = transform[14] = 0;
	transform[15] = 1;
}

/* greebo: This method calculates the normalised basis vectors of the texture plane as defined by <normal>
 *
 * If the normal vector points to the z-direction, the basis vectors are part
 * of the xy-plane: texS = <0,1,0> and texT = <1,0,0>
 *
 * If normal vector points to the negative z-direction, the above case applies, but with
 * the x-direction inversed: texS = <0,1,0> and texT = <-1,0,0> (note the minus)
 *
 * If none of the two above cases apply, the basis is calculated via cross products
 * that result in vectors perpendicular to <normal>. These lie within the plane
 * that is defined by the normal vector itself.
 *
 * Note: the vector <normal> MUST be normalised for this to function correctly.
 */
inline void ComputeAxisBase (const Vector3& normal, Vector3& texS, Vector3& texT)
{
	const Vector3 up(0, 0, 1);
	const Vector3 down(0, 0, -1);

	if (vector3_equal_epsilon(normal, up, float(1e-6))) {
		texS = Vector3(0, 1, 0);
		texT = Vector3(1, 0, 0);
	} else if (vector3_equal_epsilon(normal, down, float(1e-6))) {
		texS = Vector3(0, 1, 0);
		texT = Vector3(-1, 0, 0);
	} else {
		texS = normal.crossProduct(up).getNormalised();
		texT = normal.crossProduct(texS).getNormalised();
		texS = -texS;
	}
}

// greebo: Helper function
// handles degenerate cases, just in case library atan2 doesn't
inline double arctangent_yx (double y, double x)
{
	if (fabs(x) > 1.0E-6) {
		return atan2(y, x);
	} else if (y > 0) {
		return c_half_pi;
	} else {
		return -c_half_pi;
	}
}

#endif
