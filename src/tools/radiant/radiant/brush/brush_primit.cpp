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

#include "brush_primit.h"

#include "debugging/debugging.h"

#include "TexDef.h"
#include "itextures.h"

#include <algorithm>

#include "stringio.h"
#include "texturelib.h"
#include "math/matrix.h"
#include "math/Plane3.h"
#include "math/aabb.h"

#include "winding.h"
#include "../settings/preferences.h"

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
void ComputeAxisBase (const Vector3& normal, Vector3& texS, Vector3& texT)
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

/*!
 \brief Construct a transform from XYZ space to ST space (3d to 2d).
 This will be one of three axis-aligned spaces, depending on the surface normal.
 NOTE: could also be done by swapping values.
 */
void Normal_GetTransform (const Vector3& normal, Matrix4& transform)
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

inline void Texdef_fromTransform (TexDef& texdef, float width, float height, const Matrix4& transform)
{
	texdef.scale[0] = static_cast<float> ((1.0 / Vector2(transform[0], transform[4]).getLength()) / width);
	texdef.scale[1] = static_cast<float> ((1.0 / Vector2(transform[1], transform[5]).getLength()) / height);

	texdef.rotate = static_cast<float> (-radians_to_degrees(arctangent_yx(-transform[4], transform[0])));

	if (texdef.rotate == -180.0f) {
		texdef.rotate = 180.0f;
	}

	texdef.shift[0] = transform[12] * width;
	texdef.shift[1] = transform[13] * height;

	// If the 2d cross-product of the x and y axes is positive, one of the axes has a negative scale.
	if (Vector2(transform[0], transform[4]).crossProduct(Vector2(transform[1], transform[5])) > 0) {
		if (texdef.rotate >= 180.0f) {
			texdef.rotate -= 180.0f;
			texdef.scale[0] = -texdef.scale[0];
		} else {
			texdef.scale[1] = -texdef.scale[1];
		}
	}
}

inline void Texdef_fromTransform (TextureProjection& projection, float width, float height, const Matrix4& transform)
{
	ASSERT_MESSAGE((transform[0] != 0 || transform[4] != 0)
			&& (transform[1] != 0 || transform[5] != 0), "invalid texture matrix");
	Texdef_fromTransform(projection.m_texdef, width, height, transform);
}

inline void Texdef_normalise (TexDef& texdef, float width, float height)
{
	// it may be useful to also normalise the rotation here, if this function is used elsewhere.
	texdef.shift[0] = float_mod(texdef.shift[0], width);
	texdef.shift[1] = float_mod(texdef.shift[1], height);
}

/// \brief Normalise \p projection for a given texture \p width and \p height.
///
/// All texture-projection translation (shift) values are congruent modulo the dimensions of the texture.
/// This function normalises shift values to the smallest positive congruent values.
void Texdef_normalise (TextureProjection& projection, float width, float height)
{
	Texdef_normalise(projection.m_texdef, width, height);
}

void Texdef_basisForNormal (const TextureProjection& projection, const Vector3& normal, Matrix4& basis)
{
	Normal_GetTransform(normal, basis);
}

void Texdef_EmitTextureCoordinates (const TextureProjection& projection, std::size_t width, std::size_t height,
		Winding& w, const Vector3& normal, const Matrix4& localToWorld)
{
	if (w.numpoints < 3) {
		return;
	}

	Matrix4 local2tex = projection.m_texdef.getTransform((float) width, (float) height);

	{
		Matrix4 xyz2st;
		// we don't care if it's not normalised...
		Texdef_basisForNormal(projection, matrix4_transformed_direction(localToWorld, normal), xyz2st);
		matrix4_multiply_by_matrix4(local2tex, xyz2st);
	}

	Vector3 tangent(local2tex.getTransposed().x().getVector3().getNormalised());
	Vector3 bitangent(local2tex.getTransposed().y().getVector3().getNormalised());

	matrix4_multiply_by_matrix4(local2tex, localToWorld);

	for (Winding::iterator i = w.begin(); i != w.end(); ++i) {
		Vector3 texcoord = matrix4_transformed_point(local2tex, (*i).vertex);
		(*i).texcoord[0] = texcoord[0];
		(*i).texcoord[1] = texcoord[1];

		(*i).tangent = tangent;
		(*i).bitangent = bitangent;
	}
}

