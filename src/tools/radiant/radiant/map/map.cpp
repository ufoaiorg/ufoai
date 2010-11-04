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
#include "../referencecache.h"
#include "../ui/mru/MRU.h"
#include "AutoSaver.h"
#include "../brush/brush.h"
#include "../brush/BrushNode.h"
#include "../ump.h"
#include "string/string.h"
#include "../ui/menu/UMPMenu.h"

#include "MapFileChooserPreview.h"

#include "../namespace/BasicNamespace.h"

std::list<Namespaced*> g_cloned;

inline Namespaced* Node_getNamespaced (scene::Node& node)
{
	return NodeTypeCast<Namespaced>::cast(node);
}

void Node_gatherNamespaced (scene::Node& node)
{
	Namespaced* namespaced = Node_getNamespaced(node);
	if (namespaced != 0) {
		g_cloned.push_back(namespaced);
	}
}

class GatherNamespaced: public scene::Traversable::Walker
{
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

bool Map_Unnamed (const Map& map)
{
	const std::string name = Map_Name(map);
	return (name.empty() || name == "unnamed.map");
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

/**
 * @sa Sys_SetTitle
 */
void Map_UpdateTitle (const Map& map)
{
	Sys_SetTitle(map.m_name, Map_Modified(map));
}

scene::Node* Map_GetWorldspawn (const Map& map)
{
	return map.m_world_node.get();
}

void Map_SetWorldspawn (Map& map, scene::Node* node)
{
	map.m_world_node.set(node);
}

/*
 ================
 Map_Free
 free all map elements, reinitialize the structures that depend on them
 ================
 */
void Map_Free (void)
{
	g_map.m_resource->detach(g_map);
	GlobalReferenceCache().release(g_map.m_name.c_str());
	g_map.m_resource = 0;

	FlushReferences();

	g_currentMap = 0;
}

class EntityFindByClassname: public scene::Graph::Walker
{
		const char* m_name;
		Entity*& m_entity;
	public:
		EntityFindByClassname (const char* name, Entity*& entity) :
			m_name(name), m_entity(entity)
		{
			m_entity = 0;
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_entity == 0) {
				Entity* entity = Node_getEntity(path.top());
				if (entity != 0 && string_equal(m_name, entity->getKeyValue("classname"))) {
					m_entity = entity;
				}
			}
			return true;
		}
};

Entity* Scene_FindEntityByClass (const char* name)
{
	Entity* entity;
	GlobalSceneGraph().traverse(EntityFindByClassname(name, entity));
	return entity;
}

Entity *Scene_FindPlayerStart (void)
{
	typedef const char* StaticString;
	StaticString strings[] = {
			"info_player_start",
			"info_human_deathmatch",
			"team_civilian_start",
			"info_alien_deathmatch",
			"info_ugv_deathmatch", };
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

#include "stringio.h"

inline bool node_is_worldspawn (scene::Node& node)
{
	Entity* entity = Node_getEntity(node);
	return entity != 0 && string_equal(entity->getKeyValue("classname"), "worldspawn");
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

scene::Node& createWorldspawn (void)
{
	NodeSmartReference worldspawn(GlobalEntityCreator().createEntity(GlobalEntityClassManager().findOrInsert(
			"worldspawn", true)));
	Node_insertChildFirst(GlobalSceneGraph().root(), worldspawn);
	return worldspawn;
}

void Map_UpdateWorldspawn (Map& map)
{
	if (Map_FindWorldspawn(map) == 0) {
		Map_SetWorldspawn(map, &createWorldspawn());
	}
}

scene::Node& Map_FindOrInsertWorldspawn (Map& map)
{
	Map_UpdateWorldspawn(map);
	return *Map_GetWorldspawn(map);
}

class MapMergeAll: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		MapMergeAll (const scene::Path& root) :
			m_path(root)
		{
		}
		bool pre (scene::Node& node) const
		{
			Node_getTraversable(m_path.top())->insert(node);
			m_path.push(makeReference(node));
			selectPath(m_path, true);
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

class MapMergeEntities: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		MapMergeEntities (const scene::Path& root) :
			m_path(root)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (node_is_worldspawn(node)) {
				scene::Node* world_node = Map_FindWorldspawn(g_map);
				if (world_node == 0) {
					Map_SetWorldspawn(g_map, &node);
					Node_getTraversable(m_path.top().get())->insert(node);
					m_path.push(makeReference(node));
					Node_getTraversable(node)->traverse(SelectChildren(m_path));
				} else {
					m_path.push(makeReference(*world_node));
					Node_getTraversable(node)->traverse(MapMergeAll(m_path));
				}
			} else {
				Node_getTraversable(m_path.top())->insert(node);
				m_path.push(makeReference(node));
				if (node_is_group(node)) {
					Node_getTraversable(node)->traverse(SelectChildren(m_path));
				} else {
					selectPath(m_path, true);
				}
			}
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

class BasicContainer: public scene::Node
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts (void)
				{
					NodeContainedCast<BasicContainer, scene::Traversable>::install(m_casts);
				}
				NodeTypeCastTable& get (void)
				{
					return m_casts;
				}
		};

		TraversableNodeSet m_traverse;
	public:

		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		scene::Traversable& get (NullType<scene::Traversable> )
		{
			return m_traverse;
		}

		BasicContainer () :
			scene::Node(this, StaticTypeCasts::instance().get())
		{
		}

		scene::Node& node (void)
		{
			return *this;
		}
};

/// Merges the map graph rooted at \p node into the global scene-graph.
void MergeMap (scene::Node& node)
{
	Node_getTraversable(node)->traverse(MapMergeEntities(scene::Path(makeReference(GlobalSceneGraph().root()))));
}
void Map_ImportSelected (TextInputStream& in, const MapFormat& format)
{
	NodeSmartReference node((new BasicContainer)->node());
	format.readGraph(node, in, GlobalEntityCreator());
	Map_gatherNamespaced(node);
	Map_mergeClonedNames();
	MergeMap(node);
}

inline scene::Cloneable* Node_getCloneable (scene::Node& node)
{
	return dynamic_cast<scene::Cloneable*> (&node);
}

inline scene::Node& node_clone (scene::Node& node)
{
	scene::Cloneable* cloneable = Node_getCloneable(node);
	if (cloneable != 0) {
		return cloneable->clone();
	}

	return (new scene::NullNode)->node();
}

class CloneAll: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		CloneAll (scene::Node& root) :
			m_path(makeReference(root))
		{
		}
		bool pre (scene::Node& node) const
		{
			if (node.isRoot()) {
				return false;
			}

			m_path.push(makeReference(node_clone(node)));
			m_path.top().get().IncRef();

			return true;
		}
		void post (scene::Node& node) const
		{
			if (node.isRoot()) {
				return;
			}

			Node_getTraversable(m_path.parent())->insert(m_path.top());

			m_path.top().get().DecRef();
			m_path.pop();
		}
};

scene::Node& Node_Clone (scene::Node& node)
{
	scene::Node& clone = node_clone(node);
	scene::Traversable* traversable = Node_getTraversable(node);
	if (traversable != 0) {
		traversable->traverse(CloneAll(clone));
	}
	return clone;
}

static void Map_StartPosition (void)
{
	Entity* entity = Scene_FindPlayerStart();

	if (entity) {
		Vector3 origin;
		string_parse_vector3(entity->getKeyValue("origin"), origin);
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
	if (GlobalRadiant().getMapName().empty())
		return;

	/* reload the map */
	Map_RegionOff();
	Map_Free();
	Map_LoadFile(GlobalRadiant().getMapName());
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

class AnyInstanceSelected: public scene::Instantiable::Visitor
{
		bool& m_selected;
	public:
		AnyInstanceSelected (bool& selected) :
			m_selected(selected)
		{
			m_selected = false;
		}
		void visit (scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected()) {
				m_selected = true;
			}
		}
};

bool Node_instanceSelected (scene::Node& node)
{
	scene::Instantiable* instantiable = Node_getInstantiable(node);
	ASSERT_NOTNULL(instantiable);
	bool selected;
	instantiable->forEachInstance(AnyInstanceSelected(selected));
	return selected;
}

class SelectedDescendantWalker: public scene::Traversable::Walker
{
		bool& m_selected;
	public:
		SelectedDescendantWalker (bool& selected) :
			m_selected(selected)
		{
			m_selected = false;
		}

		bool pre (scene::Node& node) const
		{
			if (node.isRoot()) {
				return false;
			}

			if (Node_instanceSelected(node)) {
				m_selected = true;
			}

			return true;
		}
};

static bool Node_selectedDescendant (scene::Node& node)
{
	bool selected;
	Node_traverseSubgraph(node, SelectedDescendantWalker(selected));
	return selected;
}

class SelectionExcluder: public Excluder
{
	public:
		bool excluded (scene::Node& node) const
		{
			return !Node_selectedDescendant(node);
		}
};

class IncludeSelectedWalker: public scene::Traversable::Walker
{
		const scene::Traversable::Walker& m_walker;
		mutable std::size_t m_selected;
		mutable bool m_skip;

		bool selectedParent () const
		{
			return m_selected != 0;
		}
	public:
		IncludeSelectedWalker (const scene::Traversable::Walker& walker) :
			m_walker(walker), m_selected(0), m_skip(false)
		{
		}
		bool pre (scene::Node& node) const
		{
			// include node if:
			// node is not a 'root' AND ( node is selected OR any child of node is selected OR any parent of node is selected )
			if (!node.isRoot() && (Node_selectedDescendant(node) || selectedParent())) {
				if (Node_instanceSelected(node)) {
					++m_selected;
				}
				m_walker.pre(node);
				return true;
			} else {
				m_skip = true;
				return false;
			}
		}
		void post (scene::Node& node) const
		{
			if (m_skip) {
				m_skip = false;
			} else {
				if (Node_instanceSelected(node)) {
					--m_selected;
				}
				m_walker.post(node);
			}
		}
};

void Map_Traverse_Selected (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(IncludeSelectedWalker(walker));
	}
}

void Map_ExportSelected (TextOutputStream& out, const MapFormat& format)
{
	format.writeGraph(GlobalSceneGraph().root(), Map_Traverse_Selected, out);
}

void Map_Traverse (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(walker);
	}
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

