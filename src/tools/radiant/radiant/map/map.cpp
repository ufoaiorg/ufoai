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

#include "debugging/debugging.h"

#include "imap.h"
#include "iselection.h"
#include "iundo.h"
#include "../brush/TexDef.h"
#include "ibrush.h"
#include "ifilter.h"
#include "ireference.h"
#include "ifiletypes.h"
#include "ieclass.h"
#include "irender.h"
#include "iradiant.h"
#include "ientity.h"
#include "editable.h"
#include "ifilesystem.h"
#include "ieventmanager.h"
#include "namespace.h"
#include "moduleobserver.h"

#include <set>

#include "scenelib.h"
#include "transformlib.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "traverselib.h"
#include "maplib.h"
#include "eclasslib.h"
#include "entitylib.h"
#include "stream/textfilestream.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "signal/signal.h"

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "../timer.h"
#include "../select.h"
#include "../plugin.h"
#include "../filetypes.h"
#include "../sidebar/MapInfo.h"
#include "../qe3.h"
#include "../camera/CamWnd.h"
#include "../xyview/GlobalXYWnd.h"
#include "../mainframe.h"
#include "../settings/preferences.h"
#include "../referencecache/referencecache.h"
#include "../ui/mru/MRU.h"
#include "AutoSaver.h"
#include "../brush/brush.h"
#include "../brush/BrushNode.h"
#include "../ump.h"
#include "string/string.h"
#include "../ui/menu/UMPMenu.h"
#include "../selection/algorithm/General.h"

#include "MapFileChooserPreview.h"

#include "../namespace/BasicNamespace.h"

std::list<Namespaced*> g_cloned;

inline Namespaced* Node_getNamespaced (scene::Node& node)
{
	return NodeTypeCast<Namespaced>::cast(node);
}

class GatherNamespaced: public scene::Traversable::Walker
{
	private:
		void Node_gatherNamespaced (scene::Node& node) const
		{
			Namespaced* namespaced = Node_getNamespaced(node);
			if (namespaced != 0) {
				g_cloned.push_back(namespaced);
			}
		}
	public:
		bool pre (scene::Node& node) const
		{
			Node_gatherNamespaced(node);
			return true;
		}
};

void Map_gatherNamespaced (scene::Node& root)
{
	Node_traverseSubgraph(root, GatherNamespaced());
}

void Map_mergeClonedNames (void)
{
	for (std::list<Namespaced*>::const_iterator i = g_cloned.begin(); i != g_cloned.end(); ++i) {
		(*i)->setNamespace(g_cloneNamespace);
	}
	g_cloneNamespace.mergeNames(g_defaultNamespace);
	for (std::list<Namespaced*>::const_iterator i = g_cloned.begin(); i != g_cloned.end(); ++i) {
		(*i)->setNamespace(g_defaultNamespace);
	}

	g_cloned.clear();
}

class WorldNode
{
		scene::Node* m_node;
	public:
		WorldNode () :
			m_node(0)
		{
		}
		void set (scene::Node* node)
		{
			if (m_node != 0)
				m_node->DecRef();
			m_node = node;
			if (m_node != 0)
				m_node->IncRef();
		}
		scene::Node* get () const
		{
			return m_node;
		}
};

class Map;
void Map_SetValid (Map& map, bool valid);
void Map_UpdateTitle (const Map& map);
void Map_SetWorldspawn (Map& map, scene::Node* node);

inline bool Map_Unnamed (const Map& map)
{
	const std::string name = Map_Name(map);
	return (name.empty() || name == "unnamed.map");
}

class Map: public ModuleObserver
{
	public:
		std::string m_name;
		Resource* m_resource;
		bool m_valid;

		bool m_modified;
		void (*m_modified_changed) (const Map&);

		Signal0 m_mapValidCallbacks;

		WorldNode m_world_node; // "classname" "worldspawn" !

		Map () :
			m_resource(0), m_valid(false), m_modified_changed(Map_UpdateTitle)
		{
		}

		void realise (void)
		{
			if (m_resource != 0) {
				if (Map_Unnamed(*this)) {
					g_map.m_resource->setNode(NewMapRoot("").get_pointer());
					MapFile* map = Node_getMapFile(*g_map.m_resource->getNode());
					if (map != 0) {
						map->save();
					}
				} else {
					m_resource->load();
				}

				GlobalSceneGraph().insert_root(*m_resource->getNode());

				map::AutoSaver().clearChanges();

				Map_SetValid(g_map, true);
			}
		}
		void unrealise (void)
		{
			if (m_resource != 0) {
				Map_SetValid(g_map, false);
				Map_SetWorldspawn(g_map, 0);

				GlobalUndoSystem().clear();

				GlobalSceneGraph().erase_root();
			}
		}
};

