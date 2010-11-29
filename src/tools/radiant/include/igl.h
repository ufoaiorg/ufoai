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

#if !defined(INCLUDED_IGL_H)
#define INCLUDED_IGL_H

#include <cstddef>
#include <string.h>
#include <math.h>
#include "generic/constant.h"

#ifdef __APPLE__
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
#else
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#ifdef _WIN32
#define QGL_DLLEXPORT __stdcall
#include <GL/glext.h>
#else
#define QGL_DLLEXPORT
#endif

#ifndef GL_VERSION_1_4
#define GL_GENERATE_MIPMAP                0x8191
#endif

#define glActiveTexture GlobalOpenGL().m_glActiveTexture
#define glClientActiveTexture GlobalOpenGL().m_glClientActiveTexture
#define glBlendColor(r,g,b,a) if (GlobalOpenGL().ARB_imaging()) GlobalOpenGL().m_glBlendColor(r,g,b,a)

/// \brief A module which wraps a runtime-binding of the standard OpenGL functions.
/// Provides convenience functions for querying availabiliy of extensions, rendering text and error-checking.
struct OpenGLBinding
{
		INTEGER_CONSTANT(Version, 2);
		STRING_CONSTANT(Name, "qgl");

		/// \brief OpenGL version, extracted from the GL_VERSION string.
		int major_version, minor_version;

		/// \brief Is true if the global shared OpenGL context is valid.
		bool contextValid;

		OpenGLBinding () :
			contextValid(false)
		{
		}

		/// \brief Asserts that there no OpenGL errors have occurred since the last call to glGetError.
		void (*assertNoErrors) ();

		GLuint m_font;
		int m_fontHeight;

		/// \brief Renders \p string at the current raster-position of the current context.
		void drawString (const std::string& string) const
		{
			glListBase(m_font);
			glCallLists(GLsizei(string.length()), GL_UNSIGNED_BYTE, reinterpret_cast<const GLubyte*> (string.c_str()));
		}

		void perspective (GLdouble fovy, GLdouble yaspect, GLdouble zNear, GLdouble zFar)
		{
			GLdouble xmin, xmax, ymin, ymax;

			xmax = zNear * tan(fovy * 3.14159265 / 360.0);
			xmin = -xmax;

			ymin = xmin * yaspect;
			ymax = xmax * yaspect;

			glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
		}

		// GL_ARB_multitexture
		void (QGL_DLLEXPORT *m_glActiveTexture) (GLenum texture);
		void (QGL_DLLEXPORT *m_glClientActiveTexture) (GLenum texture);

		// GL_ARB_imaging
		void (QGL_DLLEXPORT *m_glBlendColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

		bool support_ARB_multitexture;
		bool ARB_multitexture ()
		{
			return support_ARB_multitexture;
		}

		// ARB_texture_compression
		bool support_ARB_texture_compression;
		bool ARB_texture_compression ()
		{
			return support_ARB_texture_compression;
		}

		// EXT_texture_compression_s3tc
		bool support_EXT_texture_compression_s3tc;
		bool EXT_texture_compression_s3tc ()
		{
			return support_EXT_texture_compression_s3tc;
		}

		// GL 1.2
		bool support_GL_1_2;
		bool GL_1_2 ()
		{
			return support_GL_1_2;
		}

		// GL 1.3
		bool support_GL_1_3;
		bool GL_1_3 ()
		{
			return support_GL_1_3;
		}

		// GL 1.4
		bool support_GL_1_4;
		bool GL_1_4 ()
		{
			return support_GL_1_4;
		}

		// GL 1.5
		bool support_GL_1_5;
		bool GL_1_5 ()
		{
			return support_GL_1_5;
		}

		// GL_ARB_vertex_program
		bool support_ARB_vertex_program;
		bool ARB_vertex_program ()
		{
			return support_ARB_vertex_program;
		}

		// GL_ARB_fragment_program
		bool support_ARB_fragment_program;
		bool ARB_fragment_program ()
		{
			return support_ARB_fragment_program;
		}

		// GL_ARB_shader_objects
		bool support_ARB_shader_objects;
		bool ARB_shader_objects ()
		{
			return support_ARB_shader_objects;
		}

		// GL_ARB_vertex_shader
		bool support_ARB_vertex_shader;
		bool ARB_vertex_shader ()
		{
			return support_ARB_vertex_shader;
		}

		// ARB_fragment_shader
		bool support_ARB_fragment_shader;
		bool ARB_fragment_shader ()
		{
			return support_ARB_fragment_shader;
		}

		// ARB_shading_language_100
		bool support_ARB_shading_language_100;
		bool ARB_shading_language_100 ()
		{
			return support_ARB_shading_language_100;
		}

		// ARB_imaging
		bool support_ARB_imaging;
		bool ARB_imaging ()
		{
			return support_ARB_imaging;
		}
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<OpenGLBinding> GlobalOpenGLModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<OpenGLBinding> GlobalOpenGLModuleRef;

inline OpenGLBinding& GlobalOpenGL ()
{
	return GlobalOpenGLModule::getTable();
}

#endif
