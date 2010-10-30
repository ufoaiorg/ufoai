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
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "../brush/TexDef.h"
#include "ientity.h"
#include "iregistry.h"
#include "igl.h"
#include "ibrush.h"
#include "iundo.h"
#include "iimage.h"
#include "ifilesystem.h"
#include "ifiletypes.h"
#include "os/path.h"
#include "os/file.h"
#include "../image.h"
#include "gtkutil/messagebox.h"

#include "generic/callback.h"
#include "string/string.h"
#include "stream/stringstream.h"

#include "scenelib.h"
#include "eclasslib.h"
#include "../renderer.h"
#include "moduleobserver.h"

#include "iclipper.h"
#include "ieventmanager.h"

#include "gtkutil/menu.h"
#include "gtkutil/container.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/filechooser.h"
#include "../gtkmisc.h"
#include "../select.h"
#include "../brush/csg/csg.h"
#include "../brush/brushmanip.h"
#include "../entity.h"
#include "../camera/camwindow.h"
#include "../mainframe.h"
#include "../settings/preferences.h"
#include "../commands.h"
#include "grid.h"
#include "../sidebar/sidebar.h"
#include "../windowobservers.h"
#include "../ui/ortho/OrthoContextMenu.h"
#include "XYRenderer.h"
#include "../selection/SelectionBox.h"
#include "../camera/GlobalCamera.h"
#include "../plugin.h"
#include "XYWnd.h"
#include "GlobalXYWnd.h"

/*
 ============================================================================
 DRAWING
 ============================================================================
 */

/* This function determines the point currently being "looked" at, it is used for toggling the ortho views
 * If something is selected the center of the selection is taken as new origin, otherwise the camera
 * position is considered to be the new origin of the toggled orthoview.
*/
inline Vector3 getFocusPosition() {
	Vector3 position(0,0,0);

	if (GlobalSelectionSystem().countSelected() != 0) {
		Select_GetMid(position);
	}
	else {
		position = g_pParentWnd->GetCamWnd()->getCameraOrigin();
	}

	return position;
}

void XY_Focus() {
	GlobalXYWnd().positionView(getFocusPosition());
}

void XY_Top() {
	GlobalXYWnd().setActiveViewType(XY);
	GlobalXYWnd().positionView(getFocusPosition());
}

void XY_Side() {
	GlobalXYWnd().setActiveViewType(XZ);
	GlobalXYWnd().positionView(getFocusPosition());
}

void XY_Front() {
	GlobalXYWnd().setActiveViewType(YZ);
	GlobalXYWnd().positionView(getFocusPosition());
}

void toggleActiveOrthoView() {
	XYWnd* xywnd = GlobalXYWnd().getActiveXY();

	if (xywnd != NULL) {
		if (xywnd->getViewType() == XY) {
			xywnd->setViewType(XZ);
		}
		else if (xywnd->getViewType() == XZ) {
			xywnd->setViewType(YZ);
		}
		else {
			xywnd->setViewType(XY);
		}
	}
	xywnd->positionView(getFocusPosition());
}

void XY_CenterViews ()
{
	// Re-position all available views
	GlobalXYWnd().positionAllViews(getFocusPosition());
}

void XYShow_registerCommands ()
{
	GlobalEventManager().addRegistryToggle("ShowAngles", RKEY_SHOW_ENTITY_ANGLES);
	GlobalEventManager().addRegistryToggle("ShowNames", RKEY_SHOW_ENTITY_NAMES);
	GlobalEventManager().addRegistryToggle("ShowBlocks", RKEY_SHOW_BLOCKS);
	GlobalEventManager().addRegistryToggle("ShowCoordinates", RKEY_SHOW_COORDINATES);
	GlobalEventManager().addRegistryToggle("ShowWindowOutline", RKEY_SHOW_OUTLINE);
	GlobalEventManager().addRegistryToggle("ShowAxes", RKEY_SHOW_AXES);
	GlobalEventManager().addRegistryToggle("ShowWorkzone", RKEY_SHOW_WORKZONE);
}

#include "preferencesystem.h"
#include "stringio.h"

void ToggleShown_importBool (ToggleShown& self, bool value)
{
	self.set(value);
}
typedef ReferenceCaller1<ToggleShown, bool, ToggleShown_importBool> ToggleShownImportBoolCaller;
void ToggleShown_exportBool (const ToggleShown& self, const BoolImportCallback& importer)
{
	importer(self.active());
}
typedef ConstReferenceCaller1<ToggleShown, const BoolImportCallback&, ToggleShown_exportBool>
		ToggleShownExportBoolCaller;

void XYWindow_Construct ()
{
	// eRegular
	GlobalEventManager().addCommand("NextView", FreeCaller<toggleActiveOrthoView> ());
	GlobalEventManager().addCommand("ViewTop", FreeCaller<XY_Top> ());
	GlobalEventManager().addCommand("ViewSide", FreeCaller<XY_Side> ());
	GlobalEventManager().addCommand("ViewFront", FreeCaller<XY_Front> ());

	// general commands
	GlobalEventManager().addCommand("ToggleCrosshairs", MemberCaller<XYWndManager, &XYWndManager::toggleCrossHairs> (
			GlobalXYWnd()));
	GlobalEventManager().addCommand("ToggleGrid", MemberCaller<XYWndManager, &XYWndManager::toggleGrid> (
			GlobalXYWnd()));

	GlobalEventManager().addCommand("ZoomIn", MemberCaller<XYWndManager, &XYWndManager::zoomIn> (GlobalXYWnd()));
	GlobalEventManager().addCommand("ZoomOut", MemberCaller<XYWndManager, &XYWndManager::zoomOut> (GlobalXYWnd()));
	GlobalEventManager().addCommand("Zoom100", MemberCaller<XYWndManager, &XYWndManager::zoomOut> (GlobalXYWnd()));
	GlobalEventManager().addCommand("CenterXYViews", FreeCaller<XY_CenterViews> ());

	XYWnd::captureStates();

	/* add screenshot filetype */
	GlobalFiletypesModule::getTable().addType("bmp", "screenshot bitmap", filetype_t("bitmap", "*.bmp"));
}

void XYWindow_Destroy ()
{
	XYWnd::releaseStates();
}
