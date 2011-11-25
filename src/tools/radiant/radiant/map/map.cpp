/**
 * @file map.cpp
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

#include "map.h"
#include "radiant_i18n.h"

#include "imap.h"
#include "iselection.h"
#include "iundo.h"
#include "ibrush.h"
#include "ifiletypes.h"
#include "ieclass.h"
#include "irender.h"
#include "iradiant.h"
#include "ientity.h"
#include "ifilesystem.h"
#include "ieventmanager.h"
#include "inamespace.h"

#include <set>

#include "entitylib.h"
#include "os/path.h"

#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/dialog.h"
#include "../plugin.h"
#include "../filetypes.h"
#include "../environment.h"
#include "../sidebar/MapInfo.h"
#include "../camera/CamWnd.h"
#include "../xyview/GlobalXYWnd.h"
#include "../ui/mainframe/mainframe.h"
#include "../referencecache/referencecache.h"
#include "../ui/mru/MRU.h"
#include "AutoSaver.h"
#include "../brush/BrushNode.h"
#include "../ui/menu/UMPMenu.h"
#include "../selection/algorithm/General.h"
#include "../selection/shaderclipboard/ShaderClipboard.h"

#include "RootNode.h"
#include "MapFileChooserPreview.h"
#include "BasicContainer.h"
#include "MoveLevelWalker.h"

#include "algorithm/Traverse.h"
#include "algorithm/Merge.h"
#include "algorithm/Clone.h"

#include "RegionWalkers.h"

namespace map {

namespace {

std::string g_mapsPath;

class CollectAllWalker: public scene::Traversable::Walker
{
		scene::Node& m_root;
		UnsortedNodeSet& m_nodes;
	public:
		CollectAllWalker (scene::Node& root, UnsortedNodeSet& nodes) :
			m_root(root), m_nodes(nodes)
		{
		}
		bool pre (scene::Node& node) const
		{
			m_nodes.insert(NodeSmartReference(node));
			Node_getTraversable(m_root)->erase(node);
			return false;
		}
};

void Node_insertChildFirst (scene::Node& parent, scene::Node& child)
{
	UnsortedNodeSet nodes;
	Node_getTraversable(parent)->traverse(CollectAllWalker(parent, nodes));
	Node_getTraversable(parent)->insert(child);

	for (UnsortedNodeSet::iterator i = nodes.begin(); i != nodes.end(); ++i) {
		Node_getTraversable(parent)->insert((*i));
	}
}

scene::Node& createWorldspawn ()
{
	NodeSmartReference worldspawn(GlobalEntityCreator().createEntity(GlobalEntityClassManager().findOrInsert(
			"worldspawn", true)));
	Node_insertChildFirst(GlobalSceneGraph().root(), worldspawn);
	return worldspawn;
}

/* Walker class to subtract a Vector3 origin from each selected brush
 * that it visits.
 */

class BrushOriginSubtractor: public scene::Graph::Walker
{
		// The translation matrix from the vector3
		Matrix4 _transMat;

	public:

		// Constructor
		BrushOriginSubtractor (const Vector3& origin) :
			_transMat(Matrix4::getTranslation(origin * (-1)))
		{
		}

		// Pre visit function
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (Node_isPrimitive(path.top())) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected() && path.size() > 1) {
					return false;
				}
			}
			return true;
		}

		// Post visit function
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (Node_isPrimitive(path.top())) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected() && path.size() > 1) {
					// Node is selected, check if it is a brush.
					Brush* brush = Node_getBrush(path.top());
					if (brush != 0) {
						// We have a brush, apply the transformation
						brush->transform(_transMat);
						brush->freezeTransform();
					}
				}
			}
		} // post()

}; // BrushOriginSubtractor


/** Walker class to count the number of selected brushes in the current
 * scene.
 */

class CountSelectedBrushes: public scene::Graph::Walker
{
		int& m_count;
		mutable std::size_t m_depth;
	public:
		CountSelectedBrushes (int& count) :
			m_count(count), m_depth(0)
		{
			m_count = 0;
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (++m_depth != 1 && path.top().get().isRoot()) {
				return false;
			}
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected() && Node_isPrimitive(path.top())) {
				++m_count;
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
};

} // namespace

