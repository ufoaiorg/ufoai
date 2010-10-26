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
#include "irender.h"
#include "igl.h"
#include "cullable.h"
#include "renderable.h"
#include "preferencesystem.h"

#include "signal/signal.h"
#include "container/array.h"
#include "scenelib.h"
#include "render.h"
#include "math/frustum.h"

#include "gtkutil/widget.h"
#include "gtkutil/button.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/xorrectangle.h"
#include "../gtkmisc.h"
#include "../selection/RadiantWindowObserver.h"
#include "../mainframe.h"
#include "../settings/preferences.h"
#include "../commands.h"
#include "../xyview/GlobalXYWnd.h"
#include "../windowobservers.h"
#include "../ui/Icons.h"
#include "../render/RenderStatistics.h"
#include "../timer.h"
#include "CamRenderer.h"
#include "../ui/eventmapper/EventMapper.h"

#include "GlobalCamera.h"
#include "Camera.h"

gboolean camera_keymove (gpointer data)
{
	Camera* cam = reinterpret_cast<Camera*> (data);
	cam->keyMove();
	return TRUE;
}
#include "CamWnd.h"

static CamWnd* g_camwnd = 0;

GtkWindow* CamWnd_getParent (CamWnd& camwnd)
{
	return camwnd.m_parent;
}

camwindow_globals_t g_camwindow_globals;

// =============================================================================
// CamWnd class

void Camera_SetStats (bool value)
{
	g_camwindow_globals_private.m_showStats = value;
}
void ShowStatsToggle ()
{
	g_camwindow_globals_private.m_showStats ^= 1;
}
typedef FreeCaller<ShowStatsToggle> ShowStatsToggleCaller;

void ShowStatsExport (const BoolImportCallback& importer)
{
	importer(g_camwindow_globals_private.m_showStats);
}
typedef FreeCaller1<const BoolImportCallback&, ShowStatsExport> ShowStatsExportCaller;

ShowStatsExportCaller g_show_stats_caller;
BoolExportCallback g_show_stats_callback(g_show_stats_caller);
ToggleItem g_show_stats(g_show_stats_callback);

void GlobalCamera_ResetAngles ()
{
	CamWnd& camwnd = *g_camwnd;
	Vector3 angles;
	angles[CAMERA_ROLL] = angles[CAMERA_PITCH] = 0;
	angles[CAMERA_YAW] = static_cast<float> (22.5 * floor((camwnd.getCameraAngles()[CAMERA_YAW] + 11) / 22.5));
	camwnd.setCameraAngles(angles);
}

void Camera_ChangeFloorUp ()
{
	CamWnd& camwnd = *g_camwnd;
	camwnd.changeFloor(true);
}

void Camera_ChangeFloorDown ()
{
	CamWnd& camwnd = *g_camwnd;
	camwnd.changeFloor(false);
}

void Camera_CubeIn ()
{
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals.m_nCubicScale--;
	if (g_camwindow_globals.m_nCubicScale < 1)
		g_camwindow_globals.m_nCubicScale = 1;
	camwnd.getCamera().updateProjection();
	camwnd.update();
}

void Camera_CubeOut ()
{
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals.m_nCubicScale++;
	if (g_camwindow_globals.m_nCubicScale > 23)
		g_camwindow_globals.m_nCubicScale = 23;
	camwnd.getCamera().updateProjection();
	camwnd.update();
}

bool Camera_GetFarClip ()
{
	return g_camwindow_globals_private.m_bCubicClipping;
}

BoolExportCaller g_getfarclip_caller(g_camwindow_globals_private.m_bCubicClipping);
ToggleItem g_getfarclip_item(g_getfarclip_caller);

void Camera_SetFarClip (bool value)
{
	CamWnd& camwnd = *g_camwnd;
	g_camwindow_globals_private.m_bCubicClipping = value;
	g_getfarclip_item.update();
	camwnd.getCamera().updateProjection();
	camwnd.update();
}

void Camera_ToggleFarClip ()
{
	Camera_SetFarClip(!Camera_GetFarClip());
}

void CamWnd_constructToolbar (GtkToolbar* toolbar)
{
	toolbar_append_toggle_button(toolbar, _("Cubic clip the camera view (\\)"), ui::icons::ICON_VIEW_CLIPPING,
			"ToggleCubicClip");
}

