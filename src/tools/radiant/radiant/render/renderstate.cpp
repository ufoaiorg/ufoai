/**
 * @file renderstate.cpp
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#include "debugging/debugging.h"

#include "ishadersystem.h"
#include "imaterial.h"
#include "irender.h"
#include "itextures.h"
#include "igl.h"
#include "iglrender.h"
#include "renderable.h"
#include "iradiant.h"

#include <set>
#include <vector>
#include <list>
#include <map>
#include <string>

#include "math/matrix.h"
#include "math/aabb.h"
#include "generic/callback.h"
#include "texturelib.h"
#include "container/hashfunc.h"
#include "container/cache.h"
#include "generic/reference.h"
#include "moduleobservers.h"
#include "stream/filestream.h"
#include "stream/stringstream.h"
#include "os/file.h"
#include "../settings/preferences.h"
#include "RenderStatistics.h"

#include "../xyview/xywindow.h"

inline bool OpenGLState_less (const OpenGLState& self, const OpenGLState& other)
{
	// Sort by sort-order override.
	if (self.m_sort != other.m_sort) {
		return self.m_sort < other.m_sort;
	}
	// Sort by texture handle.
	if (self.m_texture != other.m_texture) {
		return self.m_texture < other.m_texture;
	}
	if (self.m_texture1 != other.m_texture1) {
		return self.m_texture1 < other.m_texture1;
	}
	if (self.m_texture2 != other.m_texture2) {
		return self.m_texture2 < other.m_texture2;
	}
	if (self.m_texture3 != other.m_texture3) {
		return self.m_texture3 < other.m_texture3;
	}
	if (self.m_texture4 != other.m_texture4) {
		return self.m_texture4 < other.m_texture4;
	}
	if (self.m_texture5 != other.m_texture5) {
		return self.m_texture5 < other.m_texture5;
	}
	if (self.m_texture6 != other.m_texture6) {
		return self.m_texture6 < other.m_texture6;
	}
	if (self.m_texture7 != other.m_texture7) {
		return self.m_texture7 < other.m_texture7;
	}
	// Sort by state bit-vector.
	if (self.m_state != other.m_state) {
		return self.m_state < other.m_state;
	}
	// Comparing address makes sure states are never equal.
	return &self < &other;
}

void OpenGLState::constructDefault ()
{
	m_state = RENDER_DEFAULT;

	m_texture = 0;
	m_texture1 = 0;
	m_texture2 = 0;
	m_texture3 = 0;
	m_texture4 = 0;
	m_texture5 = 0;
	m_texture6 = 0;
	m_texture7 = 0;

	m_colour.set(1, 1, 1, 1);

	m_depthfunc = GL_LESS;

	m_blend_src = GL_SRC_ALPHA;
	m_blend_dst = GL_ONE_MINUS_SRC_ALPHA;

	m_alphafunc = GL_ALWAYS;
	m_alpharef = 0;

	m_linewidth = 1;
	m_pointsize = 1;

	m_linestipple_factor = 1;
	m_linestipple_pattern = 0xaaaa;
}

/// \brief A container of Renderable references.
/// May contain the same Renderable multiple times, with different transforms.
class OpenGLStateBucket
{
	public:
		struct RenderTransform
		{
				const Matrix4* m_transform;
				const OpenGLRenderable *m_renderable;
				const RendererLight* m_light;

				RenderTransform (const OpenGLRenderable& renderable, const Matrix4& transform,
						const RendererLight* light) :
					m_transform(&transform), m_renderable(&renderable), m_light(light)
				{
				}
		};

		typedef std::vector<RenderTransform> Renderables;

	private:

		OpenGLState m_state;
		Renderables m_renderables;
		render::RenderStatistics& _stats;

	public:
		OpenGLStateBucket () :
			_stats(render::RenderStatistics::Instance())
		{
		}
		void addRenderable (const OpenGLRenderable& renderable, const Matrix4& modelview, const RendererLight* light =
				0)
		{
			m_renderables.push_back(RenderTransform(renderable, modelview, light));
		}

		OpenGLState& state ()
		{
			return m_state;
		}

		void render (OpenGLState& current, unsigned int globalstate, const Vector3& viewer);
		void flush (OpenGLState& current, unsigned int globalstate,	const Vector3& viewer);
};

class OpenGLStateLess
{
	public:
		bool operator() (const OpenGLState& self, const OpenGLState& other) const
		{
			return OpenGLState_less(self, other);
		}
};

typedef ConstReference<OpenGLState> OpenGLStateReference;
typedef std::map<OpenGLStateReference, OpenGLStateBucket*, OpenGLStateLess> OpenGLStates;
OpenGLStates g_state_sorted;

class OpenGLStateBucketAdd
{
		OpenGLStateBucket& m_bucket;
		const OpenGLRenderable& m_renderable;
		const Matrix4& m_modelview;
	public:
		typedef const RendererLight& first_argument_type;

		OpenGLStateBucketAdd (OpenGLStateBucket& bucket, const OpenGLRenderable& renderable, const Matrix4& modelview) :
			m_bucket(bucket), m_renderable(renderable), m_modelview(modelview)
		{
		}
		void operator() (const RendererLight& light)
		{
			m_bucket.addRenderable(m_renderable, m_modelview, &light);
		}
};

class OpenGLShader: public Shader
{
	private:
		typedef std::list<OpenGLStateBucket*> Passes;
		Passes m_passes;
		IShader* m_shader;
		std::size_t m_used;
		ModuleObservers m_observers;
	public:
		OpenGLShader () :
			m_shader(0), m_used(0)
		{
		}
		~OpenGLShader ()
		{
		}

		void appendBlendLayer(const ShaderLayer& layer);
		void visitShaderLayers (const ShaderLayer& layer);

		void construct (const std::string& name);
		void destroy ()
		{
			if (m_shader) {
				m_shader->DecRef();
			}
			m_shader = 0;

			for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
				delete *i;
			}
			m_passes.clear();
		}
		void addRenderable (const OpenGLRenderable& renderable, const Matrix4& modelview, const LightList* lights)
		{
			for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
				(*i)->addRenderable(renderable, modelview);
			}
		}
		void incrementUsed ()
		{
			if (++m_used == 1 && m_shader != 0) {
				m_shader->SetInUse(true);
			}
		}
		void decrementUsed ()
		{
			if (--m_used == 0 && m_shader != 0) {
				m_shader->SetInUse(false);
			}
		}
		bool realised () const
		{
			return m_shader != 0;
		}
		void attach (ModuleObserver& observer)
		{
			if (realised()) {
				observer.realise();
			}
			m_observers.attach(observer);
		}
		void detach (ModuleObserver& observer)
		{
			if (realised()) {
				observer.unrealise();
			}
			m_observers.detach(observer);
		}
		void realise (const std::string& name)
		{
			if (!name.empty())
				construct(name);

			if (m_used != 0 && m_shader != 0) {
				m_shader->SetInUse(true);
			}

			for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
				g_state_sorted.insert(OpenGLStates::value_type(OpenGLStateReference((*i)->state()), *i));
			}

			m_observers.realise();
		}
		void unrealise ()
		{
			m_observers.unrealise();

			for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
				g_state_sorted.erase(OpenGLStateReference((*i)->state()));
			}

			destroy();
		}
		qtexture_t& getTexture () const
		{
			ASSERT_NOTNULL(m_shader);
			return *m_shader->getTexture();
		}
		unsigned int getFlags () const
		{
			ASSERT_NOTNULL(m_shader);
			return m_shader->getFlags();
		}
		IShader& getShader () const
		{
			ASSERT_NOTNULL(m_shader);
			return *m_shader;
		}
		OpenGLState& appendDefaultPass ()
		{
			m_passes.push_back(new OpenGLStateBucket);
			OpenGLState& state = m_passes.back()->state();
			state.constructDefault();
			return state;
		}
};

typedef std::set<RendererLight*> RendererLights;

class LinearLightList: public LightList
{
		LightCullable& m_cullable;
		RendererLights& m_allLights;
		Callback m_evaluateChanged;

		typedef std::list<RendererLight*> Lights;
		mutable Lights m_lights;
		mutable bool m_lightsChanged;
	public:
		LinearLightList (LightCullable& cullable, RendererLights& lights, const Callback& evaluateChanged) :
			m_cullable(cullable), m_allLights(lights), m_evaluateChanged(evaluateChanged)
		{
			m_lightsChanged = true;
		}
		void evaluateLights () const
		{
			m_evaluateChanged();
			if (m_lightsChanged) {
				m_lightsChanged = false;

				m_lights.clear();
				m_cullable.clearLights();
				for (RendererLights::const_iterator i = m_allLights.begin(); i != m_allLights.end(); ++i) {
					m_lights.push_back(*i);
					m_cullable.insertLight(*(*i));
				}
			}
		}
		void forEachLight (const RendererLightCallback& callback) const
		{
			evaluateLights();

			for (Lights::const_iterator i = m_lights.begin(); i != m_lights.end(); ++i) {
				callback(*(*i));
			}
		}
		void lightsChanged () const
		{
			m_lightsChanged = true;
		}
};

class OpenGLShaderCache: public ShaderCache, public TexturesCacheObserver, public ModuleObserver
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
					OpenGLShader* shader = new OpenGLShader;
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


	public:
		OpenGLShaderCache () :
			m_shaders(CreateOpenGLShader(this)),
					m_unrealised(3), // wait until shaders, gl-context and textures are realised before creating any render-states
					m_lightsChanged(true),
					m_traverseRenderablesMutex(false)
		{
		}
		~OpenGLShaderCache ()
		{
			for (Shaders::iterator i = m_shaders.begin(); i != m_shaders.end(); ++i) {
				globalOutputStream() << "leaked shader: " << makeQuoted(i->key) << "\n";
			}
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
			glLoadMatrixf(reinterpret_cast<const float*> (&projection));

			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(reinterpret_cast<const float*> (&modelview));

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

			if (globalstate & RENDER_TEXTURE) {
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

			glBindTexture(GL_TEXTURE_2D, 0);
			glColor4f(1, 1, 1, 1);
			glDepthFunc(GL_LESS);
			glAlphaFunc(GL_ALWAYS, 0);
			glLineWidth(1);
			glPointSize(1);

			// render brushes and entities
			for (OpenGLStates::iterator i = g_state_sorted.begin(); i != g_state_sorted.end(); ++i) {
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

		// light culling

		RendererLights m_lights;
		bool m_lightsChanged;
		typedef std::map<LightCullable*, LinearLightList> LightLists;
		LightLists m_lightLists;

		const LightList& attach (LightCullable& cullable)
		{
			return (*m_lightLists.insert(LightLists::value_type(&cullable, LinearLightList(cullable, m_lights,
					EvaluateChangedCaller(*this)))).first).second;
		}
		void detach (LightCullable& cullable)
		{
			m_lightLists.erase(&cullable);
		}
		void changed (LightCullable& cullable)
		{
			LightLists::iterator i = m_lightLists.find(&cullable);
			ASSERT_MESSAGE(i != m_lightLists.end(), "cullable not attached");
			(*i).second.lightsChanged();
		}
		void attach (RendererLight& light)
		{
			ASSERT_MESSAGE(m_lights.find(&light) == m_lights.end(), "light could not be attached");
			m_lights.insert(&light);
			changed(light);
		}
		void detach (RendererLight& light)
		{
			ASSERT_MESSAGE(m_lights.find(&light) != m_lights.end(), "light could not be detached");
			m_lights.erase(&light);
			changed(light);
		}
		void changed (RendererLight& light)
		{
			m_lightsChanged = true;
		}
		void evaluateChanged ()
		{
			if (m_lightsChanged) {
				m_lightsChanged = false;
				for (LightLists::iterator i = m_lightLists.begin(); i != m_lightLists.end(); ++i) {
					(*i).second.lightsChanged();
				}
			}
		}
		typedef MemberCaller<OpenGLShaderCache, &OpenGLShaderCache::evaluateChanged> EvaluateChangedCaller;

		typedef std::set<const Renderable*> Renderables;
		Renderables m_renderables;
		mutable bool m_traverseRenderablesMutex;

		// renderables
		void attachRenderable (const Renderable& renderable)
		{
			ASSERT_MESSAGE(!m_traverseRenderablesMutex, "attaching renderable during traversal");
			ASSERT_MESSAGE(m_renderables.find(&renderable) == m_renderables.end(), "renderable could not be attached");
			m_renderables.insert(&renderable);
		}
		void detachRenderable (const Renderable& renderable)
		{
			ASSERT_MESSAGE(!m_traverseRenderablesMutex, "detaching renderable during traversal");
			ASSERT_MESSAGE(m_renderables.find(&renderable) != m_renderables.end(), "renderable could not be detached");
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

inline void setTextureState (GLint& current, const GLint& texture, GLenum textureUnit)
{
	if (texture != current) {
		glActiveTexture(textureUnit);
		glClientActiveTexture(textureUnit);
		glBindTexture(GL_TEXTURE_2D, texture);

		current = texture;
	}
}

inline void setTextureState (GLint& current, const GLint& texture)
{
	if (texture != current) {
		glBindTexture(GL_TEXTURE_2D, texture);
		current = texture;
	}
}

inline void setState (unsigned int state, unsigned int delta, unsigned int flag, GLenum glflag)
{
	if (delta & state & flag) {
		glEnable(glflag);
	} else if (delta & ~state & flag) {
		glDisable(glflag);
	}
}

void OpenGLState::apply (OpenGLState& current, unsigned int globalstate)
{
	render::RenderStatistics::Instance().increaseStates();

	if (m_state & RENDER_OVERRIDE) {
		globalstate |= RENDER_FILL | RENDER_DEPTHWRITE;
	}

	const unsigned int state = m_state & globalstate;
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

	if (delta & state & RENDER_TEXTURE) {
		if (GlobalOpenGL().GL_1_3()) {
			glActiveTexture(GL_TEXTURE0);
			glClientActiveTexture(GL_TEXTURE0);
		}

		glEnable(GL_TEXTURE_2D);
		glColor4f(1, 1, 1, m_colour[3]);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	} else if (delta & ~state & RENDER_TEXTURE) {
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
		glColor4fv(m_colour);
	}

	if (delta & ~state & RENDER_COLOURCHANGE) {
		glColor4fv(m_colour);
	}

	setState(state, delta, RENDER_LINESTIPPLE, GL_LINE_STIPPLE);

	if (state & RENDER_DEPTHTEST && m_depthfunc != current.m_depthfunc) {
		glDepthFunc(m_depthfunc);

		current.m_depthfunc = m_depthfunc;
	}

	if (state & RENDER_LINESTIPPLE && (m_linestipple_factor != current.m_linestipple_factor
			|| m_linestipple_pattern != current.m_linestipple_pattern)) {
		glLineStipple(m_linestipple_factor, m_linestipple_pattern);

		current.m_linestipple_factor = m_linestipple_factor;
		current.m_linestipple_pattern = m_linestipple_pattern;
	}

	if (state & RENDER_ALPHATEST && (m_alphafunc != current.m_alphafunc || m_alpharef != current.m_alpharef)) {
		glAlphaFunc(m_alphafunc, m_alpharef);

		current.m_alphafunc = m_alphafunc;
		current.m_alpharef = m_alpharef;
	}

	{
		if (GlobalOpenGL().GL_1_3()) {
			setTextureState(current.m_texture, m_texture, GL_TEXTURE0);
			setTextureState(current.m_texture1, m_texture1, GL_TEXTURE1);
			setTextureState(current.m_texture2, m_texture2, GL_TEXTURE2);
			setTextureState(current.m_texture3, m_texture3, GL_TEXTURE3);
			setTextureState(current.m_texture4, m_texture4, GL_TEXTURE4);
			setTextureState(current.m_texture5, m_texture5, GL_TEXTURE5);
			setTextureState(current.m_texture6, m_texture6, GL_TEXTURE6);
			setTextureState(current.m_texture7, m_texture7, GL_TEXTURE7);
		} else {
			setTextureState(current.m_texture, m_texture);
		}
	}

	if (state & RENDER_TEXTURE && m_colour[3] != current.m_colour[3]) {
		glColor4f(1, 1, 1, m_colour[3]);
	}

	if (!(state & RENDER_TEXTURE) && (m_colour[0] != current.m_colour[0] || m_colour[1]
			!= current.m_colour[1] || m_colour[2] != current.m_colour[2] || m_colour[3]
			!= current.m_colour[3])) {
		glColor4fv(m_colour);
	}
	current.m_colour = m_colour;

	if (state & RENDER_BLEND && (m_blend_src != current.m_blend_src || m_blend_dst != current.m_blend_dst)) {
		glBlendFunc(m_blend_src, m_blend_dst);
		current.m_blend_src = m_blend_src;
		current.m_blend_dst = m_blend_dst;
	}

	if (!(state & RENDER_FILL) && m_linewidth != current.m_linewidth) {
		glLineWidth(m_linewidth);
		current.m_linewidth = m_linewidth;
	}

	if (!(state & RENDER_FILL) && m_pointsize != current.m_pointsize) {
		glPointSize(m_pointsize);
		current.m_pointsize = m_pointsize;
	}

	current.m_state = state;
}

void OpenGLStateBucket::flush (OpenGLState& current, unsigned int globalstate, const Vector3& viewer)
{
	const Matrix4* transform = 0;
	glPushMatrix();
	for (OpenGLStateBucket::Renderables::const_iterator i = m_renderables.begin(); i != m_renderables.end(); ++i) {
		if (!transform || (transform != (*i).m_transform && !matrix4_affine_equal(*transform, *(*i).m_transform))) {
			_stats.increaseTransforms();
			transform = (*i).m_transform;
			glPopMatrix();
			glPushMatrix();
			glMultMatrixf(reinterpret_cast<const float*> (transform));
			glFrontFace(((current.m_state & RENDER_CULLFACE) != 0 && matrix4_handedness(*transform)
					== MATRIX4_RIGHTHANDED) ? GL_CW : GL_CCW);
		}

		_stats.increasePrimitive();

		(*i).m_renderable->render(current.m_state);
	}
	glPopMatrix();
	m_renderables.clear();
}

void OpenGLStateBucket::render (OpenGLState& current, unsigned int globalstate, const Vector3& viewer)
{
	if ((globalstate & m_state.m_state & RENDER_SCREEN) != 0) {
		m_state.apply(current, globalstate);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrixf(reinterpret_cast<const float*> (&g_matrix4_identity));

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadMatrixf(reinterpret_cast<const float*> (&g_matrix4_identity));

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
		m_state.apply(current, globalstate);
		flush(current, globalstate, viewer);
	}
}

class OpenGLStateMap: public OpenGLStateLibrary
{
		typedef std::map<std::string, OpenGLState> States;
		States m_states;
	public:
		virtual ~OpenGLStateMap ()
		{
			ASSERT_MESSAGE(m_states.empty(), "OpenGLStateMap::~OpenGLStateMap: not empty");
		}

		typedef States::iterator iterator;
		iterator begin ()
		{
			return m_states.begin();
		}
		iterator end ()
		{
			return m_states.end();
		}

		void getDefaultState (OpenGLState& state) const
		{
			state.constructDefault();
		}

		void insert (const std::string& name, const OpenGLState& state)
		{
			bool inserted = m_states.insert(States::value_type(name, state)).second;
			ASSERT_MESSAGE(inserted, "OpenGLStateMap::insert: " << name << " already exists");
		}
		void erase (const std::string& name)
		{
			std::size_t count = m_states.erase(name);
			ASSERT_MESSAGE(count == 1, "OpenGLStateMap::erase: " << name << " does not exist");
		}

		iterator find (const std::string& name)
		{
			return m_states.find(name);
		}
};

static OpenGLStateMap* g_openglStates = 0;

inline GLenum convertBlendFactor (BlendFactor factor)
{
	switch (factor) {
	case BLEND_ZERO:
		return GL_ZERO;
	case BLEND_ONE:
		return GL_ONE;
	case BLEND_SRC_COLOUR:
		return GL_SRC_COLOR;
	case BLEND_ONE_MINUS_SRC_COLOUR:
		return GL_ONE_MINUS_SRC_COLOR;
	case BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case BLEND_ONE_MINUS_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case BLEND_DST_COLOUR:
		return GL_DST_COLOR;
	case BLEND_ONE_MINUS_DST_COLOUR:
		return GL_ONE_MINUS_DST_COLOR;
	case BLEND_DST_ALPHA:
		return GL_DST_ALPHA;
	case BLEND_ONE_MINUS_DST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case BLEND_SRC_ALPHA_SATURATE:
		return GL_SRC_ALPHA_SATURATE;
	}
	return GL_ZERO;
}

// Append a blend (non-interaction) layer
void OpenGLShader::appendBlendLayer(const ShaderLayer& layer)
{
	qtexture_t* layerTex = layer.getTexture();

	OpenGLState& state = appendDefaultPass();
	state.m_state = RENDER_FILL
					| RENDER_BLEND
					| RENDER_DEPTHTEST
					| RENDER_COLOURWRITE;

	// Set the texture
	state.m_texture = layerTex->texture_number;

	// Get the blend function
	BlendFunc blendFunc = layer.getBlendFunc();
	state.m_blend_src = convertBlendFactor(blendFunc.m_src);
	state.m_blend_dst = convertBlendFactor(blendFunc.m_dst);

	// Alpha-tested stages or one-over-zero blends should use the depth buffer
	if (state.m_blend_src == GL_SRC_ALPHA || state.m_blend_dst == GL_SRC_ALPHA ||
		(state.m_blend_src == GL_ONE && state.m_blend_dst == GL_ZERO))
	{
		state.m_state |= RENDER_DEPTHWRITE;
	}

	state.m_state |= RENDER_TEXTURE;

	// Colour modulation
	reinterpret_cast<Vector3&> (state.m_colour) = layer.getColour();
	state.m_colour[3] = 1.0f;

	state.m_sort = OpenGLState::eSortFullbright;
}


void OpenGLShader::visitShaderLayers (const ShaderLayer& layer)
{
	if (layer.getType() == ShaderLayer::BLEND)
		appendBlendLayer(layer);
}

typedef MemberCaller1<OpenGLShader, const ShaderLayer&, &OpenGLShader::visitShaderLayers> ShaderLayerVisitor;

/// \todo Define special-case shaders in a data file.
void OpenGLShader::construct (const std::string& shaderName)
{
	const char *name = shaderName.c_str();
	OpenGLState& state = appendDefaultPass();
	switch (shaderName[0]) {
	case '(':
		sscanf(name, "(%g %g %g)", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2]);
		state.m_colour[3] = 1.0f;
		state.m_state = RENDER_FILL | RENDER_LIGHTING | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_COLOURWRITE
				| RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
		break;

	case '[':
		sscanf(name, "[%g %g %g]", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2]);
		state.m_colour[3] = 0.5f;
		state.m_state = RENDER_FILL | RENDER_LIGHTING | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_COLOURWRITE
				| RENDER_DEPTHWRITE | RENDER_BLEND;
		state.m_sort = OpenGLState::eSortTranslucent;
		break;

	case '<':
		sscanf(name, "<%g %g %g>", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2]);
		state.m_colour[3] = 1;
		state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
		state.m_depthfunc = GL_LESS;
		state.m_linewidth = 1;
		state.m_pointsize = 1;
		break;

	case '$': {
		OpenGLStateMap::iterator i = g_openglStates->find(name);
		if (i != g_openglStates->end()) {
			state = (*i).second;
			break;
		}
	}
		if (string_equal(name + 1, "POINT")) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst;
			state.m_pointsize = 4;
		} else if (string_equal(name + 1, "SELPOINT")) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst + 1;
			state.m_pointsize = 4;
		} else if (string_equal(name + 1, "PIVOT")) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHTEST | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_linewidth = 2;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHTEST | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_linewidth = 2;
			hiddenLine.m_depthfunc = GL_GREATER;
		} else if (string_equal(name + 1, "WIREFRAME")) {
			state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFullbright;
		} else if (string_equal(name + 1, "CAM_HIGHLIGHT")) {
			state.m_colour[0] = 1;
			state.m_colour[1] = 0;
			state.m_colour[2] = 0;
			state.m_colour[3] = 0.3f;
			state.m_state = RENDER_FILL | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_BLEND | RENDER_COLOURWRITE
					| RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortHighlight;
			state.m_depthfunc = GL_LEQUAL;
		} else if (string_equal(name + 1, "CAM_OVERLAY")) {
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortOverlayFirst + 1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_colour[0] = 0.75;
			hiddenLine.m_colour[1] = 0.75;
			hiddenLine.m_colour[2] = 0.75;
			hiddenLine.m_colour[3] = 1;
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortOverlayFirst;
			hiddenLine.m_depthfunc = GL_GREATER;
			hiddenLine.m_linestipple_factor = 2;
		} else if (string_equal(name + 1, "XY_OVERLAY")) {
			state.m_colour[0] = g_xywindow_globals.color_selbrushes[0];
			state.m_colour[1] = g_xywindow_globals.color_selbrushes[1];
			state.m_colour[2] = g_xywindow_globals.color_selbrushes[2];
			state.m_colour[3] = 1;
			state.m_state = RENDER_COLOURWRITE | RENDER_LINESTIPPLE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
			state.m_linewidth = 2;
			state.m_linestipple_factor = 3;
		} else if (string_equal(name + 1, "DEBUG_CLIPPED")) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortLast;
		} else if (string_equal(name + 1, "Q3MAP2_LIGHT_SPHERE")) {
			state.m_colour[0] = .05f;
			state.m_colour[1] = .05f;
			state.m_colour[2] = .05f;
			state.m_colour[3] = 1;
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_BLEND | RENDER_FILL;
			state.m_blend_src = GL_ONE;
			state.m_blend_dst = GL_ONE;
			state.m_sort = OpenGLState::eSortTranslucent;
		} else if (string_equal(name + 1, "WIRE_OVERLAY")) {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST
					| RENDER_OVERRIDE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST
					| RENDER_OVERRIDE | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
		} else if (string_equal(name + 1, "FLATSHADE_OVERLAY")) {
			state.m_state = RENDER_CULLFACE | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED | RENDER_COLOURARRAY
					| RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED | RENDER_COLOURARRAY
					| RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
		} else if (string_equal(name + 1, "CLIPPER_OVERLAY")) {
			state.m_colour[0] = g_xywindow_globals.color_clipper[0];
			state.m_colour[1] = g_xywindow_globals.color_clipper[1];
			state.m_colour[2] = g_xywindow_globals.color_clipper[2];
			state.m_colour[3] = 1;
			state.m_state = RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_FILL;
			state.m_sort = OpenGLState::eSortOverlayFirst;
		} else if (string_equal(name + 1, "OVERBRIGHT")) {
			const float lightScale = 2;
			state.m_colour[0] = lightScale * 0.5f;
			state.m_colour[1] = lightScale * 0.5f;
			state.m_colour[2] = lightScale * 0.5f;
			state.m_colour[3] = 0.5;
			state.m_state = RENDER_FILL | RENDER_BLEND | RENDER_COLOURWRITE | RENDER_SCREEN;
			state.m_sort = OpenGLState::eSortOverbrighten;
			state.m_blend_src = GL_DST_COLOR;
			state.m_blend_dst = GL_SRC_COLOR;
		} else {
			// default to something recognisable.. =)
			ERROR_MESSAGE("hardcoded renderstate not found");
			state.m_colour[0] = 1;
			state.m_colour[1] = 0;
			state.m_colour[2] = 1;
			state.m_colour[3] = 1;
			state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFirst;
		}
		break;
	default:
		// construction from IShader
		m_shader = GlobalShaderSystem().getShaderForName(name);

		state.m_texture = m_shader->getTexture()->texture_number;

		state.m_state = RENDER_FILL | RENDER_TEXTURE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_LIGHTING
				| RENDER_SMOOTH;
		state.m_state |= RENDER_CULLFACE;
		if ((m_shader->getFlags() & QER_ALPHATEST) != 0) {
			state.m_state |= RENDER_ALPHATEST;
			IShader::EAlphaFunc alphafunc;
			m_shader->getAlphaFunc(&alphafunc, &state.m_alpharef);
			switch (alphafunc) {
			case IShader::eAlways:
				state.m_alphafunc = GL_ALWAYS;
			case IShader::eEqual:
				state.m_alphafunc = GL_EQUAL;
			case IShader::eLess:
				state.m_alphafunc = GL_LESS;
			case IShader::eGreater:
				state.m_alphafunc = GL_GREATER;
			case IShader::eLEqual:
				state.m_alphafunc = GL_LEQUAL;
			case IShader::eGEqual:
				state.m_alphafunc = GL_GEQUAL;
			}
		}

		reinterpret_cast<Vector3&> (state.m_colour) = m_shader->getTexture()->color;
		state.m_colour[3] = 1.0f;

		if ((m_shader->getFlags() & QER_TRANS) != 0) {
			state.m_state |= RENDER_BLEND;
			state.m_colour[3] = m_shader->getTrans();
			state.m_sort = OpenGLState::eSortTranslucent;
			BlendFunc blendFunc = m_shader->getBlendFunc();
			state.m_blend_src = convertBlendFactor(blendFunc.m_src);
			state.m_blend_dst = convertBlendFactor(blendFunc.m_dst);
			if (state.m_blend_src == GL_SRC_ALPHA || state.m_blend_dst == GL_SRC_ALPHA) {
				state.m_state |= RENDER_DEPTHWRITE;
			}
		} else {
			state.m_state |= RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFullbright;
		}

		m_shader->forEachLayer(ShaderLayerVisitor(*this));
	}
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class OpenGLStateLibraryAPI
{
		OpenGLStateMap m_stateMap;
	public:
		typedef OpenGLStateLibrary Type;
		STRING_CONSTANT(Name, "*");

		OpenGLStateLibraryAPI ()
		{
			g_openglStates = &m_stateMap;
		}
		~OpenGLStateLibraryAPI ()
		{
			g_openglStates = 0;
		}
		OpenGLStateLibrary* getTable ()
		{
			return &m_stateMap;
		}
};

typedef SingletonModule<OpenGLStateLibraryAPI> OpenGLStateLibraryModule;
typedef Static<OpenGLStateLibraryModule> StaticOpenGLStateLibraryModule;
StaticRegisterModule staticRegisterOpenGLStateLibrary(StaticOpenGLStateLibraryModule::instance());

class ShaderCacheDependencies: public GlobalShadersModuleRef,
		public GlobalTexturesModuleRef,
		public GlobalOpenGLStateLibraryModuleRef
{
	public:
		ShaderCacheDependencies () :
			GlobalShadersModuleRef("ufo")
		{
		}
};

class ShaderCacheAPI
{
		OpenGLShaderCache* m_shaderCache;
	public:
		typedef ShaderCache Type;
		STRING_CONSTANT(Name, "*");

		ShaderCacheAPI ()
		{
			m_shaderCache = new OpenGLShaderCache;
			GlobalTexturesCache().attach(*m_shaderCache);
			GlobalShaderSystem().attach(*m_shaderCache);
		}
		~ShaderCacheAPI ()
		{
			GlobalShaderSystem().detach(*m_shaderCache);
			GlobalTexturesCache().detach(*m_shaderCache);
			delete m_shaderCache;
		}
		ShaderCache* getTable ()
		{
			return m_shaderCache;
		}
};

typedef SingletonModule<ShaderCacheAPI, ShaderCacheDependencies> ShaderCacheModule;
typedef Static<ShaderCacheModule> StaticShaderCacheModule;
StaticRegisterModule staticRegisterShaderCache(StaticShaderCacheModule::instance());
