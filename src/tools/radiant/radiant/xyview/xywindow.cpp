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
#include "../clipper/ClipPoint.h"
#include "../clipper/Clipper.h"

xywindow_globals_private_t g_xywindow_globals_private;

// =============================================================================
// variables

void WXY_BackgroundSelect (void)
{
	bool brushesSelected = map::countSelectedBrushes() != 0;
	if (!brushesSelected) {
		gtk_MessageBox(0, _("You have to select some brushes to get the bounding box for.\n"), _("No selection"),
				eMB_OK, eMB_ICONERROR);
		return;
	}

	gtkutil::FileChooser fileChooser(GTK_WIDGET(GlobalRadiant().getMainWindow()), _("Background Image"), true, false);
	std::string filename = fileChooser.display();
	XYWnd *xy = GlobalXYWnd().getActiveXY();
	if (xy) {
		xy->disableBackground();
		if (!filename.empty())
			xy->loadBackgroundImage(filename);
	}
}

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

void ShowNamesToggle ()
{
	GlobalEntityCreator().setShowNames(!GlobalEntityCreator().getShowNames());
	GlobalXYWnd().updateAllViews();
}
typedef FreeCaller<ShowNamesToggle> ShowNamesToggleCaller;
void ShowNamesExport (const BoolImportCallback& importer)
{
	importer(GlobalEntityCreator().getShowNames());
}
typedef FreeCaller1<const BoolImportCallback&, ShowNamesExport> ShowNamesExportCaller;

void ShowAnglesToggle ()
{
	GlobalEntityCreator().setShowAngles(!GlobalEntityCreator().getShowAngles());
	GlobalXYWnd().updateAllViews();
}
typedef FreeCaller<ShowAnglesToggle> ShowAnglesToggleCaller;
void ShowAnglesExport (const BoolImportCallback& importer)
{
	importer(GlobalEntityCreator().getShowAngles());
}
typedef FreeCaller1<const BoolImportCallback&, ShowAnglesExport> ShowAnglesExportCaller;

void ShowBlocksToggle ()
{
	g_xywindow_globals_private.show_blocks ^= 1;
	GlobalXYWnd().updateAllViews();
}
typedef FreeCaller<ShowBlocksToggle> ShowBlocksToggleCaller;
void ShowBlocksExport (const BoolImportCallback& importer)
{
	importer(g_xywindow_globals_private.show_blocks);
}
typedef FreeCaller1<const BoolImportCallback&, ShowBlocksExport> ShowBlocksExportCaller;

void ShowCoordinatesToggle ()
{
	g_xywindow_globals_private.show_coordinates ^= 1;
	XY_UpdateAllWindows();
}
typedef FreeCaller<ShowCoordinatesToggle> ShowCoordinatesToggleCaller;
void ShowCoordinatesExport (const BoolImportCallback& importer)
{
	importer(g_xywindow_globals_private.show_coordinates);
}
typedef FreeCaller1<const BoolImportCallback&, ShowCoordinatesExport> ShowCoordinatesExportCaller;

void ShowOutlineToggle ()
{
	g_xywindow_globals_private.show_outline ^= 1;
	GlobalXYWnd().updateAllViews();
}
typedef FreeCaller<ShowOutlineToggle> ShowOutlineToggleCaller;
void ShowOutlineExport (const BoolImportCallback& importer)
{
	importer(g_xywindow_globals_private.show_outline);
}
typedef FreeCaller1<const BoolImportCallback&, ShowOutlineExport> ShowOutlineExportCaller;

void ShowAxesToggle ()
{
	g_xywindow_globals_private.show_axis ^= 1;
	GlobalXYWnd().updateAllViews();
}
typedef FreeCaller<ShowAxesToggle> ShowAxesToggleCaller;
void ShowAxesExport (const BoolImportCallback& importer)
{
	importer(g_xywindow_globals_private.show_axis);
}
typedef FreeCaller1<const BoolImportCallback&, ShowAxesExport> ShowAxesExportCaller;

void ShowWorkzoneToggle ()
{
	g_xywindow_globals_private.d_show_work ^= 1;
	GlobalXYWnd().updateAllViews();
}
typedef FreeCaller<ShowWorkzoneToggle> ShowWorkzoneToggleCaller;
void ShowWorkzoneExport (const BoolImportCallback& importer)
{
	importer(g_xywindow_globals_private.d_show_work);
}
typedef FreeCaller1<const BoolImportCallback&, ShowWorkzoneExport> ShowWorkzoneExportCaller;

ShowNamesExportCaller g_show_names_caller;
BoolExportCallback g_show_names_callback(g_show_names_caller);
ToggleItem g_show_names(g_show_names_callback);

ShowAnglesExportCaller g_show_angles_caller;
BoolExportCallback g_show_angles_callback(g_show_angles_caller);
ToggleItem g_show_angles(g_show_angles_callback);