/* Subtract the given origin from all selected brushes in the map. Uses
 * a BrushOriginSubtractor walker class to subtract the origin from
 * each selected brush in the scene.
 */

void selectedBrushesSubtractOrigin (const Vector3& origin)
{
	GlobalSceneGraph().traverse(BrushOriginSubtractor(origin));
}

/* Return the number of selected brushes in the map, using the
 * CountSelectedBrushes walker.
 */

int countSelectedBrushes ()
{
	int count;
	GlobalSceneGraph().traverse(CountSelectedBrushes(count));
	return count;
}

scene::Node& Map::findOrInsertWorldspawn ()
{
	if (findWorldspawn() == 0) {
		setWorldspawn(&createWorldspawn());
	}
	return *getWorldspawn();
}

const std::string& getMapsPath ()
{
	return Environment::Instance().getMapsPath();
}

WorldNode::WorldNode () :
	m_node(0)
{
}
void WorldNode::set (scene::Node* node)
{
	if (m_node != 0)
		m_node->DecRef();
	m_node = node;
	if (m_node != 0)
		m_node->IncRef();
}
scene::Node* WorldNode::get () const
{
	return m_node;
}

Map::Map () :
	m_name(""), m_valid(false), m_modified(false), region_mins(-65536, -65536, -65536),
			region_maxs(65536, 65536, 65536), m_resource(0)
{
}

const Vector3& Map::getRegionMins () const
{
	return region_mins;
}

const Vector3& Map::getRegionMaxs () const
{
	return region_maxs;
}

bool Map::isUnnamed ()
{
	const std::string name = getName();
	return (name.empty() || name == "unnamed.map");
}

void Map::SetValid (bool valid)
{
	m_valid = valid;
	m_mapValidCallbacks();
}

void Map::realise ()
{
	if (m_resource != 0) {
		if (isUnnamed()) {
			m_resource->setNode(NewMapRoot("").get_pointer());
			MapFile* map = Node_getMapFile(*m_resource->getNode());
			if (map != 0) {
				map->save();
			}
		} else {
			m_resource->load();
		}

		GlobalSceneGraph().insert_root(*m_resource->getNode());

		map::AutoSaver().clearChanges();

		SetValid(true);
	}
}

void Map::unrealise ()
{
	if (m_resource != 0) {
		SetValid(false);
		setWorldspawn(0);

		GlobalUndoSystem().clear();

		GlobalSceneGraph().erase_root();
	}
}

void Map::addValidCallback (const SignalHandler& handler)
{
	m_mapValidCallbacks.connectLast(handler);
}

bool Map::isValid ()
{
	return m_valid;
}

const std::string& Map::getName () const
{
	return m_name;
}

inline const MapFormat& MapFormat_forFile (const std::string& filename)
{
	const std::string moduleName = findModuleName(GetFileTypeRegistry(), std::string(MapFormat::Name()),
			os::getExtension(filename));
	MapFormat* format = Radiant_getMapModules().findModule(moduleName);
	return *format;
}

const MapFormat& Map::getFormat ()
{
	return MapFormat_forFile(getName());
}

bool Map::isModified ()
{
	return m_modified;
}

void Map::setModified (bool modified)
{
	if (m_modified ^ modified) {
		m_modified = modified;

		updateTitle();
	}
}

void Map::updateTitle ()
{
	std::string title = "UFORadiant " + m_name;
	if (isModified())
		title += " *";

	gtk_window_set_title(GlobalRadiant().getMainWindow(), title.c_str());
}

scene::Node* Map::getWorldspawn ()
{
	return m_world_node.get();
}

void Map::setWorldspawn (scene::Node* node)
{
	m_world_node.set(node);
}

// free all map elements, reinitialize the structures that depend on them
void Map::free ()
{
	// Clear the shaderclipboard, the references are most probably invalid now
	GlobalShaderClipboard().clear();

	m_resource->detach(*this);
	GlobalReferenceCache().release(m_name);
	m_resource = 0;

	// TODO: what about materials?!

	FlushReferences();
}

