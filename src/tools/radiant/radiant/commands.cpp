#include "ui/mainframe/mainframe.h"
#include "radiant_i18n.h"

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
#include "ipathfinding.h"
#include "ishadersystem.h"
#include "igamemanager.h"
#include "iuimanager.h"
#include "itextures.h"
#include "igl.h"
#include "iump.h"
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
#include "referencecache/referencecache.h"

#include "gtkutil/clipboard.h"
#include "gtkutil/container.h"
#include "gtkutil/frame.h"
#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/widget.h"
#include "gtkutil/dialog.h"
#include "gtkutil/IconTextMenuToggle.h"
#include "gtkutil/menu.h"

#include "map/AutoSaver.h"
#include "map/MapCompileException.h"
#include "map/map.h"
#include "brush/brushmanip.h"
#include "brush/BrushModule.h"
#include "brush/csg/csg.h"
#include "brushexport/BrushExportOBJ.h"
#include "log/console.h"
#include "entity/entity.h"
#include "sidebar/sidebar.h"
#include "sidebar/surfaceinspector/surfaceinspector.h"
#include "ui/mainframe/SplitPaneLayout.h"
#include "ui/findshader/FindShader.h"
#include "ui/about/AboutDialog.h"
#include "ui/findbrush/findbrush.h"
#include "ui/textureoverview/TextureOverviewDialog.h"
#include "ui/maptools/ErrorCheckDialog.h"
#include "ui/colourscheme/ColourSchemeEditor.h"
#include "ui/colourscheme/ColourSchemeManager.h"
#include "ui/commandlist/CommandList.h"
#include "ui/transform/TransformDialog.h"
#include "ui/filterdialog/FilterDialog.h"
#include "ui/mru/MRU.h"
#include "ui/splash/Splash.h"
#include "ui/Icons.h"
#include "settings/PreferenceDialog.h"
#include "render/OpenGLRenderSystem.h"
#include "filters/LevelFilter.h"
#include "sound/SoundManager.h"
#include "clipper/Clipper.h"
#include "camera/CamWnd.h"
#include "camera/GlobalCamera.h"
#include "xyview/GlobalXYWnd.h"
#include "textool/TexTool.h"
#include "selection/algorithm/General.h"
#include "selection/algorithm/Group.h"
#include "selection/algorithm/Shader.h"
#include "selection/algorithm/Transformation.h"
#include "selection/shaderclipboard/ShaderClipboard.h"

#include "plugin.h"
#include "select.h"
#include "url.h"
#include "windowobservers.h"
#include "model.h"
#include "environment.h"

namespace {
bool g_currentToolModeSupportsComponentEditing = false;
}

void Exit (void)
{
	if (GlobalMap().askForSave(_("Exit Radiant"))) {
		gtk_main_quit();
	}
}

void Undo (void)
{
	GlobalUndoSystem().undo();
	SceneChangeNotify();
	GlobalShaderClipboard().clear();
}

void Redo (void)
{
	GlobalUndoSystem().redo();
	SceneChangeNotify();
	GlobalShaderClipboard().clear();
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
		selection::algorithm::pickShaderFromSelection();
	} else {
		Selection_Copy();
	}
}

void Cut (void)
{
	Copy();
	selection::algorithm::deleteSelection();
}

