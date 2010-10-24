#ifndef OPENGLSTATEBUCKET_H_
#define OPENGLSTATEBUCKET_H_

#include "../RenderStatistics.h"
#include "math/matrix.h"

/**
 * @brief A container of Renderable references.
 * May contain the same Renderable multiple times, with different transforms.
 */
class OpenGLShaderPass {
	public:
		struct RenderTransform {
				const Matrix4* m_transform;
				const OpenGLRenderable *m_renderable;
				const RendererLight* m_light;

				RenderTransform(const OpenGLRenderable& renderable, const Matrix4& transform,
						const RendererLight* light) :
					m_transform(&transform), m_renderable(&renderable), m_light(light) {
				}
		};

		typedef std::vector<RenderTransform> Renderables;

	private:

		OpenGLState m_state;
		Renderables m_renderables;
		render::RenderStatistics& _stats;

	public:
		OpenGLShaderPass() :
			_stats(render::RenderStatistics::Instance()) {
		}
		void addRenderable(const OpenGLRenderable& renderable, const Matrix4& modelview,
				const RendererLight* light = 0) {
			m_renderables.push_back(RenderTransform(renderable, modelview, light));
		}

		OpenGLState& state() {
			return m_state;
		}

		void flush(OpenGLState& current, unsigned int globalstate, const Vector3& viewer) {
			const Matrix4* transform = 0;
			glPushMatrix();
			for (OpenGLShaderPass::Renderables::const_iterator i = m_renderables.begin(); i
					!= m_renderables.end(); ++i) {
				if (!transform || (transform != (*i).m_transform && !matrix4_affine_equal(
						*transform, *(*i).m_transform))) {
					_stats.increaseTransforms();
					transform = (*i).m_transform;
					glPopMatrix();
					glPushMatrix();
					glMultMatrixf(*transform);
					glFrontFace(((current.m_state & RENDER_CULLFACE) != 0 && matrix4_handedness(
							*transform) == MATRIX4_RIGHTHANDED) ? GL_CW : GL_CCW);
				}

				_stats.increasePrimitive();

				(*i).m_renderable->render(current.m_state);
			}
			glPopMatrix();
			m_renderables.clear();
		}

		void render(OpenGLState& current, unsigned int globalstate, const Vector3& viewer) {
			if ((globalstate & m_state.m_state & RENDER_SCREEN) != 0) {
				m_state.apply(current, globalstate);

				glMatrixMode( GL_PROJECTION);
				glPushMatrix();
				glLoadMatrixf(Matrix4::getIdentity());

				glMatrixMode( GL_MODELVIEW);
				glPushMatrix();
				glLoadMatrixf(Matrix4::getIdentity());

				glBegin( GL_QUADS);
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
				m_state.apply(current, globalstate);
				flush(current, globalstate, viewer);
			}
		}
};

#endif /* OPENGLSTATEBUCKET_H_ */