ShowBlocksExportCaller g_show_blocks_caller;
BoolExportCallback g_show_blocks_callback(g_show_blocks_caller);
ToggleItem g_show_blocks(g_show_blocks_callback);

ShowCoordinatesExportCaller g_show_coordinates_caller;
BoolExportCallback g_show_coordinates_callback(g_show_coordinates_caller);
ToggleItem g_show_coordinates(g_show_coordinates_callback);

ShowOutlineExportCaller g_show_outline_caller;
BoolExportCallback g_show_outline_callback(g_show_outline_caller);
ToggleItem g_show_outline(g_show_outline_callback);

ShowAxesExportCaller g_show_axes_caller;
BoolExportCallback g_show_axes_callback(g_show_axes_caller);
ToggleItem g_show_axes(g_show_axes_callback);

ShowWorkzoneExportCaller g_show_workzone_caller;
BoolExportCallback g_show_workzone_callback(g_show_workzone_caller);
ToggleItem g_show_workzone(g_show_workzone_callback);

void XYShow_registerCommands ()
{
	GlobalToggles_insert("ShowAngles", ShowAnglesToggleCaller(), ToggleItem::AddCallbackCaller(g_show_angles));
	GlobalToggles_insert("ShowNames", ShowNamesToggleCaller(), ToggleItem::AddCallbackCaller(g_show_names));
	GlobalToggles_insert("ShowBlocks", ShowBlocksToggleCaller(), ToggleItem::AddCallbackCaller(g_show_blocks));
	GlobalToggles_insert("ShowCoordinates", ShowCoordinatesToggleCaller(), ToggleItem::AddCallbackCaller(
			g_show_coordinates));
	GlobalToggles_insert("ShowWindowOutline", ShowOutlineToggleCaller(), ToggleItem::AddCallbackCaller(g_show_outline));
	GlobalToggles_insert("ShowAxes", ShowAxesToggleCaller(), ToggleItem::AddCallbackCaller(g_show_axes));
	GlobalToggles_insert("ShowWorkzone", ShowWorkzoneToggleCaller(), ToggleItem::AddCallbackCaller(g_show_workzone));
}

void XYWnd_registerShortcuts ()
{
	command_connect_accelerator("ToggleCrosshairs");
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
	GlobalRadiant().commandInsert("NextView", FreeCaller<toggleActiveOrthoView> (), Accelerator(GDK_Tab,
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("ViewTop", FreeCaller<XY_Top> ());
	GlobalCommands_insert("ViewSide", FreeCaller<XY_Side> ());
	GlobalCommands_insert("ViewFront", FreeCaller<XY_Front> ());

	// general commands
	GlobalRadiant().commandInsert("ToggleCrosshairs", MemberCaller<XYWndManager, &XYWndManager::toggleCrossHairs> (
			GlobalXYWnd()), Accelerator('X', (GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("ToggleGrid", MemberCaller<XYWndManager, &XYWndManager::toggleGrid> (
			GlobalXYWnd()), Accelerator('0'));

	GlobalRadiant().commandInsert("ZoomIn", MemberCaller<XYWndManager, &XYWndManager::zoomIn> (GlobalXYWnd()),
			Accelerator(GDK_Delete));
	GlobalRadiant().commandInsert("ZoomOut", MemberCaller<XYWndManager, &XYWndManager::zoomOut> (GlobalXYWnd()),
			Accelerator(GDK_Insert));
	GlobalCommands_insert("Zoom100", MemberCaller<XYWndManager, &XYWndManager::zoomOut> (GlobalXYWnd()));
	GlobalRadiant().commandInsert("CenterXYViews", FreeCaller<XY_CenterViews> (), Accelerator(GDK_Tab,
			(GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));

	// register preference settings
	GlobalPreferenceSystem().registerPreference("ShowWorkzone", BoolImportStringCaller(
			g_xywindow_globals_private.d_show_work), BoolExportStringCaller(g_xywindow_globals_private.d_show_work));

	GlobalPreferenceSystem().registerPreference("SI_ShowCoords", BoolImportStringCaller(
			g_xywindow_globals_private.show_coordinates), BoolExportStringCaller(
			g_xywindow_globals_private.show_coordinates));
	GlobalPreferenceSystem().registerPreference("SI_ShowOutlines", BoolImportStringCaller(
			g_xywindow_globals_private.show_outline), BoolExportStringCaller(g_xywindow_globals_private.show_outline));
	GlobalPreferenceSystem().registerPreference("SI_ShowAxis", BoolImportStringCaller(
			g_xywindow_globals_private.show_axis), BoolExportStringCaller(g_xywindow_globals_private.show_axis));

	XYWnd::captureStates();

	/* add screenshot filetype */
	GlobalFiletypesModule::getTable().addType("bmp", "screenshot bitmap", filetype_t("bitmap", "*.bmp"));
}

void XYWindow_Destroy ()
{
	XYWnd::releaseStates();
}
