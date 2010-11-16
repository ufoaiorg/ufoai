/**
 * @file mainframe.cpp
 * @brief Main Window for UFORadiant
 * @note Creates the editing windows and dialogs, creates commands and
 * registers preferences as well as handling internal paths
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

#ifdef HAVE_CONFIG_H
#include "../../../../config.h"
#endif
#include "mainframe.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"
#include "version.h"
#include "environment.h"

#include "ui/common/ToolbarCreator.h"
#include "ui/menu/FiltersMenu.h"
#include "ui/menu/UMPMenu.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "ifilter.h"
#include "iradiant.h"
#include "iregistry.h"
#include "editable.h"
#include "ientity.h"
#include "ishadersystem.h"
#include "iuimanager.h"
#include "igl.h"
#include "imapcompiler.h"
#include "imaterial.h"
#include "ieventmanager.h"
#include "iselectionset.h"
#include "moduleobservers.h"
#include "server.h"

#include <ctime>
#include <string>

#include "scenelib.h"
#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "os/path.h"
#include "os/file.h"
#include "eclasslib.h"

#include "gtkutil/clipboard.h"
#include "gtkutil/container.h"
#include "gtkutil/frame.h"
#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/paned.h"
#include "gtkutil/widget.h"
#include "gtkutil/dialog.h"
#include "gtkutil/IconTextMenuToggle.h"

#include "map/AutoSaver.h"
#include "map/MapCompileException.h"
#include "brush/brushmanip.h"
#include "brush/BrushModule.h"
#include "brush/csg/csg.h"
#include "log/console.h"
#include "entity.h"
#include "sidebar/sidebar.h"
#include "ui/findshader/FindShader.h"
#include "brushexport/BrushExportOBJ.h"
#include "ui/about/AboutDialog.h"
#include "ui/findbrush/findbrush.h"
#include "ui/maptools/ErrorCheckDialog.h"
#include "pathfinding.h"
#include "gtkmisc.h"
#include "map/map.h"
#include "ump.h"
#include "plugin.h"
#include "settings/preferences.h"
#include "render/OpenGLRenderSystem.h"
#include "select.h"
#include "textures.h"
#include "url.h"
#include "windowobservers.h"
#include "referencecache/referencecache.h"
#include "filters/levelfilters.h"
#include "sound/SoundManager.h"
#include "ui/Icons.h"
#include "pathfinding.h"
#include "model.h"
#include "clipper/Clipper.h"
#include "camera/CamWnd.h"
#include "camera/GlobalCamera.h"
#include "xyview/GlobalXYWnd.h"
#include "ui/colourscheme/ColourSchemeEditor.h"
#include "ui/colourscheme/ColourSchemeManager.h"
#include "ui/commandlist/CommandList.h"
#include "ui/transform/TransformDialog.h"
#include "ui/filterdialog/FilterDialog.h"
#include "ui/mru/MRU.h"
#include "ui/splash/Splash.h"
#include "environment.h"
#include "gtkutil/menu.h"
#include "textool/TexTool.h"
#include "selection/algorithm/General.h"
#include "selection/algorithm/Group.h"
#include "selection/algorithm/Transformation.h"

struct LayoutGlobals
{
		WindowPosition m_position;

		int nXYHeight;
		int nXYWidth;
		int nCamWidth;
		int nCamHeight;
		int nState;

		LayoutGlobals () :
			m_position(-1, -1, 640, 480),

			nXYHeight(650), nXYWidth(300), nCamWidth(200), nCamHeight(200), nState(GDK_WINDOW_STATE_MAXIMIZED)
		{
		}
};

LayoutGlobals g_layout_globals;
GLWindowGlobals g_glwindow_globals;

// Virtual file system
class VFSModuleObserver: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		VFSModuleObserver () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				GlobalFileSystem().initDirectory(GlobalRadiant().getFullGamePath());
				GlobalFileSystem().initialise();
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				GlobalFileSystem().shutdown();
			}
		}
};

VFSModuleObserver g_VFSModuleObserver;

void VFS_Construct (void)
{
	Radiant_attachHomePathsObserver(g_VFSModuleObserver);
}
void VFS_Destroy (void)
{
	Radiant_detachHomePathsObserver(g_VFSModuleObserver);
}

// Home Paths

void HomePaths_Realise (void)
{
	g_mkdir_with_parents(GlobalRadiant().getFullGamePath().c_str(), 0775);
}

ModuleObservers g_homePathObservers;

void Radiant_attachHomePathsObserver (ModuleObserver& observer)
{
	g_homePathObservers.attach(observer);
}

void Radiant_detachHomePathsObserver (ModuleObserver& observer)
{
	g_homePathObservers.detach(observer);
}

class HomePathsModuleObserver: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		HomePathsModuleObserver () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				HomePaths_Realise();
				g_homePathObservers.realise();
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				g_homePathObservers.unrealise();
			}
		}
};

HomePathsModuleObserver g_HomePathsModuleObserver;

void HomePaths_Construct (void)
{
	Radiant_attachEnginePathObserver(g_HomePathsModuleObserver);
}
void HomePaths_Destroy (void)
{
	Radiant_detachEnginePathObserver(g_HomePathsModuleObserver);
}

// Engine Path

static std::string g_strEnginePath;
ModuleObservers g_enginePathObservers;
std::size_t g_enginepath_unrealised = 1;

void Radiant_attachEnginePathObserver (ModuleObserver& observer)
{
	g_enginePathObservers.attach(observer);
}

void Radiant_detachEnginePathObserver (ModuleObserver& observer)
{
	g_enginePathObservers.detach(observer);
}

void EnginePath_Realise (void)
{
	if (--g_enginepath_unrealised == 0) {
		g_enginePathObservers.realise();
	}
}

const std::string& EnginePath_get (void)
{
	ASSERT_MESSAGE(g_enginepath_unrealised == 0, "EnginePath_get: engine path not realised");
	return g_strEnginePath;
}

void EnginePath_Unrealise (void)
{
	if (++g_enginepath_unrealised == 1) {
		g_enginePathObservers.unrealise();
	}
}

void setEnginePath (const std::string& path)
{
	StringOutputStream buffer(256);
	buffer << DirectoryCleaned(path.c_str());
	if (g_strEnginePath != buffer.toString()) {
		ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Changing Engine Path"));

		EnginePath_Unrealise();

		g_strEnginePath = buffer.toString();

		EnginePath_Realise();
	}
}

// App Path

const std::string AppPath_get (void)
{
	return Environment::Instance().getAppPath();
}

const std::string SettingsPath_get (void)
{
	return Environment::Instance().getSettingsPath();
}

void EnginePathImport (std::string& self, const char* value)
{
	setEnginePath(value);
}
typedef ReferenceCaller1<std::string, const char*, EnginePathImport> EnginePathImportCaller;

void Paths_constructPreferences (PrefPage* page)
{
	page->appendPathEntry(_("Engine Path"), true, StringImportCallback(EnginePathImportCaller(g_strEnginePath)),
			StringExportCallback(StringExportCaller(g_strEnginePath)));
}
void Paths_constructPage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Paths"), _("Path Settings"));
	Paths_constructPreferences(reinterpret_cast<PrefPage*>(page));
}

void Paths_registerPreferencesPage (void)
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Paths_constructPage> ());
}

class PathsDialog: public Dialog
{
	public:
		GtkWindow* BuildDialog (void)
		{
			GtkFrame* frame = create_dialog_frame(_("Path Settings"), GTK_SHADOW_ETCHED_IN);

			GtkVBox* vbox2 = create_dialog_vbox(0, 4);
			gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox2));

			{
				PrefPage preferencesPage(*this, GTK_WIDGET(vbox2));
				Paths_constructPreferences(&preferencesPage);
			}

			return create_simple_modal_dialog_window(_("Engine Path Not Found"), m_modal, GTK_WIDGET(frame));
		}
};

PathsDialog g_PathsDialog;

void EnginePath_verify (void)
{
	if (!file_exists(g_strEnginePath)) {
		g_PathsDialog.Create();
		g_PathsDialog.DoModal();
		g_PathsDialog.Destroy();
	}
}

namespace
{
	ModuleObservers g_gameModeObservers;
}

const std::string& basegame_get (void)
{
	return g_pGameDescription->getRequiredKeyValue("basegame");
}

void Radiant_attachGameModeObserver (ModuleObserver& observer)
{
	g_gameModeObservers.attach(observer);
}

void Radiant_detachGameModeObserver (ModuleObserver& observer)
{
	g_gameModeObservers.detach(observer);
}

ModuleObservers g_gameToolsPathObservers;

void Radiant_attachGameToolsPathObserver (ModuleObserver& observer)
{
	g_gameToolsPathObservers.attach(observer);
}

void Radiant_detachGameToolsPathObserver (ModuleObserver& observer)
{
	g_gameToolsPathObservers.detach(observer);
}

// This is called from main() to start up the Radiant stuff.
void Radiant_Initialise (void)
{
	// Initialise and instantiate the registry
	GlobalModuleServer::instance().set(GlobalModuleServer_get());
	StaticModuleRegistryList().instance().registerModule("registry");
	GlobalRegistryModuleRef registryRef;

	// Try to load all the XML files into the registry
	GlobalRegistry().init(AppPath_get(), SettingsPath_get());

	// Load the ColourSchemes from the registry
	ColourSchemes().loadColourSchemes();

	Preferences_Load();

	// Load the other modules
	Radiant_Construct(GlobalModuleServer_get());

	g_gameToolsPathObservers.realise();
	g_gameModeObservers.realise();

	// Construct the MRU commands and menu structure
	GlobalMRU().constructMenu();

	// Initialise the most recently used files list
	GlobalMRU().loadRecentFiles();
}

void Radiant_Shutdown (void)
{
	GlobalMRU().saveRecentFiles();

	GlobalSurfaceInspector().shutdown();

	g_gameModeObservers.unrealise();
	g_gameToolsPathObservers.unrealise();

	if (!g_preferences_globals.disable_ini) {
		g_message("Start writing prefs\n");
		Preferences_Save();
		g_message("Done prefs\n");
	}

	Radiant_Destroy();
}

void Exit (void)
{
	if (ConfirmModified(_("Exit Radiant"))) {
		gtk_main_quit();
	}
}

void Undo (void)
{
	GlobalUndoSystem().undo();
	SceneChangeNotify();
}

void Redo (void)
{
	GlobalUndoSystem().redo();
	SceneChangeNotify();
}

void Map_ExportSelected (TextOutputStream& ostream)
{
	GlobalMap().exportSelected(ostream);
}

void Map_ImportSelected (TextInputStream& istream)
{
	GlobalMap().importSelected(istream);
}

void Selection_Copy (void)
{
	clipboard_copy(Map_ExportSelected);
}

void Selection_Paste (void)
{
	clipboard_paste(Map_ImportSelected);
}

void Copy (void)
{
	if (GlobalSelectionSystem().areFacesSelected()) {
		GlobalSurfaceInspector().copyTextureFromSelectedFaces();
	} else {
		Selection_Copy();
	}
}

void Paste (void)
{
	if (GlobalSelectionSystem().areFacesSelected()) {
		GlobalSurfaceInspector().pasteTextureFromSelectedFaces();
	} else {
		UndoableCommand undo("paste");

		GlobalSelectionSystem().setSelectedAll(false);
		Selection_Paste();
	}
}

void PasteToCamera (void)
{
	CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
	GlobalSelectionSystem().setSelectedAll(false);

	UndoableCommand undo("pasteToCamera");

	Selection_Paste();

	// Work out the delta
	Vector3 mid = selection::algorithm::getCurrentSelectionCenter();
	Vector3 delta = vector3_snapped(camwnd.getCameraOrigin(), GlobalGrid().getGridSize()) - mid;

	// Move to camera
	GlobalSelectionSystem().translateSelected(delta);
}

void OpenHelpURL (void)
{
	OpenURL("http://ufoai.ninex.info/wiki/index.php/Category:Mapping");
}

void OpenBugReportURL (void)
{
	OpenURL("http://sourceforge.net/tracker/?func=add&group_id=157793&atid=805242");
}

static void ModeChangeNotify ();

typedef void(*ToolMode) ();
static ToolMode g_currentToolMode = 0;
bool g_currentToolModeSupportsComponentEditing = false;
static ToolMode g_defaultToolMode = 0;

void SelectionSystem_DefaultMode (void)
{
	GlobalSelectionSystem().SetMode(SelectionSystem::ePrimitive);
	GlobalSelectionSystem().SetComponentMode(SelectionSystem::eDefault);
	ModeChangeNotify();
}

bool EdgeMode (void)
{
	return GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().ComponentMode()
			== SelectionSystem::eEdge;
}

bool VertexMode (void)
{
	return GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().ComponentMode()
			== SelectionSystem::eVertex;
}

bool FaceMode (void)
{
	return GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().ComponentMode()
			== SelectionSystem::eFace;
}

void ComponentModeChanged (void)
{
	GlobalEventManager().setToggled("DragVertices", VertexMode());
	GlobalEventManager().setToggled("DragEdges", EdgeMode());
	GlobalEventManager().setToggled("DragFaces", FaceMode());
}

void ComponentMode_SelectionChanged (const Selectable& selectable)
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().countSelected() == 0) {
		SelectionSystem_DefaultMode();
		ComponentModeChanged();
	}
}

void ToggleEntityMode ()
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eEntity) {
		SelectionSystem_DefaultMode();
	} else {
		GlobalSelectionSystem().SetMode(SelectionSystem::eEntity);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eDefault);
	}
	ComponentModeChanged();

	ModeChangeNotify();
}

void ToggleEdgeMode()
{
	if (EdgeMode()) {
		SelectionSystem_DefaultMode();
	} else if (GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eEdge);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void ToggleVertexMode()
{
	if (VertexMode()) {
		SelectionSystem_DefaultMode();
	} else if(GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eVertex);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void ToggleFaceMode()
{
	if (FaceMode()) {
		SelectionSystem_DefaultMode();
	} else if (GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eFace);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

enum ENudgeDirection
{
	eNudgeUp = 1, eNudgeDown = 3, eNudgeLeft = 0, eNudgeRight = 2
};

struct AxisBase
{
		Vector3 x;
		Vector3 y;
		Vector3 z;
		AxisBase (const Vector3& x_, const Vector3& y_, const Vector3& z_) :
			x(x_), y(y_), z(z_)
		{
		}
};

AxisBase AxisBase_forViewType (EViewType viewtype)
{
	switch (viewtype) {
	case XY:
		return AxisBase(g_vector3_axis_x, g_vector3_axis_y, g_vector3_axis_z);
	case XZ:
		return AxisBase(g_vector3_axis_x, g_vector3_axis_z, g_vector3_axis_y);
	case YZ:
		return AxisBase(g_vector3_axis_y, g_vector3_axis_z, g_vector3_axis_x);
	}

	ERROR_MESSAGE("invalid viewtype");
	return AxisBase(Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(0, 0, 0));
}

Vector3 AxisBase_axisForDirection (const AxisBase& axes, ENudgeDirection direction)
{
	switch (direction) {
	case eNudgeLeft:
		return -axes.x;
	case eNudgeUp:
		return axes.y;
	case eNudgeRight:
		return axes.x;
	case eNudgeDown:
		return -axes.y;
	}

	ERROR_MESSAGE("invalid direction");
	return Vector3(0, 0, 0);
}

void NudgeSelection (ENudgeDirection direction, float fAmount, EViewType viewtype)
{
	AxisBase axes(AxisBase_forViewType(viewtype));
	Vector3 view_direction(-axes.z);
	Vector3 nudge(AxisBase_axisForDirection(axes, direction) * fAmount);
	GlobalSelectionSystem().NudgeManipulator(nudge, view_direction);
}

// called when the escape key is used (either on the main window or on an inspector)
void Selection_Deselect (void)
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		if (GlobalSelectionSystem().countSelectedComponents() != 0) {
			GlobalSelectionSystem().setSelectedAllComponents(false);
		} else {
			SelectionSystem_DefaultMode();
			ComponentModeChanged();
		}
	} else {
		if (GlobalSelectionSystem().countSelectedComponents() != 0) {
			GlobalSelectionSystem().setSelectedAllComponents(false);
		} else {
			GlobalSelectionSystem().setSelectedAll(false);
		}
	}
}

void Selection_NudgeUp (void)
{
	UndoableCommand undo("nudgeSelectedUp");
	NudgeSelection(eNudgeUp, GlobalGrid().getGridSize(), GlobalXYWnd().getActiveViewType());
}

void Selection_NudgeDown (void)
{
	UndoableCommand undo("nudgeSelectedDown");
	NudgeSelection(eNudgeDown, GlobalGrid().getGridSize(), GlobalXYWnd().getActiveViewType());
}

void Selection_NudgeLeft (void)
{
	UndoableCommand undo("nudgeSelectedLeft");
	NudgeSelection(eNudgeLeft, GlobalGrid().getGridSize(), GlobalXYWnd().getActiveViewType());
}

void Selection_NudgeRight (void)
{
	UndoableCommand undo("nudgeSelectedRight");
	NudgeSelection(eNudgeRight, GlobalGrid().getGridSize(), GlobalXYWnd().getActiveViewType());
}

void ToolChanged() {
	GlobalEventManager().setToggled("ToggleClipper", GlobalClipper().clipMode());
	GlobalEventManager().setToggled("MouseTranslate", GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eTranslate);
	GlobalEventManager().setToggled("MouseRotate", GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eRotate);
	GlobalEventManager().setToggled("MouseScale", GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eScale);
	GlobalEventManager().setToggled("MouseDrag", GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eDrag);
}

static const char* const c_ResizeMode_status = "QE4 Drag Tool";

void DragMode (void)
{
	if (g_currentToolMode == DragMode && g_defaultToolMode != DragMode) {
		g_defaultToolMode();
	} else {
		g_currentToolMode = DragMode;
		g_currentToolModeSupportsComponentEditing = true;

		GlobalClipper().onClipMode(false);

		Sys_Status(c_ResizeMode_status);
		GlobalSelectionSystem().SetManipulatorMode(SelectionSystem::eDrag);
		ToolChanged();
		ModeChangeNotify();
	}
}

static const char* const c_TranslateMode_status = "Translate Tool";

void TranslateMode (void)
{
	if (g_currentToolMode == TranslateMode && g_defaultToolMode != TranslateMode) {
		g_defaultToolMode();
	} else {
		g_currentToolMode = TranslateMode;
		g_currentToolModeSupportsComponentEditing = true;

		GlobalClipper().onClipMode(false);

		Sys_Status(c_TranslateMode_status);
		GlobalSelectionSystem().SetManipulatorMode(SelectionSystem::eTranslate);
		ToolChanged();
		ModeChangeNotify();
	}
}

static const char* const c_RotateMode_status = "Rotate Tool";

void RotateMode (void)
{
	if (g_currentToolMode == RotateMode && g_defaultToolMode != RotateMode) {
		g_defaultToolMode();
	} else {
		g_currentToolMode = RotateMode;
		g_currentToolModeSupportsComponentEditing = true;

		GlobalClipper().onClipMode(false);

		Sys_Status(c_RotateMode_status);
		GlobalSelectionSystem().SetManipulatorMode(SelectionSystem::eRotate);
		ToolChanged();
		ModeChangeNotify();
	}
}

static const char* const c_ScaleMode_status = "Scale Tool";

void ScaleMode (void)
{
	if (g_currentToolMode == ScaleMode && g_defaultToolMode != ScaleMode) {
		g_defaultToolMode();
	} else {
		g_currentToolMode = ScaleMode;
		g_currentToolModeSupportsComponentEditing = true;

		GlobalClipper().onClipMode(false);

		Sys_Status(c_ScaleMode_status);
		GlobalSelectionSystem().SetManipulatorMode(SelectionSystem::eScale);
		ToolChanged();
		ModeChangeNotify();
	}
}

static const char* const c_ClipperMode_status = "Clipper Tool";

void ClipperMode (void)
{
	if (g_currentToolMode == ClipperMode && g_defaultToolMode != ClipperMode) {
		g_defaultToolMode();
	} else {
		g_currentToolMode = ClipperMode;
		g_currentToolModeSupportsComponentEditing = false;

		SelectionSystem_DefaultMode();

		GlobalClipper().onClipMode(true);

		Sys_Status(c_ClipperMode_status);
		GlobalSelectionSystem().SetManipulatorMode(SelectionSystem::eClip);
		ToolChanged();
		ModeChangeNotify();
	}
}

// greebo: This toggles the brush size info display in the ortho views
void ToggleShowSizeInfo ()
{
	if (GlobalRegistry().get("user/ui/showSizeInfo") == "1") {
		GlobalRegistry().set("user/ui/showSizeInfo", "0");
	} else {
		GlobalRegistry().set("user/ui/showSizeInfo", "1");
	}
	SceneChangeNotify();
}

void Texdef_Rotate (float angle)
{
	StringOutputStream command;
	command << "brushRotateTexture -angle " << angle;
	UndoableCommand undo(command.toString());
	Select_RotateTexture(angle);
}

void Texdef_RotateClockwise (void)
{
	Texdef_Rotate(static_cast<float> (fabs(GlobalSurfaceInspector().getRotate())));
}

void Texdef_RotateAntiClockwise (void)
{
	Texdef_Rotate(static_cast<float> (-fabs(GlobalSurfaceInspector().getRotate())));
}

void Texdef_Scale (float x, float y)
{
	StringOutputStream command;
	command << "brushScaleTexture -x " << x << " -y " << y;
	UndoableCommand undo(command.toString());
	Select_ScaleTexture(x, y);
}

void Texdef_ScaleUp (void)
{
	Texdef_Scale(0, GlobalSurfaceInspector().getScale()[1]);
}

void Texdef_ScaleDown (void)
{
	Texdef_Scale(0, -GlobalSurfaceInspector().getScale()[1]);
}

void Texdef_ScaleLeft (void)
{
	Texdef_Scale(-GlobalSurfaceInspector().getScale()[0], 0);
}

void Texdef_ScaleRight (void)
{
	Texdef_Scale(GlobalSurfaceInspector().getScale()[0], 0);
}

void Texdef_Shift (float x, float y)
{
	StringOutputStream command;
	command << "brushShiftTexture -x " << x << " -y " << y;
	UndoableCommand undo(command.toString());
	Select_ShiftTexture(x, y);
}

void Texdef_ShiftLeft (void)
{
	Texdef_Shift(-GlobalSurfaceInspector().getShift()[0], 0);
}

void Texdef_ShiftRight (void)
{
	Texdef_Shift(GlobalSurfaceInspector().getShift()[0], 0);
}

void Texdef_ShiftUp (void)
{
	Texdef_Shift(0, GlobalSurfaceInspector().getShift()[1]);
}

void Texdef_ShiftDown (void)
{
	Texdef_Shift(0, -GlobalSurfaceInspector().getShift()[1]);
}

class SnappableSnapToGridSelected: public scene::Graph::Walker
{
		float m_snap;
	public:
		SnappableSnapToGridSelected (float snap) :
			m_snap(snap)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				Snappable* snappable = Node_getSnappable(path.top());
				if (snappable != 0 && Instance_getSelectable(instance)->isSelected()) {
					snappable->snapto(m_snap);
				}
			}
			return true;
		}
};

/* greebo: This is the visitor class to snap all components of a selected instance to the grid
 * While visiting all the instances, it checks if the instance derives from ComponentSnappable
 */