	Node_getTraversable(GlobalSceneGraph().root())->traverse(CloneAll(clone));

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
		g_map.m_resource = GlobalReferenceCache().capture(g_map.m_name.c_str());
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

static inline void exclude_node (scene::Node& node, bool exclude)
{
	if (exclude)
		node.enable(scene::Node::eExcluded);
	else
		node.disable(scene::Node::eExcluded);
}

class ExcludeAllWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeAllWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), m_exclude);

			return true;
		}
};

void Scene_Exclude_All (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeAllWalker(exclude));
}

class ExcludeSelectedWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeSelectedWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), (instance.isSelected() || instance.childSelected() || instance.parentSelected())
					== m_exclude);
			return true;
		}
};

void Scene_Exclude_Selected (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeSelectedWalker(exclude));
}

class ExcludeRegionedWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeRegionedWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), !((aabb_intersects_aabb(instance.worldAABB(), AABB::createFromMinMax(region_mins,
					region_maxs)) != 0) ^ m_exclude));

			return true;
		}
};

void Scene_Exclude_Region (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeRegionedWalker(exclude));
}

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

	Scene_Exclude_All(false);
}

void Map_ApplyRegion (void)
{
	Scene_Exclude_Region(false);
}

void Map_RegionSelectedBrushes (void)
{
	Map_RegionOff();

	if (GlobalSelectionSystem().countSelected() != 0 && GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
		Select_GetBounds(region_mins, region_maxs);

		Scene_Exclude_Selected(false);

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

	deleteSelection();

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
		Resource* resource = GlobalReferenceCache().capture(filename.c_str());
		resource->refresh(); /* avoid loading old version if map has changed on disk since last import */
		if (resource->load()) {
			NodeSmartReference clone(NewMapRoot(""));
			Node_getTraversable(*resource->getNode())->traverse(CloneAll(clone));

			Map_gatherNamespaced(clone);
			Map_mergeClonedNames();
			MergeMap(clone);
			success = true;
		}
		GlobalReferenceCache().release(filename.c_str());
	}

	SceneChangeNotify();

	return success;
}

