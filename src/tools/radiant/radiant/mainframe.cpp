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

#include "ifilesystem.h"
#include "iundo.h"
#include "ifilter.h"
#include "iradiant.h"
#include "itoolbar.h"
#include "iregistry.h"
#include "editable.h"
#include "ientity.h"
#include "ishadersystem.h"
#include "igl.h"
#include "moduleobserver.h"
#include "server.h"

#include <ctime>
#include <string>

#include "scenelib.h"
#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "os/path.h"
#include "os/file.h"
#include "eclasslib.h"
#include "moduleobservers.h"

#include "gtkutil/clipboard.h"
#include "gtkutil/container.h"
#include "gtkutil/frame.h"
#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/menu.h"
#include "gtkutil/paned.h"
#include "gtkutil/widget.h"
#include "gtkutil/dialog.h"
#include "gtkutil/IconTextMenuToggle.h"

#include "map/autosave.h"
#include "brush/brushmanip.h"
#include "brush/brushmodule.h"
#include "colorscheme.h"
#include "camera/camwindow.h"
#include "brush/csg/csg.h"
#include "commands.h"
#include "console.h"
#include "entity.h"
#include "sidebar/sidebar.h"
#include "filters.h"
#include "dialogs/findtextures.h"
#include "xyview/grid.h"
#include "brushexport/BrushExportOBJ.h"
#include "dialogs/about.h"
#include "dialogs/findbrush.h"
#include "dialogs/maptools.h"
#include "pathfinding.h"
#include "gtkmisc.h"
#include "map/map.h"
#include "lastused.h"
#include "ump.h"
#include "plugin.h"
#include "plugin/PluginManager.h"
#include "pluginmenu.h"
#include "plugintoolbar.h"
#include "settings/preferences.h"
#include "qe3.h"
#include "render/OpenGLRenderSystem.h"
#include "select.h"
#include "textures.h"
#include "url.h"
#include "xyview/GlobalXYWnd.h"
#include "windowobservers.h"
#include "referencecache.h"
#include "levelfilters.h"
#include "sound/SoundManager.h"
#include "ui/Icons.h"
#include "pathfinding.h"
#include "model.h"
#include "clipper/GlobalClipPoints.h"
#include "camera/CamWnd.h"
#include "camera/GlobalCamera.h"

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
				QE_InitVFS();
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
	g_qeglobals.m_userEnginePath = EnginePath_get();
	g_qeglobals.m_userGamePath = g_qeglobals.m_userEnginePath + basegame_get() + "/";

	ASSERT_MESSAGE(!g_qeglobals.m_userGamePath.empty(), "HomePaths_Realise: user-game-path is empty");
	g_mkdir_with_parents(g_qeglobals.m_userGamePath.c_str(), 0775);
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

// Compiler Path

std::string g_strCompilerBinaryWithPath("");

const std::string& CompilerBinaryWithPath_get (void)
{
	return g_strCompilerBinaryWithPath;
}

// App Path

std::string g_strAppPath; ///< holds the full path of the executable

const std::string& AppPath_get (void)
{
	return g_strAppPath;
}

const std::string& SettingsPath_get (void)
{
	return environment_get_home_path();
}

void EnginePathImport (std::string& self, const char* value)
{
	setEnginePath(value);
}
typedef ReferenceCaller1<std::string, const char*, EnginePathImport> EnginePathImportCaller;

void Paths_constructPreferences (PreferencesPage& page)
{
	page.appendPathEntry(_("Engine Path"), true, StringImportCallback(EnginePathImportCaller(g_strEnginePath)),
			StringExportCallback(StringExportCaller(g_strEnginePath)));
	page.appendPathEntry(_("Compiler Binary"), g_strCompilerBinaryWithPath, false);
}
void Paths_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Paths"), _("Path Settings")));
	Paths_constructPreferences(page);
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
				PreferencesPage preferencesPage(*this, GTK_WIDGET(vbox2));
				Paths_constructPreferences(preferencesPage);
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
	std::string g_gamename;
	ModuleObservers g_gameNameObservers;
	ModuleObservers g_gameModeObservers;
}

void Radiant_attachGameNameObserver (ModuleObserver& observer)
{
	g_gameNameObservers.attach(observer);
}

void Radiant_detachGameNameObserver (ModuleObserver& observer)
{
	g_gameNameObservers.detach(observer);
}

const std::string& basegame_get (void)
{
	return g_pGameDescription->getRequiredKeyValue("basegame");
}

const std::string gamepath_get (void)
{
	return g_strEnginePath + g_gamename + "/";
}

void gamename_set (const std::string& gamename)
{
	if (gamename != g_gamename) {
		g_gameNameObservers.unrealise();
		g_gamename = gamename;
		g_gameNameObservers.realise();
	}
}

void Radiant_attachGameModeObserver (ModuleObserver& observer)
{
	g_gameModeObservers.attach(observer);
}

void Radiant_detachGameModeObserver (ModuleObserver& observer)
{
	g_gameModeObservers.detach(observer);
}

#include "os/dir.h"

/** Module loader functor class. This class is used to traverse a directory and
 * load each module into the GlobalModuleServer.
 */
class ModuleLoader
{
		// The path of the directory we are in
		const std::string _path;

		// The filename extension which indicates a module (platform-specific)
		const std::string _ext;

	public:
		ModuleLoader (const std::string& path) :
			_path(path),
#if defined(WIN32)
					_ext("dll")
#elif defined (__APPLE__)
					_ext("dylib")
#elif defined(__linux__) || defined (__FreeBSD__)
					_ext("so")
#endif

		{
		}
		void operator() (const std::string& name) const
		{
			if (os::getExtension(name) == _ext) {
				std::string fullname = _path + name;
				g_message("Found '%s'\n", fullname.c_str());
				GlobalModuleServer_loadModule(fullname);
			}
		}
};

/** Load modules from a specified directory.
 *
 * @param path
 * The directory path to load from.
 */
void Radiant_loadModules (const std::string& path)
{
	ModuleLoader loader(path);
	Directory_forEach(path, loader);
}

/** Load all of the modules in the DarkRadiant install directory.
 * Plugins are loaded from plugins/.
 *
 * @param directory
 * The root directory to search.
 */
