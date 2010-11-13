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

#include "iscenegraph.h"
#include "generic/callback.h"
#include "signal/signalfwd.h"
#include <string>

class Map;
extern Map g_map;

class MapFormat;

void Map_addValidCallback (Map& map, const SignalHandler& handler);
bool Map_Valid (const Map& map);

class DeferredDraw
{
		Callback m_draw;
		bool m_defer;
		bool m_deferred;
	public:
		DeferredDraw (const Callback& draw) :
			m_draw(draw), m_defer(false), m_deferred(false)
		{
		}
		void defer ()
		{
			m_defer = true;
		}
		void draw ()
		{
			if (m_defer) {
				m_deferred = true;
			} else {
				m_draw();
			}
		}
		void flush ()
		{
			if (m_defer && m_deferred) {
				m_draw();
			}
			m_deferred = false;
			m_defer = false;
		}
};

inline void DeferredDraw_onMapValidChanged (DeferredDraw& self)
{
	if (Map_Valid(g_map)) {
		self.flush();
	} else {
		self.defer();
	}
}
typedef ReferenceCaller<DeferredDraw, DeferredDraw_onMapValidChanged> DeferredDrawOnMapValidChangedCaller;

const std::string& Map_Name (const Map& map);
const MapFormat& Map_getFormat (const Map& map);

namespace scene
{
	class Node;
	class Graph;
}

scene::Node* Map_GetWorldspawn (const Map& map);
scene::Node* Map_FindWorldspawn (Map& map);

template<typename Element> class BasicVector3;
typedef BasicVector3<float> Vector3;

extern Vector3 region_mins, region_maxs;

void Map_Reload (void);
bool Map_LoadFile (const std::string& filename);
bool Map_SaveFile (const std::string& filename);

bool Map_ChangeMap (const std::string &dialogTitle, const std::string& newFilename = "");

void Map_New ();
void Map_Free ();

void Map_RegionOff ();

bool Map_SaveRegion (const std::string& filename);

class TextInputStream;
class TextOutputStream;

void Map_ImportSelected (TextInputStream& in, const MapFormat& format);
void Map_ExportSelected (TextOutputStream& out, const MapFormat& format);

bool Map_Modified (const Map& map);
void Map_SetModified (Map& map, bool modified);

bool Map_Save ();
bool Map_SaveAs ();

void Scene_parentSelectedBrushesToEntity (scene::Graph& graph, scene::Node& parent);
std::size_t Scene_countSelectedBrushes (scene::Graph& graph);

void NewMap ();
void OpenMap ();
void ImportMap ();
void SaveMapAs ();
void SaveMap ();
void ExportMap ();
void SaveRegion ();
bool Map_ImportFile (const std::string& filename);

void Map_Traverse (scene::Node& root, const scene::Traversable::Walker& walker);

void SelectBrush (int entitynum, int brushnum, int select);

void Map_Construct ();
void Map_Destroy ();

void Map_gatherNamespaced (scene::Node& root);
void Map_mergeClonedNames ();

const std::string& getMapsPath ();

namespace map
{
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

	int countSelectedBrushes();

	/**
	 * @return @c true if the map is not yet saved, @c false otherwise
	 */
	bool isUnnamed ();

	scene::Node& findOrInsertWorldspawn();

	// returns the full path to the current loaded map
	const std::string& getMapName();

	const std::string& getMapsPath ();
}

#endif
