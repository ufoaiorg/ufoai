/**
 * @file xywindow.cpp
 * @brief XY Window rendering and input code
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

#include "xywindow.h"

#include "ieventmanager.h"

#include "generic/callback.h"
#include "gtkutil/widget.h"
#include "../commands.h"

#include "XYWnd.h"
#include "GlobalXYWnd.h"

/*
 ============================================================================
 DRAWING
 ============================================================================
 */

void XY_Focus() {
	GlobalXYWnd().positionView(GlobalXYWnd().getFocusPosition());
}

void XY_Top() {
	GlobalXYWnd().setActiveViewType(XY);
	GlobalXYWnd().positionView(GlobalXYWnd().getFocusPosition());
}

void XY_Side() {
	GlobalXYWnd().setActiveViewType(XZ);
	GlobalXYWnd().positionView(GlobalXYWnd().getFocusPosition());
}

void XY_Front() {
	GlobalXYWnd().setActiveViewType(YZ);
	GlobalXYWnd().positionView(GlobalXYWnd().getFocusPosition());
}

void XY_CenterViews ()
{
	// Re-position all available views
	GlobalXYWnd().positionAllViews(GlobalXYWnd().getFocusPosition());
}

#include "preferencesystem.h"
#include "stringio.h"

void XYWindow_Construct ()
{
	// eRegular
	GlobalEventManager().addCommand("NextView", MemberCaller<XYWndManager, &XYWndManager::toggleActiveView> (
			GlobalXYWnd()));
	GlobalEventManager().addCommand("ViewTop", FreeCaller<XY_Top> ());
	GlobalEventManager().addCommand("ViewSide", FreeCaller<XY_Side> ());
	GlobalEventManager().addCommand("ViewFront", FreeCaller<XY_Front> ());

	GlobalEventManager().addCommand("ZoomIn", MemberCaller<XYWndManager, &XYWndManager::zoomIn> (GlobalXYWnd()));
	GlobalEventManager().addCommand("ZoomOut", MemberCaller<XYWndManager, &XYWndManager::zoomOut> (GlobalXYWnd()));
	GlobalEventManager().addCommand("Zoom100", MemberCaller<XYWndManager, &XYWndManager::zoomOut> (GlobalXYWnd()));
	GlobalEventManager().addCommand("CenterXYViews", FreeCaller<XY_CenterViews> ());

	XYWnd::captureStates();
}

void XYWindow_Destroy ()
{
	XYWnd::releaseStates();
}