void Radiant_loadModulesFromRoot (const std::string& directory)
{
	const std::string g_pluginsDir = "plugins/";
	Radiant_loadModules(directory + g_pluginsDir);
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

void populateRegistry ()
{
	// Load default values for darkradiant, located in the game directory
	const std::string base = std::string(AppPath_get()) + "user.xml";
	const std::string input = std::string(AppPath_get()) + "input.xml";

	if (file_exists(base)) {
		GlobalRegistry().importFromFile(base, "");

		// Try to load the default input definitions
		if (file_exists(input.c_str())) {
			GlobalRegistry().importFromFile(input, "user/ui");
		} else {
			gtkutil::errorDialog(std::string("Could not find default input definitions:\n") + input);
		}
	} else {
		gtkutil::fatalErrorDialog(std::string("Could not find base registry:\n") + base);
	}

	// Construct the filename and load it into the registry
	const std::string filename = AppPath_get() + "games/ufoai.game";
	GlobalRegistry().importFromFile(filename, "");

	// Load user preferences, these overwrite any values that have defined before
	// The called method also checks for any upgrades that have to be performed
	const std::string userSettingsFile = SettingsPath_get() + "user.xml";
	if (file_exists(userSettingsFile)) {
		GlobalRegistry().importFromFile(userSettingsFile, "");
	}

	const std::string userInputFile = SettingsPath_get() + "input.xml";
	if (file_exists(userInputFile)) {
		GlobalRegistry().importFromFile(userInputFile, "user/ui");
	}
}

// This is called from main() to start up the Radiant stuff.
void Radiant_Initialise (void)
{
	GlobalModuleServer_Initialise();

	// Load the Radiant plugins
	Radiant_loadModulesFromRoot(AppPath_get());

	Preferences_Load();

	// Load the other modules
	Radiant_Construct(GlobalModuleServer_get());

	g_gameToolsPathObservers.realise();
	g_gameModeObservers.realise();
	g_gameNameObservers.realise();

}

void Radiant_Shutdown (void)
{
	// Export the input definitions into the user's settings folder and remove them as well
	GlobalRegistry().exportToFile("user/ui/input", SettingsPath_get() + "input.xml");
	GlobalRegistry().deleteXPath("user/ui/input");

	// Save the whole /uforadiant/user tree to user.xml so that the current settings are preserved
	GlobalRegistry().exportToFile("user", SettingsPath_get() + "user.xml");

	g_gameNameObservers.unrealise();
	g_gameModeObservers.unrealise();
	g_gameToolsPathObservers.unrealise();

	if (!g_preferences_globals.disable_ini) {
		g_message("Start writing prefs\n");
		Preferences_Save();
		g_message("Done prefs\n");
	}

	Radiant_Destroy();

	GlobalModuleServer_Shutdown();
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

void deleteSelection (void)
{
	UndoableCommand undo("deleteSelected");
	Select_Delete();
}

void Map_ExportSelected (TextOutputStream& ostream)
{
	Map_ExportSelected(ostream, Map_getFormat(g_map));
}

void Map_ImportSelected (TextInputStream& istream)
{
	Map_ImportSelected(istream, Map_getFormat(g_map));
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
		SelectedFaces_copyTexture();
	} else {
		Selection_Copy();
	}
}

void Paste (void)
{
	if (GlobalSelectionSystem().areFacesSelected()) {
		SelectedFaces_pasteTexture();
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
	Vector3 mid;
	Select_GetMid(mid);
	Vector3 delta = vector3_snapped(camwnd.getCameraOrigin(), GetGridSize()) - mid;

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

typedef FreeCaller1<const BoolImportCallback&, &BoolFunctionExport<EdgeMode>::apply> EdgeModeApplyCaller;
EdgeModeApplyCaller g_edgeMode_button_caller;
BoolExportCallback g_edgeMode_button_callback(g_edgeMode_button_caller);
ToggleItem g_edgeMode_button(g_edgeMode_button_callback);

typedef FreeCaller1<const BoolImportCallback&, &BoolFunctionExport<VertexMode>::apply> VertexModeApplyCaller;
VertexModeApplyCaller g_vertexMode_button_caller;
BoolExportCallback g_vertexMode_button_callback(g_vertexMode_button_caller);
ToggleItem g_vertexMode_button(g_vertexMode_button_callback);

typedef FreeCaller1<const BoolImportCallback&, &BoolFunctionExport<FaceMode>::apply> FaceModeApplyCaller;
FaceModeApplyCaller g_faceMode_button_caller;
BoolExportCallback g_faceMode_button_callback(g_faceMode_button_caller);
ToggleItem g_faceMode_button(g_faceMode_button_callback);

void ComponentModeChanged (void)
{
	g_edgeMode_button.update();
	g_vertexMode_button.update();
	g_faceMode_button.update();
}

void ComponentMode_SelectionChanged (const Selectable& selectable)
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().countSelected() == 0) {
		SelectionSystem_DefaultMode();
		ComponentModeChanged();
	}
}

void SelectEdgeMode (void)
{
#if 0
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		GlobalSelectionSystem().Select(false);
	}
#endif

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

void SelectVertexMode (void)
{
#if 0
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		GlobalSelectionSystem().Select(false);
	}
#endif

	if (VertexMode()) {
		SelectionSystem_DefaultMode();
	} else if (GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eVertex);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void SelectFaceMode (void)
{
#if 0
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		GlobalSelectionSystem().Select(false);
	}
#endif

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

class CloneSelected: public scene::Graph::Walker
{
	public:
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.size() == 1)
				return true;

			if (!path.top().get().isRoot()) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected()) {
					return false;
				}
			}

			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.size() == 1)
				return;

			if (!path.top().get().isRoot()) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected()) {
					NodeSmartReference clone(Node_Clone(path.top()));
					Map_gatherNamespaced(clone);
					Node_getTraversable(path.parent().get())->insert(clone);
				}
			}
		}
};

void Scene_Clone_Selected (scene::Graph& graph)
{
	graph.traverse(CloneSelected());

	Map_mergeClonedNames();
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

void Selection_Clone (void)
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
		UndoableCommand undo("cloneSelected");

		Scene_Clone_Selected(GlobalSceneGraph());

		//NudgeSelection(eNudgeRight, GetGridSize(), GlobalXYWnd_getCurrentViewType());
		//NudgeSelection(eNudgeDown, GetGridSize(), GlobalXYWnd_getCurrentViewType());
	}
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
	NudgeSelection(eNudgeUp, GetGridSize(), GlobalXYWnd_getCurrentViewType());
}

void Selection_NudgeDown (void)
{
	UndoableCommand undo("nudgeSelectedDown");
	NudgeSelection(eNudgeDown, GetGridSize(), GlobalXYWnd_getCurrentViewType());
}

void Selection_NudgeLeft (void)
{
	UndoableCommand undo("nudgeSelectedLeft");
	NudgeSelection(eNudgeLeft, GetGridSize(), GlobalXYWnd_getCurrentViewType());
}

void Selection_NudgeRight (void)
{
	UndoableCommand undo("nudgeSelectedRight");
	NudgeSelection(eNudgeRight, GetGridSize(), GlobalXYWnd_getCurrentViewType());
}

void TranslateToolExport (const BoolImportCallback& importCallback)
{
	importCallback(GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eTranslate);
}

void RotateToolExport (const BoolImportCallback& importCallback)
{
	importCallback(GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eRotate);
}

void ScaleToolExport (const BoolImportCallback& importCallback)
{
	importCallback(GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eScale);
}

void DragToolExport (const BoolImportCallback& importCallback)
{
	importCallback(GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eDrag);
}

void ClipperToolExport (const BoolImportCallback& importCallback)
{
	importCallback(GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip);
}

void ShowSizeInfoExport (const BoolImportCallback& importCallback)
{
	importCallback(GlobalRegistry().get("user/ui/showSizeInfo") == "1");
}

FreeCaller1<const BoolImportCallback&, TranslateToolExport> g_translatemode_button_caller;
BoolExportCallback g_translatemode_button_callback(g_translatemode_button_caller);
ToggleItem g_translatemode_button(g_translatemode_button_callback);

FreeCaller1<const BoolImportCallback&, RotateToolExport> g_rotatemode_button_caller;
BoolExportCallback g_rotatemode_button_callback(g_rotatemode_button_caller);
ToggleItem g_rotatemode_button(g_rotatemode_button_callback);

FreeCaller1<const BoolImportCallback&, ScaleToolExport> g_scalemode_button_caller;
BoolExportCallback g_scalemode_button_callback(g_scalemode_button_caller);
ToggleItem g_scalemode_button(g_scalemode_button_callback);

FreeCaller1<const BoolImportCallback&, DragToolExport> g_dragmode_button_caller;
BoolExportCallback g_dragmode_button_callback(g_dragmode_button_caller);
ToggleItem g_dragmode_button(g_dragmode_button_callback);

FreeCaller1<const BoolImportCallback&, ClipperToolExport> g_clipper_button_caller;
BoolExportCallback g_clipper_button_callback(g_clipper_button_caller);
ToggleItem g_clipper_button(g_clipper_button_callback);

FreeCaller1<const BoolImportCallback&, ShowSizeInfoExport> g_showSizeInfoCaller;
BoolExportCallback g_showSizeInfoCallback(g_showSizeInfoCaller);
ToggleItem g_showSizeInfoButton(g_showSizeInfoCallback);

void ToolChanged (void)
{
	g_translatemode_button.update();
	g_rotatemode_button.update();
	g_scalemode_button.update();
	g_dragmode_button.update();
	g_clipper_button.update();
}

static const char* const c_ResizeMode_status = "QE4 Drag Tool";