void CamWnd_registerShortcuts ()
{
	toggle_add_accelerator("ToggleCubicClip");

	command_connect_accelerator("CameraSpeedInc");
	command_connect_accelerator("CameraSpeedDec");
}

void GlobalCamera_Benchmark ()
{
	g_camwnd->BenchMark();
}

void GlobalCamera_Update ()
{
	g_camwnd->update();
}

camera_draw_mode CamWnd_GetMode ()
{
	return Camera::draw_mode;
}
void CamWnd_SetMode (camera_draw_mode mode)
{
	Camera::draw_mode = mode;
	if (g_camwnd != 0) {
		g_camwnd->update();
	}
}

static CameraModel* g_camera_model = 0;

void CamWnd_LookThroughCamera (CamWnd& camwnd)
{
	if (g_camera_model != 0) {
		camwnd.addHandlersMove();
		g_camera_model->setCameraView(0, Callback());
		g_camera_model = 0;
		camwnd.getCamera().updateModelview();
		camwnd.getCamera().updateProjection();
		camwnd.update();
	}
}

void CamWnd_LookThroughSelected (CamWnd& camwnd)
{
	if (g_camera_model != 0) {
		CamWnd_LookThroughCamera(camwnd);
	}

	if (GlobalSelectionSystem().countSelected() != 0) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		CameraModel* cameraModel = Instance_getCameraModel(instance);
		if (cameraModel != 0) {
			camwnd.removeHandlersMove();
			g_camera_model = cameraModel;
			g_camera_model->setCameraView(camwnd.getCameraView(), ReferenceCaller<CamWnd, CamWnd_LookThroughCamera> (
					camwnd));
		}
	}
}

void GlobalCamera_LookThroughSelected ()
{
	CamWnd_LookThroughSelected(*g_camwnd);
}

void GlobalCamera_LookThroughCamera ()
{
	CamWnd_LookThroughCamera(*g_camwnd);
}

void RenderModeImport (int value)
{
	switch (value) {
	case 0:
		CamWnd_SetMode(cd_wire);
		break;
	case 1:
		CamWnd_SetMode(cd_solid);
		break;
	case 2:
		CamWnd_SetMode(cd_texture);
		break;
	case 3:
		CamWnd_SetMode(cd_lighting);
		break;
	default:
		CamWnd_SetMode(cd_texture);
	}
}
typedef FreeCaller1<int, RenderModeImport> RenderModeImportCaller;

void RenderModeExport (const IntImportCallback& importer)
{
	switch (CamWnd_GetMode()) {
	case cd_wire:
		importer(0);
		break;
	case cd_solid:
		importer(1);
		break;
	case cd_texture:
		importer(2);
		break;
	case cd_lighting:
		importer(3);
		break;
	}
}
typedef FreeCaller1<const IntImportCallback&, RenderModeExport> RenderModeExportCaller;

#define MIN_CAM_SPEED 10
#define MAX_CAM_SPEED 610
#define CAM_SPEED_STEP 50

void CamWnd_Move_Discrete_Import (CamWnd& camwnd, bool value)
{
	if (g_camwindow_globals_private.m_bCamDiscrete) {
		camwnd.moveDiscreteDisable();
	} else {
		camwnd.moveDisable();
	}

	g_camwindow_globals_private.m_bCamDiscrete = value;

	if (g_camwindow_globals_private.m_bCamDiscrete) {
		camwnd.moveDiscreteEnable();
	} else {
		camwnd.moveEnable();
	}
}

void CamWnd_Move_Discrete_Import (bool value)
{
	if (g_camwnd != 0) {
		CamWnd_Move_Discrete_Import(*g_camwnd, value);
	} else {
		g_camwindow_globals_private.m_bCamDiscrete = value;
	}
}