Map g_map;
Map* g_currentMap = 0;

void Map_addValidCallback (Map& map, const SignalHandler& handler)
{
	map.m_mapValidCallbacks.connectLast(handler);
}

bool Map_Valid (const Map& map)
{
	return map.m_valid;
}

void Map_SetValid (Map& map, bool valid)
{
	map.m_valid = valid;
	map.m_mapValidCallbacks();
}

const std::string& Map_Name (const Map& map)
{
	return map.m_name;
}

inline const MapFormat& MapFormat_forFile (const std::string& filename)
{
	const std::string moduleName = findModuleName(GetFileTypeRegistry(), std::string(MapFormat::Name()),
			os::getExtension(filename));
	MapFormat* format = Radiant_getMapModules().findModule(moduleName);
	return *format;
}

const MapFormat& Map_getFormat (const Map& map)
{
	return MapFormat_forFile(Map_Name(map));
}

bool Map_Modified (const Map& map)
{
	return map.m_modified;
}

void Map_SetModified (Map& map, bool modified)
{
	if (map.m_modified ^ modified) {
		map.m_modified = modified;

		map.m_modified_changed(map);
	}
}

void Map_UpdateTitle (const Map& map)
{
	std::string title = "UFORadiant " + map.m_name;
	if (Map_Modified(map))
		title += " *";

	gtk_window_set_title(GlobalRadiant().getMainWindow(), title.c_str());
}

scene::Node* Map_GetWorldspawn (const Map& map)
{
	return map.m_world_node.get();
}

void Map_SetWorldspawn (Map& map, scene::Node* node)
{
	map.m_world_node.set(node);
}

// free all map elements, reinitialize the structures that depend on them
void Map_Free (void)
{
	g_map.m_resource->detach(g_map);
	GlobalReferenceCache().release(g_map.m_name);
	g_map.m_resource = 0;

	FlushReferences();

	g_currentMap = 0;
}

Entity* Map_FindPlayerStart (void)
{
	// TODO: get this list from entities.ufo
	typedef const char* StaticString;
	StaticString strings[] = {
			"info_alien_start",
			"info_human_start",
			"info_civilian_start",
			"info_player_start" };
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
void FocusViews (const Vector3& point, float angle)
{
	ASSERT_NOTNULL(g_pParentWnd);
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
				if (Map_GetWorldspawn(g_map) == 0) {
					Map_SetWorldspawn(g_map, &node);
				}
			}
			return false;
		}
};

scene::Node* Map_FindWorldspawn (Map& map)
{
	Map_SetWorldspawn(map, 0);

	Node_getTraversable(GlobalSceneGraph().root())->traverse(entity_updateworldspawn());

	return Map_GetWorldspawn(map);
}

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

#include "BasicContainer.h"
#include "algorithm/Merge.h"
#include "algorithm/Clone.h"

void Map_ImportSelected (TextInputStream& in, const MapFormat& format)
{
	NodeSmartReference node((new map::BasicContainer)->node());
	format.readGraph(node, in, GlobalEntityCreator());
	Map_gatherNamespaced(node);
	Map_mergeClonedNames();
	map::MergeMap(node);
}

