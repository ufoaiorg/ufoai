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

#if !defined(INCLUDED_VIEW_H)
#define INCLUDED_VIEW_H

#include "cullable.h"
#include "math/frustum.h"

char const* Cull_GetStats ();

void Cull_ResetStats ();

/// \brief View-volume culling and transformations.
class View: public VolumeTest {
	/// modelview matrix
	Matrix4 m_modelview;
	/// projection matrix
	Matrix4 m_projection;
	/// device-to-screen transform
	Matrix4 m_viewport;

	Matrix4 m_scissor;

	/// combined modelview and projection matrix
	Matrix4 m_viewproj;
	/// camera position in world space
	Vector4 m_viewer;
	/// view frustum in world space
	Frustum m_frustum;

	bool m_fill;

	void construct ();
public:
	View (bool fill = false);

	void Construct (const Matrix4& projection, const Matrix4& modelview, std::size_t width, std::size_t height);

	void EnableScissor (float min_x, float max_x, float min_y, float max_y);

	void DisableScissor ();

	bool TestPoint (const Vector3& point) const;

	bool TestLine (const Segment& segment) const;

	bool TestPlane (const Plane3& plane) const;

	bool TestPlane (const Plane3& plane, const Matrix4& localToWorld) const;

	VolumeIntersectionValue TestAABB (const AABB& aabb) const;

	VolumeIntersectionValue TestAABB (const AABB& aabb, const Matrix4& localToWorld) const;

	const Matrix4& GetViewMatrix () const;

	const Matrix4& GetViewport () const;

	const Matrix4& GetModelview () const;

	const Matrix4& GetProjection () const;

	bool fill () const;

	const Vector3& getViewer () const;
};

#endif