Entity* Map::findPlayerStart ()
{
	// TODO: get this list from entities.ufo
	typedef const char* StaticString;
	StaticString strings[] = { "info_alien_start", "info_human_start", "info_civilian_start", "info_player_start" };
	typedef const StaticString* StaticStringIterator;
	for (StaticStringIterator i = strings, end = strings + (sizeof(strings) / sizeof(StaticString)); i != end; ++i) {
		Entity* entity = Scene_FindEntityByClass(*i);
		if (entity != 0) {
			return entity;
		}
	}
	return 0;
}

/**
 * @brief move the view to a given position
 * @todo Use at mapstart
 */
void Map::FocusViews (const Vector3& point, float angle)
{
	CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
	camwnd.setCameraOrigin(point);
	Vector3 angles(camwnd.getCameraAngles());

	angles[CAMERA_YAW] = angle;
	camwnd.setCameraAngles(angles);

	// Try to retrieve the XY view, if there exists one
	GlobalXYWnd().setOrigin(point);
}

// use first worldspawn
class entity_updateworldspawn: public scene::Traversable::Walker
{
	public:
		bool pre (scene::Node& node) const
		{
			if (node_is_worldspawn(node)) {
				if (GlobalMap().getWorldspawn() == 0) {
					GlobalMap().setWorldspawn(&node);
				}
			}
			return false;
		}
};

scene::Node* Map::findWorldspawn ()
{
	setWorldspawn(0);

	Node_getTraversable(GlobalSceneGraph().root())->traverse(entity_updateworldspawn());

	return getWorldspawn();
}

void Map::importSelected (TextInputStream& in)
{
	NodeSmartReference node((new map::BasicContainer)->node());
	const MapFormat& format = getFormat();
	format.readGraph(node, in, GlobalEntityCreator());
	GlobalNamespace().gatherNamespaced(node);
	GlobalNamespace().mergeClonedNames();
	map::MergeMap(node);
}

void Map::focusOnStartPosition ()
{
	Entity* entity = findPlayerStart();
	if (entity) {
		Vector3 origin(entity->getKeyValue("origin"));
		FocusViews(origin, string::toFloat(entity->getKeyValue("angle")));
	} else {
		FocusViews(g_vector3_identity, 0);
	}
}

/**
 * @brief Loads a map file
 * @param[in] filename The filename of the map to load
 * @note The file must be checked for existence and readability already.
 * If file is not a valid map, this will result in a new unnamed map and
 * returns false.
 * @return bool indicating whether file loading was successful
 */
bool Map::loadFile (const std::string& filename)
{
	g_message("Loading map from %s\n", filename.c_str());

	m_name = filename;
	updateTitle();

	{
		m_resource = GlobalReferenceCache().capture(m_name);
		m_resource->attach(*this);

		scene::Traversable* traversible = Node_getTraversable(GlobalSceneGraph().root());
		if (traversible)
			traversible->traverse(entity_updateworldspawn());
		else {
			gtkutil::errorDialog(_("Error during load of map."));
			free();
			createNew();
			return false;
		}
	}

	// move the view to a start position
	focusOnStartPosition();

	sidebar::MapInfo::Instance().update();
	ui::UMPMenu::addItems();
	return true;
}

void Map::reload ()
{
	if (isUnnamed())
		return;

	/* reload the map */
	regionOff();
	free();
	loadFile(getName());
}

class Excluder
{
	public:
		virtual ~Excluder ()
		{
		}
		virtual bool excluded (scene::Node& node) const = 0;
};

class ExcludeWalker: public scene::Traversable::Walker
{
		const scene::Traversable::Walker& m_walker;
		const Excluder* m_exclude;
		mutable bool m_skip;
	public:
		ExcludeWalker (const scene::Traversable::Walker& walker, const Excluder& exclude) :
			m_walker(walker), m_exclude(&exclude), m_skip(false)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (m_exclude->excluded(node) || node.isRoot()) {
				m_skip = true;
				return false;
			} else {
				m_walker.pre(node);
			}
			return true;
		}
		void post (scene::Node& node) const
		{
			if (m_skip) {
				m_skip = false;
			} else {
				m_walker.post(node);
			}
		}
};

