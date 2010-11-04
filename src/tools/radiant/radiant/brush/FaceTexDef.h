#ifndef FACETEXDEF_H_
#define FACETEXDEF_H_

#include "winding.h"
#include "math/matrix.h"
#include "math/Vector3.h"
#include "math/Plane3.h"

#include "FaceShader.h"
#include "TextureProjection.h"

class FaceTexdef: public FaceShaderObserver
{
		// not copyable
		FaceTexdef (const FaceTexdef& other);
		// not assignable
		FaceTexdef& operator= (const FaceTexdef& other);
	public:
		class SavedState
		{
			public:
				TextureProjection m_projection;

				SavedState (const FaceTexdef& faceTexdef)
				{
					m_projection = faceTexdef.m_projection;
				}

				void exportState (FaceTexdef& faceTexdef) const
				{
					faceTexdef.m_projection.m_texdef = m_projection.m_texdef;
				}
		};

		FaceShader& m_shader;
		TextureProjection m_projection;
		bool m_projectionInitialised;
		bool m_scaleApplied;

		FaceTexdef (FaceShader& shader, const TextureProjection& projection, bool projectionInitialised = true) :
			m_shader(shader), m_projection(projection), m_projectionInitialised(projectionInitialised), m_scaleApplied(
					false)
		{
			m_shader.attach(*this);
		}
		~FaceTexdef ()
		{
			m_shader.detach(*this);
		}

		void realiseShader ()
		{
		}
		void unrealiseShader ()
		{
		}

		void setTexdef (const TextureProjection& projection)
		{
			m_projection.m_texdef = projection.m_texdef;
		}

		void shift (float s, float t)
		{
			ASSERT_MESSAGE(m_projection.m_texdef.isSane(), "FaceTexdef::shift: bad texdef");
			m_projection.m_texdef.shift(s, t);
		}

		void scale (float s, float t)
		{
			m_projection.m_texdef.scale(s, t);
		}

		void rotate (float angle)
		{
			m_projection.m_texdef.rotate(angle);
		}

		void fit (const Vector3& normal, const Winding& winding, float s_repeat, float t_repeat)
		{
			m_projection.fitTexture(m_shader.width(), m_shader.height(), normal, winding, s_repeat, t_repeat);
		}

		void flipTexture(const Vector3& flipAxis) {
			m_projection.flipTexture(flipAxis);
		}

		void emitTextureCoordinates (Winding& winding, const Vector3& normal, const Matrix4& localToWorld)
		{
			m_projection.emitTextureCoordinates(m_shader.width(), m_shader.height(), winding, normal,
					localToWorld);
		}

		void transform (const Plane3& plane, const Matrix4& matrix)
		{
			m_projection.transformLocked(m_shader.width(), m_shader.height(), plane, matrix);
		}

		TextureProjection normalised () const
		{
			return TextureProjection(m_projection.m_texdef);
		}
}; // class FaceTexDef

#endif /*FACETEXDEF_H_*/