class ComponentSnappableSnapToGridSelected: public scene::Graph::Walker
{
		float m_snap;
	public:
		// Constructor: expects the grid size that should be snapped to
		ComponentSnappableSnapToGridSelected (float snap) :
			m_snap(snap)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				// Check if the visited instance is ComponentSnappable
				ComponentSnappable* componentSnappable = Instance_getComponentSnappable(instance);
				// Call the snapComponents() method if the instance is also a _selected_ Selectable
				if (componentSnappable != 0 && Instance_getSelectable(instance)->isSelected()) {
					componentSnappable->snapComponents(m_snap);
				}
			}
			return true;
		}
};

void Selection_SnapToGrid (void)
{
	StringOutputStream command;
	command << "snapSelected -grid " << GlobalGrid().getGridSize();
	UndoableCommand undo(command.toString());

	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		GlobalSceneGraph().traverse(ComponentSnappableSnapToGridSelected(GlobalGrid().getGridSize()));
	} else {
		GlobalSceneGraph().traverse(SnappableSnapToGridSelected(GlobalGrid().getGridSize()));
	}
}

static gint window_realize_remove_decoration (GtkWidget* widget, gpointer data)
{
	gdk_window_set_decorations(widget->window, (GdkWMDecoration) (GDK_DECOR_ALL | GDK_DECOR_MENU | GDK_DECOR_MINIMIZE
			| GDK_DECOR_MAXIMIZE));
	return FALSE;
}