class SelectionExcluder: public Excluder
{
	public:
		bool excluded (scene::Node& node) const
		{
			return !Node_selectedDescendant(node);
		}
};

void Map::exportSelected (TextOutputStream& out)
{
	const MapFormat& format = getFormat();
	format.writeGraph(GlobalSceneGraph().root(), map::Map_Traverse_Selected, out);
}

class RegionExcluder: public Excluder
{
	public:
		bool excluded (scene::Node& node) const
		{
			return node.excluded();
		}
};

void Map::traverseRegion (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(ExcludeWalker(walker, RegionExcluder()));
	}
}

bool Map::saveRegion (const std::string& filename)
{
	const bool success = MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), traverseRegion,
			filename);

	return success;
}

void Map::renameAbsolute (const std::string& absolute)
{
	Resource* resource = GlobalReferenceCache().capture(absolute);
	NodeSmartReference clone(NewMapRoot(GlobalFileSystem().getRelative(absolute)));
	resource->setNode(clone.get_pointer());

	Node_getTraversable(GlobalSceneGraph().root())->traverse(map::CloneAll(clone));

	m_resource->detach(*this);
	GlobalReferenceCache().release(m_name);

	m_resource = resource;

	m_name = absolute;
	updateTitle();

	m_resource->attach(*this);
}

void Map::rename (const std::string& filename)
{
	if (m_name != filename) {
		ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));

		renameAbsolute(filename);

		SceneChangeNotify();
	} else {
		SaveReferences();
	}
}

bool Map::save ()
{
	SaveReferences();
	// notify about complete save process
	g_pParentWnd->SaveComplete();
	return true; // assume success..
}

void Map::createNew ()
{
	m_name = "unnamed.map";
	updateTitle();

	{
		m_resource = GlobalReferenceCache().capture(m_name);
		m_resource->attach(*this);

		SceneChangeNotify();
	}

	FocusViews(g_vector3_identity, 0);
}

/**
 * @note Other filtering options may still be on
 */
void Map::regionOff ()
{
	float maxWorldCoord = GlobalRegistry().getFloat("game/defaults/maxWorldCoord");
	float minWorldCoord = GlobalRegistry().getFloat("game/defaults/minWorldCoord");

	region_maxs[0] = maxWorldCoord - 64;
	region_mins[0] = minWorldCoord + 64;
	region_maxs[1] = maxWorldCoord - 64;
	region_mins[1] = minWorldCoord + 64;
	region_maxs[2] = maxWorldCoord - 64;
	region_mins[2] = minWorldCoord + 64;

	map::Scene_Exclude_All(false);
}

void Map::applyRegion ()
{
	map::Scene_Exclude_Region(false);
}

void Map::regionSelectedBrushes ()
{
	regionOff();

	if (GlobalSelectionSystem().countSelected() != 0 && GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
		AABB aabb = selection::algorithm::getCurrentSelectionBounds();
		region_mins = aabb.getMins();
		region_maxs = aabb.getMaxs();

		map::Scene_Exclude_Selected(false);

		GlobalSelectionSystem().setSelectedAll(false);
	}
}

void Map::regionXY (float x_min, float y_min, float x_max, float y_max)
{
	regionOff();

	region_mins[0] = x_min;
	region_maxs[0] = x_max;
	region_mins[1] = y_min;
	region_maxs[1] = y_max;
	region_mins[2] = GlobalRegistry().getFloat("game/defaults/minWorldCoord") + 64;
	region_maxs[2] = GlobalRegistry().getFloat("game/defaults/maxWorldCoord") - 64;

	applyRegion();
}

void Map::regionBounds (const AABB& bounds)
{
	regionOff();

	region_mins = bounds.origin - bounds.extents;
	region_maxs = bounds.origin + bounds.extents;

	selection::algorithm::deleteSelection();

	applyRegion();
}

void Map::regionBrush ()
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		regionBounds(instance.worldAABB());
	}
}