void DragMode (void)
{
	if (g_currentToolMode == DragMode && g_defaultToolMode != DragMode) {
		g_defaultToolMode();
	} else {
		g_currentToolMode = DragMode;
		g_currentToolModeSupportsComponentEditing = true;

		GlobalClipPoints()->onClipMode(false);

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

		GlobalClipPoints()->onClipMode(false);

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

		GlobalClipPoints()->onClipMode(false);

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

		GlobalClipPoints()->onClipMode(false);

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

		GlobalClipPoints()->onClipMode(true);

		Sys_Status(c_ClipperMode_status);
		GlobalSelectionSystem().SetManipulatorMode(SelectionSystem::eClip);
		ToolChanged();
		ModeChangeNotify();
	}
}

// greebo: This toggles the brush/patch size info display in the ortho views
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
	Texdef_Rotate(static_cast<float> (fabs(g_si_globals.rotate)));
}

void Texdef_RotateAntiClockwise (void)
{
	Texdef_Rotate(static_cast<float> (-fabs(g_si_globals.rotate)));
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
	Texdef_Scale(0, g_si_globals.scale[1]);
}

void Texdef_ScaleDown (void)
{
	Texdef_Scale(0, -g_si_globals.scale[1]);
}

void Texdef_ScaleLeft (void)
{
	Texdef_Scale(-g_si_globals.scale[0], 0);
}

void Texdef_ScaleRight (void)
{
	Texdef_Scale(g_si_globals.scale[0], 0);
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
	Texdef_Shift(-g_si_globals.shift[0], 0);
}

void Texdef_ShiftRight (void)
{
	Texdef_Shift(g_si_globals.shift[0], 0);
}

void Texdef_ShiftUp (void)
{
	Texdef_Shift(0, g_si_globals.shift[1]);
}

void Texdef_ShiftDown (void)
{
	Texdef_Shift(0, -g_si_globals.shift[1]);
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

void Scene_SnapToGrid_Selected (scene::Graph& graph, float snap)
{
	graph.traverse(SnappableSnapToGridSelected(snap));
}

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
				// Check if the visited instance is ComponentSelectable
				ComponentSnappable* componentSnappable = Instance_getComponentSnappable(instance);
				// Call the snapComponents() method if the instance is also a _selected_ Selectable
				if (componentSnappable != 0 && Instance_getSelectable(instance)->isSelected()) {
					componentSnappable->snapComponents(m_snap);
				}
			}
			return true;
		}
};

void Scene_SnapToGrid_Component_Selected (scene::Graph& graph, float snap)
{
	graph.traverse(ComponentSnappableSnapToGridSelected(snap));
}

void Selection_SnapToGrid (void)
{
	StringOutputStream command;
	command << "snapSelected -grid " << GetGridSize();
	UndoableCommand undo(command.toString());

	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		Scene_SnapToGrid_Component_Selected(GlobalSceneGraph(), GetGridSize());
	} else {
		Scene_SnapToGrid_Selected(GlobalSceneGraph(), GetGridSize());
	}
}

/**
 * @brief Timer function that is called every second
 */
static gint qe_every_second (gpointer data)
{
	GdkModifierType mask;

	gdk_window_get_pointer(0, 0, 0, &mask);

	if ((mask & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) == 0) {
		QE_CheckAutoSave();
	}

	return TRUE;
}

static guint s_qe_every_second_id = 0;

void EverySecondTimer_enable (void)
{
	if (s_qe_every_second_id == 0) {
		s_qe_every_second_id = gtk_timeout_add(1000, qe_every_second, 0);
	}
}

static void EverySecondTimer_disable (void)
{
	if (s_qe_every_second_id != 0) {
		gtk_timeout_remove(s_qe_every_second_id);
		s_qe_every_second_id = 0;
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
		EverySecondTimer_disable();

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
		EverySecondTimer_enable();
		gtk_grab_remove(GTK_WIDGET(g_wait.m_window));
		destroy_floating_window(g_wait.m_window);
		g_wait.m_window = 0;
	} else if (GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		gtk_label_set_text(g_wait.m_label, g_wait_stack.back().c_str());
		ScreenUpdates_process();
	}
}

static void GlobalCamera_UpdateWindow (void)
{
	if (g_pParentWnd != 0) {
		g_pParentWnd->GetCamWnd()->update();
	}
}

void XY_UpdateWindow (MainFrame& mainframe)
{
	if (mainframe.GetXYWnd() != 0) {
		mainframe.GetXYWnd()->queueDraw();
	}
}

void XZ_UpdateWindow (MainFrame& mainframe)
{
	if (mainframe.GetXZWnd() != 0) {
		mainframe.GetXZWnd()->queueDraw();
	}
}

void YZ_UpdateWindow (MainFrame& mainframe)
{
	if (mainframe.GetYZWnd() != 0) {
		mainframe.GetYZWnd()->queueDraw();
	}
}

void XY_UpdateAllWindows (MainFrame& mainframe)
{
	XY_UpdateWindow(mainframe);
	XZ_UpdateWindow(mainframe);
	YZ_UpdateWindow(mainframe);
}