class WaitDialog
{
	public:
		GtkWindow* m_window;
		GtkLabel* m_label;
};

static WaitDialog create_wait_dialog (const std::string& title, const std::string& text)
{
	WaitDialog dialog;

	dialog.m_window = create_floating_window(title, GlobalRadiant().getMainWindow());
	gtk_window_set_resizable(dialog.m_window, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(dialog.m_window), 0);
	gtk_window_set_position(dialog.m_window, GTK_WIN_POS_CENTER_ON_PARENT);

	g_signal_connect(G_OBJECT(dialog.m_window), "realize", G_CALLBACK(window_realize_remove_decoration), 0);

	{
		dialog.m_label = GTK_LABEL(gtk_label_new(text.c_str()));
		gtk_misc_set_alignment(GTK_MISC(dialog.m_label), 0.5, 0.5);
		gtk_label_set_justify(dialog.m_label, GTK_JUSTIFY_CENTER);
		gtk_widget_show(GTK_WIDGET(dialog.m_label));
		gtk_widget_set_size_request(GTK_WIDGET(dialog.m_label), 300, 100);

		gtk_container_add(GTK_CONTAINER(dialog.m_window), GTK_WIDGET(dialog.m_label));
	}
	return dialog;
}

namespace
{
	clock_t g_lastRedrawTime = 0;
	const clock_t c_redrawInterval = clock_t(CLOCKS_PER_SEC / 10);

