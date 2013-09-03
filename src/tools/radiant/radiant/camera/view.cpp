/**
 * @file
 * @brief Culling stats for the 3d window
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

#include "view.h"

#include <stdio.h>

static int g_count_planes;
static int g_count_oriented_planes;
static int g_count_bboxs;
static int g_count_oriented_bboxs;

void Cull_ResetStats ()
{
	g_count_planes = 0;
	g_count_oriented_planes = 0;
	g_count_bboxs = 0;
	g_count_oriented_bboxs = 0;
}

const char* Cull_GetStats ()
{
	static char g_cull_stats[1024];
	snprintf(g_cull_stats, sizeof(g_cull_stats), "planes %d + %d | bboxs %d + %d", g_count_planes,
			g_count_oriented_planes, g_count_bboxs, g_count_oriented_bboxs);
	return g_cull_stats;
}

static inline void debug_count_plane ()
{
	++g_count_planes;
}

static inline void debug_count_oriented_plane ()
{
	++g_count_oriented_planes;
}

static inline void debug_count_bbox ()
{
	++g_count_bboxs;
}

static inline void debug_count_oriented_bbox ()
{
	++g_count_oriented_bboxs;
}

void View::construct ()
{
	m_viewproj = matrix4_multiplied_by_matrix4(matrix4_multiplied_by_matrix4(m_scissor, m_projection), m_modelview);

	m_frustum = frustum_from_viewproj(m_viewproj);
	m_viewer = viewer_from_viewproj(m_viewproj);
}

View::View (bool fill) :
		m_modelview(Matrix4::getIdentity()), m_projection(Matrix4::getIdentity()), m_scissor(Matrix4::getIdentity()), m_fill(
				fill)
{
}

void View::Construct (const Matrix4& projection, const Matrix4& modelview, std::size_t width, std::size_t height)
{
	// modelview
	m_modelview = modelview;

	// projection
	m_projection = projection;

	// viewport
	m_viewport = Matrix4::getIdentity();
	m_viewport[0] = float(width / 2);
	m_viewport[5] = float(height / 2);
	if (fabs(m_projection[11]) > 0.0000001)
		m_viewport[10] = m_projection[0] * m_viewport[0];
	else
		m_viewport[10] = 1 / m_projection[10];

	construct();
}

void View::EnableScissor (float min_x, float max_x, float min_y, float max_y)
{
	m_scissor = Matrix4::getIdentity();
	m_scissor[0] = static_cast<float>((max_x - min_x) * 0.5);
	m_scissor[5] = static_cast<float>((max_y - min_y) * 0.5);
	m_scissor[12] = static_cast<float>((min_x + max_x) * 0.5);
	m_scissor[13] = static_cast<float>((min_y + max_y) * 0.5);
	matrix4_full_invert(m_scissor);

	construct();
}

void View::DisableScissor ()
{
	m_scissor = Matrix4::getIdentity();

	construct();
}

bool View::TestPoint (const Vector3& point) const
{
	return viewproj_test_point(m_viewproj, point);
}

bool View::TestLine (const Segment& segment) const
{
	return frustum_test_line(m_frustum, segment);
}

bool View::TestPlane (const Plane3& plane) const
{
	debug_count_plane();
	return viewer_test_plane(m_viewer, plane);
}

bool View::TestPlane (const Plane3& plane, const Matrix4& localToWorld) const
{
	debug_count_oriented_plane();
	return viewer_test_transformed_plane(m_viewer, plane, localToWorld);
}

VolumeIntersectionValue View::TestAABB (const AABB& aabb) const
{
	debug_count_bbox();
	return frustum_test_aabb(m_frustum, aabb);
}

VolumeIntersectionValue View::TestAABB (const AABB& aabb, const Matrix4& localToWorld) const
{
	debug_count_oriented_bbox();
	return frustum_intersects_transformed_aabb(m_frustum, aabb, localToWorld);
}

const Matrix4& View::GetViewMatrix () const
{
	return m_viewproj;
}

const Matrix4& View::GetViewport () const
{
	return m_viewport;
}

const Matrix4& View::GetModelview () const
{
	return m_modelview;
}

const Matrix4& View::GetProjection () const
{
	return m_projection;
}

bool View::fill () const
{
	return m_fill;
}

const Vector3& View::getViewer () const
{
	return m_viewer.getVector3();
}
