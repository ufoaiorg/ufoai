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

#include <windows.h>
#include <float.h>
#include "../../ref_gl/gl_local.h"
#include "glw_win.h"

static int ( WINAPI * qwglChoosePixelFormat )(HDC, CONST PIXELFORMATDESCRIPTOR *);
static int ( WINAPI * qwglDescribePixelFormat) (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
static BOOL ( WINAPI * qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
static BOOL ( WINAPI * qwglSwapBuffers)(HDC);
static HGLRC ( WINAPI * qwglCreateContext)(HDC);
static BOOL ( WINAPI * qwglDeleteContext)(HGLRC);
static PROC ( WINAPI * qwglGetProcAddress)(LPCSTR);
static BOOL ( WINAPI * qwglMakeCurrent)(HDC, HGLRC);
static BOOL (WINAPI * qwglSwapIntervalEXT) (int interval);

/* not used */
static int ( WINAPI * qwglGetPixelFormat)(HDC);
static BOOL ( WINAPI * qwglCopyContext)(HGLRC, HGLRC, UINT);
static HGLRC ( WINAPI * qwglCreateLayerContext)(HDC, int);
static HGLRC ( WINAPI * qwglGetCurrentContext)(VOID);
static HDC ( WINAPI * qwglGetCurrentDC)(VOID);
static BOOL ( WINAPI * qwglShareLists)(HGLRC, HGLRC);
static int ( WINAPI * qwglSetLayerPaletteEntries)(HDC, int, int, int, CONST COLORREF *);
static int ( WINAPI * qwglGetLayerPaletteEntries)(HDC, int, int, int, COLORREF *);
static BOOL ( WINAPI * qwglRealizeLayerPalette)(HDC, int, BOOL);
static BOOL (WINAPI * qwglGetDeviceGammaRampEXT) (unsigned char *pRed, unsigned char *pGreen, unsigned char *pBlue);
static BOOL (WINAPI * qwglSetDeviceGammaRampEXT) (const unsigned char *pRed, const unsigned char *pGreen, const unsigned char *pBlue);

/**
 * @brief Unloads the specified DLL then nulls out all the proc pointers.
 * @sa QGL_UnLink
 */
void QGL_Shutdown( void )
{
	if ( glw_state.hinstOpenGL ) {
		FreeLibrary( glw_state.hinstOpenGL );
		glw_state.hinstOpenGL = NULL;
	}

	glw_state.hinstOpenGL = NULL;

	/* general pointers */
	QGL_UnLink();
	/* windows specific */
	qwglCopyContext              = NULL;
	qwglCreateContext            = NULL;
	qwglCreateLayerContext       = NULL;
	qwglDeleteContext            = NULL;
	qwglGetCurrentContext        = NULL;
	qwglGetCurrentDC             = NULL;
	qwglGetLayerPaletteEntries   = NULL;
	qwglGetProcAddress           = NULL;
	qwglMakeCurrent              = NULL;
	qwglRealizeLayerPalette      = NULL;
	qwglSetLayerPaletteEntries   = NULL;
	qwglShareLists               = NULL;
	qwglChoosePixelFormat        = NULL;
	qwglDescribePixelFormat      = NULL;
	qwglGetPixelFormat           = NULL;
	qwglSetPixelFormat           = NULL;
	qwglSwapBuffers              = NULL;
	qwglSwapIntervalEXT          = NULL;
	qwglGetDeviceGammaRampEXT    = NULL;
	qwglSetDeviceGammaRampEXT    = NULL;
}


/**
 * @brief This is responsible for binding our qgl function pointers to the appropriate GL stuff.
 * @sa QGL_Init
 */
qboolean QGL_Init( const char *dllname )
{
	if ( ( glw_state.hinstOpenGL = LoadLibrary( dllname ) ) == 0 ) {
		char *buf = NULL;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		ri.Con_Printf( PRINT_ALL, "%s\n", buf );
		return qfalse;
	}

	/* general qgl bindings */
	QGL_Link();
	/* windows specific ones */
	qwglCopyContext              = GPA("wglCopyContext");
	qwglCreateContext            = GPA("wglCreateContext");
	qwglCreateLayerContext       = GPA("wglCreateLayerContext");
	qwglDeleteContext            = GPA("wglDeleteContext");
	qwglGetCurrentContext        = GPA("wglGetCurrentContext");
	qwglGetCurrentDC             = GPA("wglGetCurrentDC");
	qwglGetLayerPaletteEntries   = GPA("wglGetLayerPaletteEntries");
	qwglGetProcAddress           = GPA("wglGetProcAddress");
	qwglMakeCurrent              = GPA("wglMakeCurrent");
	qwglRealizeLayerPalette      = GPA("wglRealizeLayerPalette");
	qwglSetLayerPaletteEntries   = GPA("wglSetLayerPaletteEntries");
	qwglShareLists               = GPA("wglShareLists");
	qwglChoosePixelFormat        = GPA("wglChoosePixelFormat");
	qwglDescribePixelFormat      = GPA("wglDescribePixelFormat");
	qwglGetPixelFormat           = GPA("wglGetPixelFormat");
	qwglSetPixelFormat           = GPA("wglSetPixelFormat");
	qwglSwapBuffers              = GPA("wglSwapBuffers");
	qwglSwapIntervalEXT          = NULL;

	return qtrue;
}

#ifdef _MSC_VER
#pragma warning (default : 4113 4133 4047 )
#endif
