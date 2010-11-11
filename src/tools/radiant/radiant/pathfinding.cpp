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
#include "map/map.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "ifilesystem.h"
#include "map/map.h"
#include "signal/isignal.h"
#include "stringio.h"
#include "settings/preferences.h"
#include "preferencesystem.h"
#include "radiant_i18n.h"
#include "gtkutil/widget.h"
#include "gtkmisc.h"
#include "ui/Icons.h"
#include "ieventmanager.h"

static GtkMenuItem *menuItemShowIn2D;
static GtkMenuItem *menuItemShowLowerLevels;

namespace routing {

namespace {
const std::string RKEY_PATHFINDING_SHOW_ALL_LOWER = "user/ui/pathfinding/showAllLowerLevels";
const std::string RKEY_PATHFINDING_SHOW_IN_2D = "user/ui/pathfinding/showIn2D";
const std::string RKEY_PATHFINDING_SHOW = "user/ui/pathfinding/show";
}

class Pathfinding: public PreferenceConstructor, public RegistryKeyObserver
{
	private:
		bool _showPathfinding;
		bool _showIn2D;
		Routing *_routingRender;
		bool _showAllLowerLevels;

	public:

		Pathfinding () :
			_showPathfinding(GlobalRegistry().get(RKEY_PATHFINDING_SHOW) == "1"), _showIn2D(GlobalRegistry().get(
					RKEY_PATHFINDING_SHOW_IN_2D) == "1"), _routingRender(0), _showAllLowerLevels(GlobalRegistry().get(
					RKEY_PATHFINDING_SHOW_ALL_LOWER) == "1")
		{
			_routingRender = new Routing();
			GlobalShaderCache().attachRenderable(*_routingRender);

			GlobalRegistry().addKeyObserver(this, RKEY_PATHFINDING_SHOW_ALL_LOWER);
			GlobalRegistry().addKeyObserver(this, RKEY_PATHFINDING_SHOW_IN_2D);
			GlobalRegistry().addKeyObserver(this, RKEY_PATHFINDING_SHOW);

			GlobalPreferenceSystem().addConstructor(this);
		}

		// Update the internally stored variables on registry key change
		void keyChanged ()
		{
			_showPathfinding = (GlobalRegistry().get(RKEY_PATHFINDING_SHOW) == "1");
			_showAllLowerLevels = (GlobalRegistry().get(RKEY_PATHFINDING_SHOW_ALL_LOWER) == "1");
			_showIn2D = (GlobalRegistry().get(RKEY_PATHFINDING_SHOW_IN_2D) == "1");

			_routingRender->setShowPathfinding(_showPathfinding);
			_routingRender->setShowAllLowerLevels(_showAllLowerLevels);
			_routingRender->setShowIn2D(_showIn2D);

			GlobalEventManager().setToggled("ShowPathfinding", _showPathfinding);
			GlobalEventManager().setToggled("ShowPathfindingLowerLevels", _showAllLowerLevels);
			GlobalEventManager().setToggled("ShowPathfindingIn2D", _showIn2D);

			showPathfinding();
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
				showPathfinding();
			}
		}

		void setShowAllLowerLevels (bool showAllLowerLevels)
		{
			GlobalRegistry().set(RKEY_PATHFINDING_SHOW_ALL_LOWER, showAllLowerLevels ? "1" : "0");
		}

		void setShowIn2D (bool showIn2D)
		{
			GlobalRegistry().set(RKEY_PATHFINDING_SHOW_IN_2D, showIn2D ? "1" : "0");
		}

		void setShow (bool show)
		{
			GlobalRegistry().set(RKEY_PATHFINDING_SHOW, show ? "1" : "0");
		}

		void toggleShowPathfindingIn2D ()
		{
			setShowIn2D(!_showIn2D);
		}

		void toggleShowLowerLevelsForPathfinding ()
		{
			setShowAllLowerLevels(!_showAllLowerLevels);
		}

		void showPathfinding ()
		{
			if (_showPathfinding) {
				//update current pathfinding data on every activation
				const std::string& mapName = GlobalRadiant().getMapName();
				const std::string baseName = os::stripExtension(mapName);
				const std::string bspName = baseName + ".bsp";
				_routingRender->updateRouting(GlobalFileSystem().getRelative(bspName));
			}
			SceneChangeNotify();
		}

		/**
		 * @todo Maybe also use the ufo2map output directly
		 * @sa ToolsCompile
		 */
		void toggleShowPathfinding ()
		{
			setShow(!_showPathfinding);
		}

		void constructPreferencePage (PreferenceGroup& group)
		{
			PreferencesPage* page = group.createPage(_("Pathfinding"), _("Pathfinding Settings"));
			PrefPage *p = reinterpret_cast<PrefPage*> (page);
			p->appendCheckBox("", _("Show pathfinding information"), RKEY_PATHFINDING_SHOW);
			p->appendCheckBox("", _("Show all lower levels"), RKEY_PATHFINDING_SHOW_ALL_LOWER);
			p->appendCheckBox("", _("Show in 2D views"), RKEY_PATHFINDING_SHOW_IN_2D);
		}
};

Pathfinding *pathfinding;

/**
 * @brief callback function for map changes to update routing data.
 */
void Pathfinding_onMapValidChanged ()
{
	pathfinding->onMapValidChanged();
}
}

void Pathfinding_Construct (void)
{
	routing::pathfinding = new routing::Pathfinding();

	GlobalEventManager().addToggle("ShowPathfinding", MemberCaller<routing::Pathfinding,
			&routing::Pathfinding::toggleShowPathfinding> (*routing::pathfinding));
	GlobalEventManager().addToggle("ShowPathfindingIn2D", MemberCaller<routing::Pathfinding,
			&routing::Pathfinding::toggleShowPathfindingIn2D> (*routing::pathfinding));
	GlobalEventManager().addToggle("ShowPathfindingLowerLevels", MemberCaller<routing::Pathfinding,
			&routing::Pathfinding::toggleShowLowerLevelsForPathfinding> (*routing::pathfinding));

	Map_addValidCallback(g_map, SignalHandler(FreeCaller<routing::Pathfinding_onMapValidChanged> ()));
}

void Pathfinding_Destroy (void)
{
	delete routing::pathfinding;
}