void XY_UpdateAllWindows (void)
{
	if (g_pParentWnd != 0) {
		XY_UpdateAllWindows(*g_pParentWnd);
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
LatchedBool g_Layout_enableDetachableMenus(false, _("Detachable Menus"));
static LatchedBool g_Layout_enablePluginToolbar(true, _("Plugin Toolbar"));

static GtkMenuItem* create_file_menu (MainFrame *mainFrame)
{
	// File menu
	GtkMenuItem* file_menu_item = new_sub_menu_item_with_mnemonic(_("_File"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(file_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	create_menu_item_with_mnemonic(menu, _("_New Map"), "NewMap");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("_Open..."), "OpenMap");
	create_menu_item_with_mnemonic(menu, _("_Import..."), "ImportMap");
	mainFrame->SetSaveMenuItem(create_menu_item_with_mnemonic(menu, _("_Save"), "SaveMap"));
	create_menu_item_with_mnemonic(menu, _("Save _as..."), "SaveMapAs");
	create_menu_item_with_mnemonic(menu, _("Save s_elected..."), "SaveSelected");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Save re_gion..."), "SaveRegion");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("_Refresh models"), "RefreshReferences");
	menu_separator(menu);
	MRU_constructMenu(menu);
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("E_xit"), "Exit");

	return file_menu_item;
}

static GtkMenuItem* create_edit_menu (MainFrame *mainFrame)
{
	// Edit menu
	GtkMenuItem* edit_menu_item = new_sub_menu_item_with_mnemonic(_("_Edit"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(edit_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);
	mainFrame->SetUndoMenuItem(create_menu_item_with_mnemonic(menu, _("_Undo"), "Undo"));
	mainFrame->SetRedoMenuItem(create_menu_item_with_mnemonic(menu, _("_Redo"), "Redo"));
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("_Copy"), "Copy");
	create_menu_item_with_mnemonic(menu, _("_Paste"), "Paste");
	create_menu_item_with_mnemonic(menu, _("P_aste To Camera"), "PasteToCamera");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("_Duplicate"), "CloneSelection");
	create_menu_item_with_mnemonic(menu, _("D_elete"), "DeleteSelection");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Pa_rent"), "ParentSelection");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("C_lear Selection"), "UnSelectSelection");
	create_menu_item_with_mnemonic(menu, _("_Invert Selection"), "InvertSelection");

	GtkMenu* convert_menu = create_sub_menu_with_mnemonic(menu, _("E_xpand Selection"));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(convert_menu);
	create_menu_item_with_mnemonic(convert_menu, _("Select i_nside"), "SelectInside");
	create_menu_item_with_mnemonic(convert_menu, _("Select _touching"), "SelectTouching");
	create_menu_item_with_mnemonic(convert_menu, _("To Whole _Entities"), "ExpandSelectionToEntities");
	create_menu_item_with_mnemonic(convert_menu, _("Select all of _same type"), "SelectAllOfType");

	create_menu_item_with_mnemonic(convert_menu, _("Select all Faces of same te_xture"), "SelectAllFacesOfTex");

	menu_separator(menu);

	create_menu_item_with_mnemonic(menu, _("Export Selected Brushes to _OBJ"), "BrushExportOBJ");

	menu_separator(menu);

	create_menu_item_with_mnemonic(menu, _("Shortcuts list"), FreeCaller<DoCommandListDlg> ());
	create_menu_item_with_mnemonic(menu, _("Pre_ferences..."), "Preferences");

	return edit_menu_item;
}

static GtkMenuItem* create_view_menu (MainFrame::EViewStyle style)
{
	// View menu
	GtkMenuItem* view_menu_item = new_sub_menu_item_with_mnemonic(_("Vie_w"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(view_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	create_menu_item_with_mnemonic(menu, _("Toggle Sidebar"), "ToggleSidebar");
	create_menu_item_with_mnemonic(menu, _("Toggle EntityInspector"), "ToggleEntityInspector");
	create_menu_item_with_mnemonic(menu, _("Toggle SurfaceInspector"), "ToggleSurfaceInspector");
	create_menu_item_with_mnemonic(menu, _("Toggle Prefabs"), "TogglePrefabs");

	menu_separator(menu);
	{
		GtkMenu* camera_menu = create_sub_menu_with_mnemonic(menu, _("Camera"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(camera_menu);
		create_menu_item_with_mnemonic(camera_menu, _("_Center"), "CenterView");
		create_menu_item_with_mnemonic(camera_menu, _("_Up Floor"), "UpFloor");
		create_menu_item_with_mnemonic(camera_menu, _("_Down Floor"), "DownFloor");
		menu_separator(camera_menu);
		create_menu_item_with_mnemonic(camera_menu, _("Far Clip Plane In"), "CubicClipZoomIn");
		create_menu_item_with_mnemonic(camera_menu, _("Far Clip Plane Out"), "CubicClipZoomOut");
		menu_separator(camera_menu);
		create_menu_item_with_mnemonic(camera_menu, _("Look Through Selected"), "LookThroughSelected");
		create_menu_item_with_mnemonic(camera_menu, _("Look Through Camera"), "LookThroughCamera");
	}

	menu_separator(menu);
	{
		GtkMenu* orthographic_menu = create_sub_menu_with_mnemonic(menu, _("Orthographic"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(orthographic_menu);
		if (style == MainFrame::eRegular) {
			create_menu_item_with_mnemonic(orthographic_menu, _("_Next (XY, YZ, YZ)"), "NextView");
			create_menu_item_with_mnemonic(orthographic_menu, _("XY (Top)"), "ViewTop");
			create_menu_item_with_mnemonic(orthographic_menu, _("XZ (Side)"), "ViewSide");
			create_menu_item_with_mnemonic(orthographic_menu, _("YZ (Front)"), "ViewFront");
			menu_separator(orthographic_menu);
		}

		create_menu_item_with_mnemonic(orthographic_menu, _("_XY 100%"), "Zoom100");
		create_menu_item_with_mnemonic(orthographic_menu, _("XY Zoom _In"), "ZoomIn");
		create_menu_item_with_mnemonic(orthographic_menu, _("XY Zoom _Out"), "ZoomOut");
	}

	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Show"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Toggle Grid"), "ToggleGrid");
		create_menu_item_with_mnemonic(menu_in_menu, _("Toggle Crosshairs"), "ToggleCrosshairs");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show _Angles"), "ShowAngles");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show _Names"), "ShowNames");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show Blocks"), "ShowBlocks");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show C_oordinates"), "ShowCoordinates");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show Window Outline"), "ShowWindowOutline");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show Axes"), "ShowAxes");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show Workzone"), "ShowWorkzone");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("Show Size Info"), "ToggleShowSizeInfo");
	}

	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Hide/Show"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Hide Selected"), "HideSelected");
		create_menu_item_with_mnemonic(menu_in_menu, _("Show Hidden"), "ShowHidden");
	}

	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Region"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("_Off"), "RegionOff");
		create_menu_item_with_mnemonic(menu_in_menu, _("_Set XY"), "RegionSetXY");
		create_menu_item_with_mnemonic(menu_in_menu, _("Set _Brush"), "RegionSetBrush");
		create_menu_item_with_mnemonic(menu_in_menu, _("Set Se_lected Brushes"), "RegionSetSelection");
	}

	menu_separator(menu);
	{
		GtkMenu *menuInMenu = create_sub_menu_with_mnemonic(menu, _("_Pathfinding"));
		Pathfinding_ConstructMenu(menuInMenu);
	}

	command_connect_accelerator("CenterXYViews");

	return view_menu_item;
}

static GtkMenuItem* create_selection_menu (void)
{
	// Selection menu
	GtkMenuItem* selection_menu_item = new_sub_menu_item_with_mnemonic(_("M_odify"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(selection_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Components"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_check_menu_item_with_mnemonic(menu_in_menu, _("_Edges"), "DragEdges");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("_Vertices"), "DragVertices");
		create_check_menu_item_with_mnemonic(menu_in_menu, _("_Faces"), "DragFaces");
	}

	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Nudge"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Nudge Left"), "SelectNudgeLeft");
		create_menu_item_with_mnemonic(menu_in_menu, _("Nudge Right"), "SelectNudgeRight");
		create_menu_item_with_mnemonic(menu_in_menu, _("Nudge Up"), "SelectNudgeUp");
		create_menu_item_with_mnemonic(menu_in_menu, _("Nudge Down"), "SelectNudgeDown");
	}
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Rotate"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Rotate X"), "RotateSelectionX");
		create_menu_item_with_mnemonic(menu_in_menu, _("Rotate Y"), "RotateSelectionY");
		create_menu_item_with_mnemonic(menu_in_menu, _("Rotate Z"), "RotateSelectionZ");
	}
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Flip"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Flip _X"), "MirrorSelectionX");
		create_menu_item_with_mnemonic(menu_in_menu, _("Flip _Y"), "MirrorSelectionY");
		create_menu_item_with_mnemonic(menu_in_menu, _("Flip _Z"), "MirrorSelectionZ");
	}

	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Arbitrary rotation..."), "ArbitraryRotation");
	create_menu_item_with_mnemonic(menu, _("Arbitrary scale..."), "ArbitraryScale");

	return selection_menu_item;
}

