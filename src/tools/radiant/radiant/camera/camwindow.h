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

#if !defined(INCLUDED_CAMWINDOW_H)
#define INCLUDED_CAMWINDOW_H

#include "math/Vector3.h"

#include "Camera.h"

typedef struct _GtkMenu GtkMenu;
typedef struct _GtkToolbar GtkToolbar;
void CamWnd_constructToolbar(GtkToolbar* toolbar);
void CamWnd_registerShortcuts();

void GlobalCamera_Benchmark();

void Camera_SetFarClip (bool value);

struct camwindow_globals_t {
	int m_nCubicScale;	/**< far clip distance */

	camwindow_globals_t() :
			m_nCubicScale(13) {
	}

};

extern camwindow_globals_t g_camwindow_globals;

void CamWnd_Construct();
void CamWnd_Destroy();


#endif
