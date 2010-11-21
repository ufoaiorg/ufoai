#include "BrushClipPlane.h"
#include "Brush.h"

void BrushClipPlane::constructStatic ()
{
	m_state = GlobalShaderCache().capture("$CLIPPER_OVERLAY");
}
void BrushClipPlane::destroyStatic ()
{
	GlobalShaderCache().release("$CLIPPER_OVERLAY");
}

void BrushClipPlane::setPlane (const Brush& brush, const Plane3& plane)
{
	m_plane = plane;
	if (m_plane.isValid()) {
		brush.windingForClipPlane(m_winding, m_plane);
	} else {
		m_winding.resize(0);
	}
}

void BrushClipPlane::render (RenderStateFlags state) const
{
	if ((state & RENDER_FILL) != 0) {
		m_winding.draw(m_plane.normal(), state);
	} else {
		m_winding.drawWireframe();

		// also draw a line indicating the direction of the cut
		Vector3 lineverts[2];
		Winding_Centroid(m_winding, m_plane, lineverts[0]);
		lineverts[1] = lineverts[0] + (m_plane.normal() * (Brush::m_maxWorldCoord * 4));

		glVertexPointer(3, GL_FLOAT, sizeof(Vector3), &lineverts[0]);
		glDrawArrays(GL_LINES, 0, GLsizei(2));
	}
}

void BrushClipPlane::render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	renderer.SetState(m_state, Renderer::eWireframeOnly);
	renderer.SetState(m_state, Renderer::eFullMaterials);
	renderer.addRenderable(*this, localToWorld);
}

Shader* BrushClipPlane::m_state = 0;
