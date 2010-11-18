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

#if !defined(INCLUDED_WINDING_H)
#define INCLUDED_WINDING_H

#include "debugging/debugging.h"
#include "shared.h"

#include <vector>

#include "iclipper.h"
#include "irender.h"
#include "igl.h"
#include "selectable.h"

#include "texturelib.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "container/array.h"
#include "string/string.h"

struct indexremap_t
{
		indexremap_t (std::size_t _x, std::size_t _y, std::size_t _z) :
			x(_x), y(_y), z(_z)
		{
		}
		std::size_t x, y, z;
};

inline indexremap_t indexremap_for_projectionaxis (const ProjectionAxis axis)
{
	switch (axis) {
	case eProjectionAxisX:
		return indexremap_t(1, 2, 0);
	case eProjectionAxisY:
		return indexremap_t(2, 0, 1);
	default:
		return indexremap_t(0, 1, 2);
	}
}

#define MAX_POINTS_ON_WINDING 64
const std::size_t c_brush_maxFaces = 1024;

class WindingVertex
{
	public:
		Vector3 vertex;			// The 3D coordinates of the point
		Vector2 texcoord;		// The UV coordinates
		Vector3 tangent;		// The tangent
		Vector3 bitangent;		// The bitangent
		std::size_t adjacent;	// The index of the adjacent WindingVertex
};

// A Winding consists of several connected WindingVertex objects,
// each of which holding information about a single corner point.
typedef std::vector<WindingVertex> IWinding;

class Winding : public IWinding
{
	public:
		// Wraps the given index around if it's larger than the size of this winding
		inline std::size_t wrap(std::size_t i) const
		{
			ASSERT_MESSAGE(size() != 0, "Winding_wrap: empty winding");
			return i % size();
		}

		// Returns the next winding index (wraps around)
		inline std::size_t next(std::size_t i) const
		{
			return wrap(++i);
		}

		void testSelect (SelectionTest& test, SelectionIntersection& best)
		{
			if (empty())
				return;
			test.TestPolygon(VertexPointer(&front().vertex, sizeof(WindingVertex)), size(), best);
		}

		void draw (const Vector3& normal, RenderStateFlags state) const
		{
			if (empty())
				return;

			glVertexPointer(3, GL_FLOAT, sizeof(WindingVertex), &front().vertex);
			if (state & RENDER_LIGHTING) {
				Vector3 normals[c_brush_maxFaces];
				typedef Vector3* Vector3Iter;
				for (Vector3Iter i = normals, last = normals + size(); i != last; ++i) {
					*i = normal;
				}
				glNormalPointer(GL_FLOAT, sizeof(Vector3), normals);
			}
			if (state & RENDER_TEXTURE_2D) {
				glTexCoordPointer(2, GL_FLOAT, sizeof(WindingVertex), &front().texcoord);
			}
			glDrawArrays(GL_POLYGON, 0, GLsizei(size()));
		}

		void drawWireframe () const
		{
			if (!empty()) {
				glVertexPointer(3, GL_FLOAT, sizeof(WindingVertex), &front().vertex);
				glDrawArrays(GL_LINE_LOOP, 0, GLsizei(size()));
			}
		}
};

class DoubleLine
{
	public:
		DoubleVector3 origin;
		DoubleVector3 direction;
};

class FixedWindingVertex
{
	public:
		DoubleVector3 vertex;
		DoubleLine edge;
		std::size_t adjacent;

		FixedWindingVertex (const DoubleVector3& vertex_, const DoubleLine& edge_, std::size_t adjacent_) :
			vertex(vertex_), edge(edge_), adjacent(adjacent_)
		{
		}
};

struct FixedWinding
{
		typedef std::vector<FixedWindingVertex> Points;
		Points points;

		FixedWinding ()
		{
			points.reserve(MAX_POINTS_ON_WINDING);
		}

		FixedWindingVertex& front ()
		{
			return points.front();
		}
		const FixedWindingVertex& front () const
		{
			return points.front();
		}
		FixedWindingVertex& back ()
		{
			return points.back();
		}
		const FixedWindingVertex& back () const
		{
			return points.back();
		}

		void clear ()
		{
			points.clear();
		}

		void push_back (const FixedWindingVertex& point)
		{
			points.push_back(point);
		}
		std::size_t size () const
		{
			return points.size();
		}

		FixedWindingVertex& operator[] (std::size_t index)
		{
			//ASSERT_MESSAGE(index < MAX_POINTS_ON_WINDING, "winding: index out of bounds");
			return points[index];
		}
		const FixedWindingVertex& operator[] (std::size_t index) const
		{
			//ASSERT_MESSAGE(index < MAX_POINTS_ON_WINDING, "winding: index out of bounds");
			return points[index];
		}

};

inline void Winding_forFixedWinding (Winding& winding, const FixedWinding& fixed)
{
	winding.resize(fixed.size());
	for (std::size_t i = 0; i < fixed.size(); ++i) {
		winding[i].vertex[0] = static_cast<float> (fixed[i].vertex[0]);
		winding[i].vertex[1] = static_cast<float> (fixed[i].vertex[1]);
		winding[i].vertex[2] = static_cast<float> (fixed[i].vertex[2]);
		winding[i].adjacent = fixed[i].adjacent;
	}
}

class Plane3;

void Winding_createInfinite (FixedWinding& w, const Plane3& plane, double infinity);

/// \brief Returns true if edge (\p x, \p y) is smaller than the epsilon used to classify winding points against a plane.
/// ON_EPSILON was 1.0 / (1 << 8) - but now we are using the global value
inline bool Edge_isDegenerate (const Vector3& x, const Vector3& y)
{
	return (y - x).getLengthSquared() < (ON_EPSILON * ON_EPSILON);
}

void Winding_Clip (const FixedWinding& winding, const Plane3& plane, const Plane3& clipPlane, std::size_t adjacent,
		FixedWinding& clipped);

BrushSplitType Winding_ClassifyPlane (const Winding& w, const Plane3& plane);

bool Winding_PlanesConcave (const Winding& w1, const Winding& w2, const Plane3& plane1, const Plane3& plane2);
bool Winding_TestPlane (const Winding& w, const Plane3& plane, bool flipped);

std::size_t Winding_FindAdjacent (const Winding& w, std::size_t face);

std::size_t Winding_Opposite (const Winding& w, const std::size_t& index, const std::size_t& other);
std::size_t Winding_Opposite (const Winding& w, std::size_t index);

void Winding_Centroid (const Winding& w, const Plane3& plane, Vector3& centroid);

inline void Winding_printConnectivity (Winding& winding)
{
	for (Winding::iterator i = winding.begin(); i != winding.end(); ++i) {
		std::size_t vertexIndex = std::distance(winding.begin(), i);
		globalOutputStream() << "vertex: " << string::toString(vertexIndex) << " adjacent: " << string::toString((*i).adjacent) << "\n";
	}
}

#endif