static GtkMenuItem* create_grid_menu (void)
{
	// Grid menu
	GtkMenuItem* grid_menu_item = new_sub_menu_item_with_mnemonic(_("_Grid"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(grid_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	Grid_constructMenu(menu);

	return grid_menu_item;
}

void CallBrushExportOBJ ()
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		export_selected(GlobalRadiant().getMainWindow());
	} else {
		gtkutil::errorDialog(_("No Brushes Selected!"));
	}
}

static GtkMenuItem* create_misc_menu (void)
{
	// Misc menu
	GtkMenuItem* misc_menu_item = new_sub_menu_item_with_mnemonic(_("M_isc"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(misc_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(create_colours_menu()));

	create_menu_item_with_mnemonic(menu, _("Find brush..."), "FindBrush");
	create_menu_item_with_mnemonic(menu, _("_Background select"), FreeCaller<WXY_BackgroundSelect> ());
	create_menu_item_with_mnemonic(menu, _("_Benchmark"), FreeCaller<GlobalCamera_Benchmark>());

	return misc_menu_item;
}

static GtkMenuItem* create_map_menu (void)
{
	// Map menu
	GtkMenuItem* map_menu_item = new_sub_menu_item_with_mnemonic(_("Map"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(map_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Compile"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Check for errors"), "ToolsCheckErrors");
		create_menu_item_with_mnemonic(menu_in_menu, _("Compile the map"), "ToolsCompile");
		create_menu_item_with_mnemonic(menu_in_menu, _("Generate materials"), "ToolsGenerateMaterials");
	}
	create_menu_item_with_mnemonic(menu, _("Edit UMP"), "EditUMPDefinition");
	{
		GtkMenuItem* menuItem = new_sub_menu_item_with_mnemonic(_("RMA tiles"));
		container_add_widget(GTK_CONTAINER(menu), GTK_WIDGET(menuItem));
		GtkMenu* menu_in_menu = GTK_MENU(gtk_menu_item_get_submenu(menuItem));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		UMP_constructMenu(menuItem, menu_in_menu);
	}
	create_menu_item_with_mnemonic(menu, _("Edit material"), "ShowMaterialDefinition");
	create_menu_item_with_mnemonic(menu, _("Edit terrain definition"), "EditTerrainDefinition");
	create_menu_item_with_mnemonic(menu, _("Edit map definition"), "EditMapDefinition");
	return map_menu_item;
}

static GtkMenuItem* create_tools_menu (void)
{
	// Tools menu
	GtkMenuItem* tools_menu_item = new_sub_menu_item_with_mnemonic(_("Tools"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(tools_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	create_menu_item_with_mnemonic(menu, _("Find/replace texture"), "FindReplaceTextures");
	create_check_menu_item_with_mnemonic(menu, _("Play Sounds"), "PlaySounds");
	create_menu_item_with_mnemonic(menu, _("One level up"), "ObjectsUp");
	create_menu_item_with_mnemonic(menu, _("One level down"), "ObjectsDown");

	return tools_menu_item;
}

static GtkMenuItem* create_entity_menu (void)
{
	// Brush menu
	GtkMenuItem* entity_menu_item = new_sub_menu_item_with_mnemonic(_("E_ntity"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(entity_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	Entity_constructMenu(menu);

	return entity_menu_item;
}

static GtkMenuItem* create_brush_menu (void)
{
	// Brush menu
	GtkMenuItem* brush_menu_item = new_sub_menu_item_with_mnemonic(_("B_rush"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(brush_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	Brush_constructMenu(menu);

	return brush_menu_item;
}

static GtkMenuItem* create_help_menu (void)
{
	// Help menu
	GtkMenuItem* help_menu_item = new_sub_menu_item_with_mnemonic(_("_Help"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(help_menu_item));
	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff(menu);

	create_menu_item_with_mnemonic(menu, _("Manual"), "OpenManual");

	create_menu_item_with_mnemonic(menu, _("Bug report"), FreeCaller<OpenBugReportURL> ());
	create_menu_item_with_mnemonic(menu, _("_About"), FreeCaller<DoAbout> ());

	return help_menu_item;
}

static GtkMenuBar* create_main_menu (MainFrame *mainframe)
{
	GtkMenuBar* menu_bar = GTK_MENU_BAR(gtk_menu_bar_new());
	gtk_widget_show(GTK_WIDGET(menu_bar));

	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_file_menu(mainframe)));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_edit_menu(mainframe)));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_view_menu(mainframe->CurrentStyle())));

	// Filters menu
	ui::FiltersMenu filtersMenu;
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), filtersMenu);

	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_selection_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_grid_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_misc_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_map_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_tools_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_entity_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_brush_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_plugins_menu()));
	gtk_container_add(GTK_CONTAINER(menu_bar), GTK_WIDGET(create_help_menu()));

	return menu_bar;
}

static void Manipulators_registerShortcuts (void)
{
	toggle_add_accelerator("MouseRotate");
	toggle_add_accelerator("MouseTranslate");
	toggle_add_accelerator("MouseScale");
	toggle_add_accelerator("MouseDrag");
	toggle_add_accelerator("ToggleClipper");
}

static void TexdefNudge_registerShortcuts (void)
{
	command_connect_accelerator("TexRotateClock");
	command_connect_accelerator("TexRotateCounter");
	command_connect_accelerator("TexScaleUp");
	command_connect_accelerator("TexScaleDown");
	command_connect_accelerator("TexScaleLeft");
	command_connect_accelerator("TexScaleRight");
	command_connect_accelerator("TexShiftUp");
	command_connect_accelerator("TexShiftDown");
	command_connect_accelerator("TexShiftLeft");
	command_connect_accelerator("TexShiftRight");
}

static void SelectNudge_registerShortcuts (void)
{
	command_connect_accelerator("MoveSelectionDOWN");
	command_connect_accelerator("MoveSelectionUP");
	//command_connect_accelerator("SelectNudgeLeft");
	//command_connect_accelerator("SelectNudgeRight");
	//command_connect_accelerator("SelectNudgeUp");
	//command_connect_accelerator("SelectNudgeDown");
}

static void SnapToGrid_registerShortcuts (void)
{
	command_connect_accelerator("SnapToGrid");
}

static void SelectByType_registerShortcuts (void)
{
	command_connect_accelerator("SelectAllOfType");
}

static void SurfaceInspector_registerShortcuts (void)
{
	command_connect_accelerator("FitTexture");
}

void register_shortcuts (void)
{
	Grid_registerShortcuts();
	XYWnd_registerShortcuts();
	CamWnd_registerShortcuts();
	Manipulators_registerShortcuts();
	SurfaceInspector_registerShortcuts();
	TexdefNudge_registerShortcuts();
	SelectNudge_registerShortcuts();
	SnapToGrid_registerShortcuts();
	SelectByType_registerShortcuts();
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

SignalHandlerId XYWindowDestroyed_connect (const SignalHandler& handler)
{
	return g_pParentWnd->GetXYWnd()->onDestroyed.connectFirst(handler);
}

void XYWindowDestroyed_disconnect (SignalHandlerId id)
{
	g_pParentWnd->GetXYWnd()->onDestroyed.disconnect(id);
}

MouseEventHandlerId XYWindowMouseDown_connect (const MouseEventHandler& handler)
{
	return g_pParentWnd->GetXYWnd()->onMouseDown.connectFirst(handler);
}

void XYWindowMouseDown_disconnect (MouseEventHandlerId id)
{
	g_pParentWnd->GetXYWnd()->onMouseDown.disconnect(id);
}

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
	m_pXYWnd = 0;
	m_pCamWnd = 0;
	m_pYZWnd = 0;
	m_pXZWnd = 0;
	m_pActiveXY = 0;

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

void MainFrame::SetActiveXY (XYWnd* p)
{
	if (m_pActiveXY)
		m_pActiveXY->SetActive(false);

	m_pActiveXY = p;

	if (m_pActiveXY)
		m_pActiveXY->SetActive(true);

}

// Create and show the splash screen.

static const char *SPLASH_FILENAME = "uforadiantsplash.png";

GtkWindow* create_splash ()
{
	GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_decorated(window, FALSE);
	gtk_window_set_resizable(window, FALSE);
	gtk_window_set_modal(window, TRUE);
	gtk_window_set_default_size(window, -1, -1);
	gtk_window_set_position(window, GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(window), 0);

	GtkWidget* image = gtkutil::getImage(SPLASH_FILENAME);
	gtk_widget_show(image);
	gtk_container_add(GTK_CONTAINER(window), image);

	gtk_widget_set_size_request(GTK_WIDGET(window), -1, -1);
	gtk_widget_show(GTK_WIDGET(window));

	return window;
}

static GtkWindow *splash_screen = 0;

void show_splash ()
{
	splash_screen = create_splash();

	process_gui();
}

void hide_splash ()
{
	gtk_widget_destroy(GTK_WIDGET(splash_screen));
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

	GlobalWindowObservers_connectTopLevel(window);

#if !defined(WIN32)
	{
		GdkPixbuf* pixbuf = gtkutil::getLocalPixbuf(ui::icons::ICON);
		if (pixbuf != 0) {
			gtk_window_set_icon(window, pixbuf);
			gdk_pixbuf_unref(pixbuf);
		}
	}
#endif

	GtkWidget *notebook = Sidebar_construct();

	gtk_widget_add_events(GTK_WIDGET(window), GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(mainframe_delete), this);

	m_position_tracker.connect(window);

	g_MainWindowActive.connect(window);

	GetPlugInMgr().Init(GTK_WIDGET(window));

	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	global_accel_connect_window(window);

	m_nCurrentStyle = (EViewStyle) g_Layout_viewStyle.m_value;

	register_shortcuts();

	GtkMenuBar* main_menu = create_main_menu(this);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(main_menu), FALSE, FALSE, 0);

	// Instantiate the ToolbarCreator and retrieve the standard toolbar widget
	ui::ToolbarCreator toolbarCreator;

	GtkToolbar* generalToolbar = toolbarCreator.getToolbar("view");
	gtk_widget_show(GTK_WIDGET(generalToolbar));

	// Pack it into the main window
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(generalToolbar), FALSE, FALSE, 0);

	GtkToolbar* plugin_toolbar = create_plugin_toolbar();
	if (!g_Layout_enablePluginToolbar.m_value) {
		gtk_widget_hide(GTK_WIDGET(plugin_toolbar));
	}
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(plugin_toolbar), FALSE, FALSE, 0);

	GtkWidget* main_statusbar = create_main_statusbar(m_pStatusLabel);
	gtk_box_pack_end(GTK_BOX(vbox), main_statusbar, FALSE, TRUE, 2);

	GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	GtkToolbar* main_toolbar_v = toolbarCreator.getToolbar("edit");
	gtk_widget_show(GTK_WIDGET(main_toolbar_v));

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(main_toolbar_v), FALSE, FALSE, 0);
#ifdef DEBUG
	gint width, height,posx,posy;
	int stateBackup = g_layout_globals.nState;
	g_message("recorded window state: %i\n",g_layout_globals.nState);
#endif
	if ((g_layout_globals.nState & GDK_WINDOW_STATE_MAXIMIZED)) {
#ifdef DEBUG
		g_message("trying to maximize, recorded size was %ix%i@%ix%iy (not used)\n",g_layout_globals.m_position.w,g_layout_globals.m_position.h,g_layout_globals.m_position.x,g_layout_globals.m_position.y);
#endif
		/* set stored position and default height/width, otherwise problems with extended screens */
		g_layout_globals.m_position.h = 800;
		g_layout_globals.m_position.w = 600;
#ifdef DEBUG
		gtk_window_get_size(window, &width,&height);
		gtk_window_get_position(window,&posx,&posy);
		g_message("actual size: %ix%i@%ix%iy; setting size + position\n",width,height,posx,posy);
#endif
		window_set_position(window, g_layout_globals.m_position);
#ifdef DEBUG
		gtk_window_get_size(window, &width,&height);
		gtk_window_get_position(window,&posx,&posy);
		g_message("size after set: %ix%i@%ix%iy; maximize now\n",width,height,posx,posy);
#endif
		/* maximize will be done when window is shown */
		gtk_window_maximize(window);
#ifdef DEBUG
		gtk_window_get_size(window, &width,&height);
		gtk_window_get_position(window,&posx,&posy);
		g_message("size after maximize call: %ix%i@%ix%iy;could be that this is same as previous\n",width,height,posx,posy);
#endif
	} else {
		window_set_position(window, g_layout_globals.m_position);
	}

	m_window = window;

	gtk_widget_show(GTK_WIDGET(window));
#ifdef DEBUG
	gtk_window_get_size(window, &width,&height);
	gtk_window_get_position(window,&posx,&posy);
	int state = gdk_window_get_state(GTK_WIDGET(window)->window);
	g_message("size after show: %ix%i@%ix%iy state %i;\n",width,height,posx,posy,state);
	if (state != stateBackup)
	if (stateBackup & GDK_WINDOW_STATE_MAXIMIZED) {
		gtk_window_maximize(window);
		g_message("another try to maximize\n");
	}
	state = gdk_window_get_state(GTK_WIDGET(window)->window);
	g_message("state after retry maximize: %i;\n",state);
#endif

	GtkWidget* mainHBox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), mainHBox, TRUE, TRUE, 0);
	gtk_widget_show(mainHBox);

	int w, h;
	gtk_window_get_size(window, &w, &h);

	// create edit windows according to user setable style
	switch (CurrentStyle()) {
	case eRegular: {
		GtkWidget* vsplit = gtk_vpaned_new();
		m_vSplit = vsplit;
		gtk_box_pack_start(GTK_BOX(mainHBox), vsplit, TRUE, TRUE, 0);
		gtk_widget_show(vsplit);

		// console
		GtkWidget* console_window = Console_constructWindow(window);
		gtk_paned_pack2(GTK_PANED(vsplit), console_window, FALSE, TRUE);

		{
			GtkWidget* hsplit = gtk_hpaned_new();
			gtk_widget_show(hsplit);
			m_hSplit = hsplit;
			gtk_paned_add1(GTK_PANED(vsplit), hsplit);

			// xy
			m_pXYWnd = new XYWnd();
			m_pXYWnd->setViewType(XY);
			GtkWidget* xy_window = GTK_WIDGET(create_framed_widget(m_pXYWnd->getWidget()));

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

				// textures
				//GtkFrame* texture_window = create_framed_widget(TextureBrowser_constructNotebookTab(window,false));
				//gtk_paned_add2(GTK_PANED(vsplit2), GTK_WIDGET(texture_window));
			}
		}
		gtk_paned_set_position(GTK_PANED(m_vSplit), g_layout_globals.nXYHeight);
		gtk_paned_set_position(GTK_PANED(m_hSplit), g_layout_globals.nXYWidth);
		gtk_paned_set_position(GTK_PANED(m_vSplit2), g_layout_globals.nCamHeight);
		break;
	}

	case eSplit: {
		GtkWidget* vsplit = gtk_vpaned_new();
		m_vSplit = vsplit;
		gtk_box_pack_start(GTK_BOX(mainHBox), vsplit, TRUE, TRUE, 0);
		gtk_widget_show(vsplit);

		// camera
		m_pCamWnd = GlobalCamera().newCamWnd();
		GlobalCamera().setCamWnd(m_pCamWnd);
		GlobalCamera().setParent(m_pCamWnd, window);
		GtkWidget* camera = m_pCamWnd->getWidget();

		// yz window
		m_pYZWnd = new XYWnd();
		m_pYZWnd->setViewType(YZ);
		GtkWidget* yz = m_pYZWnd->getWidget();

		// xy window
		m_pXYWnd = new XYWnd();
		m_pXYWnd->setViewType(XY);
		GtkWidget* xy = m_pXYWnd->getWidget();

		// xz window
		m_pXZWnd = new XYWnd();
		m_pXZWnd->setViewType(XZ);
		GtkWidget* xz = m_pXZWnd->getWidget();

		// split view (4 views)
		GtkHPaned* split = create_split_views(camera, yz, xy, xz);
		gtk_paned_pack1(GTK_PANED(m_vSplit), GTK_WIDGET(split), TRUE, TRUE);

		// console
		GtkWidget* console_window = Console_constructWindow(window);
		gtk_paned_pack2(GTK_PANED(m_vSplit), GTK_WIDGET(console_window), TRUE, TRUE);

		// set height of the upper split view (seperates the 4 views and the console)
		gtk_paned_set_position(GTK_PANED(m_vSplit), g_layout_globals.nXYHeight);

		break;
	}
	default:
		gtkutil::errorDialog(_("Invalid layout type set - remove your radiant config files and retry"));
	}

	gtk_box_pack_start(GTK_BOX(mainHBox), GTK_WIDGET(notebook), FALSE, FALSE, 0);

	SetActiveXY(m_pXYWnd);

	PreferencesDialog_constructWindow(window);
	FindTextureDialog_constructWindow(window);

	AddGridChangeCallback(ReferenceCaller<MainFrame, XY_UpdateAllWindows> (*this));

	g_defaultToolMode = DragMode;
	g_defaultToolMode();
	SetStatusText(m_command_status, c_TranslateMode_status);

	/* enable button state tracker, set default states for begin */
	GlobalUndoSystem().trackerAttach(m_saveStateTracker);
	gtk_widget_set_sensitive(GTK_WIDGET(this->GetSaveMenuItem()), false);
	//gtk_widget_set_sensitive(GTK_WIDGET(this->GetSaveButton()), false);

	gtk_widget_set_sensitive(GTK_WIDGET(this->GetUndoMenuItem()), false);
	//gtk_widget_set_sensitive(GTK_WIDGET(this->GetUndoButton()), false);

	gtk_widget_set_sensitive(GTK_WIDGET(this->GetRedoMenuItem()), false);
	//gtk_widget_set_sensitive(GTK_WIDGET(this->GetRedoButton()), false);

	EverySecondTimer_enable();

	GlobalShortcuts_reportUnregistered();
}

