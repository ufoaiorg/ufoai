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
#include "stringio.h"
#include "preferences.h"
#include "preferencesystem.h"
#include "commands.h"
#include "radiant_i18n.h"

namespace routing
{
	bool showAllLowerLevels;

	class Pathfinding
	{
		private:
			bool _showPathfinding;
			Routing *_routingRender;
			bool _showAllLowerLevels;

		public:

			Pathfinding () :
				_showPathfinding(false), _routingRender(0)
			{
				_routingRender = new Routing();
				GlobalShaderCache().attachRenderable(*_routingRender);
			}

			~Pathfinding ()
			{
				GlobalShaderCache().detachRenderable(*_routingRender);
				delete _routingRender;
			}
			/**
			 * @brief callback function for map changes to update routing data.
			 */
			void onMapValidChanged (void)
			{
				if (Map_Valid(g_map) && _showPathfinding) {
					_showPathfinding = false;
					ShowPathfinding();
				}
			}

			void setShowAllLowerLevels (bool showAllLowerLevels)
			{
				_showAllLowerLevels = showAllLowerLevels;
				_routingRender->setShowAllLowerLevels(_showAllLowerLevels);
			}

			/**
			 * @todo Maybe also use the ufo2map output directly
			 * @sa ToolsCompile
			 */
			void show (void)
			{
				if (!Map_Unnamed(g_map))
					_showPathfinding ^= true;
				else
					_showPathfinding = false;

				_routingRender->setShowPathfinding(_showPathfinding);
				_routingRender->setShowAllLowerLevels(_showAllLowerLevels);

				if (_showPathfinding) {
					//update current pathfinding data on every activation
					const std::string& mapname = Map_Name(g_map);
					StringOutputStream bspStream(256);
					bspStream << StringRange(mapname.c_str(), path_get_filename_base_end(mapname.c_str())) << ".bsp";
					const char* bspname = path_make_relative(bspStream.c_str(), GlobalFileSystem().findRoot(
							bspStream.c_str()));
					_routingRender->updateRouting(bspname);
				}
			}
	};

	Pathfinding *pathfinding;

	/**
	 * @todo Maybe also use the ufo2map output directly
	 * @sa ToolsCompile
	 */
	void ShowPathfinding (void)
	{
		pathfinding->show();
	}

	/**
	 * @brief callback function for map changes to update routing data.
	 */
	void Pathfinding_onMapValidChanged (void)
	{
		pathfinding->onMapValidChanged();
	}

	void setShowAllLowerLevels (bool value)
	{
		showAllLowerLevels = value;
		pathfinding->setShowAllLowerLevels(showAllLowerLevels);
		SceneChangeNotify();
	}

	void Pathfinding_constructPage (PreferenceGroup& group)
	{
		PreferencesPage page(group.createPage(_("Pathfinding"), _("Pathfinding Settings")));
		page.appendCheckBox("", _("Show all lower levels"), FreeCaller1<bool, setShowAllLowerLevels> (),
				BoolExportCaller(showAllLowerLevels));
	}

	void Pathfinding_registerPreferences (void)
	{
		GlobalPreferenceSystem().registerPreference("PathfindingShowLowerLevels", BoolImportStringCaller(
				showAllLowerLevels), BoolExportStringCaller(showAllLowerLevels));

		PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Pathfinding_constructPage> ());
	}
}

void Pathfinding_Construct (void)
{
	routing::pathfinding = new routing::Pathfinding();

	GlobalCommands_insert("ShowPathfinding", FreeCaller<routing::ShowPathfinding> ());

	/** @todo listener also should activate/deactivate "show pathfinding" menu entry if no appropriate data is available */
	Map_addValidCallback(g_map, SignalHandler(FreeCaller<routing::Pathfinding_onMapValidChanged> ()));

	routing::Pathfinding_registerPreferences();
}

void Pathfinding_Destroy (void)
{
	delete routing::pathfinding;
}
