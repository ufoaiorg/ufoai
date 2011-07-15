#include "OpenGLShaderPass.h"

OpenGLShaderPass::OpenGLShaderPass () :
	_stats(render::RenderStatistics::Instance())
{
}
void OpenGLShaderPass::addRenderable (const OpenGLRenderable& renderable, const Matrix4& modelview)
{
	m_renderables.push_back(RenderTransform(renderable, modelview));
}

OpenGLState& OpenGLShaderPass::getState ()
{
	return m_state;
}

void OpenGLShaderPass::flush (OpenGLState& current, unsigned int globalstate, const Vector3& viewer)
{
	const Matrix4* transform = 0;
	glPushMatrix();
	for (OpenGLShaderPass::Renderables::const_iterator i = m_renderables.begin(); i != m_renderables.end(); ++i) {
		if (!transform || (transform != (*i).m_transform && !matrix4_affine_equal(*transform, *(*i).m_transform))) {
			_stats.increaseTransforms();
			transform = (*i).m_transform;
			glPopMatrix();
			glPushMatrix();
			glMultMatrixf(*transform);
			glFrontFace(((current.m_state & RENDER_CULLFACE) != 0 && matrix4_handedness(*transform)
					== MATRIX4_RIGHTHANDED) ? GL_CW : GL_CCW);
		}

		_stats.increasePrimitive();

		(*i).m_renderable->render(current.m_state);
	}
	glPopMatrix();
	m_renderables.clear();
}

void OpenGLShaderPass::setTextureState (GLint& current, const GLint& texture, GLenum textureUnit)
{
	if (texture != current) {
		glActiveTexture(textureUnit);
		glClientActiveTexture(textureUnit);
		glBindTexture(GL_TEXTURE_2D, texture);

		current = texture;
	}
}

void OpenGLShaderPass::setTextureState (GLint& current, const GLint& texture)
{
	if (texture != current) {
		glBindTexture(GL_TEXTURE_2D, texture);
		current = texture;
	}
}

void OpenGLShaderPass::setState (unsigned int state, unsigned int delta, unsigned int flag, GLenum glflag)
{
	if (delta & state & flag) {
		glEnable(glflag);
	} else if (delta & ~state & flag) {
		glDisable(glflag);
	}
}