bool Map::importFile (const std::string& filename)
{
	bool success = false;
	{
		Resource* resource = GlobalReferenceCache().capture(filename);
		resource->refresh(); /* avoid loading old version if map has changed on disk since last import */
		if (resource->load()) {
			NodeSmartReference clone(NewMapRoot(""));
			Node_getTraversable(*resource->getNode())->traverse(CloneAll(clone));

			GlobalNamespace().gatherNamespaced(clone);
			GlobalNamespace().mergeClonedNames();
			MergeMap(clone);
			success = true;
		}
		GlobalReferenceCache().release(filename);
	}

	SceneChangeNotify();

	return success;
}

bool Map::saveFile (const std::string& filename)
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));
	if (!MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), Map_Traverse, filename))
		return false;

	// TODO: Resave the material file with the (maybe) new name of the map file
	// TODO: Update ump file with the maybe new name of the map file
	return true;
}

/**
 * @brief Saves selected world brushes and whole entities with partial/full selections
 */
bool Map::saveSelected (const std::string& filename)
{
	return MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), Map_Traverse_Selected, filename);
}

void Map::NewMap ()
{
	if (askForSave(_("New Map"))) {
		regionOff();
		free();
		createNew();
	}
}

/* Display a GTK file chooser and select a map file to open or close. The last
 * path used is set as the default the next time the dialog is displayed.
 * Parameters:
 * title -- the title to display in the dialog
 * open -- true to open, false to save
 *
 */
const std::string Map::selectMapFile (const std::string& title, bool open)
{
	// Save the most recently-used path so that successive maps can be opened
	// from the same directory.
	static std::string lastPath = getMapsPath();
	gtkutil::FileChooser fileChooser(GTK_WIDGET(GlobalRadiant().getMainWindow()), title, open, false, "map", ".map");
	/** @todo is this distinction still needed? lastPath should contain the name of the map if saved(named). */
	if (isUnnamed()) {
		fileChooser.setCurrentPath(lastPath);
	} else {
		const std::string mapName = getName();
		fileChooser.setCurrentPath(os::stripFilename(mapName));
	}

	// Instantiate a new preview object
	// map::MapFileChooserPreview preview;

	// attach the preview object
	// fileChooser.attachPreview(&preview);

	const std::string filePath = fileChooser.display();

	// Update the lastPath static variable with the path to the last directory
	// opened.
	if (!filePath.empty())
		lastPath = os::stripFilename(filePath);

	return filePath;
}

void Map::OpenMap ()
{
	changeMap(_("Open Map"));
}

/**
 * Load a map with given name or chosen by dialog.
 * Will display confirm dialog for changed maps if @c dialogTitle is not empty.
 * @param dialogTitle dialog title for confirm and load dialog if needed
 * @param newFilename filename to load if known. If this is empty, load dialog is used.
 * @return @c true if map loading was successful
 */
bool Map::changeMap (const std::string &dialogTitle, const std::string& newFilename)
{
	if (!dialogTitle.empty())
		if (!askForSave(dialogTitle))
			return false;

	std::string filename;
	if (newFilename.empty())
		filename = selectMapFile(dialogTitle, true);
	else
		filename = newFilename;
	if (!filename.empty()) {
		regionOff();
		free();
		if (loadFile(filename)) {
			GlobalMRU().insert(filename);
			return true;
		}
	}
	return false;
}

void Map::ImportMap ()
{
	const std::string filename = selectMapFile(_("Import Map"), true);
	if (!filename.empty()) {
		UndoableCommand undo("mapImport");
		importFile(filename);
	}
}

bool Map::saveAsDialog ()
{
	const std::string filename = selectMapFile(_("Save Map"), false);
	if (!filename.empty()) {
		GlobalMRU().insert(filename);
		rename(filename);
		return save();
	}
	return false;
}

void Map::SaveMapAs ()
{
	saveAsDialog();
}

void Map::SaveMap ()
{
	if (isUnnamed()) {
		SaveMapAs();
	} else if (isModified()) {
		save();
	}
}

void Map::ExportMap ()
{
	const std::string filename = selectMapFile(_("Export Selection"), false);
	if (!filename.empty()) {
		saveSelected(filename);
	}
}