static void Map_StartPosition (void)
{
	Entity* entity = Map_FindPlayerStart();

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
bool Map_LoadFile (const std::string& filename)
{
	g_message("Loading map from %s\n", filename.c_str());

	g_map.m_name = filename;
	Map_UpdateTitle(g_map);

	{
		g_map.m_resource = GlobalReferenceCache().capture(g_map.m_name);
		g_map.m_resource->attach(g_map);

		scene::Traversable* traversible = Node_getTraversable(GlobalSceneGraph().root());
		if (traversible)
			traversible->traverse(entity_updateworldspawn());
		else {
			gtkutil::errorDialog(_("Error during load of map."));
			Map_Free();
			Map_New();
			return false;
		}
	}

	g_message("LoadMapFile: %s\n", g_map.m_name.c_str());

	// move the view to a start position
	Map_StartPosition();

	g_currentMap = &g_map;

	sidebar::MapInfo::getInstance().update();
	ui::UMPMenu::addItems();
	return true;
}

void Map_Reload (void)
{
	if (map::isUnnamed())
		return;

	/* reload the map */
	Map_RegionOff();
	Map_Free();
	Map_LoadFile(map::getMapName());
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

#include "algorithm/Traverse.h"

void Map_ExportSelected (TextOutputStream& out, const MapFormat& format)
{
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

void Map_Traverse_Region (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(ExcludeWalker(walker, RegionExcluder()));
	}
}

bool Map_SaveRegion (const std::string& filename)
{
	const bool success = MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(),
			Map_Traverse_Region, filename);

	return success;
}

void Map_RenameAbsolute (const std::string& absolute)
{
	Resource* resource = GlobalReferenceCache().capture(absolute);
	NodeSmartReference clone(NewMapRoot(GlobalFileSystem().getRelative(absolute)));
	resource->setNode(clone.get_pointer());

	Node_getTraversable(GlobalSceneGraph().root())->traverse(map::CloneAll(clone));

	g_map.m_resource->detach(g_map);
	GlobalReferenceCache().release(g_map.m_name);

	g_map.m_resource = resource;

	g_map.m_name = absolute;
	Map_UpdateTitle(g_map);

	g_map.m_resource->attach(g_map);
}

void Map_Rename (const std::string& filename)
{
	if (g_map.m_name != filename) {
		ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));

		Map_RenameAbsolute(filename);

		SceneChangeNotify();
	} else {
		SaveReferences();
	}
}

bool Map_Save (void)
{
	SaveReferences();
	// notify about complete save process
	g_pParentWnd->SaveComplete();
	return true; // assume success..
}

/*
 ===========
 Map_New

 ===========
 */
void Map_New (void)
{
	g_map.m_name = "unnamed.map";
	Map_UpdateTitle(g_map);

	{
		g_map.m_resource = GlobalReferenceCache().capture(g_map.m_name);
		g_map.m_resource->attach(g_map);

		SceneChangeNotify();
	}

	FocusViews(g_vector3_identity, 0);

	g_currentMap = &g_map;
}

/*
 ===========================================================
 REGION
 ===========================================================
 */

// greebo: this has to be moved into some class and the values should be loaded from the registry
// I'll leave it hardcoded for now
Vector3	region_mins(-65536, -65536, -65536);
Vector3	region_maxs(65536, 65536, 65536);

// old code:
//Vector3	region_mins(g_MinWorldCoord, g_MinWorldCoord, g_MinWorldCoord);
//Vector3	region_maxs(g_MaxWorldCoord, g_MaxWorldCoord, g_MaxWorldCoord);

#include "RegionWalkers.h"

/**
 * @note Other filtering options may still be on
 */
void Map_RegionOff (void)
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

void Map_ApplyRegion (void)
{
	map::Scene_Exclude_Region(false);
}

void Map_RegionSelectedBrushes (void)
{
	Map_RegionOff();

	if (GlobalSelectionSystem().countSelected() != 0 && GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
		AABB aabb = selection::algorithm::getCurrentSelectionBounds();
		region_mins = aabb.getMins();
		region_maxs = aabb.getMaxs();

		map::Scene_Exclude_Selected(false);

		GlobalSelectionSystem().setSelectedAll(false);
	}
}

void Map_RegionXY (float x_min, float y_min, float x_max, float y_max)
{
	Map_RegionOff();

	region_mins[0] = x_min;
	region_maxs[0] = x_max;
	region_mins[1] = y_min;
	region_maxs[1] = y_max;
	region_mins[2] = GlobalRegistry().getFloat("game/defaults/minWorldCoord") + 64;
	region_maxs[2] = GlobalRegistry().getFloat("game/defaults/maxWorldCoord") - 64;

	Map_ApplyRegion();
}

void Map_RegionBounds (const AABB& bounds)
{
	Map_RegionOff();

	region_mins = bounds.origin - bounds.extents;
	region_maxs = bounds.origin + bounds.extents;

	selection::algorithm::deleteSelection();

	Map_ApplyRegion();
}

void Map_RegionBrush (void)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		Map_RegionBounds(instance.worldAABB());
	}
}

bool Map_ImportFile (const std::string& filename)
{
	bool success = false;
	{
		Resource* resource = GlobalReferenceCache().capture(filename);
		resource->refresh(); /* avoid loading old version if map has changed on disk since last import */
		if (resource->load()) {
			NodeSmartReference clone(NewMapRoot(""));
			Node_getTraversable(*resource->getNode())->traverse(map::CloneAll(clone));

			Map_gatherNamespaced(clone);
			Map_mergeClonedNames();
			map::MergeMap(clone);
			success = true;
		}
		GlobalReferenceCache().release(filename);
	}

	SceneChangeNotify();

	return success;
}

