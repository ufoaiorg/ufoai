#ifndef OPENGLSTATEBUCKET_H_
#define OPENGLSTATEBUCKET_H_

#include "irender.h"
#include "iglrender.h"
#include "math/matrix.h"
#include "../RenderStatistics.h"

/**
 * @brief A container of Renderable references.
 * May contain the same Renderable multiple times, with different transforms.
 */
class OpenGLShaderPass
{
	private:

		class RenderTransform
		{
			public:
				const Matrix4* m_transform;
				const OpenGLRenderable *m_renderable;

				RenderTransform (const OpenGLRenderable& renderable, const Matrix4& transform) :
					m_transform(&transform), m_renderable(&renderable)
				{
				}
		};

		typedef std::vector<RenderTransform> Renderables;

		void setTextureState (GLint& current, const GLint& texture, GLenum textureUnit);

		void setTextureState (GLint& current, const GLint& texture);

		void setState (unsigned int state, unsigned int delta, unsigned int flag, GLenum glflag);

		void apply (OpenGLState& current, unsigned int globalstate);

		OpenGLState m_state;
		Renderables m_renderables;
		render::RenderStatistics& _stats;

	public:

		OpenGLShaderPass ();

		void addRenderable (const OpenGLRenderable& renderable, const Matrix4& modelview);

		OpenGLState& getState ();

		void flush (OpenGLState& current, unsigned int globalstate, const Vector3& viewer);

		void render (OpenGLState& current, unsigned int globalstate, const Vector3& viewer);
};

#endif /* OPENGLSTATEBUCKET_H_ */