	bool redrawRequired (void)
	{
		clock_t currentTime = std::clock();
		if (currentTime - g_lastRedrawTime >= c_redrawInterval) {
			g_lastRedrawTime = currentTime;
			return true;
		}
		return false;
	}
}

bool MainFrame_isActiveApp (void)
{
	GList* list = gtk_window_list_toplevels();
	for (GList* i = list; i != 0; i = g_list_next(i)) {
		if (gtk_window_is_active(GTK_WINDOW(i->data))) {
			return true;
		}
	}
	return false;
}

typedef std::list<std::string> StringStack;
static StringStack g_wait_stack;
static WaitDialog g_wait;

bool ScreenUpdates_Enabled (void)
{
	return g_wait_stack.empty();
}

void ScreenUpdates_process (void)
{
	if (redrawRequired() && GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		process_gui();
	}
}

void ScreenUpdates_Disable (const std::string& message, const std::string& title)
{
	if (g_wait_stack.empty()) {
		map::AutoSaver().stopTimer();

		process_gui();

		const bool isActiveApp = MainFrame_isActiveApp();

		g_wait = create_wait_dialog(title, message);

		if (isActiveApp) {
			gtk_widget_show(GTK_WIDGET(g_wait.m_window));
			gtk_grab_add(GTK_WIDGET(g_wait.m_window));
			ScreenUpdates_process();
		}
	} else if (GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		gtk_label_set_text(g_wait.m_label, message.c_str());
		ScreenUpdates_process();
	}
	g_wait_stack.push_back(message);
}

void ScreenUpdates_Enable (void)
{
	ASSERT_MESSAGE(!ScreenUpdates_Enabled(), "screen updates already enabled");
	g_wait_stack.pop_back();
	if (g_wait_stack.empty()) {
		map::AutoSaver().startTimer();
		gtk_grab_remove(GTK_WIDGET(g_wait.m_window));
		destroy_floating_window(g_wait.m_window);
		g_wait.m_window = 0;
	} else if (GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		gtk_label_set_text(g_wait.m_label, g_wait_stack.back().c_str());
		ScreenUpdates_process();
	}
}

void GlobalCamera_UpdateWindow (void)
{
	if (g_pParentWnd != 0) {
		g_pParentWnd->GetCamWnd()->update();
	}
}

void XY_UpdateAllWindows (void)
{
	if (g_pParentWnd != 0) {
		GlobalXYWnd().updateAllViews();
	}
}

void UpdateAllWindows (void)
{
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}

static void ModeChangeNotify (void)
{
	SceneChangeNotify();
}

void ClipperChangeNotify (void)
{
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}

static LatchedInt g_Layout_viewStyle(MainFrame::eSplit, _("Window Layout"));

void CallBrushExportOBJ ()
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		export_selected(GlobalRadiant().getMainWindow());
	} else {
		gtkutil::errorDialog(_("No Brushes Selected!"));
	}
}

