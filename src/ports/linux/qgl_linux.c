/**
 * @file qgl_linux.c
 * @brief  This file implements the operating system binding of GL to QGL function pointers
 * @note When doing a port of Quake2 you must implement the following two functions:
 ** QGL_Init() - loads libraries, assigns function pointers, etc.
 ** QGL_Shutdown() - unloads libraries, NULLs function pointers
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <float.h>
#include "../../ref_gl/gl_local.h"
#include "glw_linux.h"

#include <GL/glx.h>

#include <dlfcn.h>

const char so_file[] = "/etc/ufo.conf";

#if 0
/*FX Mesa Functions */
fxMesaContext (*qfxMesaCreateContext)(GLuint win, GrScreenResolution_t, GrScreenRefresh_t, const GLint attribList[]);
fxMesaContext (*qfxMesaCreateBestContext)(GLuint win, GLint width, GLint height, const GLint attribList[]);
void (*qfxMesaDestroyContext)(fxMesaContext ctx);
void (*qfxMesaMakeCurrent)(fxMesaContext ctx);
fxMesaContext (*qfxMesaGetCurrentContext)(void);
void (*qfxMesaSwapBuffers)(void);
#endif

/*GLX Functions */
XVisualInfo * (*qglXChooseVisual)( Display *dpy, int screen, int *attribList );
GLXContext (*qglXCreateContext)( Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct );
void (*qglXDestroyContext)( Display *dpy, GLXContext ctx );
Bool (*qglXMakeCurrent)( Display *dpy, GLXDrawable drawable, GLXContext ctx);
void (*qglXCopyContext)( Display *dpy, GLXContext src, GLXContext dst, GLuint mask );
void (*qglXSwapBuffers)( Display *dpy, GLXDrawable drawable );
int (*qglXGetConfig) (Display *dpy, XVisualInfo *vis, int attrib, int *value);

/**
 * @brief Unloads the specified DLL then nulls out all the proc pointers.
 * @sa QGL_UnLink
 */
void QGL_Shutdown( void )
{
	if (glw_state.OpenGLLib) {
		dlclose ( glw_state.OpenGLLib );
		glw_state.OpenGLLib = NULL;
	}

	glw_state.OpenGLLib = NULL;
	/* general links */
	QGL_UnLink();
	/* linux specific */
	qglXChooseVisual             = NULL;
	qglXCreateContext            = NULL;
	qglXDestroyContext           = NULL;
	qglXMakeCurrent              = NULL;
	qglXCopyContext              = NULL;
	qglXSwapBuffers              = NULL;
	qglXGetConfig                = NULL;
}

/**
 * @brief
 */
void *qwglGetProcAddress(char *symbol)
{
	if (glw_state.OpenGLLib)
		return GPA ( symbol );
	return NULL;
}

/**
 * @brief This is responsible for binding our qgl function pointers to the appropriate GL stuff
 * @sa QGL_Link
 */
qboolean QGL_Init( const char *dllname )
{
	if ( ( glw_state.OpenGLLib = dlopen( dllname, RTLD_LAZY | RTLD_GLOBAL ) ) == 0 ) {
		char	fn[MAX_OSPATH];
		FILE *fp;

		/* try path in /etc/ufo.conf */
		if ((fp = fopen(so_file, "r")) == NULL) {
			strcpy(fn, "." );
		} else {
			fgets(fn, sizeof(fn), fp);
			fclose(fp);
			while (*fn && isspace(fn[strlen(fn) - 1]))
			fn[strlen(fn) - 1] = 0;
		}

		Q_strcat(fn, "/", sizeof(fn));
		Q_strcat(fn, dllname, sizeof(fn));

		if ( ( glw_state.OpenGLLib = dlopen( fn, RTLD_LAZY ) ) == 0 ) {
			ri.Con_Printf( PRINT_ALL, "%s\n", dlerror() );
			return qfalse;
		}
	}

	/* general qgl bindings */
	QGL_Link();
	/* linux specific ones */
	qglXChooseVisual             = GPA("glXChooseVisual");
	qglXCreateContext            = GPA("glXCreateContext");
	qglXDestroyContext           = GPA("glXDestroyContext");
	qglXMakeCurrent              = GPA("glXMakeCurrent");
	qglXCopyContext              = GPA("glXCopyContext");
	qglXSwapBuffers              = GPA("glXSwapBuffers");
	qglXGetConfig                = GPA("glXGetConfig");
	return qtrue;
}


