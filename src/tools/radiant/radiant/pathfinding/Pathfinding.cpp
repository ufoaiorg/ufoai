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

#include "Pathfinding.h"

#include "ifilesystem.h"
#include "iregistry.h"
#include "ieventmanager.h"

#include "stringio.h"
#include "signal/isignal.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "preferencesystem.h"
#include "radiant_i18n.h"
#include "gtkutil/widget.h"

#include "../ui/Icons.h"
#include "../map/map.h"

namespace routing {

namespace {
const std::string RKEY_PATHFINDING_SHOW_ALL_LOWER = "user/ui/pathfinding/showAllLowerLevels";
const std::string RKEY_PATHFINDING_SHOW_IN_2D = "user/ui/pathfinding/showIn2D";
const std::string RKEY_PATHFINDING_SHOW = "user/ui/pathfinding/show";
}

Pathfinding::Pathfinding () :
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
void Pathfinding::keyChanged (const std::string& changedKey, const std::string& newValue)
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

Pathfinding::~Pathfinding ()
{
	GlobalShaderCache().detachRenderable(*_routingRender);
	delete _routingRender;
}
/**
 * @brief callback function for map changes to update routing data.
 */
void Pathfinding::onMapValidChanged (void)
{
	if (GlobalMap().isValid() && _showPathfinding) {
		_showPathfinding = false;
		showPathfinding();
	}
}

void Pathfinding::setShowAllLowerLevels (bool showAllLowerLevels)
{
	GlobalRegistry().set(RKEY_PATHFINDING_SHOW_ALL_LOWER, showAllLowerLevels ? "1" : "0");
}

void Pathfinding::setShowIn2D (bool showIn2D)
{
	GlobalRegistry().set(RKEY_PATHFINDING_SHOW_IN_2D, showIn2D ? "1" : "0");
}

void Pathfinding::setShow (bool show)
{
	GlobalRegistry().set(RKEY_PATHFINDING_SHOW, show ? "1" : "0");
}

void Pathfinding::toggleShowPathfindingIn2D ()
{
	setShowIn2D(!_showIn2D);
}

void Pathfinding::toggleShowLowerLevelsForPathfinding ()
{
	setShowAllLowerLevels(!_showAllLowerLevels);
}

void Pathfinding::showPathfinding ()
{
	if (_showPathfinding) {
		//update current pathfinding data on every activation
		const std::string& mapName = GlobalMap().getName();
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
void Pathfinding::toggleShowPathfinding ()
{
	setShow(!_showPathfinding);
}

void Pathfinding::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Pathfinding"), _("Pathfinding Settings"));
	page->appendCheckBox("", _("Show pathfinding information"), RKEY_PATHFINDING_SHOW);
	page->appendCheckBox("", _("Show all lower levels"), RKEY_PATHFINDING_SHOW_ALL_LOWER);
	page->appendCheckBox("", _("Show in 2D views"), RKEY_PATHFINDING_SHOW_IN_2D);
}

void Pathfinding::init ()
{
	GlobalEventManager().addToggle("ShowPathfinding", MemberCaller<Pathfinding, &Pathfinding::toggleShowPathfinding> (
			*this));
	GlobalEventManager().addToggle("ShowPathfindingIn2D", MemberCaller<Pathfinding,
			&Pathfinding::toggleShowPathfindingIn2D> (*this));
	GlobalEventManager().addToggle("ShowPathfindingLowerLevels", MemberCaller<Pathfinding,
			&Pathfinding::toggleShowLowerLevelsForPathfinding> (*this));

	GlobalMap().addValidCallback(SignalHandler(MemberCaller<Pathfinding, &Pathfinding::onMapValidChanged> (*this)));
}

}
