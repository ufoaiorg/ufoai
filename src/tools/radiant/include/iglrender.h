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

		float m_polygonOffset;
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