static GtkWidget* create_main_statusbar (GtkWidget *pStatusLabel[c_count_status])
{
	GtkTable* table = GTK_TABLE(gtk_table_new(1, c_count_status + 1, FALSE));
	gtk_widget_show(GTK_WIDGET(table));

	{
		GtkLabel* label = GTK_LABEL(gtk_label_new(_("Label")));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_misc_set_padding(GTK_MISC(label), 2, 2);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_table_attach_defaults(table, GTK_WIDGET(label), 0, 1, 0, 1);
		pStatusLabel[c_command_status] = GTK_WIDGET(label);
	}

	for (int i = 1; i < c_count_status; ++i) {
		GtkFrame* frame = GTK_FRAME(gtk_frame_new(0));
		gtk_widget_show(GTK_WIDGET(frame));
		gtk_table_attach_defaults(table, GTK_WIDGET(frame), i, i + 1, 0, 1);
		gtk_frame_set_shadow_type(frame, GTK_SHADOW_IN);

		GtkLabel* label = GTK_LABEL(gtk_label_new(_("Label")));
		gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_misc_set_padding(GTK_MISC(label), 2, 2);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(label));
		pStatusLabel[i] = GTK_WIDGET(label);
	}

	return GTK_WIDGET(table);
}

class MainWindowActive
{
		static gboolean notify (GtkWindow* window, gpointer dummy, MainWindowActive* self)
		{
			if (g_wait.m_window != 0 && gtk_window_is_active(window) && !GTK_WIDGET_VISIBLE(g_wait.m_window)) {
				gtk_widget_show(GTK_WIDGET(g_wait.m_window));
			}

			return FALSE;
		}
	public:
		void connect (GtkWindow* toplevel_window)
		{
			g_signal_connect(G_OBJECT(toplevel_window), "notify::is-active", G_CALLBACK(notify), this);
		}
};

MainWindowActive g_MainWindowActive;

// =============================================================================
// MainFrame class

MainFrame* g_pParentWnd = 0;

GtkWindow* MainFrame_getWindow (void)
{
	if (g_pParentWnd == 0) {
		return 0;
	}
	return g_pParentWnd->m_window;
}

MainFrame::MainFrame () :
	m_window(0), m_idleRedrawStatusText(RedrawStatusTextCaller(*this))
{
	m_pCamWnd = 0;

	for (int n = 0; n < c_count_status; n++) {
		m_pStatusLabel[n] = 0;
	}

	Create();
}

MainFrame::~MainFrame (void)
{
	SaveWindowInfo();

	gtk_widget_hide(GTK_WIDGET(m_window));

	Shutdown();

	gtk_widget_destroy(GTK_WIDGET(m_window));
}

static gint mainframe_delete (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (ConfirmModified("Exit Radiant")) {
		gtk_main_quit();
	}

	return TRUE;
}

/**
 * @brief Create the user settable window layout
 */
void MainFrame::Create (void)
{
	GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

	// do this here, because the commands are needed
	_sidebar = new ui::Sidebar();
	GtkWidget *sidebar = _sidebar->getWidget();

	// Tell the XYManager which window the xyviews should be transient for
	GlobalXYWnd().setGlobalParentWindow(window);

	GlobalWindowObservers_connectTopLevel(window);

	gtk_window_set_transient_for(ui::Splash::Instance().getWindow(), window);

#if !defined(WIN32)
	{
		GdkPixbuf* pixbuf = gtkutil::getLocalPixbuf(ui::icons::ICON);
		if (pixbuf != 0) {
			gtk_window_set_icon(window, pixbuf);
			gdk_pixbuf_unref(pixbuf);
		}
	}
#endif

	gtk_widget_add_events(GTK_WIDGET(window), GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(mainframe_delete), this);

	m_position_tracker.connect(window);

	g_MainWindowActive.connect(window);

	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	GlobalEventManager().connect(GTK_OBJECT(window));
	GlobalEventManager().connectAccelGroup(GTK_WINDOW(window));

	m_nCurrentStyle = (EViewStyle) g_Layout_viewStyle.m_value;

	// Create the Filter menu entries
	ui::FiltersMenu::addItemsToMainMenu();

	// Retrieve the "main" menubar from the UIManager
	GtkMenuBar* mainMenu = GTK_MENU_BAR(GlobalUIManager().getMenuManager()->get("main"));
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(mainMenu), false, false, 0);

	// Instantiate the ToolbarCreator and retrieve the standard toolbar widget
	ui::ToolbarCreator toolbarCreator;

	GtkToolbar* generalToolbar = toolbarCreator.getToolbar("view");
	gtk_widget_show(GTK_WIDGET(generalToolbar));

	GlobalSelectionSetManager().init(generalToolbar);

	// Pack it into the main window
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(generalToolbar), FALSE, FALSE, 0);

	GtkWidget* main_statusbar = create_main_statusbar(m_pStatusLabel);
	gtk_box_pack_end(GTK_BOX(vbox), main_statusbar, FALSE, TRUE, 2);

	GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	GtkToolbar* main_toolbar_v = toolbarCreator.getToolbar("edit");
	gtk_widget_show(GTK_WIDGET(main_toolbar_v));

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(main_toolbar_v), FALSE, FALSE, 0);
	if ((g_layout_globals.nState & GDK_WINDOW_STATE_MAXIMIZED)) {
		/* set stored position and default height/width, otherwise problems with extended screens */
		g_layout_globals.m_position.h = 800;
		g_layout_globals.m_position.w = 600;
		window_set_position(window, g_layout_globals.m_position);
		/* maximize will be done when window is shown */
		gtk_window_maximize(window);
	} else {
		window_set_position(window, g_layout_globals.m_position);
	}

	m_window = window;

	gtk_widget_show(GTK_WIDGET(window));

	// The default XYView pointer
	XYWnd* xyWnd;

	GtkWidget* mainHBox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), mainHBox, TRUE, TRUE, 0);
	gtk_widget_show(mainHBox);

	int w, h;
	gtk_window_get_size(window, &w, &h);

	// create edit windows according to user setable style
	switch (CurrentStyle()) {
	case eRegular: {
		{
			GtkWidget* hsplit = gtk_hpaned_new();
			gtk_widget_show(hsplit);
			m_hSplit = hsplit;

			// Allocate a new OrthoView and set its ViewType to XY
			xyWnd = GlobalXYWnd().createXY();
			xyWnd->setViewType(XY);

			// Create a framed window out of the view's internal widget
			GtkWidget* xy_window = GTK_WIDGET(create_framed_widget(xyWnd->getWidget()));

			{
				GtkWidget* vsplit2 = gtk_vpaned_new();
				gtk_widget_show(vsplit2);
				m_vSplit2 = vsplit2;

				if (CurrentStyle() == eRegular) {
					gtk_paned_add1(GTK_PANED(hsplit), xy_window);
					gtk_paned_add2(GTK_PANED(hsplit), vsplit2);
				} else {
					gtk_paned_add1(GTK_PANED(hsplit), vsplit2);
					gtk_paned_add2(GTK_PANED(hsplit), xy_window);
				}

				// camera
				m_pCamWnd = GlobalCamera().newCamWnd();
				GlobalCamera().setCamWnd(m_pCamWnd);
				GlobalCamera().setParent(m_pCamWnd, window);
				GtkFrame* camera_window = create_framed_widget(m_pCamWnd->getWidget());
				gtk_paned_add1(GTK_PANED(vsplit2), GTK_WIDGET(camera_window));
			}
		}
		gtk_paned_set_position(GTK_PANED(m_vSplit2), g_layout_globals.nCamHeight);
		break;
	}

	case eSplit: {
		// camera
		m_pCamWnd = GlobalCamera().newCamWnd();
		GlobalCamera().setCamWnd(m_pCamWnd);
		GlobalCamera().setParent(m_pCamWnd, window);
		GtkWidget* camera = m_pCamWnd->getWidget();

		// Allocate the three ortho views
		xyWnd = GlobalXYWnd().createXY();
		xyWnd->setViewType(XY);
		GtkWidget* xy = xyWnd->getWidget();

		XYWnd* yzWnd = GlobalXYWnd().createXY();
		yzWnd->setViewType(YZ);
		GtkWidget* yz = yzWnd->getWidget();

		XYWnd* xzWnd = GlobalXYWnd().createXY();
		xzWnd->setViewType(XZ);
		GtkWidget* xz = xzWnd->getWidget();

		// split view (4 views)
		GtkHPaned* split = create_split_views(camera, yz, xy, xz);
		gtk_box_pack_start(GTK_BOX(mainHBox), GTK_WIDGET(split), TRUE, TRUE, 0);

		break;
	}
	default:
		gtkutil::errorDialog(_("Invalid layout type set - remove your radiant config files and retry"));
	}

	// greebo: In any layout, there is at least the XY view present, make it active
	GlobalXYWnd().setActiveXY(xyWnd);

	PreferencesDialog_constructWindow(window);

	GlobalGrid().addGridChangeCallback(FreeCaller<XY_UpdateAllWindows>());

	g_defaultToolMode = DragMode;
	g_defaultToolMode();
	SetStatusText(m_command_status, c_TranslateMode_status);

	/* enable button state tracker, set default states for begin */
	GlobalUndoSystem().trackerAttach(m_saveStateTracker);

	gtk_box_pack_start(GTK_BOX(mainHBox), GTK_WIDGET(sidebar), FALSE, FALSE, 0);

	// Start the autosave timer so that it can periodically check the map for changes
	map::AutoSaver().startTimer();
}