void Camera_constructPreferences (PreferencesPage& page)
{
	page.appendSlider(_("Movement Speed"), g_camwindow_globals_private.m_nMoveSpeed, TRUE, 0, 0, 100, MIN_CAM_SPEED,
			MAX_CAM_SPEED, 1, 10, 10);
	page.appendCheckBox("", _("Link strafe speed to movement speed"), g_camwindow_globals_private.m_bCamLinkSpeed);
	page.appendSlider(_("Rotation Speed"), g_camwindow_globals_private.m_nAngleSpeed, TRUE, 0, 0, 3, 1, 180, 1, 10, 10);
	page.appendCheckBox("", _("Invert mouse vertical axis"), g_camwindow_globals_private.m_bCamInverseMouse);
	page.appendCheckBox("", _("Discrete movement"), FreeCaller1<bool, CamWnd_Move_Discrete_Import> (),
			BoolExportCaller(g_camwindow_globals_private.m_bCamDiscrete));
	page.appendCheckBox("", _("Enable far-clip plane"), FreeCaller1<bool, Camera_SetFarClip> (), BoolExportCaller(
			g_camwindow_globals_private.m_bCubicClipping));
	page.appendCheckBox("", _("Enable statistics"), FreeCaller1<bool, Camera_SetStats> (), BoolExportCaller(
			g_camwindow_globals_private.m_showStats));

	const char* render_mode[] = { C_("Render Mode", "Wireframe"), C_("Render Mode", "Flatshade"),
			C_("Render Mode", "Textured") };
	page.appendCombo(_("Render Mode"), STRING_ARRAY_RANGE(render_mode), IntImportCallback(RenderModeImportCaller()),
			IntExportCallback(RenderModeExportCaller()));

	const char* strafe_mode[] = { C_("Strafe Mode", "Both"), C_("Strafe Mode", "Forward"), C_("Strafe Mode", "Up") };
	page.appendCombo(_("Strafe Mode"), g_camwindow_globals_private.m_nStrafeMode, STRING_ARRAY_RANGE(strafe_mode));
}

void Camera_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Camera"), _("Camera View Preferences")));
	Camera_constructPreferences(page);
}
void Camera_registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Camera_constructPage> ());
}

#include "preferencesystem.h"
#include "stringio.h"
#include "../dialog.h"

typedef FreeCaller1<bool, CamWnd_Move_Discrete_Import> CamWndMoveDiscreteImportCaller;

void CameraSpeed_increase ()
{
	if (g_camwindow_globals_private.m_nMoveSpeed <= (MAX_CAM_SPEED - CAM_SPEED_STEP - 10)) {
		g_camwindow_globals_private.m_nMoveSpeed += CAM_SPEED_STEP;
	} else {
		g_camwindow_globals_private.m_nMoveSpeed = MAX_CAM_SPEED - 10;
	}
}

void CameraSpeed_decrease ()
{
	if (g_camwindow_globals_private.m_nMoveSpeed >= (MIN_CAM_SPEED + CAM_SPEED_STEP)) {
		g_camwindow_globals_private.m_nMoveSpeed -= CAM_SPEED_STEP;
	} else {
		g_camwindow_globals_private.m_nMoveSpeed = MIN_CAM_SPEED;
	}
}