void MainFrame::SaveWindowInfo (void)
{
	if (CurrentStyle() == eRegular) {
		g_layout_globals.nXYWidth = gtk_paned_get_position(GTK_PANED(m_hSplit));
		g_layout_globals.nXYHeight = gtk_paned_get_position(GTK_PANED(m_vSplit));
		g_layout_globals.nCamHeight = gtk_paned_get_position(GTK_PANED(m_vSplit2));
	} else {
		g_layout_globals.nXYHeight = gtk_paned_get_position(GTK_PANED(m_vSplit));
	}

	g_layout_globals.m_position = m_position_tracker.getPosition();
	g_layout_globals.nState = gdk_window_get_state(GTK_WIDGET(m_window)->window);
}

void MainFrame::Shutdown (void)
{
	EverySecondTimer_disable();

	GlobalUndoSystem().trackerDetach(m_saveStateTracker);
	delete m_pXYWnd;
	m_pXYWnd = 0;
	delete m_pYZWnd;
	m_pYZWnd = 0;
	delete m_pXZWnd;
	m_pXZWnd = 0;

	GlobalCamera().deleteCamWnd(m_pCamWnd);
	m_pCamWnd = 0;

	PreferencesDialog_destroyWindow();
	FindTextureDialog_destroyWindow();
}

