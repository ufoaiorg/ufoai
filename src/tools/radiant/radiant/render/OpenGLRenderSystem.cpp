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
PROC ( WINAPI * qwglGetProcAddress)(LPCSTR);

#elif defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)

# include <GL/glx.h>
# include <dlfcn.h>
# include <gdk/gdkx.h>
Bool (*qglXQueryExtension) (Display *dpy, int *errorb, int *event);
void* (*qglXGetProcAddressARB) (const GLubyte *procName);
typedef void* (*glXGetProcAddressARBProc) (const GLubyte *procName);

#else

# error "unsupported platform"

#endif

void QGL_Shutdown (OpenGLBinding& table)
{
	g_message("Shutting down OpenGL module...");

#ifdef _WIN32
	qwglGetProcAddress = 0;
#elif defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)
	qglXQueryExtension = 0;
	qglXGetProcAddressARB = 0;
#else
#error "unsupported platform"
#endif

	g_message("Done.\n");
}

typedef struct glu_error_struct
{
		GLenum errnum;
		const char *errstr;
} GLU_ERROR_STRUCT;

GLU_ERROR_STRUCT glu_errlist[] = { { GL_NO_ERROR, "GL_NO_ERROR - no error" }, { GL_INVALID_ENUM,
		"GL_INVALID_ENUM - An unacceptable value is specified for an enumerated argument." }, { GL_INVALID_VALUE,
		"GL_INVALID_VALUE - A numeric argument is out of range." }, { GL_INVALID_OPERATION,
		"GL_INVALID_OPERATION - The specified operation is not allowed in the current state." }, { GL_STACK_OVERFLOW,
		"GL_STACK_OVERFLOW - Function would cause a stack overflow." }, { GL_STACK_UNDERFLOW,
		"GL_STACK_UNDERFLOW - Function would cause a stack underflow." }, { GL_OUT_OF_MEMORY,
		"GL_OUT_OF_MEMORY - There is not enough memory left to execute the function." }, { 0, 0 } };

const GLubyte* qgluErrorString (GLenum errCode)
{
	int search = 0;
	for (search = 0; glu_errlist[search].errstr; search++) {
		if (errCode == glu_errlist[search].errnum)
			return (const GLubyte *) glu_errlist[search].errstr;
	} //end for
	return (const GLubyte *) "Unknown error";
}

void glInvalidFunction ()
{
	ERROR_MESSAGE("calling an invalid OpenGL function");
}

bool QGL_ExtensionSupported (const char* extension)
{
	const GLubyte *extensions = 0;
	const GLubyte *start;
	GLubyte *where, *terminator;

	// Extension names should not have spaces.
	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return false;

	extensions = glGetString(GL_EXTENSIONS);
#ifndef __APPLE__
	if (!extensions)
		return false;
#endif

	// It takes a bit of care to be fool-proof about parsing the
	// OpenGL extensions string. Don't be fooled by sub-strings, etc.
	for (start = extensions;;) {
		where = (GLubyte *) strstr((const char *) start, extension);
		if (!where)
			break;

		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;

		start = terminator;
	}

	return false;
}

typedef int (QGL_DLLEXPORT *QGLFunctionPointer) ();

QGLFunctionPointer QGL_getExtensionFunc (const char* symbol)
{
#if defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)
	if (qglXGetProcAddressARB == 0) {
		return reinterpret_cast<QGLFunctionPointer> (glInvalidFunction);
	} else {
		return (QGLFunctionPointer) qglXGetProcAddressARB(reinterpret_cast<const GLubyte*> (symbol));
	}
#elif defined(_WIN32)
	ASSERT_NOTNULL(qwglGetProcAddress);
	return qwglGetProcAddress(symbol);
#else
#error "unsupported platform"
#endif
}

template<typename Func>
bool QGL_constructExtensionFunc (Func& func, const char* symbol)
{
	func = reinterpret_cast<Func> (QGL_getExtensionFunc(symbol));
	return func != 0;
}

template<typename Func>
void QGL_invalidateExtensionFunc (Func& func)
{
	func = reinterpret_cast<Func> (glInvalidFunction);
}

void QGL_clear (OpenGLBinding& table)
{
}

int QGL_Init (OpenGLBinding& table)
{
	QGL_clear(table);

#ifdef _WIN32
	qwglGetProcAddress = wglGetProcAddress;
#elif defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)
	qglXQueryExtension = glXQueryExtension;
	qglXGetProcAddressARB = (glXGetProcAddressARBProc) dlsym(RTLD_DEFAULT, "glXGetProcAddressARB");
	if ((qglXQueryExtension == 0) || (qglXQueryExtension(GDK_DISPLAY(), 0, 0) != True))
		return 0;
#else
#error "unsupported platform"
#endif

	return 1;
}

int g_qglMajorVersion = 0;
int g_qglMinorVersion = 0;

// requires a valid gl context
void QGL_InitVersion ()
{
	const std::size_t versionSize = 256;
	char version[versionSize];
	strncpy(version, reinterpret_cast<const char*> (glGetString(GL_VERSION)), versionSize - 1);
	version[versionSize - 1] = '\0';
	char* firstDot = strchr(version, '.');
	ASSERT_NOTNULL(firstDot);
	*firstDot = '\0';
	g_qglMajorVersion = atoi(version);
	char* secondDot = strchr(firstDot + 1, '.');
	if (secondDot != 0) {
		*secondDot = '\0';
	}
	g_qglMinorVersion = atoi(firstDot + 1);
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
	QGL_InitVersion();

	table.major_version = g_qglMajorVersion;
	table.minor_version = g_qglMinorVersion;

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

void QGL_sharedContextDestroyed (OpenGLBinding& table)
{
	QGL_clear(table);
}

void QGL_assertNoErrors ()
{
	GLenum error = glGetError();
	while (error != GL_NO_ERROR) {
		const char* errorString = reinterpret_cast<const char*> (qgluErrorString(error));
		if (error == GL_OUT_OF_MEMORY) {
			ERROR_MESSAGE("OpenGL out of memory error: " << errorString);
		} else {
			ERROR_MESSAGE("OpenGL error: " << errorString);
		}
		error = glGetError();
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
