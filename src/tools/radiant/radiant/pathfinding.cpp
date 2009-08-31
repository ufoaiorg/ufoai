/**
 * @file pathfinding.cpp
 * @brief Show pathfinding related info in the radiant windows
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

#include "pathfinding.h"
#include "pathfinding/Routing.h"
#include "map.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "ifilesystem.h"
#include "map.h"
#include "signal/isignal.h"

namespace routing
{
	static bool showPathfinding;
	static Routing routingRender = Routing();
	/**
	 * @todo Maybe also use the ufo2map output directly
	 * @sa ToolsCompile
	 */
	void ShowPathfinding (void)
	{
		if (!Map_Unnamed(g_map)) {
			showPathfinding ^= true;
			routingRender.setShowPathfinding(showPathfinding);
			if (showPathfinding) {
				//update current pathfinding data on every activation
				const char* mapname = Map_Name(g_map);
				StringOutputStream bspStream(256);
				bspStream << StringRange(mapname, path_get_filename_base_end(mapname)) << ".bsp";
				const char* bspname = path_make_relative(bspStream.c_str(), GlobalFileSystem().findRoot(
						bspStream.c_str()));
				routingRender.updateRouting(bspname);
			}
		} else {
			showPathfinding = false;
			routingRender.setShowPathfinding(false);
		}

	}

	/**
	 * @brief callback function for map changes to update routing data.
	 */
	void Pathfinding_onMapValidChanged (void)
	{
		if (Map_Valid(g_map) && showPathfinding) {
			showPathfinding = false;
			ShowPathfinding();
		}
	}
}

void Pathfinding_Construct (void)
{
	/**@todo listener also should activate/deactivate "show pathfinding" menu entry if no appropriate data is available */
	GlobalShaderCache().attachRenderable(routing::routingRender);
	Map_addValidCallback(g_map, SignalHandler(FreeCaller<routing::Pathfinding_onMapValidChanged> ()));
}

void Pathfinding_Destroy (void)
{
	GlobalShaderCache().detachRenderable(routing::routingRender);
}