void OpenGLShaderPass::apply (OpenGLState& current, unsigned int globalstate)
{
	if (m_state.m_state & RENDER_OVERRIDE) {
		globalstate |= RENDER_FILL | RENDER_DEPTHWRITE;
	}

	const unsigned int state = m_state.m_state & globalstate;
	const unsigned int delta = state ^ current.m_state;

	if (delta & state & RENDER_FILL) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	} else if (delta & ~state & RENDER_FILL) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	if (delta & state & RENDER_LIGHTING) {
		glEnable(GL_LIGHTING);
		glEnable(GL_COLOR_MATERIAL);
		glEnableClientState(GL_NORMAL_ARRAY);
	} else if (delta & ~state & RENDER_LIGHTING) {
		glDisable(GL_LIGHTING);
		glDisable(GL_COLOR_MATERIAL);
		glDisableClientState(GL_NORMAL_ARRAY);
	}

	if (delta & state & RENDER_TEXTURE_2D) {
		if (GlobalOpenGL().GL_1_3()) {
			glActiveTexture(GL_TEXTURE0);
			glClientActiveTexture(GL_TEXTURE0);
		}

		glEnable(GL_TEXTURE_2D);
		glColor4f(1, 1, 1, m_state.m_colour[3]);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	} else if (delta & ~state & RENDER_TEXTURE_2D) {
		if (GlobalOpenGL().GL_1_3()) {
			glActiveTexture(GL_TEXTURE0);
			glClientActiveTexture(GL_TEXTURE0);
		}

		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (delta & state & RENDER_BLEND) {
		glEnable(GL_BLEND);
	} else if (delta & ~state & RENDER_BLEND) {
		glDisable(GL_BLEND);
	}

	setState(state, delta, RENDER_CULLFACE, GL_CULL_FACE);

	if (delta & state & RENDER_SMOOTH) {
		glShadeModel(GL_SMOOTH);
	} else if (delta & ~state & RENDER_SMOOTH) {
		glShadeModel(GL_FLAT);
	}

	setState(state, delta, RENDER_SCALED, GL_NORMALIZE); // not GL_RESCALE_NORMAL
	setState(state, delta, RENDER_DEPTHTEST, GL_DEPTH_TEST);

	if (delta & state & RENDER_DEPTHWRITE) {
		glDepthMask(GL_TRUE);
	} else if (delta & ~state & RENDER_DEPTHWRITE) {
		glDepthMask(GL_FALSE);
	}

	if (delta & state & RENDER_COLOURWRITE) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	} else if (delta & ~state & RENDER_COLOURWRITE) {
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	}

	setState(state, delta, RENDER_ALPHATEST, GL_ALPHA_TEST);

	if (delta & state & RENDER_COLOURARRAY) {
		glEnableClientState(GL_COLOR_ARRAY);
	} else if (delta & ~state & RENDER_COLOURARRAY) {
		glDisableClientState(GL_COLOR_ARRAY);
		glColor4fv(m_state.m_colour);
	}

	if (delta & ~state & RENDER_COLOURCHANGE) {
		glColor4fv(m_state.m_colour);
	}

	setState(state, delta, RENDER_LINESTIPPLE, GL_LINE_STIPPLE);

	if ((state & RENDER_DEPTHTEST) && m_state.m_depthfunc != current.m_depthfunc) {
		glDepthFunc(m_state.m_depthfunc);

		current.m_depthfunc = m_state.m_depthfunc;
	}

	if ((state & RENDER_LINESTIPPLE) && (m_state.m_linestipple_factor != current.m_linestipple_factor
			|| m_state.m_linestipple_pattern != current.m_linestipple_pattern)) {
		glLineStipple(m_state.m_linestipple_factor, m_state.m_linestipple_pattern);

		current.m_linestipple_factor = m_state.m_linestipple_factor;
		current.m_linestipple_pattern = m_state.m_linestipple_pattern;
	}

	if ((state & RENDER_ALPHATEST) && (m_state.m_alphafunc != current.m_alphafunc || m_state.m_alpharef
			!= current.m_alpharef)) {
		glAlphaFunc(m_state.m_alphafunc, m_state.m_alpharef);

		current.m_alphafunc = m_state.m_alphafunc;
		current.m_alpharef = m_state.m_alpharef;
	}

	if (GlobalOpenGL().GL_1_3()) {
		setTextureState(current.m_texture, m_state.m_texture, GL_TEXTURE0);
		setTextureState(current.m_texture1, m_state.m_texture1, GL_TEXTURE1);
		setTextureState(current.m_texture2, m_state.m_texture2, GL_TEXTURE2);
		setTextureState(current.m_texture3, m_state.m_texture3, GL_TEXTURE3);
		setTextureState(current.m_texture4, m_state.m_texture4, GL_TEXTURE4);
		setTextureState(current.m_texture5, m_state.m_texture5, GL_TEXTURE5);
		setTextureState(current.m_texture6, m_state.m_texture6, GL_TEXTURE6);
		setTextureState(current.m_texture7, m_state.m_texture7, GL_TEXTURE7);
	} else {
		setTextureState(current.m_texture, m_state.m_texture);
	}

	if ((state & RENDER_TEXTURE_2D) && m_state.m_colour[3] != current.m_colour[3]) {
		glColor4f(1, 1, 1, m_state.m_colour[3]);
	}

	if (!(state & RENDER_TEXTURE_2D) && (m_state.m_colour[0] != current.m_colour[0] || m_state.m_colour[1]
			!= current.m_colour[1] || m_state.m_colour[2] != current.m_colour[2] || m_state.m_colour[3]
			!= current.m_colour[3])) {
		glColor4fv(m_state.m_colour);
	}
	current.m_colour = m_state.m_colour;

	if ((state & RENDER_BLEND) && (m_state.m_blend_src != current.m_blend_src || m_state.m_blend_dst
			!= current.m_blend_dst)) {
		glBlendFunc(m_state.m_blend_src, m_state.m_blend_dst);
		current.m_blend_src = m_state.m_blend_src;
		current.m_blend_dst = m_state.m_blend_dst;
	}

	if (!(state & RENDER_FILL) && m_state.m_linewidth != current.m_linewidth) {
		glLineWidth(m_state.m_linewidth);
		current.m_linewidth = m_state.m_linewidth;
	}

	if (!(state & RENDER_FILL) && m_state.m_pointsize != current.m_pointsize) {
		glPointSize(m_state.m_pointsize);
		current.m_pointsize = m_state.m_pointsize;
	}

	// Apply polygon offset
	if (m_state.m_polygonOffset != current.m_polygonOffset) {
		current.m_polygonOffset = m_state.m_polygonOffset;

		if (current.m_polygonOffset > 0.0f) {
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1, -1 * current.m_polygonOffset);
		} else {
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
	}

	current.m_state = state;
}

void OpenGLShaderPass::render (OpenGLState& current, unsigned int globalstate, const Vector3& viewer)
{
	if ((globalstate & m_state.m_state & RENDER_SCREEN) != 0) {
		apply(current, globalstate);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrixf(Matrix4::getIdentity());

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadMatrixf(Matrix4::getIdentity());

		glBegin(GL_QUADS);
		glVertex3f(-1, -1, 0);
		glVertex3f(1, -1, 0);
		glVertex3f(1, 1, 0);
		glVertex3f(-1, 1, 0);
		glEnd();

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	} else if (!m_renderables.empty()) {
		apply(current, globalstate);
		flush(current, globalstate, viewer);
	}
}