void Map::SaveRegion ()
{
	const std::string filename = selectMapFile(_("Export Region"), false);
	if (!filename.empty()) {
		saveRegion(filename);
	}
}

void Map::RegionOff ()
{
	regionOff();
	SceneChangeNotify();
}

void Map::RegionXY ()
{
	XYWnd* xyWnd = GlobalXYWnd().getView(XY);
	if (xyWnd != NULL) {
		regionXY(xyWnd->getOrigin()[0] - 0.5f * xyWnd->getWidth() / xyWnd->getScale(), xyWnd->getOrigin()[1] - 0.5f
				* xyWnd->getHeight() / xyWnd->getScale(), xyWnd->getOrigin()[0] + 0.5f * xyWnd->getWidth()
				/ xyWnd->getScale(), xyWnd->getOrigin()[1] + 0.5f * xyWnd->getHeight() / xyWnd->getScale());
	} else {
		regionXY(0, 0, 0, 0);
	}
	SceneChangeNotify();
}

void Map::RegionBrush ()
{
	regionBrush();
	SceneChangeNotify();
}

void Map::RegionSelected ()
{
	regionSelectedBrushes();
	SceneChangeNotify();
}

class BrushFindByIndexWalker: public scene::Traversable::Walker
{
		mutable std::size_t m_index;
		scene::Path& m_path;
	public:
		BrushFindByIndexWalker (std::size_t index, scene::Path& path) :
			m_index(index), m_path(path)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (Node_isPrimitive(node) && m_index-- == 0) {
				m_path.push(makeReference(node));
			}
			return false;
		}
};

class EntityFindByIndexWalker: public scene::Traversable::Walker
{
		mutable std::size_t m_index;
		scene::Path& m_path;
	public:
		EntityFindByIndexWalker (std::size_t index, scene::Path& path) :
			m_index(index), m_path(path)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (Node_isEntity(node) && m_index-- == 0) {
				m_path.push(makeReference(node));
			}
			return false;
		}
};

void Map::FindEntityBrush (std::size_t entity, std::size_t brush, scene::Path& path)
{
	path.push(makeReference(GlobalSceneGraph().root()));
	{
		Node_getTraversable(path.top())->traverse(EntityFindByIndexWalker(entity, path));
	}
	if (path.size() == 2) {
		scene::Traversable* traversable = Node_getTraversable(path.top());
		if (traversable != 0) {
			traversable->traverse(BrushFindByIndexWalker(brush, path));
		}
	}
}

inline bool Node_hasChildren (scene::Node& node)
{
	scene::Traversable* traversable = Node_getTraversable(node);
	return traversable != 0 && !traversable->empty();
}

/**
 * @brief Selects a brush given by entity- and brushnumber
 * @param[in] entitynum The entity number (0 for worldspawn, every other for
 * bmodels)
 * @param[in] brushnum The brush number to select
 */
void Map::SelectBrush (int entitynum, int brushnum, int select)
{
	scene::Path path;
	FindEntityBrush(entitynum, brushnum, path);
	if (path.size() == 3 || (path.size() == 2 && !Node_hasChildren(path.top()))) {
		scene::Instance* instance = GlobalSceneGraph().find(path);
		Selectable* selectable = Instance_getSelectable(*instance);
		selectable->setSelected(select);
		XYWnd* xyView = GlobalXYWnd().getActiveXY();
		if (xyView != NULL) {
			xyView->positionView(instance->worldAABB().origin);
		}
	}
}

bool Map::askForSave (const std::string& title)
{
	if (!isModified())
		return true;

	EMessageBoxReturn result = gtk_MessageBox(
		GTK_WIDGET(GlobalRadiant().getMainWindow()),
		_("The current map has changed since it was last saved.\nDo you want to save the current map before continuing?"),
		title, eMB_YESNOCANCEL, eMB_ICONQUESTION);
	if (result == eIDCANCEL)
		return false;

	if (result == eIDYES) {
		if (isUnnamed())
			return saveAsDialog();
		else
			return save();
	}
	return true;
}