void Paste (void)
{
	if (GlobalSelectionSystem().areFacesSelected()) {
		selection::algorithm::pasteShaderToSelection();
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
	OpenURL("http://ufoai.org/wiki/index.php/Category:Mapping");
}

void OpenBugReportURL (void)
{
	OpenURL("http://sourceforge.net/tracker/?func=add&group_id=157793&atid=805242");
}

static void ModeChangeNotify (void)
{
	SceneChangeNotify();
}

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

void ToggleEdgeMode ()
{
	if (EdgeMode()) {
		SelectionSystem_DefaultMode();
	} else if (GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_pParentWnd->getDefaultToolMode()();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eEdge);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void ToggleVertexMode ()
{
	if (VertexMode()) {
		SelectionSystem_DefaultMode();
	} else if (GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_pParentWnd->getDefaultToolMode()();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eVertex);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void ToggleFaceMode ()
{
	if (FaceMode()) {
		SelectionSystem_DefaultMode();
	} else if (GlobalSelectionSystem().countSelected() != 0) {
		if (!g_currentToolModeSupportsComponentEditing) {
			g_pParentWnd->getDefaultToolMode()();
		}

		GlobalSelectionSystem().SetMode(SelectionSystem::eComponent);
		GlobalSelectionSystem().SetComponentMode(SelectionSystem::eFace);
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void CallBrushExportOBJ ()
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		export_selected(GlobalRadiant().getMainWindow());
	} else {
		gtkutil::errorDialog(_("No Brushes Selected!"));
	}
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

void ToolChanged ()
{
	GlobalEventManager().setToggled("ToggleClipper", GlobalClipper().clipMode());
	GlobalEventManager().setToggled("MouseTranslate", GlobalSelectionSystem().ManipulatorMode()
			== SelectionSystem::eTranslate);
	GlobalEventManager().setToggled("MouseRotate", GlobalSelectionSystem().ManipulatorMode()
			== SelectionSystem::eRotate);
	GlobalEventManager().setToggled("MouseScale", GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eScale);
	GlobalEventManager().setToggled("MouseDrag", GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eDrag);
}

static const char* const c_ResizeMode_status = "QE4 Drag Tool";

void DragMode (void)
{
	if (g_pParentWnd->getCurrentToolMode() == DragMode && g_pParentWnd->getDefaultToolMode() != DragMode) {
		g_pParentWnd->getDefaultToolMode()();
	} else {
		g_pParentWnd->setCurrentToolMode(DragMode);
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
	if (g_pParentWnd->getCurrentToolMode() == TranslateMode && g_pParentWnd->getDefaultToolMode() != TranslateMode) {
		g_pParentWnd->getDefaultToolMode()();
	} else {
		g_pParentWnd->setCurrentToolMode(TranslateMode);
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
	if (g_pParentWnd->getCurrentToolMode() == RotateMode && g_pParentWnd->getDefaultToolMode() != RotateMode) {
		g_pParentWnd->getDefaultToolMode()();
	} else {
		g_pParentWnd->setCurrentToolMode(RotateMode);
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
	if (g_pParentWnd->getCurrentToolMode() == ScaleMode && g_pParentWnd->getDefaultToolMode() != ScaleMode) {
		g_pParentWnd->getDefaultToolMode()();
	} else {
		g_pParentWnd->setCurrentToolMode(ScaleMode);
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
	if (g_pParentWnd->getCurrentToolMode() == ClipperMode && g_pParentWnd->getDefaultToolMode() != ClipperMode) {
		g_pParentWnd->getDefaultToolMode()();
	} else {
		g_pParentWnd->setCurrentToolMode(ClipperMode);
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

void TextureOverview ()
{
	new ui::TextureOverviewDialog;
}

void FindBrushOrEntity ()
{
	new ui::FindBrushDialog;
}

void EditColourScheme ()
{
	new ui::ColourSchemeEditor(); // self-destructs in GTK callback
}

class NullMapCompilerObserver: public ICompilerObserver
{
		void notify (const std::string &message)
		{
		}
};

void ToolsCompile ()
{
	if (!GlobalMap().askForSave(_("Compile Map")))
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

void ToolsCheckErrors ()
{
	if (!GlobalMap().askForSave(_("Check Map")))
		return;

	/* empty map? */
	if (!GlobalRadiant().getCounter(counterBrushes).get()) {
		gtkutil::errorDialog(_("Nothing to fix in this map\n"));
		return;
	}

	ui::ErrorCheckDialog::showDialog();
}

void ToolsGenerateMaterials ()
{
	if (!GlobalMap().askForSave(_("Generate Materials")))
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

void Commands_Register ()
{
	GlobalEventManager().addCommand("OpenManual", FreeCaller<OpenHelpURL> ());

	GlobalEventManager().addCommand("RefreshReferences", FreeCaller<RefreshReferences> ());
	GlobalEventManager().addCommand("Exit", FreeCaller<Exit> ());

	GlobalEventManager().addCommand("Undo", FreeCaller<Undo> ());
	GlobalEventManager().addCommand("Redo", FreeCaller<Redo> ());
	GlobalEventManager().addCommand("Cut", FreeCaller<Cut> ());
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
	GlobalEventManager().addCommand("ExpandSelectionToEntities", FreeCaller<
			selection::algorithm::expandSelectionToEntities> ());
	GlobalEventManager().addCommand("TextureNatural", FreeCaller<selection::algorithm::naturalTexture> ());
	GlobalEventManager().addCommand("Preferences", FreeCaller<PreferencesDialog_showDialog> ());

	GlobalEventManager().addCommand("ShowHidden", FreeCaller<selection::algorithm::showAllHidden> ());
	GlobalEventManager().addCommand("HideSelected", FreeCaller<selection::algorithm::hideSelected> ());

	GlobalEventManager().addToggle("DragVertices", FreeCaller<ToggleVertexMode> ());
	GlobalEventManager().addToggle("DragEdges", FreeCaller<ToggleEdgeMode> ());
	GlobalEventManager().addToggle("DragFaces", FreeCaller<ToggleFaceMode> ());
	GlobalEventManager().addToggle("DragEntities", FreeCaller<ToggleEntityMode> ());

	GlobalEventManager().setToggled("DragEntities", false);

	GlobalEventManager().addCommand("MirrorSelectionX", FreeCaller<Selection_Flipx> ());
	GlobalEventManager().addCommand("RotateSelectionX", FreeCaller<Selection_Rotatex> ());
	GlobalEventManager().addCommand("MirrorSelectionY", FreeCaller<Selection_Flipy> ());
	GlobalEventManager().addCommand("RotateSelectionY", FreeCaller<Selection_Rotatey> ());
	GlobalEventManager().addCommand("MirrorSelectionZ", FreeCaller<Selection_Flipz> ());
	GlobalEventManager().addCommand("RotateSelectionZ", FreeCaller<Selection_Rotatez> ());

	GlobalEventManager().addCommand("TransformDialog", FreeCaller<ui::TransformDialog::toggle> ());

	GlobalEventManager().addCommand("TextureOverview", FreeCaller<TextureOverview> ());
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

	GlobalEventManager().addCommand("CSGSubtractOrig", FreeCaller<CSG_SubtractOrig> ());
	GlobalEventManager().addCommand("CSGSubtract", FreeCaller<CSG_Subtract> ());
	GlobalEventManager().addCommand("CSGMerge", FreeCaller<CSG_Merge> ());
	GlobalEventManager().addCommand("CSGHollow", FreeCaller<CSG_MakeHollow> ());

	GlobalEventManager().addCommand("SnapToGrid", FreeCaller<Selection_SnapToGrid> ());

	GlobalEventManager().addCommand("SelectAllOfType", FreeCaller<selection::algorithm::selectAllOfType> ());

	GlobalEventManager().addCommand("SelectAllFacesOfTex",
			FreeCaller<selection::algorithm::selectAllFacesWithTexture> ());

	GlobalEventManager().addCommand("TexRotateClock", FreeCaller<selection::algorithm::rotateTextureClock> ());
	GlobalEventManager().addCommand("TexRotateCounter", FreeCaller<selection::algorithm::rotateTextureCounter> ());
	GlobalEventManager().addCommand("TexScaleUp", FreeCaller<selection::algorithm::scaleTextureUp> ());
	GlobalEventManager().addCommand("TexScaleDown", FreeCaller<selection::algorithm::scaleTextureDown> ());
	GlobalEventManager().addCommand("TexScaleLeft", FreeCaller<selection::algorithm::scaleTextureLeft> ());
	GlobalEventManager().addCommand("TexScaleRight", FreeCaller<selection::algorithm::scaleTextureRight> ());
	GlobalEventManager().addCommand("TexShiftUp", FreeCaller<selection::algorithm::shiftTextureUp> ());
	GlobalEventManager().addCommand("TexShiftDown", FreeCaller<selection::algorithm::shiftTextureDown> ());
	GlobalEventManager().addCommand("TexShiftLeft", FreeCaller<selection::algorithm::shiftTextureLeft> ());
	GlobalEventManager().addCommand("TexShiftRight", FreeCaller<selection::algorithm::shiftTextureRight> ());

	GlobalEventManager().addCommand("MoveSelectionDOWN", FreeCaller<Selection_MoveDown> ());
	GlobalEventManager().addCommand("MoveSelectionUP", FreeCaller<Selection_MoveUp> ());

	GlobalEventManager().addCommand("SelectNudgeLeft", FreeCaller<Selection_NudgeLeft> ());
	GlobalEventManager().addCommand("SelectNudgeRight", FreeCaller<Selection_NudgeRight> ());
	GlobalEventManager().addCommand("SelectNudgeUp", FreeCaller<Selection_NudgeUp> ());
	GlobalEventManager().addCommand("SelectNudgeDown", FreeCaller<Selection_NudgeDown> ());
	GlobalEventManager().addCommand("EditColourScheme", FreeCaller<EditColourScheme> ());

	GlobalEventManager().addCommand("BrushExportOBJ", FreeCaller<CallBrushExportOBJ> ());

	GlobalEventManager().addCommand("EditFiltersDialog", FreeCaller<ui::FilterDialog::showDialog> ());
	GlobalEventManager().addCommand("FindReplaceTextures", FreeCaller<ui::FindAndReplaceShader::showDialog> ());
	GlobalEventManager().addCommand("ShowCommandList", FreeCaller<ShowCommandListDialog> ());
	GlobalEventManager().addCommand("About", FreeCaller<ui::AboutDialog::showDialog> ());
	GlobalEventManager().addCommand("BugReport", FreeCaller<OpenBugReportURL> ());

	ui::TexTool::registerCommands();

	GlobalLevelFilter().registerCommands();

	typedef FreeCaller1<const Selectable&, ComponentMode_SelectionChanged> ComponentModeSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(ComponentModeSelectionChangedCaller());
}

void Commands_Init()
{
	g_pParentWnd->setDefaultToolMode(DragMode);
	DragMode();
	g_pParentWnd->SetStatusText(g_pParentWnd->m_command_status, c_TranslateMode_status);
}