void Texdef_Assign (TexDef& td, const TexDef& other)
{
	td = other;
}

void Texdef_Shift (TexDef& td, float s, float t)
{
	td.shift[0] += s;
	td.shift[1] += t;
}

void Texdef_Scale (TexDef& td, float s, float t)
{
	td.scale[0] += s;
	td.scale[1] += t;
}

void Texdef_Rotate (TexDef& td, float angle)
{
	td.rotate += angle;
	td.rotate = static_cast<float> (float_to_integer(td.rotate) % 360);
}

template<typename Element>
inline BasicVector3<Element> vector3_inverse (const BasicVector3<Element>& self)
{
	return BasicVector3<Element> (Element(1.0 / self.x()), Element(1.0 / self.y()), Element(1.0 / self.z()));
}

float g_texdef_default_scale;

void Texdef_Assign (TextureProjection& projection, const TextureProjection& other)
{
	Texdef_Assign(projection.m_texdef, other.m_texdef);
}

void Texdef_Shift (TextureProjection& projection, float s, float t)
{
	Texdef_Shift(projection.m_texdef, s, t);
}

void Texdef_Scale (TextureProjection& projection, float s, float t)
{
	Texdef_Scale(projection.m_texdef, s, t);
}

void Texdef_Rotate (TextureProjection& projection, float angle)
{
	Texdef_Rotate(projection.m_texdef, angle);
}

void Texdef_FitTexture (TextureProjection& projection, std::size_t width, std::size_t height, const Vector3& normal,
		const Winding& w, float s_repeat, float t_repeat)
{
	if (w.numpoints < 3) {
		return;
	}

	Matrix4 st2tex = projection.m_texdef.getTransform((float) width, (float) height);

	// the current texture transform
	Matrix4 local2tex = st2tex;
	{
		Matrix4 xyz2st;
		Texdef_basisForNormal(projection, normal, xyz2st);
		matrix4_multiply_by_matrix4(local2tex, xyz2st);
	}

	// the bounds of the current texture transform
	AABB bounds;
	for (Winding::const_iterator i = w.begin(); i != w.end(); ++i) {
		Vector3 texcoord = matrix4_transformed_point(local2tex, (*i).vertex);
		aabb_extend_by_point_safe(bounds, texcoord);
	}
	bounds.origin.z() = 0;
	bounds.extents.z() = 1;

	// the bounds of a perfectly fitted texture transform
	AABB perfect(Vector3(s_repeat * 0.5, t_repeat * 0.5, 0), Vector3(s_repeat * 0.5, t_repeat * 0.5, 1));

	// the difference between the current texture transform and the perfectly fitted transform
	Matrix4 matrix(Matrix4::getTranslation(bounds.origin - perfect.origin));
	matrix4_pivoted_scale_by_vec3(matrix, bounds.extents / perfect.extents, perfect.origin);
	matrix4_affine_invert(matrix);

	// apply the difference to the current texture transform
	matrix4_premultiply_by_matrix4(st2tex, matrix);

	Texdef_fromTransform(projection, (float) width, (float) height, st2tex);
	Texdef_normalise(projection, (float) width, (float) height);
}

float Texdef_getDefaultTextureScale ()
{
	return g_texdef_default_scale;
}

void TexDef_Construct_Default (TextureProjection& projection)
{
	projection.m_texdef.scale[0] = Texdef_getDefaultTextureScale();
	projection.m_texdef.scale[1] = Texdef_getDefaultTextureScale();
	projection.m_texdef.shift[0] = 0;
	projection.m_texdef.shift[1] = 0;
	projection.m_texdef.rotate = 0;
}

