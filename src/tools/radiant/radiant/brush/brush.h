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

#if !defined(INCLUDED_BRUSH_H)
#define INCLUDED_BRUSH_H

/// \file
/// \brief The brush primitive.
///
/// A collection of planes that define a convex polyhedron.
/// The Boundary-Representation of this primitive is a manifold polygonal mesh.
/// Each face polygon is represented by a list of vertices in a \c Winding.
/// Each vertex is associated with another face that is adjacent to the edge
/// formed by itself and the next vertex in the winding. This information can
/// be used to find edge-pairs and vertex-rings.

#include "signal/isignal.h"
#include <vector>

#include "FaceShader.h"
#include "ContentsFlagsValue.h"
#include "FaceTexDef.h"
#include "PlanePoints.h"
#include "FacePlane.h"
#include "Face.h"
#include "SelectableComponents.h"
#include "BrushClass.h"
#include "FaceInstance.h"
#include "BrushClipPlane.h"
#include "EdgeInstance.h"
#include "VertexInstance.h"
#include "BrushInstance.h"
#include "BrushVisit.h"

template<typename TextOuputStreamType>
inline TextOuputStreamType& ostream_write (TextOuputStreamType& ostream, const Matrix4& m)
{
	return ostream << "(" << m[0] << " " << m[1] << " " << m[2] << " " << m[3] << ", " << m[4] << " " << m[5] << " "
			<< m[6] << " " << m[7] << ", " << m[8] << " " << m[9] << " " << m[10] << " " << m[11] << ", " << m[12]
			<< " " << m[13] << " " << m[14] << " " << m[15] << ")";
}

#endif