bool Map_SaveFile (const std::string& filename)
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));
	if (!MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), map::Map_Traverse, filename))
		return false;

	// TODO: Resave the material file with the (maybe) new name of the map file
	// TODO: Update ump file with the maybe new name of the map file
	return true;
}

/**
 * @brief Saves selected world brushes and whole entities with partial/full selections
 */
bool Map_SaveSelected (const std::string& filename)
{
	return MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), map::Map_Traverse_Selected, filename);
}

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

namespace map
{

	namespace
	{

		std::string g_mapsPath;

		scene::Node& createWorldspawn (void)
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

	const std::string& getMapName() {
		return Map_Name(g_map);
	}

	scene::Node& findOrInsertWorldspawn() {
		if (Map_FindWorldspawn(g_map) == 0) {
			Map_SetWorldspawn(g_map, &createWorldspawn());
		}
		return *Map_GetWorldspawn(g_map);
	}

	bool isUnnamed ()
	{
		return Map_Unnamed(g_map);
	}

	const std::string& getMapsPath ()
	{
		return g_mapsPath;
	}

} // namespace map

/**
 * @sa Map_Reload
 * @sa Map_New
 * @sa Map_Load
 */
void NewMap (void)
{
	if (ConfirmModified(_("New Map"))) {
		Map_RegionOff();
		Map_Free();
		Map_New();
	}
}
namespace ui
{

	/* Display a GTK file chooser and select a map file to open or close. The last
	 * path used is set as the default the next time the dialog is displayed.
	 * Parameters:
	 * title -- the title to display in the dialog
	 * open -- true to open, false to save
	 *
	 */
	const std::string selectMapFile (const std::string& title, bool open)
	{
		// Save the most recently-used path so that successive maps can be opened
		// from the same directory.
		static std::string lastPath = map::getMapsPath();
		gtkutil::FileChooser fileChooser(GTK_WIDGET(GlobalRadiant().getMainWindow()), title, open, false, /*MapFormat::Name()*/
		"map", "map");
		/** @todo is this distinction still needed? lastPath should contain the name of the map if saved(named). */
		if (map::isUnnamed()) {
			fileChooser.setCurrentPath(lastPath);
		} else {
			const std::string mapName = map::getMapName();
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

} //namespace ui

void OpenMap (void)
{
	Map_ChangeMap(_("Open Map"));
}

/**
 * Load a map with given name or chosen by dialog.
 * Will display confirm dialog for changed maps if @c dialogTitle is not empty.
 * @param dialogTitle dialog title for confirm and load dialog if needed
 * @param newFilename filename to load if known. If this is empty, load dialog is used.
 * @return @c true if map loading was successful
 */
bool Map_ChangeMap (const std::string &dialogTitle, const std::string& newFilename)
{
	if (!dialogTitle.empty())
		if (!ConfirmModified(dialogTitle))
			return false;

	std::string filename;
	if (newFilename.empty())
		filename = ui::selectMapFile(dialogTitle, true);
	else
		filename = newFilename;
	if (!filename.empty()) {
		Map_RegionOff();
		Map_Free();
		if (Map_LoadFile(filename)) {
			GlobalMRU().insert(filename);
			return true;
		}
	}
	return false;
}

void ImportMap (void)
{
	const std::string filename = ui::selectMapFile(_("Import Map"), true);
	if (!filename.empty()) {
		UndoableCommand undo("mapImport");
		Map_ImportFile(filename);
	}
}

bool Map_SaveAs (void)
{
	const std::string filename = ui::selectMapFile(_("Save Map"), false);
	if (!filename.empty()) {
		GlobalMRU().insert(filename);
		Map_Rename(filename);
		return Map_Save();
	}
	return false;
}

void SaveMapAs (void)
{
	Map_SaveAs();
}

void SaveMap (void)
{
	if (map::isUnnamed()) {
		SaveMapAs();
	} else if (Map_Modified(g_map)) {
		Map_Save();
	}
}

void ExportMap (void)
{
	const std::string filename = ui::selectMapFile(_("Export Selection"), false);
	if (!filename.empty()) {
		Map_SaveSelected(filename);
	}
}

void SaveRegion (void)
{
	const std::string filename = ui::selectMapFile(_("Export Region"), false);
	if (!filename.empty()) {
		Map_SaveRegion(filename);
	}
}

void RegionOff (void)
{
	Map_RegionOff();
	SceneChangeNotify();
}

void RegionXY (void)
{
	XYWnd* xyWnd = GlobalXYWnd().getView(XY);
	if (xyWnd != NULL) {
		Map_RegionXY(xyWnd->getOrigin()[0] - 0.5f * xyWnd->getWidth() / xyWnd->getScale(), xyWnd->getOrigin()[1] - 0.5f
				* xyWnd->getHeight() / xyWnd->getScale(), xyWnd->getOrigin()[0] + 0.5f * xyWnd->getWidth()
				/ xyWnd->getScale(), xyWnd->getOrigin()[1] + 0.5f * xyWnd->getHeight() / xyWnd->getScale());
	} else {
		Map_RegionXY(0, 0, 0, 0);
	}
	SceneChangeNotify();
}

void RegionBrush (void)
{
	Map_RegionBrush();
	SceneChangeNotify();
}

void RegionSelected (void)
{
	Map_RegionSelectedBrushes();
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

static void Scene_FindEntityBrush (std::size_t entity, std::size_t brush, scene::Path& path)
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

static inline bool Node_hasChildren (scene::Node& node)
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
void SelectBrush (int entitynum, int brushnum, int select)
{
	scene::Path path;
	Scene_FindEntityBrush(entitynum, brushnum, path);
	if (path.size() == 3 || (path.size() == 2 && !Node_hasChildren(path.top()))) {
		scene::Instance* instance = GlobalSceneGraph().find(path);
		ASSERT_MESSAGE(instance != 0, "SelectBrush: path not found in scenegraph");
		Selectable* selectable = Instance_getSelectable(*instance);
		ASSERT_MESSAGE(selectable != 0, "SelectBrush: path not selectable");
		selectable->setSelected(select);
		XYWnd* xyView = GlobalXYWnd().getActiveXY();
		if (xyView != NULL) {
			xyView->positionView(instance->worldAABB().origin);
		}
	}
}

class MapEntityClasses: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		MapEntityClasses () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				if (g_map.m_resource != 0) {
					g_map.m_resource->realise();
				}
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				if (g_map.m_resource != 0) {
					g_map.m_resource->flush();
					g_map.m_resource->unrealise();
				}
			}
		}
};

MapEntityClasses g_MapEntityClasses;

class MapModuleObserver: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		MapModuleObserver () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				map::g_mapsPath = g_qeglobals.m_userGamePath + "maps/";
				g_mkdir(map::g_mapsPath.c_str(), 0775);
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				map::g_mapsPath = "";
			}
		}
};