void ShiftScaleRotate_fromFace (TexDef& shiftScaleRotate, const TextureProjection& projection)
{
	shiftScaleRotate = projection.m_texdef;
}

void ShiftScaleRotate_toFace (const TexDef& shiftScaleRotate, TextureProjection& projection)
{
	projection.m_texdef = shiftScaleRotate;
}

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
	return Matrix4::byColumns(1 - (2 * plane.normal().x() * plane.normal().x()), -2
			* plane.normal().x() * plane.normal().y(),
			-2 * plane.normal().x() * plane.normal().z(), 0, -2 * plane.normal().y()
					* plane.normal().x(), 1 - (2 * plane.normal().y() * plane.normal().y()), -2
					* plane.normal().y() * plane.normal().z(), 0, -2 * plane.normal().z()
					* plane.normal().x(), -2 * plane.normal().z() * plane.normal().y(), 1 - (2
					* plane.normal().z() * plane.normal().z()), 0, -2 * plane.dist()
					* plane.normal().x(), -2 * plane.dist() * plane.normal().y(), -2 * plane.dist()
					* plane.normal().z(), 1);
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

	Matrix4 tmp = matrix4_reflection_for_plane(plane);

	swap.tx() = -(-2 * plane.normal().x() * plane.dist());
	swap.ty() = -(-2 * plane.normal().y() * plane.dist());
	swap.tz() = -(-2 * plane.normal().z() * plane.dist());

	return swap;
}

void Texdef_transformLocked (TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane,
		const Matrix4& identity2transformed)
{

	Vector3 normalTransformed(matrix4_transformed_direction(identity2transformed, plane.normal()));

	// identity: identity space
	// transformed: transformation
	// stIdentity: base st projection space before transformation
	// stTransformed: base st projection space after transformation
	// stOriginal: original texdef space

	// stTransformed2stOriginal = stTransformed -> transformed -> identity -> stIdentity -> stOriginal

	Matrix4 identity2stIdentity;
	Texdef_basisForNormal(projection, plane.normal(), identity2stIdentity);

	Matrix4 transformed2stTransformed;
	Texdef_basisForNormal(projection, normalTransformed, transformed2stTransformed);

	Matrix4 stTransformed2identity(matrix4_affine_inverse(matrix4_multiplied_by_matrix4(transformed2stTransformed,
			identity2transformed)));

	Vector3 originalProjectionAxis(matrix4_affine_inverse(identity2stIdentity).z().getVector3());

	Vector3 transformedProjectionAxis(stTransformed2identity.z().getVector3());

	Matrix4 stIdentity2stOriginal = projection.m_texdef.getTransform((float) width, (float) height);
	Matrix4 identity2stOriginal(matrix4_multiplied_by_matrix4(stIdentity2stOriginal, identity2stIdentity));

	double dot = originalProjectionAxis.dot(transformedProjectionAxis);
	if (dot == 0) {
		// The projection axis chosen for the transformed normal is at 90 degrees
		// to the transformed projection axis chosen for the original normal.
		// This happens when the projection axis is ambiguous - e.g. for the plane
		// 'X == Y' the projection axis could be either X or Y.

		Matrix4 identityCorrected = matrix4_reflection_for_plane45(plane, originalProjectionAxis,
				transformedProjectionAxis);

		identity2stOriginal = matrix4_multiplied_by_matrix4(identity2stOriginal, identityCorrected);
	}

	Matrix4 stTransformed2stOriginal = matrix4_multiplied_by_matrix4(identity2stOriginal, stTransformed2identity);

	Texdef_fromTransform(projection, (float) width, (float) height, stTransformed2stOriginal);
	Texdef_normalise(projection, (float) width, (float) height);
}