void MainFrame::SaveWindowInfo (void)
{
	if (CurrentStyle() == eRegular) {
		g_layout_globals.nXYWidth = gtk_paned_get_position(GTK_PANED(m_hSplit));
		g_layout_globals.nCamHeight = gtk_paned_get_position(GTK_PANED(m_vSplit2));
	}

	g_layout_globals.m_position = m_position_tracker.getPosition();
	g_layout_globals.nState = gdk_window_get_state(GTK_WIDGET(m_window)->window);
}

void MainFrame::Shutdown (void)
{
	map::AutoSaver().stopTimer();
	ui::TexTool::Instance().shutdown();

	GlobalUndoSystem().trackerDetach(m_saveStateTracker);

	GlobalXYWnd().destroyViews();

	GlobalCamera().deleteCamWnd(m_pCamWnd);
	m_pCamWnd = 0;

	PreferencesDialog_destroyWindow();

	delete _sidebar;

	 // Stop the AutoSaver class from being called
	map::AutoSaver().stopTimer();
}

/**
 * @brief Updates the statusbar text with command, position, texture and so on
 * @sa TextureBrowser_SetStatus
 */
void MainFrame::RedrawStatusText (void)
{
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_command_status]), m_command_status.c_str());
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_position_status]), m_position_status.c_str());
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_brushcount_status]), m_brushcount_status.c_str());
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_texture_status]), m_texture_status.c_str());
}

void MainFrame::UpdateStatusText (void)
{
	m_idleRedrawStatusText.queueDraw();
}

void MainFrame::SetStatusText (std::string& status_text, const std::string& pText)
{
	status_text = pText;
	UpdateStatusText();
}

/**
 * @brief Updates the first statusbar column
 * @param[in] status the status to print into the first statusbar column
 * @sa MainFrame::RedrawStatusText
 * @sa MainFrame::SetStatusText
 */
void Sys_Status (const std::string& status)
{
	if (g_pParentWnd != 0) {
		g_pParentWnd->SetStatusText(g_pParentWnd->m_command_status, status);
	}
}

namespace
{
	GLFont g_font(0, 0);
}

void GlobalGL_sharedContextCreated (void)
{
	// report OpenGL information
	globalOutputStream() << "GL_VENDOR: " << reinterpret_cast<const char*> (glGetString(GL_VENDOR)) << "\n";
	globalOutputStream() << "GL_RENDERER: " << reinterpret_cast<const char*> (glGetString(GL_RENDERER)) << "\n";
	globalOutputStream() << "GL_VERSION: " << reinterpret_cast<const char*> (glGetString(GL_VERSION)) << "\n";
	globalOutputStream() << "GL_EXTENSIONS: " << reinterpret_cast<const char*> (glGetString(GL_EXTENSIONS)) << "\n";

	QGL_sharedContextCreated(GlobalOpenGL());

	GlobalShaderCache().realise();
	Textures_Realise();

	/* use default font here (Sans 10 is gtk default) */
	GtkSettings *settings = gtk_settings_get_default();
	gchar *fontname;
	g_object_get(settings, "gtk-font-name", &fontname, (char*) 0);
	g_font = glfont_create(fontname);
	// Fallback
	if (g_font.getPixelHeight() == -1)
		g_font = glfont_create("Sans 10");

	GlobalOpenGL().m_font = g_font.getDisplayList();
	GlobalOpenGL().m_fontHeight = g_font.getPixelHeight();
}

void GlobalGL_sharedContextDestroyed (void)
{
	Textures_Unrealise();
	GlobalShaderCache().unrealise();

	QGL_sharedContextDestroyed(GlobalOpenGL());
}

void Layout_constructPreferences (PrefPage* page)
{
	const char* layouts[] = { ui::icons::ICON_WINDOW_REGULAR.c_str(), ui::icons::ICON_WINDOW_SPLIT.c_str() };
	page->appendRadioIcons(_("Window Layout"), STRING_ARRAY_RANGE(layouts), LatchedIntImportCaller(
			g_Layout_viewStyle), IntExportCaller(g_Layout_viewStyle.m_latched));
}

void Layout_constructPage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Layout"), _("Layout Preferences"));
	Layout_constructPreferences(reinterpret_cast<PrefPage*>(page));
}

void Layout_registerPreferencesPage (void)
{
	PreferencesDialog_addInterfacePage(FreeCaller1<PreferenceGroup&, Layout_constructPage> ());
}

void EditColourScheme() {
	new ui::ColourSchemeEditor(); // self-destructs in GTK callback
}

#include "preferencesystem.h"
#include "stringio.h"

class NullMapCompilerObserver : public ICompilerObserver {
	void notify (const std::string &message) {
	}
};

void ToolsCompile () {
	if (!ConfirmModified(_("Compile Map")))
		return;

	/* empty map? */
	if (!GlobalRadiant().getCounter(counterBrushes).get()) {
		gtkutil::errorDialog(_("Nothing that could get compiled\n"));
		return;
	}
	try {
		const std::string mapName = GlobalMap().getName();
		NullMapCompilerObserver observer;
		GlobalMapCompiler().compileMap(mapName, observer);
	} catch (MapCompileException& e) {
		gtkutil::errorDialog(e.what());
	}
}

