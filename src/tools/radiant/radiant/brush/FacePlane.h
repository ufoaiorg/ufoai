#ifndef FACEPLANE_H_
#define FACEPLANE_H_

#include "math/Plane3.h"
#include "math/Vector3.h"
#include "math/matrix.h"
#include "PlanePoints.h"

inline bool check_plane_is_integer (const PlanePoints& planePoints)
{
	return !float_is_integer(planePoints[0][0]) || !float_is_integer(planePoints[0][1]) || !float_is_integer(
			planePoints[0][2]) || !float_is_integer(planePoints[1][0]) || !float_is_integer(planePoints[1][1])
			|| !float_is_integer(planePoints[1][2]) || !float_is_integer(planePoints[2][0]) || !float_is_integer(
			planePoints[2][1]) || !float_is_integer(planePoints[2][2]);
}

class FacePlane
{
		PlanePoints m_planepts;
		Plane3 m_planeCached;
		Plane3 m_plane;
	public:
		Vector3 m_funcStaticOrigin;

		class SavedState
		{
			public:
				PlanePoints m_planepts;
				Plane3 m_plane;

				SavedState (const FacePlane& facePlane)
				{
					planepts_assign(m_planepts, facePlane.planePoints());
				}

				void exportState (FacePlane& facePlane) const
				{
					planepts_assign(facePlane.planePoints(), m_planepts);
					facePlane.MakePlane();
				}
		};

		FacePlane () :
			m_funcStaticOrigin(0, 0, 0)
		{
		}
		FacePlane (const FacePlane& other) :
			m_funcStaticOrigin(0, 0, 0)
		{
			planepts_assign(m_planepts, other.m_planepts);
			MakePlane();
		}

		void MakePlane ()
		{
#if 0
			if (check_plane_is_integer(m_planepts)) {
				globalErrorStream() << "non-integer planepts: ";
				planepts_print(m_planepts, globalErrorStream());
				globalErrorStream() << "\n";
			}
#endif
			m_planeCached = Plane3(m_planepts);
		}

		void reverse ()
		{
			vector3_swap(m_planepts[0], m_planepts[2]);
			MakePlane();
		}
		void transform (const Matrix4& matrix, bool mirror)
		{
#if 0
			bool off = check_plane_is_integer(planePoints());
#endif

			matrix4_transform_point(matrix, m_planepts[0]);
			matrix4_transform_point(matrix, m_planepts[1]);
			matrix4_transform_point(matrix, m_planepts[2]);

			if (mirror) {
				reverse();
			}

#if 0
			if (check_plane_is_integer(planePoints())) {
				if (!off) {
					globalErrorStream() << "caused by transform\n";
				}
			}
#endif
			MakePlane();
		}
		void offset (float offset)
		{
			Vector3 move(m_planeCached.normal() * -offset);

			m_planepts[0] -= move;
			m_planepts[1] -= move;
			m_planepts[2] -= move;

			MakePlane();
		}

		PlanePoints& planePoints ()
		{
			return m_planepts;
		}
		const PlanePoints& planePoints () const
		{
			return m_planepts;
		}
		const Plane3& plane3 () const
		{
			return m_planeCached;
		}

		void copy (const FacePlane& other)
		{
			planepts_assign(m_planepts, other.m_planepts);
			MakePlane();
		}
		void copy (const Vector3& p0, const Vector3& p1, const Vector3& p2)
		{
			m_planepts[0] = p0;
			m_planepts[1] = p1;
			m_planepts[2] = p2;
			MakePlane();
		}
}; // class FacePlane

#endif /*FACEPLANE_H_*/
