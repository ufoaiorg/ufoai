/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "OpenGLRenderSystem.h"

#include "debugging/debugging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "igl.h"

#ifdef _WIN32

# include <wtypes.h>

#elif defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__) || defined (__OpenBSD__)

# include <GL/glx.h>
# include <dlfcn.h>
# include <gdk/gdkx.h>

typedef void         QGLFunction();
typedef QGLFunction* glXGetProcAddressARBProc(GLubyte const* procName);
static glXGetProcAddressARBProc* qglXGetProcAddressARB;

#else

# error "unsupported platform"

#endif

static void QGL_Shutdown (OpenGLBinding& table)
{
	g_message("Shutting down OpenGL module...");

#if defined __linux__ || defined __FreeBSD__ || defined __APPLE__ || defined __OpenBSD__
	qglXGetProcAddressARB = 0;
#elif !defined _WIN32
#error "unsupported platform"
#endif

	g_message("Done.\n");
}

static char const* qgluErrorString (GLenum const errCode)
{
	switch (errCode) {
	case GL_NO_ERROR:          return "GL_NO_ERROR - no error";
	case GL_INVALID_ENUM:      return "GL_INVALID_ENUM - An unacceptable value is specified for an enumerated argument.";
	case GL_INVALID_VALUE:     return "GL_INVALID_VALUE - A numeric argument is out of range.";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION - The specified operation is not allowed in the current state.";
	case GL_STACK_OVERFLOW:    return "GL_STACK_OVERFLOW - Function would cause a stack overflow.";
	case GL_STACK_UNDERFLOW:   return "GL_STACK_UNDERFLOW - Function would cause a stack underflow.";
	case GL_OUT_OF_MEMORY:     return "GL_OUT_OF_MEMORY - There is not enough memory left to execute the function.";
	default:                   return "Unknown error";
	}
}

bool QGL_ExtensionSupported (const char* extension)
{
	// Extension names should not have spaces.
	if (strchr(extension, ' ') || *extension == '\0')
		return false;

	char const* const extensions = reinterpret_cast<char const*>(glGetString(GL_EXTENSIONS));
#ifndef __APPLE__
	if (!extensions)
		return false;
#endif

	// It takes a bit of care to be fool-proof about parsing the
	// OpenGL extensions string. Don't be fooled by sub-strings, etc.
	for (char const* start = extensions;;) {
		char const* const where = strstr(start, extension);
		if (!where)
			break;

		char const* const terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;

		start = terminator;
	}

	return false;
}

template<typename Func>
bool QGL_constructExtensionFunc (Func& func, const char* symbol)
{
	func = reinterpret_cast<Func>(
#if defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__) || defined(__OpenBSD__)
		qglXGetProcAddressARB ? qglXGetProcAddressARB(reinterpret_cast<GLubyte const*>(symbol)) : 0
#elif defined(_WIN32)
		wglGetProcAddress(symbol)
#else
#error "unsupported platform"
#endif
	);
	return func != 0;
}

static int QGL_Init (OpenGLBinding& table)
{
#if defined __linux__ || defined __FreeBSD__ || defined __APPLE__ || defined __OpenBSD__
	qglXGetProcAddressARB = reinterpret_cast<glXGetProcAddressARBProc*>(reinterpret_cast<uintptr_t>(dlsym(RTLD_DEFAULT, "glXGetProcAddressARB")));
	if (!glXQueryExtension(GDK_DISPLAY(), 0, 0))
		return 0;
#elif !defined _WIN32
#error "unsupported platform"
#endif

	return 1;
}

// requires a valid gl context
static void QGL_InitVersion (OpenGLBinding& table)
{
	char const* const version = reinterpret_cast<char const*>(glGetString(GL_VERSION));
	sscanf(version, "%d.%d", &table.major_version, &table.minor_version);
}

inline void extension_not_implemented (const char* extension)
{
	g_warning("OpenGL driver reports support for %s but does not implement it\n", extension);
}

static int g_maxTextureAnisotropy;

int QGL_maxTextureAnisotropy ()
{
	return g_maxTextureAnisotropy;
}