void ToolsCheckErrors () {
	if (!ConfirmModified(_("Check Map")))
		return;

	/* empty map? */
	if (!GlobalRadiant().getCounter(counterBrushes).get()) {
		gtkutil::errorDialog(_("Nothing to fix in this map\n"));
		return;
	}

	ui::ErrorCheckDialog::showDialog();
}

void ToolsGenerateMaterials () {
	if (!ConfirmModified(_("Generate Materials")))
		return;

	/* empty map? */
	if (!GlobalRadiant().getCounter(counterBrushes).get()) {
		gtkutil::errorDialog(_("Nothing to generate materials for\n"));
		return;
	}

	try {
		const std::string mapName = GlobalMap().getName();
		NullMapCompilerObserver observer;
		GlobalMapCompiler().generateMaterial(mapName, observer);
		GlobalMaterialSystem()->showMaterialDefinition();
	} catch (MapCompileException& e) {
		gtkutil::errorDialog(e.what());
	}
}

void FindBrushOrEntity() {
	new ui::FindBrushDialog;
}

void MainFrame_Construct (void)
{
	// Tell the FilterSystem to register its commands
	GlobalFilterSystem().init();

	GlobalEventManager().addCommand("OpenManual", FreeCaller<OpenHelpURL> ());

	GlobalEventManager().addCommand("RefreshReferences", FreeCaller<RefreshReferences> ());
	GlobalEventManager().addCommand("Exit", FreeCaller<Exit> ());

	GlobalEventManager().addCommand("Undo", FreeCaller<Undo> ());
	GlobalEventManager().addCommand("Redo", FreeCaller<Redo> ());
	GlobalEventManager().addCommand("Copy", FreeCaller<Copy> ());
	GlobalEventManager().addCommand("Paste", FreeCaller<Paste> ());
	GlobalEventManager().addCommand("PasteToCamera", FreeCaller<PasteToCamera> ());
	GlobalEventManager().addCommand("CloneSelection", FreeCaller<selection::algorithm::cloneSelected> ());
	GlobalEventManager().addCommand("DeleteSelection", FreeCaller<selection::algorithm::deleteSelection> ());
	GlobalEventManager().addCommand("ParentSelection", FreeCaller<selection::algorithm::parentSelection> ());
	GlobalEventManager().addCommand("UnSelectSelection", FreeCaller<Selection_Deselect> ());
	GlobalEventManager().addCommand("InvertSelection", FreeCaller<selection::algorithm::invertSelection> ());
	GlobalEventManager().addCommand("SelectInside", FreeCaller<selection::algorithm::selectInside> ());
	GlobalEventManager().addCommand("SelectTouching", FreeCaller<selection::algorithm::selectTouching> ());
	GlobalEventManager().addCommand("SelectCompleteTall", FreeCaller<selection::algorithm::selectCompleteTall> ());
	GlobalEventManager().addCommand("ExpandSelectionToEntities", FreeCaller<selection::algorithm::expandSelectionToEntities> ());
	GlobalEventManager().addCommand("Preferences", FreeCaller<PreferencesDialog_showDialog> ());

	GlobalEventManager().addCommand("ShowHidden", FreeCaller<selection::algorithm::showAllHidden> ());
	GlobalEventManager().addCommand("HideSelected", FreeCaller<selection::algorithm::hideSelected> ());

	GlobalEventManager().addToggle("DragVertices", FreeCaller<ToggleVertexMode> ());
	GlobalEventManager().addToggle("DragEdges", FreeCaller<ToggleEdgeMode> ());
	GlobalEventManager().addToggle("DragFaces", FreeCaller<ToggleFaceMode> ());
	GlobalEventManager().addToggle("DragEntities", FreeCaller<ToggleEntityMode>());

	GlobalEventManager().setToggled("DragEntities", false);

	GlobalEventManager().addCommand("MirrorSelectionX", FreeCaller<Selection_Flipx> ());
	GlobalEventManager().addCommand("RotateSelectionX", FreeCaller<Selection_Rotatex> ());
	GlobalEventManager().addCommand("MirrorSelectionY", FreeCaller<Selection_Flipy> ());
	GlobalEventManager().addCommand("RotateSelectionY", FreeCaller<Selection_Rotatey> ());
	GlobalEventManager().addCommand("MirrorSelectionZ", FreeCaller<Selection_Flipz> ());
	GlobalEventManager().addCommand("RotateSelectionZ", FreeCaller<Selection_Rotatez> ());

	GlobalEventManager().addCommand("TransformDialog", FreeCaller<ui::TransformDialog::toggle>());

	GlobalEventManager().addCommand("FindBrush", FreeCaller<FindBrushOrEntity> ());

	GlobalEventManager().addCommand("ToolsCheckErrors", FreeCaller<ToolsCheckErrors> ());
	GlobalEventManager().addCommand("ToolsCompile", FreeCaller<ToolsCompile> ());
	GlobalEventManager().addCommand("ToolsGenerateMaterials", FreeCaller<ToolsGenerateMaterials> ());
	GlobalEventManager().addToggle("PlaySounds", FreeCaller<GlobalSoundManager_switchPlaybackEnabledFlag> ());

	GlobalEventManager().addToggle("ToggleShowSizeInfo", FreeCaller<ToggleShowSizeInfo> ());

	GlobalEventManager().addToggle("ToggleClipper", FreeCaller<ClipperMode> ());

	GlobalEventManager().addToggle("MouseTranslate", FreeCaller<TranslateMode> ());
	GlobalEventManager().addToggle("MouseRotate", FreeCaller<RotateMode> ());
	GlobalEventManager().addToggle("MouseScale", FreeCaller<ScaleMode> ());
	GlobalEventManager().addToggle("MouseDrag", FreeCaller<DragMode> ());

	GlobalEventManager().addCommand("CSGSubtract", FreeCaller<CSG_Subtract> ());
	GlobalEventManager().addCommand("CSGMerge", FreeCaller<CSG_Merge> ());
	GlobalEventManager().addCommand("CSGHollow", FreeCaller<CSG_MakeHollow> ());

	GlobalEventManager().addCommand("SnapToGrid", FreeCaller<Selection_SnapToGrid> ());

	GlobalEventManager().addCommand("SelectAllOfType", FreeCaller<selection::algorithm::selectAllOfType> ());

	GlobalEventManager().addCommand("SelectAllFacesOfTex", FreeCaller<selection::algorithm::selectAllFacesWithTexture> ());

	GlobalEventManager().addCommand("TexRotateClock", FreeCaller<Texdef_RotateClockwise> ());
	GlobalEventManager().addCommand("TexRotateCounter", FreeCaller<Texdef_RotateAntiClockwise> ());
	GlobalEventManager().addCommand("TexScaleUp", FreeCaller<Texdef_ScaleUp> ());
	GlobalEventManager().addCommand("TexScaleDown", FreeCaller<Texdef_ScaleDown> ());
	GlobalEventManager().addCommand("TexScaleLeft", FreeCaller<Texdef_ScaleLeft> ());
	GlobalEventManager().addCommand("TexScaleRight", FreeCaller<Texdef_ScaleRight> ());
	GlobalEventManager().addCommand("TexShiftUp", FreeCaller<Texdef_ShiftUp> ());
	GlobalEventManager().addCommand("TexShiftDown", FreeCaller<Texdef_ShiftDown> ());
	GlobalEventManager().addCommand("TexShiftLeft", FreeCaller<Texdef_ShiftLeft> ());
	GlobalEventManager().addCommand("TexShiftRight", FreeCaller<Texdef_ShiftRight> ());

	GlobalEventManager().addCommand("MoveSelectionDOWN", FreeCaller<Selection_MoveDown> ());
	GlobalEventManager().addCommand("MoveSelectionUP", FreeCaller<Selection_MoveUp> ());

	GlobalEventManager().addCommand("SelectNudgeLeft", FreeCaller<Selection_NudgeLeft> ());
	GlobalEventManager().addCommand("SelectNudgeRight", FreeCaller<Selection_NudgeRight> ());
	GlobalEventManager().addCommand("SelectNudgeUp", FreeCaller<Selection_NudgeUp> ());
	GlobalEventManager().addCommand("SelectNudgeDown", FreeCaller<Selection_NudgeDown> ());
	GlobalEventManager().addCommand("EditColourScheme", FreeCaller<EditColourScheme>());

	GlobalEventManager().addCommand("BrushExportOBJ", FreeCaller<CallBrushExportOBJ> ());

	GlobalEventManager().addCommand("EditFiltersDialog", FreeCaller<ui::FilterDialog::showDialog>());
	GlobalEventManager().addCommand("FindReplaceTextures", FreeCaller<ui::FindAndReplaceShader::showDialog>());
	GlobalEventManager().addCommand("ShowCommandList", FreeCaller<ShowCommandListDialog>());
	GlobalEventManager().addCommand("About", FreeCaller<ui::AboutDialog::showDialog>());
	GlobalEventManager().addCommand("BugReport", FreeCaller<OpenBugReportURL> ());

	ui::TexTool::registerCommands();

	LevelFilters_registerCommands();
	Model_RegisterToggles();

	typedef FreeCaller1<const Selectable&, ComponentMode_SelectionChanged> ComponentModeSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(ComponentModeSelectionChangedCaller());

	GlobalPreferenceSystem().registerPreference("QE4StyleWindows", IntImportStringCaller(g_Layout_viewStyle.m_latched),
			IntExportStringCaller(g_Layout_viewStyle.m_latched));
	GlobalPreferenceSystem().registerPreference("XYHeight", IntImportStringCaller(g_layout_globals.nXYHeight),
			IntExportStringCaller(g_layout_globals.nXYHeight));
	GlobalPreferenceSystem().registerPreference("XYWidth", IntImportStringCaller(g_layout_globals.nXYWidth),
			IntExportStringCaller(g_layout_globals.nXYWidth));
	GlobalPreferenceSystem().registerPreference("CamWidth", IntImportStringCaller(g_layout_globals.nCamWidth),
			IntExportStringCaller(g_layout_globals.nCamWidth));
	GlobalPreferenceSystem().registerPreference("CamHeight", IntImportStringCaller(g_layout_globals.nCamHeight),
			IntExportStringCaller(g_layout_globals.nCamHeight));

	GlobalPreferenceSystem().registerPreference("State", IntImportStringCaller(g_layout_globals.nState),
			IntExportStringCaller(g_layout_globals.nState));
	GlobalPreferenceSystem().registerPreference("PositionX", IntImportStringCaller(g_layout_globals.m_position.x),
			IntExportStringCaller(g_layout_globals.m_position.x));
	GlobalPreferenceSystem().registerPreference("PositionY", IntImportStringCaller(g_layout_globals.m_position.y),
			IntExportStringCaller(g_layout_globals.m_position.y));
	GlobalPreferenceSystem().registerPreference("Width", IntImportStringCaller(g_layout_globals.m_position.w),
			IntExportStringCaller(g_layout_globals.m_position.w));
	GlobalPreferenceSystem().registerPreference("Height", IntImportStringCaller(g_layout_globals.m_position.h),
			IntExportStringCaller(g_layout_globals.m_position.h));

#ifdef PKGDATADIR
	StringOutputStream path(256);
	path << DirectoryCleaned(PKGDATADIR);
	g_strEnginePath = path.toString();
#endif

	GlobalPreferenceSystem().registerPreference("EnginePath", StringImportStringCaller(g_strEnginePath),
			StringExportStringCaller(g_strEnginePath));

	g_Layout_viewStyle.useLatched();

	Layout_registerPreferencesPage();
	Paths_registerPreferencesPage();

	GLWidget_sharedContextCreated = GlobalGL_sharedContextCreated;
	GLWidget_sharedContextDestroyed = GlobalGL_sharedContextDestroyed;

	// Broadcast the startup event
	GlobalRadiant().broadcastStartupEvent();
}

