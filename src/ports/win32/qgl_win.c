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

int   ( WINAPI * qwglChoosePixelFormat )(HDC, CONST PIXELFORMATDESCRIPTOR *);
int   ( WINAPI * qwglDescribePixelFormat) (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
int   ( WINAPI * qwglGetPixelFormat)(HDC);
BOOL  ( WINAPI * qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
BOOL  ( WINAPI * qwglSwapBuffers)(HDC);
BOOL  ( WINAPI * qwglCopyContext)(HGLRC, HGLRC, UINT);
HGLRC ( WINAPI * qwglCreateContext)(HDC);
HGLRC ( WINAPI * qwglCreateLayerContext)(HDC, int);
BOOL  ( WINAPI * qwglDeleteContext)(HGLRC);
HGLRC ( WINAPI * qwglGetCurrentContext)(VOID);
HDC   ( WINAPI * qwglGetCurrentDC)(VOID);
PROC  ( WINAPI * qwglGetProcAddress)(LPCSTR);
BOOL  ( WINAPI * qwglMakeCurrent)(HDC, HGLRC);
BOOL  ( WINAPI * qwglShareLists)(HGLRC, HGLRC);
BOOL  ( WINAPI * qwglUseFontBitmaps)(HDC, DWORD, DWORD, DWORD);
BOOL  ( WINAPI * qwglUseFontOutlines)(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT);
BOOL ( WINAPI * qwglDescribeLayerPlane)(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR);
int  ( WINAPI * qwglSetLayerPaletteEntries)(HDC, int, int, int, CONST COLORREF *);
int  ( WINAPI * qwglGetLayerPaletteEntries)(HDC, int, int, int, COLORREF *);
BOOL ( WINAPI * qwglRealizeLayerPalette)(HDC, int, BOOL);
BOOL ( WINAPI * qwglSwapLayerBuffers)(HDC, UINT);

/**
 * @brief Unloads the specified DLL then nulls out all the proc pointers.
 * @sa QGL_UnLink
 */
void QGL_Shutdown( void )
{
	if ( glw_state.hinstOpenGL )
	{
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
	qwglDescribeLayerPlane       = NULL;
	qwglGetCurrentContext        = NULL;
	qwglGetCurrentDC             = NULL;
	qwglGetLayerPaletteEntries   = NULL;
	qwglGetProcAddress           = NULL;
	qwglMakeCurrent              = NULL;
	qwglRealizeLayerPalette      = NULL;
	qwglSetLayerPaletteEntries   = NULL;
	qwglShareLists               = NULL;
	qwglSwapLayerBuffers         = NULL;
	qwglUseFontBitmaps           = NULL;
	qwglUseFontOutlines          = NULL;
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

	QGL_Init();
	return qtrue;
}

#ifdef _MSC_VER
#pragma warning (default : 4113 4133 4047 )
#endif
