#ifndef BRUSHCLIPPLANE_H_
#define BRUSHCLIPPLANE_H_

#include "math/Plane3.h"
#include "irender.h"
#include "renderable.h"
#include "winding.h"

class BrushClipPlane: public OpenGLRenderable
{
		Plane3 m_plane;
		Winding m_winding;
		static Shader* m_state;
	public:
		static void constructStatic ();
		static void destroyStatic ();

		void setPlane (const Brush& brush, const Plane3& plane);

		void render (RenderStateFlags state) const;

		void render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;
}; // class BrushClipPlane

#endif /*BRUSHCLIPPLANE_H_*/
