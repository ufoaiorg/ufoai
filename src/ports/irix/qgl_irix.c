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
/*
** qgl_irix.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake2 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/
#define QGL
#include "../../ref_gl/gl_local.h"

#define SIG( x ) fprintf(log_fp, x "\n")

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown (void)
{
	/* general links */
	QGL_UnLink();
}

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to
** the appropriate GL stuff.  In Windows this means doing a
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
**
*/
qboolean QGL_Init( const char *dllname )
{
	if ((glw_state.OpenGLLib = dlopen(dllname, RTLD_LAZY|RTLD_GLOBAL)) == 0) {
		char libPath[MAX_OSPATH];
		cvar_t* s_libdir = Cvar_Get("s_libdir", "", CVAR_ARCHIVE, "lib dir for graphic and sound renderer - no game libs");

		/* try path given via cvar */
		if (strlen(s_libdir->string))
			Q_strncpyz(libPath, s_libdir->string, sizeof(libPath));
		else
			strcpy(libPath, ".");

		Q_strcat(libPath, "/", sizeof(libPath));
		Q_strcat(libPath, dllname, sizeof(libPath));

		if ((glw_state.OpenGLLib = dlopen(libPath, RTLD_LAZY)) == 0) {
			ri.Con_Printf(PRINT_ALL, "%s\n", dlerror());
			return qfalse;
		}
	}

	/* general qgl bindings */
	QGL_Link();
	return qtrue;
}