/// \brief Initialisation for things that have the same lifespan as this module.
void CamWnd_Construct ()
{
	GlobalRadiant().commandInsert("CenterView", FreeCaller<GlobalCamera_ResetAngles> (), Accelerator(GDK_End));

	GlobalToggles_insert("ToggleCubicClip", FreeCaller<Camera_ToggleFarClip> (), ToggleItem::AddCallbackCaller(
			g_getfarclip_item), Accelerator('\\', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("CubicClipZoomIn", FreeCaller<Camera_CubeIn> (), Accelerator('[',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("CubicClipZoomOut", FreeCaller<Camera_CubeOut> (), Accelerator(']',
			(GdkModifierType) GDK_CONTROL_MASK));

	GlobalRadiant().commandInsert("UpFloor", FreeCaller<Camera_ChangeFloorUp> (), Accelerator(GDK_Prior));
	GlobalRadiant().commandInsert("DownFloor", FreeCaller<Camera_ChangeFloorDown> (), Accelerator(GDK_Next));

	GlobalToggles_insert("ToggleCamera", ToggleShown::ToggleCaller(GlobalCamera().getToggleShown()),
			ToggleItem::AddCallbackCaller(GlobalCamera().getToggleShown().m_item), Accelerator('C',
					(GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));
	GlobalCommands_insert("LookThroughSelected", FreeCaller<GlobalCamera_LookThroughSelected> ());
	GlobalCommands_insert("LookThroughCamera", FreeCaller<GlobalCamera_LookThroughCamera> ());

	GlobalRadiant().commandInsert("CameraSpeedInc", FreeCaller<CameraSpeed_increase> (), Accelerator(GDK_KP_Add,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("CameraSpeedDec", FreeCaller<CameraSpeed_decrease> (), Accelerator(GDK_KP_Subtract,
			(GdkModifierType) GDK_SHIFT_MASK));

	GlobalShortcuts_insert("CameraForward", Accelerator(GDK_Up));
	GlobalShortcuts_insert("CameraBack", Accelerator(GDK_Down));
	GlobalShortcuts_insert("CameraLeft", Accelerator(GDK_Left));
	GlobalShortcuts_insert("CameraRight", Accelerator(GDK_Right));
	GlobalShortcuts_insert("CameraStrafeRight", Accelerator(GDK_period));
	GlobalShortcuts_insert("CameraStrafeLeft", Accelerator(GDK_comma));

	GlobalShortcuts_insert("CameraUp", Accelerator('D'));
	GlobalShortcuts_insert("CameraDown", Accelerator('C'));
	GlobalShortcuts_insert("CameraAngleUp", Accelerator('A'));
	GlobalShortcuts_insert("CameraAngleDown", Accelerator('Z'));

	GlobalShortcuts_insert("CameraFreeMoveForward", Accelerator(GDK_Up));
	GlobalShortcuts_insert("CameraFreeMoveBack", Accelerator(GDK_Down));
	GlobalShortcuts_insert("CameraFreeMoveLeft", Accelerator(GDK_Left));
	GlobalShortcuts_insert("CameraFreeMoveRight", Accelerator(GDK_Right));

	GlobalToggles_insert("ShowStats", ShowStatsToggleCaller(), ToggleItem::AddCallbackCaller(g_show_stats));

	GlobalPreferenceSystem().registerPreference("ShowStats", BoolImportStringCaller(
			g_camwindow_globals_private.m_showStats), BoolExportStringCaller(g_camwindow_globals_private.m_showStats));
	GlobalPreferenceSystem().registerPreference("MoveSpeed", IntImportStringCaller(
			g_camwindow_globals_private.m_nMoveSpeed), IntExportStringCaller(g_camwindow_globals_private.m_nMoveSpeed));
	GlobalPreferenceSystem().registerPreference("CamLinkSpeed", BoolImportStringCaller(
			g_camwindow_globals_private.m_bCamLinkSpeed), BoolExportStringCaller(
			g_camwindow_globals_private.m_bCamLinkSpeed));
	GlobalPreferenceSystem().registerPreference("AngleSpeed", IntImportStringCaller(
			g_camwindow_globals_private.m_nAngleSpeed),
			IntExportStringCaller(g_camwindow_globals_private.m_nAngleSpeed));
	GlobalPreferenceSystem().registerPreference("CamInverseMouse", BoolImportStringCaller(
			g_camwindow_globals_private.m_bCamInverseMouse), BoolExportStringCaller(
			g_camwindow_globals_private.m_bCamInverseMouse));
	GlobalPreferenceSystem().registerPreference("CamDiscrete", makeBoolStringImportCallback(
			CamWndMoveDiscreteImportCaller()), BoolExportStringCaller(g_camwindow_globals_private.m_bCamDiscrete));
	GlobalPreferenceSystem().registerPreference("CubicClipping", BoolImportStringCaller(
			g_camwindow_globals_private.m_bCubicClipping), BoolExportStringCaller(
			g_camwindow_globals_private.m_bCubicClipping));
	GlobalPreferenceSystem().registerPreference("CubicScale", IntImportStringCaller(g_camwindow_globals.m_nCubicScale),
			IntExportStringCaller(g_camwindow_globals.m_nCubicScale));
	GlobalPreferenceSystem().registerPreference("SI_Colors4", Vector3ImportStringCaller(
			g_camwindow_globals.color_cameraback), Vector3ExportStringCaller(g_camwindow_globals.color_cameraback));
	GlobalPreferenceSystem().registerPreference("SI_Colors12", Vector3ImportStringCaller(
			g_camwindow_globals.color_selbrushes3d), Vector3ExportStringCaller(g_camwindow_globals.color_selbrushes3d));
	GlobalPreferenceSystem().registerPreference("CameraRenderMode", makeIntStringImportCallback(
			RenderModeImportCaller()), makeIntStringExportCallback(RenderModeExportCaller()));
	GlobalPreferenceSystem().registerPreference("StrafeMode", IntImportStringCaller(
			g_camwindow_globals_private.m_nStrafeMode),
			IntExportStringCaller(g_camwindow_globals_private.m_nStrafeMode));

	CamWnd::captureStates();

	Camera_registerPreferencesPage();
}
void CamWnd_Destroy ()
{
	CamWnd::releaseStates();
}