#include "MoveLevelWalker.h"

void ObjectsDown (void)
{
	if (GlobalSelectionSystem().countSelected() == 0) {
		gtkutil::errorDialog(_("You have to select the objects that you want to change"));
		return;
	}
	UndoableCommand undo("objectsDown");
	GlobalSceneGraph().traverse(map::MoveLevelWalker(false));
	SceneChangeNotify();
}

void ObjectsUp (void)
{
	if (GlobalSelectionSystem().countSelected() == 0) {
		gtkutil::errorDialog(_("You have to select the objects that you want to change"));
		return;
	}
	UndoableCommand undo("objectsUp");
	GlobalSceneGraph().traverse(map::MoveLevelWalker(true));
	SceneChangeNotify();
}

MapModuleObserver g_MapModuleObserver;

#include "preferencesystem.h"

void Map_Construct (void)
{
	GlobalEventManager().addCommand("ObjectsUp", FreeCaller<ObjectsUp> ());
	GlobalEventManager().addCommand("ObjectsDown", FreeCaller<ObjectsDown> ());
	GlobalEventManager().addCommand("RegionOff", FreeCaller<RegionOff> ());
	GlobalEventManager().addCommand("RegionSetXY", FreeCaller<RegionXY> ());
	GlobalEventManager().addCommand("RegionSetBrush", FreeCaller<RegionBrush> ());
	GlobalEventManager().addCommand("RegionSetSelection", FreeCaller<RegionSelected> ());

	GlobalEntityClassManager().attach(g_MapEntityClasses);
	Radiant_attachHomePathsObserver(g_MapModuleObserver);
}

void Map_Destroy (void)
{
	Radiant_detachHomePathsObserver(g_MapModuleObserver);
	GlobalEntityClassManager().detach(g_MapEntityClasses);
}