void QGL_sharedContextCreated (OpenGLBinding& table)
{
	QGL_InitVersion(table);

	if (QGL_ExtensionSupported("GL_ARB_multitexture")) {
		table.support_ARB_multitexture = QGL_constructExtensionFunc(table.m_glActiveTexture, "glActiveTextureARB")
				&& QGL_constructExtensionFunc(table.m_glClientActiveTexture, "glClientActiveTextureARB");

		if (!table.support_ARB_multitexture) {
			extension_not_implemented("GL_ARB_multitexture");
		}
	} else {
		table.support_ARB_multitexture = false;
	}

	if (QGL_ExtensionSupported("GL_ARB_texture_compression")) {
		table.support_ARB_texture_compression = true;
	} else {
		table.support_ARB_texture_compression = false;
	}

	table.support_EXT_texture_compression_s3tc = QGL_ExtensionSupported("GL_EXT_texture_compression_s3tc");

	// GL 1.2
	if (table.major_version > 1 || table.minor_version >= 2) {
		table.support_GL_1_2 = true;
	} else {
		table.support_GL_1_2 = false;
	}

	// GL 1.3
	if (table.major_version > 1 || table.minor_version >= 3) {
		table.support_GL_1_3 = QGL_constructExtensionFunc(table.m_glActiveTexture, "glActiveTexture")
				&& QGL_constructExtensionFunc(table.m_glClientActiveTexture, "glClientActiveTexture");

		if (!table.support_GL_1_3) {
			extension_not_implemented("GL_VERSION_1_3");
		}
	} else {
		table.support_GL_1_3 = false;
	}

	// GL 1.4
	if (table.major_version > 1 || table.minor_version >= 4) {
		table.support_GL_1_4 = true;
	} else {
		table.support_GL_1_4 = false;
	}

	// GL 1.5
	if (table.major_version > 1 || table.minor_version >= 5) {
		table.support_GL_1_5 = true;
	} else {
		table.support_GL_1_5 = false;
	}

	if (QGL_ExtensionSupported("GL_ARB_vertex_program")) {
		table.support_ARB_vertex_program = true;
	} else {
		table.support_ARB_vertex_program = false;
	}

	table.support_ARB_fragment_program = QGL_ExtensionSupported("GL_ARB_fragment_program");

	if (QGL_ExtensionSupported("GL_ARB_shader_objects")) {
		table.support_ARB_shader_objects = true;
	} else {
		table.support_ARB_shader_objects = false;
	}

	if (QGL_ExtensionSupported("GL_ARB_vertex_shader")) {
		table.support_ARB_vertex_shader = true;
	} else {
		table.support_ARB_vertex_shader = false;
	}

	table.support_ARB_fragment_shader = QGL_ExtensionSupported("GL_ARB_fragment_shader");
	table.support_ARB_shading_language_100 = QGL_ExtensionSupported("GL_ARB_shading_language_100");

	table.support_ARB_imaging = QGL_constructExtensionFunc(table.m_glBlendColor, "glBlendColor");

	if (QGL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_maxTextureAnisotropy);
		g_message("Anisotropic filtering possible (max %ix)\n", g_maxTextureAnisotropy);
	} else {
		g_message("No Anisotropic filtering available\n");
		g_maxTextureAnisotropy = 0;
	}
}

static void QGL_assertNoErrors ()
{
	while (GLenum const error = glGetError()) {
		ERROR_MESSAGE("OpenGL error: " << qgluErrorString(error));
	}
}

class QglAPI
{
		OpenGLBinding m_qgl;
	public:
		typedef OpenGLBinding Type;
		STRING_CONSTANT(Name, "*");

		QglAPI ()
		{
			QGL_Init(m_qgl);

			m_qgl.assertNoErrors = &QGL_assertNoErrors;
		}
		~QglAPI ()
		{
			QGL_Shutdown(m_qgl);
		}
		OpenGLBinding* getTable ()
		{
			return &m_qgl;
		}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<QglAPI> QglModule;
typedef Static<QglModule> StaticQglModule;
StaticRegisterModule staticRegisterQgl(StaticQglModule::instance());
