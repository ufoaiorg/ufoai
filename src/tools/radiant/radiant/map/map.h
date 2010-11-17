/**
 * @file src/tools/radiant/radiant/map/map.h
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

#if !defined(INCLUDED_MAP_H)
#define INCLUDED_MAP_H

#include "scenelib.h"
#include "generic/callback.h"
#include "signal/signalfwd.h"
#include "signal/signal.h"
#include "moduleobserver.h"
#include "ireference.h"
#include "imap.h"
#include <string>

namespace map {
/** Subtract the provided origin vector from all selected brushes. This is
 * necessary when reparenting worldspawn brushes to an entity, since the entity's
 * "origin" key will be added to all child brushes.
 *
 * @param origin
 * Vector3 containing the new origin for the selected brushes.
 */
void selectedBrushesSubtractOrigin (const Vector3& origin);

/** Count the number of selected brushes in the current map.
 *
 * @return The number of selected brushes.
 */

int countSelectedBrushes ();

const std::string& getMapsPath ();

class WorldNode
{
		scene::Node* m_node;
	public:
		WorldNode ();
		void set (scene::Node* node);
		scene::Node* get () const;
};

class Map: public ModuleObserver
{
	private:

		void renameAbsolute (const std::string& absolute);
		void focusOnStartPosition ();
		Entity* findPlayerStart ();

		const std::string selectMapFile (const std::string& title, bool open);

		static void traverseRegion (scene::Node& root, const scene::Traversable::Walker& walker);

		void FindEntityBrush (std::size_t entity, std::size_t brush, scene::Path& path);

		// command bindings
		void ObjectsDown ();
		void ObjectsUp ();
		void RegionOff ();
		void RegionSelected ();
		void RegionXY ();
		void RegionBrush ();
		void NewMap ();
		void OpenMap ();
		void SaveMapAs ();
		void SaveMap ();
		void ExportMap ();
		void SaveRegion ();
		void ImportMap ();

		// region functions
		void applyRegion ();
		void regionBounds (const AABB& bounds);
		void regionBrush ();
		void regionXY (float x_min, float y_min, float x_max, float y_max);
		void regionSelectedBrushes ();

	private:

		std::string m_name;
		bool m_valid;

		bool m_modified;

		Signal0 m_mapValidCallbacks;

		WorldNode m_world_node; // "classname" "worldspawn" !

		Vector3 region_mins, region_maxs;

	public:
		Resource* m_resource;

		Map ();

		const Vector3& getRegionMins() const;
		const Vector3& getRegionMaxs() const;

		void Construct ();
		void Destroy ();

		bool loadFile (const std::string& filename);
		void free (void);
		bool importFile (const std::string& filename);
		bool save ();
		void reload ();
		void createNew ();
		bool saveFile (const std::string& filename);
		bool saveSelected (const std::string& filename);
		void rename (const std::string& filename);
		bool saveAsDialog ();
		bool saveRegion (const std::string& filename);
		bool changeMap (const std::string& dialogTitle, const std::string& newFilename = "");
		bool askForSave (const std::string& title);

		void SelectBrush (int entitynum, int brushnum, int select);

		scene::Node& findOrInsertWorldspawn ();
		void regionOff ();

		void exportSelected (TextOutputStream& out);
		void importSelected (TextInputStream& in);

		const MapFormat& getFormat ();

		void SetValid (bool valid);

		void realise (void);

		void unrealise (void);

		bool isValid ();

		const std::string& getName () const;
		void updateTitle ();

		bool isUnnamed ();

		void addValidCallback (const SignalHandler& handler);

		scene::Node* getWorldspawn ();
		void setWorldspawn (scene::Node* node);
		scene::Node* findWorldspawn ();

		void FocusViews (const Vector3& point, float angle);

		bool isModified ();
		void setModified (bool modified);
};

} // namespace

map::Map& GlobalMap ();

namespace scene {
class Node;
class Graph;
}

void Scene_parentSelectedBrushesToEntity (scene::Graph& graph, scene::Node& parent);

#endif