bool Map_SaveFile (const std::string& filename)
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
bool Map_SaveSelected (const std::string& filename)
{
	return MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), Map_Traverse_Selected, filename);
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

	bool isUnnamed ()
	{
		return Map_Unnamed(g_map);
	}
} // namespace map

enum ENodeType
{
	eNodeUnknown, eNodeMap, eNodeEntity, eNodePrimitive
};

static inline const char* nodetype_get_name (ENodeType type)
{
	if (type == eNodeMap)
		return "map";
	if (type == eNodeEntity)
		return "entity";
	if (type == eNodePrimitive)
		return "primitive";
	return "unknown";
}

static ENodeType node_get_nodetype (scene::Node& node)
{
	if (Node_isEntity(node)) {
		return eNodeEntity;
	}
	if (Node_isPrimitive(node)) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

static bool contains_entity (scene::Node& node)
{
	return Node_getTraversable(node) != 0 && !Node_isBrush(node) && !Node_isEntity(node);
}

static bool contains_primitive (scene::Node& node)
{
	return Node_isEntity(node) && Node_getTraversable(node) != 0 && Node_getEntity(node)->isContainer();
}

static ENodeType node_get_contains (scene::Node& node)
{
	if (contains_entity(node)) {
		return eNodeEntity;
	}
	if (contains_primitive(node)) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

static void Path_parent (const scene::Path& parent, const scene::Path& child)
{
	ENodeType contains = node_get_contains(parent.top());
	ENodeType type = node_get_nodetype(child.top());

	if (contains != eNodeUnknown && contains == type) {
		NodeSmartReference node(child.top().get());
		Path_deleteTop(child);
		Node_getTraversable(parent.top())->insert(node);
		SceneChangeNotify();
	} else {
		globalErrorStream() << "failed - " << nodetype_get_name(type) << " cannot be parented to "
				<< nodetype_get_name(contains) << " container.\n";
	}
}

void Scene_parentSelected (void)
{
	UndoableCommand undo("parentSelected");

	if (GlobalSelectionSystem().countSelected() > 1) {
		class ParentSelectedBrushesToEntityWalker: public SelectionSystem::Visitor
		{
				const scene::Path& m_parent;
			public:
				ParentSelectedBrushesToEntityWalker (const scene::Path& parent) :
					m_parent(parent)
				{
				}
				void visit (scene::Instance& instance) const
				{
					if (&m_parent != &instance.path()) {
						Path_parent(m_parent, instance.path());
					}
				}
		};

		ParentSelectedBrushesToEntityWalker visitor(GlobalSelectionSystem().ultimateSelected().path());
		GlobalSelectionSystem().foreachSelected(visitor);
	} else {
		g_warning("Failed - did not find two selected nodes.\n");
	}
}

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

static std::string g_mapsPath;

const std::string& getMapsPath (void)
{
	return g_mapsPath;
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
		static std::string lastPath = getMapsPath();
		gtkutil::FileChooser fileChooser(GTK_WIDGET(GlobalRadiant().getMainWindow()), title, open, false, /*MapFormat::Name()*/
		"map", "map");
		/** @todo is this distinction still needed? lastPath should contain the name of the map if saved(named). */
		if (map::isUnnamed()) {
			fileChooser.setCurrentPath(lastPath);
		} else {
			const std::string mapName = GlobalRadiant().getMapName();
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
		Map_Rename(filename.c_str());
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
	if (Map_Unnamed(g_map)) {
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
				g_mapsPath = g_qeglobals.m_userGamePath + "maps/";
				g_mkdir(g_mapsPath.c_str(), 0775);
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				g_mapsPath = "";
			}
		}
};

class BrushMoveLevel
{
	private:

		bool _up;

	public:

		BrushMoveLevel (bool up) :
			_up(up)
		{
		}

		void operator() (Face& face) const
		{
			ContentsFlagsValue flags;
			face.GetFlags(flags);

			int levels = (flags.m_contentFlags >> 8) & 255;
			if (_up) {
				levels <<= 1;
				if (!levels)
					levels = 1;
			} else {
				const int newLevel = levels >> 1;
				if (newLevel != (newLevel & 255))
					return;
				levels >>= 1;
			}

			levels &= 255;
			levels <<= 8;

			flags.m_contentFlags &= levels;
			flags.m_contentFlags |= levels;

			face.SetFlags(flags);
		}
};

class MoveLevelWalker: public scene::Graph::Walker
{
	private:

		bool _up;

	public:

		MoveLevelWalker (bool up) :
			_up(up)
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				if (Node_isBrush(path.top())) {
					Brush* brush = Node_getBrush(path.top());
					if (brush != 0) {
						Brush_forEachFace(*brush, BrushMoveLevel(_up));
					}
				} else if (Node_isEntity(path.top())) {
					Entity* entity = Node_getEntity(path.top());
					if (entity != 0) {
						std::string name = entity->getKeyValue("classname");
						// TODO: get this info from entities.ufo
						if (name == "func_rotating" || name == "func_door" || name == "func_breakable" || name
								== "misc_item" || name == "misc_mission" || name == "misc_mission_alien" || name
								== "misc_model" || name == "misc_sound" || name == "misc_particle") {
							const char *spawnflags = entity->getKeyValue("spawnflags");
							if (spawnflags != 0 && *spawnflags != '\0') {
								int levels = atoi(spawnflags) & 255;
								if (_up) {
									levels <<= 1;
								} else {
									levels >>= 1;
								}
								if (levels == 0)
									levels = 1;
								entity->setKeyValue("spawnflags", string::toString(levels & 255));
							}
						}
					}
				}
			}
			return true;
		}
};

void ObjectsDown (void)
{
	if (GlobalSelectionSystem().countSelected() == 0) {
		gtkutil::errorDialog(_("You have to select the objects that you want to change"));
		return;
	}
	UndoableCommand undo("objectsDown");
	GlobalSceneGraph().traverse(MoveLevelWalker(false));
	SceneChangeNotify();
}

void ObjectsUp (void)
{
	if (GlobalSelectionSystem().countSelected() == 0) {
		gtkutil::errorDialog(_("You have to select the objects that you want to change"));
		return;
	}
	UndoableCommand undo("objectsUp");
	GlobalSceneGraph().traverse(MoveLevelWalker(true));
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
