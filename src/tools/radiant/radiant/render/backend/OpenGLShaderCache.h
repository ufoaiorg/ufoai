#ifndef OPENGLSHADERCACHE_H_
#define OPENGLSHADERCACHE_H_

#include "irender.h"
#include "OpenGLStateManager.h"
#include "container/cache.h"
#include "container/hashfunc.h"

class OpenGLShaderCache: public ShaderCache,
		public TexturesCacheObserver,
		public ModuleObserver,
		public render::OpenGLStateManager
{
		class CreateOpenGLShader
		{
				OpenGLShaderCache* m_cache;
			public:
				explicit CreateOpenGLShader (OpenGLShaderCache* cache = 0) :
					m_cache(cache)
				{
				}
				OpenGLShader* construct (const std::string& name)
				{
					OpenGLShader* shader = new OpenGLShader(*m_cache);
					if (m_cache->realised()) {
						shader->realise(name);
					}
					return shader;
				}
				void destroy (OpenGLShader* shader)
				{
					if (m_cache->realised()) {
						shader->unrealise();
					}
					delete shader;
				}
		};

		typedef HashedCache<std::string, OpenGLShader, HashString, std::equal_to<std::string>, CreateOpenGLShader>
				Shaders;
		Shaders m_shaders;
		std::size_t m_unrealised;

		render::OpenGLStates _state_sorted;

	public:
		OpenGLShaderCache () :
			m_shaders(CreateOpenGLShader(this)), m_unrealised(3), // wait until shaders, gl-context and textures are realised before creating any render-states
					m_traverseRenderablesMutex(false)
		{
		}
		~OpenGLShaderCache ()
		{
			for (Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i) {
				globalOutputStream() << "leaked shader: " << makeQuoted(i->key) << "\n";
			}
		}

		void insertSortedState (const render::OpenGLStates::value_type& val)
		{
			_state_sorted.insert(val);
		}

		void eraseSortedState (const render::OpenGLStates::key_type& key)
		{
			_state_sorted.erase(key);
		}

		/* Capture the given shader.
		 */
		Shader* capture (const std::string& name)
		{
			return m_shaders.capture(name).get();
		}

		/* Release the given shader.
		 */
		void release (const std::string& name)
		{
			m_shaders.release(name);
		}

		void render (RenderStateFlags globalstate, const Matrix4& modelview, const Matrix4& projection,
				const Vector3& viewer)
		{
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(projection);

			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(modelview);

			ASSERT_MESSAGE(realised(), "render states are not realised");

			// global settings that are not set in renderstates
			glFrontFace(GL_CW);
			glCullFace(GL_BACK);
			glPolygonOffset(-1, 1);
			{
				const GLubyte pattern[132] = { 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA,
						0x55, 0x55, 0x55, 0x55 };
				glPolygonStipple(pattern);
			}
			glEnableClientState(GL_VERTEX_ARRAY);
			glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

			if (GlobalOpenGL().GL_1_3()) {
				glActiveTexture(GL_TEXTURE0);
				glClientActiveTexture(GL_TEXTURE0);
			}

			if (globalstate & RENDER_TEXTURE_2D) {
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}

			OpenGLState current;
			current.constructDefault();
			current.m_sort = OpenGLState::eSortFirst;

			// default renderstate settings
			glLineStipple(current.m_linestipple_factor, current.m_linestipple_pattern);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glDisable(GL_LIGHTING);
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_NORMAL_ARRAY);
			glDisable(GL_BLEND);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_CULL_FACE);
			glShadeModel(GL_FLAT);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glDisable(GL_ALPHA_TEST);
			glDisable(GL_LINE_STIPPLE);
			glDisable(GL_POLYGON_STIPPLE);
			glDisable(GL_POLYGON_OFFSET_LINE);
			glDisable(GL_POLYGON_OFFSET_FILL); // greebo: otherwise tiny gap lines between brushes are visible

			glBindTexture(GL_TEXTURE_2D, 0);
			glColor4f(1, 1, 1, 1);
			glDepthFunc(GL_LESS);
			glAlphaFunc(GL_ALWAYS, 0);
			glLineWidth(1);
			glPointSize(1);

			// render brushes and entities
			for (render::OpenGLStates::iterator i = _state_sorted.begin(); i != _state_sorted.end(); ++i) {
				(*i).second->render(current, globalstate, viewer);
			}
		}
		void realise ()
		{
			if (--m_unrealised == 0) {
				for (Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i) {
					if (!(*i).value.empty()) {
						(*i).value->realise(i->key);
					}
				}
			}
		}
		void unrealise ()
		{
			if (++m_unrealised == 1) {
				for (Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i) {
					if (!(*i).value.empty()) {
						(*i).value->unrealise();
					}
				}
			}
		}
		bool realised ()
		{
			return m_unrealised == 0;
		}

		typedef std::set<const Renderable*> Renderables;
		Renderables m_renderables;
		mutable bool m_traverseRenderablesMutex;

		// renderables
		void attachRenderable (const Renderable& renderable)
		{
			ASSERT_MESSAGE(!m_traverseRenderablesMutex, "attaching renderable during traversal");
			ASSERT_MESSAGE(m_renderables.find(&renderable) == m_renderables.end(),
					"renderable could not be attached");
			m_renderables.insert(&renderable);
		}
		void detachRenderable (const Renderable& renderable)
		{
			ASSERT_MESSAGE(!m_traverseRenderablesMutex, "detaching renderable during traversal");
			ASSERT_MESSAGE(m_renderables.find(&renderable) != m_renderables.end(),
					"renderable could not be detached");
			m_renderables.erase(&renderable);
		}
		void forEachRenderable (const RenderableCallback& callback) const
		{
			ASSERT_MESSAGE(!m_traverseRenderablesMutex, "for-each during traversal");
			m_traverseRenderablesMutex = true;
			for (Renderables::const_iterator i = m_renderables.begin(); i != m_renderables.end(); ++i) {
				callback(*(*i));
			}
			m_traverseRenderablesMutex = false;
		}
};

#endif /* OPENGLSHADERCACHE_H_ */
