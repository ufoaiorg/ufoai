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

#if !defined(INCLUDED_IGLRENDER_H)
#define INCLUDED_IGLRENDER_H

#include "igl.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

//! A collection of opengl state information.
class OpenGLState {
	public:
		enum ESort {
			eSortFirst = 0,
			eSortOpaque = 1,
			eSortMultiFirst = 2,
			eSortMultiLast = 1023,
			eSortOverbrighten = 1024,
			eSortFullbright = 1025,
			eSortHighlight = 1026,
			eSortTranslucent = 1027,
			eSortOverlayFirst = 1028,
			eSortOverlayLast = 2047,
			eSortControlFirst = 2048,
			eSortControlLast = 3071,
			eSortGUI0 = 3072,
			eSortGUI1 = 3073,
			eSortLast = 4096
		};

		int m_polygonOffset;
		unsigned int m_state;
		std::size_t m_sort;
		GLint m_texture;
		GLint m_texture1;
		GLint m_texture2;
		GLint m_texture3;
		GLint m_texture4;
		GLint m_texture5;
		GLint m_texture6;
		GLint m_texture7;
		Vector4 m_colour;
		GLenum m_blend_src, m_blend_dst;
		GLenum m_depthfunc;
		GLenum m_alphafunc;
		GLfloat m_alpharef;
		GLfloat m_linewidth;
		GLfloat m_pointsize;
		GLint m_linestipple_factor;
		GLushort m_linestipple_pattern;

		OpenGLState() {
		}

		inline
		void setTextureState(GLint& current, const GLint& texture, GLenum textureUnit) {
			if (texture != current) {
				glActiveTexture(textureUnit);
				glClientActiveTexture(textureUnit);
				glBindTexture(GL_TEXTURE_2D, texture);

				current = texture;
			}
		}

		inline
		void setTextureState(GLint& current, const GLint& texture) {
			if (texture != current) {
				glBindTexture(GL_TEXTURE_2D, texture);
				current = texture;
			}
		}

		inline
		void setState(unsigned int state, unsigned int delta, unsigned int flag, GLenum glflag) {
			if (delta & state & flag) {
				glEnable(glflag);
			} else if (delta & ~state & flag) {
				glDisable(glflag);
			}
		}

		void constructDefault() {
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
			m_polygonOffset = 0;
		}

		void apply(OpenGLState& current, unsigned int globalstate) {
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

			if (state & RENDER_ALPHATEST && (m_alphafunc != current.m_alphafunc || m_alpharef
					!= current.m_alpharef)) {
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

			if (state & RENDER_BLEND && (m_blend_src != current.m_blend_src || m_blend_dst
					!= current.m_blend_dst)) {
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

			// Apply polygon offset
			if (m_polygonOffset != current.m_polygonOffset) {
				current.m_polygonOffset = m_polygonOffset;

				if (current.m_polygonOffset > 0.0f) {
					glEnable(GL_POLYGON_OFFSET_FILL);
					glPolygonOffset(-1, -1 * m_polygonOffset);
				} else {
					glDisable(GL_POLYGON_OFFSET_FILL);
				}
			}

			current.m_state = state;
		}
};

class OpenGLStateLibrary {
	public:
		virtual ~OpenGLStateLibrary() {
		}

		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "openglshaderlibrary");

		virtual void getDefaultState(OpenGLState& state) const = 0;

		virtual void insert(const std::string& name, const OpenGLState& state) = 0;
		virtual void erase(const std::string& name) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<OpenGLStateLibrary> GlobalOpenGLStateLibraryModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<OpenGLStateLibrary> GlobalOpenGLStateLibraryModuleRef;

inline OpenGLStateLibrary& GlobalOpenGLStateLibrary() {
	return GlobalOpenGLStateLibraryModule::getTable();
}

#endif