void MainFrame_Destroy (void)
{
	// Broadcast shutdown event to RadiantListeners
	GlobalRadiant().broadcastShutdownEvent();
}

void GLWindow_Construct (void)
{
	GlobalPreferenceSystem().registerPreference("MouseButtons", IntImportStringCaller(g_glwindow_globals.m_nMouseType),
			IntExportStringCaller(g_glwindow_globals.m_nMouseType));
}

void GLWindow_Destroy (void)
{
}

/**
 * Updates the sensitivity of save, undo and redo buttons and menu items according to actual state.
 */
void UndoSaveStateTracker::UpdateSensitiveStates (void)
{
}

/**
 * Called whenever save was completed. This causes the UndoSaveTracker to mark this point as saved.
 */
void MainFrame::SaveComplete ()
{
	m_saveStateTracker.storeState();
}

/**
 * This stores actual step as the saved step.
 */
void UndoSaveStateTracker::storeState (void)
{
	m_savedStep = m_undoSteps;
	UpdateSensitiveStates();
}

/**
 * increase redo steps if undo level supports it
 */
void UndoSaveStateTracker::increaseRedo (void)
{
	if (m_redoSteps < GlobalUndoSystem().getLevels()) {
		m_redoSteps++;
	}
}

/**
 * adjusts undo level if needed (e.g. if undo queue size was lowered)
 */
void UndoSaveStateTracker::checkUndoLevel (void)
{
	const unsigned int currentLevel = GlobalUndoSystem().getLevels();
	if (m_undoSteps > currentLevel)
		m_undoSteps = currentLevel;
}

/**
 * Increase undo steps if undo level supports it. If undo exceeds current undo level,
 * lower saved steps to ensure that save state is handled correctly.
 */
void UndoSaveStateTracker::increaseUndo (void)
{
	if (m_undoSteps < GlobalUndoSystem().getLevels()) {
		m_undoSteps++;
	} else {
		checkUndoLevel();
		m_savedStep--;
	}
}

/**
 * Begin a new step, invalidates all other redo states
 */
void UndoSaveStateTracker::begin (void)
{
	increaseUndo();
	m_redoSteps = 0;
	UpdateSensitiveStates();
}

/**
 * Reset all states, no save, redo and undo is available
 */
void UndoSaveStateTracker::clear (void)
{
	m_redoSteps = 0;
	m_undoSteps = 0;
	m_savedStep = 0;
	UpdateSensitiveStates();
}

/**
 * Reset redo states
 */
void UndoSaveStateTracker::clearRedo (void)
{
	m_redoSteps = 0;
	UpdateSensitiveStates();
}

/**
 * Redo a step, one more undo is available
 */
void UndoSaveStateTracker::redo (void)
{
	m_redoSteps--;
	increaseUndo();
	UpdateSensitiveStates();
}

/**
 * Undo a step, on more redo is available
 */
void UndoSaveStateTracker::undo (void)
{
	increaseRedo();
	checkUndoLevel();
	m_undoSteps--;
	UpdateSensitiveStates();
}
