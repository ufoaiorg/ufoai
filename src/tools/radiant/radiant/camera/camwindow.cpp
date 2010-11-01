/**
 * @file camwindow.cpp
 * @author Leonardo Zide (leo@lokigames.com)
 * @brief Camera Window
 */

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

#include "camwindow.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "iscenegraph.h"
#include "ieventmanager.h"
#include "irender.h"
#include "igl.h"
#include "cullable.h"
#include "renderable.h"
#include "preferencesystem.h"

#include "container/array.h"
#include "scenelib.h"
#include "render.h"
#include "math/frustum.h"

#include "gtkutil/widget.h"
#include "gtkutil/button.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/xorrectangle.h"
#include "../gtkmisc.h"
#include "../selection/RadiantWindowObserver.h"
#include "../mainframe.h"
#include "../settings/preferences.h"
#include "../windowobservers.h"
#include "../ui/Icons.h"
#include "../render/RenderStatistics.h"
#include "CamRenderer.h"

#include "GlobalCamera.h"
#include "Camera.h"
#include "CameraSettings.h"

gboolean camera_keymove (gpointer data)
{
	Camera* cam = reinterpret_cast<Camera*> (data);
	cam->keyMove();
	return TRUE;
}
#include "CamWnd.h"

// =============================================================================
// CamWnd class

#include "preferencesystem.h"
#include "stringio.h"
#include "../dialog.h"

void CamWnd_Destroy ()
{
	CamWnd::releaseStates();
}