void Map::ObjectsDown ()
{
	if (GlobalSelectionSystem().countSelected() == 0) {
		gtkutil::errorDialog(_("You have to select the objects that you want to change"));
		return;
	}
	UndoableCommand undo("objectsDown");
	GlobalSceneGraph().traverse(MoveLevelWalker(false));
	SceneChangeNotify();
}

void Map::ObjectsUp ()
{
	if (GlobalSelectionSystem().countSelected() == 0) {
		gtkutil::errorDialog(_("You have to select the objects that you want to change"));
		return;
	}
	UndoableCommand undo("objectsUp");
	GlobalSceneGraph().traverse(MoveLevelWalker(true));
	SceneChangeNotify();
}

class MapEntityClasses: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		MapEntityClasses () :
			m_unrealised(1)
		{
		}
		void realise ()
		{
			if (--m_unrealised == 0) {
				if (GlobalMap().m_resource != 0) {
					GlobalMap().m_resource->realise();
				}
			}
		}
		void unrealise ()
		{
			if (++m_unrealised == 1) {
				if (GlobalMap().m_resource != 0) {
					GlobalMap().m_resource->flush();
					GlobalMap().m_resource->unrealise();
				}
			}
		}
};

MapEntityClasses g_MapEntityClasses;

void Map::Construct ()
{
	GlobalEventManager().addCommand("NewMap", MemberCaller<Map, &Map::NewMap> (*this));
	GlobalEventManager().addCommand("OpenMap", MemberCaller<Map, &Map::OpenMap> (*this));
	GlobalEventManager().addCommand("ImportMap", MemberCaller<Map, &Map::ImportMap> (*this));
	GlobalEventManager().addCommand("SaveMap", MemberCaller<Map, &Map::SaveMap> (*this));
	GlobalEventManager().addCommand("SaveMapAs", MemberCaller<Map, &Map::SaveMapAs> (*this));
	GlobalEventManager().addCommand("SaveSelected", MemberCaller<Map, &Map::ExportMap> (*this));
	GlobalEventManager().addCommand("SaveRegion", MemberCaller<Map, &Map::SaveRegion> (*this));
	GlobalEventManager().addCommand("ObjectsUp", MemberCaller<Map, &Map::ObjectsUp> (*this));
	GlobalEventManager().addCommand("ObjectsDown", MemberCaller<Map, &Map::ObjectsDown> (*this));
	GlobalEventManager().addCommand("RegionOff", MemberCaller<Map, &Map::RegionOff> (*this));
	GlobalEventManager().addCommand("RegionSetXY", MemberCaller<Map, &Map::RegionXY> (*this));
	GlobalEventManager().addCommand("RegionSetBrush", MemberCaller<Map, &Map::RegionBrush> (*this));
	GlobalEventManager().addCommand("RegionSetSelection", MemberCaller<Map, &Map::RegionSelected> (*this));

	GlobalEntityClassManager().attach(g_MapEntityClasses);
}

void Map::Destroy ()
{
	GlobalEntityClassManager().detach(g_MapEntityClasses);
}

} // namespace map

class ParentSelectedBrushesToEntityWalker: public scene::Graph::Walker
{
		scene::Node& m_parent;
	public:
		ParentSelectedBrushesToEntityWalker (scene::Node& parent) :
			m_parent(parent)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get_pointer() != &m_parent && Node_isPrimitive(path.top())) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected() && path.size() > 1) {
					return false;
				}
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get_pointer() != &m_parent && Node_isPrimitive(path.top())) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected() && path.size() > 1) {
					scene::Node& parent = path.parent();
					if (&parent != &m_parent) {
						NodeSmartReference node(path.top().get());
						Node_getTraversable(parent)->erase(node);
						Node_getTraversable(m_parent)->insert(node);
					}
				}
			}
		}
};

void Scene_parentSelectedBrushesToEntity (scene::Graph& graph, scene::Node& parent)
{
	graph.traverse(ParentSelectedBrushesToEntityWalker(parent));
}

map::Map& GlobalMap ()
{
	static map::Map _map;
	return _map;
}