/**
 * @brief Updates the statusbar text with command, position, texture and so on
 * @sa TextureBrowser_SetStatus
 */
void MainFrame::RedrawStatusText (void)
{
	gtk_label_set_text(GTK_LABEL(m_pStatusLabel[c_command_status]), m_command_status.c_str());
	gtk_label_set_text(GTK_LABEL(m_pStatusLabel[c_position_status]), m_position_status.c_str());
	gtk_label_set_text(GTK_LABEL(m_pStatusLabel[c_brushcount_status]), m_brushcount_status.c_str());
	gtk_label_set_text(GTK_LABEL(m_pStatusLabel[c_texture_status]), m_texture_status.c_str());
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

void Layout_constructPreferences (PreferencesPage& page)
{
	{
		const char* layouts[] = { ui::icons::ICON_WINDOW_REGULAR.c_str(), ui::icons::ICON_WINDOW_SPLIT.c_str() };
		page.appendRadioIcons(_("Window Layout"), STRING_ARRAY_RANGE(layouts), LatchedIntImportCaller(
				g_Layout_viewStyle), IntExportCaller(g_Layout_viewStyle.m_latched));
	}
	page.appendCheckBox("", _("Detachable Menus"), LatchedBoolImportCaller(g_Layout_enableDetachableMenus),
			BoolExportCaller(g_Layout_enableDetachableMenus.m_latched));
	page.appendCheckBox("", _("Plugin Toolbar"), LatchedBoolImportCaller(g_Layout_enablePluginToolbar),
			BoolExportCaller(g_Layout_enablePluginToolbar.m_latched));
}

void Layout_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Layout"), _("Layout Preferences")));
	Layout_constructPreferences(page);
}

void Layout_registerPreferencesPage (void)
{
	PreferencesDialog_addInterfacePage(FreeCaller1<PreferenceGroup&, Layout_constructPage> ());
}

#include "preferencesystem.h"
#include "stringio.h"

void MainFrame_Construct (void)
{
	GlobalRadiant().commandInsert("OpenManual", FreeCaller<OpenHelpURL> (), Accelerator(GDK_F1));

	GlobalCommands_insert("NewMap", FreeCaller<NewMap> ());
	GlobalRadiant().commandInsert("OpenMap", FreeCaller<OpenMap> (), Accelerator('O',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("ImportMap", FreeCaller<ImportMap> ());
	GlobalRadiant().commandInsert("SaveMap", FreeCaller<SaveMap> (), Accelerator('S',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("SaveMapAs", FreeCaller<SaveMapAs> ());
	GlobalCommands_insert("SaveSelected", FreeCaller<ExportMap> ());
	GlobalCommands_insert("SaveRegion", FreeCaller<SaveRegion> ());
	GlobalCommands_insert("RefreshReferences", FreeCaller<RefreshReferences> ());
	GlobalCommands_insert("Exit", FreeCaller<Exit> ());

	GlobalRadiant().commandInsert("Undo", FreeCaller<Undo> (), Accelerator('Z', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Redo", FreeCaller<Redo> (), Accelerator('Y', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Copy", FreeCaller<Copy> (), Accelerator('C', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Paste", FreeCaller<Paste> (), Accelerator('V', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("PasteToCamera", FreeCaller<PasteToCamera> (), Accelerator('V',
			(GdkModifierType) GDK_MOD1_MASK));
	GlobalRadiant().commandInsert("CloneSelection", FreeCaller<Selection_Clone> (), Accelerator(GDK_space));
	GlobalRadiant().commandInsert("DeleteSelection", FreeCaller<deleteSelection> (), Accelerator(GDK_BackSpace));
	GlobalCommands_insert("ParentSelection", FreeCaller<Scene_parentSelected> ());
	GlobalRadiant().commandInsert("UnSelectSelection", FreeCaller<Selection_Deselect> (), Accelerator(GDK_Escape));
	GlobalRadiant().commandInsert("InvertSelection", FreeCaller<Select_Invert> (), Accelerator('I'));
	GlobalCommands_insert("SelectInside", FreeCaller<Select_Inside> ());
	GlobalCommands_insert("SelectTouching", FreeCaller<Select_Touching> ());
	GlobalRadiant().commandInsert("ExpandSelectionToEntities", FreeCaller<Scene_ExpandSelectionToEntities> (),
			Accelerator('E', (GdkModifierType) (GDK_MOD1_MASK | GDK_CONTROL_MASK)));
	GlobalCommands_insert("Preferences", FreeCaller<PreferencesDialog_showDialog> ());

	GlobalRadiant().commandInsert("ShowHidden", FreeCaller<Select_ShowAllHidden> (), Accelerator('H',
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("HideSelected", FreeCaller<HideSelected> (), Accelerator('H'));

	GlobalToggles_insert("DragVertices", FreeCaller<SelectVertexMode> (), ToggleItem::AddCallbackCaller(
			g_vertexMode_button), Accelerator('V'));
	GlobalToggles_insert("DragEdges", FreeCaller<SelectEdgeMode> (), ToggleItem::AddCallbackCaller(g_edgeMode_button),
			Accelerator('E'));
	GlobalToggles_insert("DragFaces", FreeCaller<SelectFaceMode> (), ToggleItem::AddCallbackCaller(g_faceMode_button),
			Accelerator('F'));

	GlobalCommands_insert("MirrorSelectionX", FreeCaller<Selection_Flipx> ());
	GlobalCommands_insert("RotateSelectionX", FreeCaller<Selection_Rotatex> ());
	GlobalCommands_insert("MirrorSelectionY", FreeCaller<Selection_Flipy> ());
	GlobalCommands_insert("RotateSelectionY", FreeCaller<Selection_Rotatey> ());
	GlobalCommands_insert("MirrorSelectionZ", FreeCaller<Selection_Flipz> ());
	GlobalCommands_insert("RotateSelectionZ", FreeCaller<Selection_Rotatez> ());

	GlobalCommands_insert("ArbitraryRotation", FreeCaller<DoRotateDlg> ());
	GlobalCommands_insert("ArbitraryScale", FreeCaller<DoScaleDlg> ());

	GlobalCommands_insert("FindBrush", FreeCaller<FindBrushOrEntity> ());

	GlobalCommands_insert("ToolsCheckErrors", FreeCaller<ToolsCheckErrors> ());
	GlobalCommands_insert("ToolsCompile", FreeCaller<ToolsCompile> ());
	GlobalCommands_insert("ToolsGenerateMaterials", FreeCaller<ToolsGenerateMaterials> ());
	GlobalToggles_insert("PlaySounds", FreeCaller<GlobalSoundManager_switchPlaybackEnabledFlag> (),
			ToggleItem::AddCallbackCaller(g_soundPlaybackEnabled_button), accelerator_null());

	GlobalToggles_insert("ToggleShowSizeInfo", FreeCaller<ToggleShowSizeInfo> (), ToggleItem::AddCallbackCaller(
			g_showSizeInfoButton));

	GlobalToggles_insert("ToggleClipper", FreeCaller<ClipperMode> (), ToggleItem::AddCallbackCaller(g_clipper_button),
			Accelerator('X'));

	GlobalToggles_insert("MouseTranslate", FreeCaller<TranslateMode> (), ToggleItem::AddCallbackCaller(
			g_translatemode_button), Accelerator('W'));
	GlobalToggles_insert("MouseRotate", FreeCaller<RotateMode> (), ToggleItem::AddCallbackCaller(g_rotatemode_button),
			Accelerator('R'));
	GlobalToggles_insert("MouseScale", FreeCaller<ScaleMode> (), ToggleItem::AddCallbackCaller(g_scalemode_button));
	GlobalToggles_insert("MouseDrag", FreeCaller<DragMode> (), ToggleItem::AddCallbackCaller(g_dragmode_button),
			Accelerator('Q'));

	ColorScheme_registerCommands();

	GlobalRadiant().commandInsert("CSGSubtract", FreeCaller<CSG_Subtract> (), Accelerator('U',
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("CSGMerge", FreeCaller<CSG_Merge> (), Accelerator('U',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("CSGHollow", FreeCaller<CSG_MakeHollow> ());

	Grid_registerCommands();

	GlobalRadiant().commandInsert("SnapToGrid", FreeCaller<Selection_SnapToGrid> (), Accelerator('G',
			(GdkModifierType) GDK_CONTROL_MASK));

	GlobalRadiant().commandInsert("SelectAllOfType", FreeCaller<Select_AllOfType> (), Accelerator('A',
			(GdkModifierType) GDK_SHIFT_MASK));

	GlobalCommands_insert("SelectAllFacesOfTex", FreeCaller<Select_AllFacesWithTexture> ());

	GlobalRadiant().commandInsert("TexRotateClock", FreeCaller<Texdef_RotateClockwise> (), Accelerator(GDK_Next,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("TexRotateCounter", FreeCaller<Texdef_RotateAntiClockwise> (), Accelerator(GDK_Prior,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("TexScaleUp", FreeCaller<Texdef_ScaleUp> (), Accelerator(GDK_Up,
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("TexScaleDown", FreeCaller<Texdef_ScaleDown> (), Accelerator(GDK_Down,
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("TexScaleLeft", FreeCaller<Texdef_ScaleLeft> (), Accelerator(GDK_Left,
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("TexScaleRight", FreeCaller<Texdef_ScaleRight> (), Accelerator(GDK_Right,
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("TexShiftUp", FreeCaller<Texdef_ShiftUp> (), Accelerator(GDK_Up,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("TexShiftDown", FreeCaller<Texdef_ShiftDown> (), Accelerator(GDK_Down,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("TexShiftLeft", FreeCaller<Texdef_ShiftLeft> (), Accelerator(GDK_Left,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("TexShiftRight", FreeCaller<Texdef_ShiftRight> (), Accelerator(GDK_Right,
			(GdkModifierType) GDK_SHIFT_MASK));

	GlobalRadiant().commandInsert("MoveSelectionDOWN", FreeCaller<Selection_MoveDown> (), Accelerator(GDK_KP_Subtract));
	GlobalRadiant().commandInsert("MoveSelectionUP", FreeCaller<Selection_MoveUp> (), Accelerator(GDK_KP_Add));

	GlobalRadiant().commandInsert("SelectNudgeLeft", FreeCaller<Selection_NudgeLeft> (), Accelerator(GDK_Left,
			(GdkModifierType) GDK_MOD1_MASK));
	GlobalRadiant().commandInsert("SelectNudgeRight", FreeCaller<Selection_NudgeRight> (), Accelerator(GDK_Right,
			(GdkModifierType) GDK_MOD1_MASK));
	GlobalRadiant().commandInsert("SelectNudgeUp", FreeCaller<Selection_NudgeUp> (), Accelerator(GDK_Up,
			(GdkModifierType) GDK_MOD1_MASK));
	GlobalRadiant().commandInsert("SelectNudgeDown", FreeCaller<Selection_NudgeDown> (), Accelerator(GDK_Down,
			(GdkModifierType) GDK_MOD1_MASK));

	GlobalCommands_insert("BrushExportOBJ", FreeCaller<CallBrushExportOBJ> ());

	XYShow_registerCommands();
	LevelFilters_registerCommands();
	Model_RegisterToggles();

	typedef FreeCaller1<const Selectable&, ComponentMode_SelectionChanged> ComponentModeSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(ComponentModeSelectionChangedCaller());

	GlobalPreferenceSystem().registerPreference("DetachableMenus", BoolImportStringCaller(
			g_Layout_enableDetachableMenus.m_latched), BoolExportStringCaller(g_Layout_enableDetachableMenus.m_latched));
	GlobalPreferenceSystem().registerPreference("PluginToolBar", BoolImportStringCaller(
			g_Layout_enablePluginToolbar.m_latched), BoolExportStringCaller(g_Layout_enablePluginToolbar.m_latched));
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

#ifdef BINDIR
	StringOutputStream ufo2mapPath(256);
	ufo2mapPath << DirectoryCleaned(BINDIR) << "ufo2map";
	g_strCompilerBinaryWithPath = ufo2mapPath.toString();
#endif

	GlobalPreferenceSystem().registerPreference("EnginePath", StringImportStringCaller(g_strEnginePath),
			StringExportStringCaller(g_strEnginePath));
	GlobalPreferenceSystem().registerPreference("CompilerBinary",
			StringImportStringCaller(g_strCompilerBinaryWithPath),
			StringExportStringCaller(g_strCompilerBinaryWithPath));

	g_Layout_viewStyle.useLatched();
	g_Layout_enableDetachableMenus.useLatched();
	g_Layout_enablePluginToolbar.useLatched();

	Layout_registerPreferencesPage();
	Paths_registerPreferencesPage();

	g_brushCount.setCountChangedCallback(FreeCaller<QE_brushCountChanged> ());
	g_entityCount.setCountChangedCallback(FreeCaller<QE_entityCountChanged> ());
	GlobalEntityCreator().setCounter(&g_entityCount);

	GLWidget_sharedContextCreated = GlobalGL_sharedContextCreated;
	GLWidget_sharedContextDestroyed = GlobalGL_sharedContextDestroyed;

	ColorScheme_Init();
}

void MainFrame_Destroy (void)
{
	ColorScheme_Destroy();

	GlobalEntityCreator().setCounter(0);
	g_entityCount.setCountChangedCallback(Callback());
	g_brushCount.setCountChangedCallback(Callback());
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
	/* security, should not occur as tracker is attached while creation */
	if (g_pParentWnd == 0)
		return;

	const bool saveEnabled = m_savedStep < 0 || (m_undoSteps != static_cast<unsigned int> (m_savedStep));
	const bool undoEnabled = m_undoSteps > 0;
	const bool redoEnabled = m_redoSteps > 0;

	gtk_widget_set_sensitive(GTK_WIDGET(g_pParentWnd->GetSaveMenuItem()), saveEnabled);
	//gtk_widget_set_sensitive(GTK_WIDGET(g_pParentWnd->GetSaveButton()), saveEnabled);

	gtk_widget_set_sensitive(GTK_WIDGET(g_pParentWnd->GetUndoMenuItem()), undoEnabled);
	//gtk_widget_set_sensitive(GTK_WIDGET(g_pParentWnd->GetUndoButton()), undoEnabled);

	gtk_widget_set_sensitive(GTK_WIDGET(g_pParentWnd->GetRedoMenuItem()), redoEnabled);
	//gtk_widget_set_sensitive(GTK_WIDGET(g_pParentWnd->GetRedoButton()), redoEnabled);
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
