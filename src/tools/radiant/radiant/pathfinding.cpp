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
#include "commands.h"
#include "radiant_i18n.h"
#include "gtkutil/widget.h"
#include "gtkmisc.h"
#include "ui/Icons.h"

static GtkCheckMenuItem *menuItemShowIn2D;
static GtkCheckMenuItem *menuItemShowLowerLevels;

namespace routing
{
	bool showAllLowerLevels;
	bool showIn2D;

	class Pathfinding
	{
		private:
			bool _showPathfinding;
			bool _showIn2D;
			Routing *_routingRender;
			bool _showAllLowerLevels;

		public:

			Pathfinding () :
				_showPathfinding(false), _showIn2D(false), _routingRender(0), _showAllLowerLevels(true)
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

			void setShowIn2D (bool showIn2D)
			{
				_showIn2D = showIn2D;
				_routingRender->setShowIn2D(_showIn2D);
			}

			/**
			 * @todo Maybe also use the ufo2map output directly
			 * @sa ToolsCompile
			 */
			bool show (void)
			{
				if (!Map_Unnamed(g_map))
					_showPathfinding ^= true;
				else
					_showPathfinding = false;

				_routingRender->setShowPathfinding(_showPathfinding);
				_routingRender->setShowAllLowerLevels(_showAllLowerLevels);

				if (_showPathfinding) {
					//update current pathfinding data on every activation
					const std::string& mapName = GlobalRadiant().getMapName();
					const std::string baseName = os::stripExtension(mapName);
					const std::string bspName = baseName + ".bsp";
					_routingRender->updateRouting(GlobalFileSystem().getRelative(bspName));
				}
				SceneChangeNotify();

				return _showPathfinding;
			}
	};

	Pathfinding *pathfinding;
	bool showPathfinding;

	bool IsPathfindingViewEnabled ()
	{
		return showPathfinding;
	}

	bool IsPathfindingIn2DViewEnabled ()
	{
		return showIn2D;
	}

	bool IsPathfindingShowLowerLevelsEnabled ()
	{
		return showAllLowerLevels;
	}

	/*!  Toggle menu callback definitions  */
	typedef FreeCaller1<const BoolImportCallback&, &BoolFunctionExport<routing::IsPathfindingViewEnabled>::apply>
			ShowPathfindingEnabledApplyCaller;
	ShowPathfindingEnabledApplyCaller g_showPathfindingEnabled_button_caller;
	BoolExportCallback g_showPathfindingEnabled_button_callback(g_showPathfindingEnabled_button_caller);

	ToggleItem g_showPathfindingEnabled_button(g_showPathfindingEnabled_button_callback);

	typedef FreeCaller1<const BoolImportCallback&, &BoolFunctionExport<routing::IsPathfindingIn2DViewEnabled>::apply>
			ShowPathfindingIn2DEnabledApplyCaller;
	ShowPathfindingIn2DEnabledApplyCaller g_showPathfindingIn2DEnabled_button_caller;
	BoolExportCallback g_showPathfindingIn2DEnabled_button_callback(g_showPathfindingIn2DEnabled_button_caller);

	ToggleItem g_showPathfindingIn2DEnabled_button(g_showPathfindingIn2DEnabled_button_callback);

	typedef FreeCaller1<const BoolImportCallback&,
			&BoolFunctionExport<routing::IsPathfindingShowLowerLevelsEnabled>::apply>
			ShowLowerLevelsForPathfindingEnabledApplyCaller;
	ShowLowerLevelsForPathfindingEnabledApplyCaller g_showLowerLevelsForPathfindingEnabled_button_caller;
	BoolExportCallback g_showLowerLevelsForPathfindingEnabled_button_callback(
			g_showLowerLevelsForPathfindingEnabled_button_caller);
	ToggleItem g_showLowerLevelsForPathfindingEnabled_button(g_showLowerLevelsForPathfindingEnabled_button_callback);

	/**
	 * @todo Maybe also use the ufo2map output directly
	 * @sa ToolsCompile
	 */
	void ShowPathfinding ()
	{
		showPathfinding = pathfinding->show();
		g_showPathfindingEnabled_button.update();
		gtk_widget_set_sensitive(GTK_WIDGET(menuItemShowIn2D), showPathfinding);
		gtk_widget_set_sensitive(GTK_WIDGET(menuItemShowLowerLevels), showPathfinding);
	}

	void ShowPathfindingIn2D ()
	{
		if (showPathfinding)
			showIn2D ^= true;
		else
			showIn2D = false;
		pathfinding->setShowIn2D(showIn2D);
		g_showPathfindingIn2DEnabled_button.update();
		SceneChangeNotify();
	}

	void ShowLowerLevelsForPathfinding ()
	{
		if (showPathfinding)
			showAllLowerLevels ^= true;
		else
			showAllLowerLevels = false;
		pathfinding->setShowAllLowerLevels(showAllLowerLevels);
		g_showLowerLevelsForPathfindingEnabled_button.update();
		SceneChangeNotify();
	}

	/**
	 * @brief callback function for map changes to update routing data.
	 */
	void Pathfinding_onMapValidChanged ()
	{
		pathfinding->onMapValidChanged();
	}

	void setShowAllLowerLevels (bool value)
	{
		showAllLowerLevels = value;
		pathfinding->setShowAllLowerLevels(showAllLowerLevels);
		SceneChangeNotify();
	}

	void setShowIn2D (bool value)
	{
		showIn2D = value;
		pathfinding->setShowIn2D(showIn2D);
		SceneChangeNotify();
	}

	void Pathfinding_constructPage (PreferenceGroup& group)
	{
		PreferencesPage* page = group.createPage(_("Pathfinding"), _("Pathfinding Settings"));
		PrefPage *p = reinterpret_cast<PrefPage*>(page);
		p->appendCheckBox("", _("Show all lower levels"), FreeCaller1<bool, setShowAllLowerLevels> (),
				BoolExportCaller(showAllLowerLevels));
		p->appendCheckBox("", _("Show in 2D views"), FreeCaller1<bool, setShowIn2D> (),
				BoolExportCaller(showIn2D));
	}

	void Pathfinding_registerPreferences (void)
	{
		bool actualShowLowerLevels = showAllLowerLevels;
		bool actualShowIn2D = showIn2D;
		GlobalPreferenceSystem().registerPreference("PathfindingShowLowerLevels", BoolImportStringCaller(
				showAllLowerLevels), BoolExportStringCaller(showAllLowerLevels));
		GlobalPreferenceSystem().registerPreference("PathfindingShowIn2D", BoolImportStringCaller(showIn2D),
				BoolExportStringCaller(showIn2D));
		if (actualShowIn2D != showIn2D)
			setShowIn2D(showIn2D);
		if (actualShowLowerLevels != showAllLowerLevels)
			setShowAllLowerLevels(showAllLowerLevels);

		PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Pathfinding_constructPage> ());
	}
}

void Pathfinding_Construct (void)
{
	routing::pathfinding = new routing::Pathfinding();

	GlobalToggles_insert("ShowPathfinding", FreeCaller<routing::ShowPathfinding> (), ToggleItem::AddCallbackCaller(
			routing::g_showPathfindingEnabled_button), accelerator_null());
	GlobalToggles_insert("ShowPathfindingIn2D", FreeCaller<routing::ShowPathfindingIn2D> (),
			ToggleItem::AddCallbackCaller(routing::g_showPathfindingIn2DEnabled_button), accelerator_null());
	GlobalToggles_insert("ShowPathfindingLowerLevels", FreeCaller<routing::ShowLowerLevelsForPathfinding> (),
			ToggleItem::AddCallbackCaller(routing::g_showLowerLevelsForPathfindingEnabled_button), accelerator_null());

	/** @todo listener also should activate/deactivate "show pathfinding" menu entry if no appropriate data is available */
	Map_addValidCallback(g_map, SignalHandler(FreeCaller<routing::Pathfinding_onMapValidChanged> ()));

	routing::Pathfinding_registerPreferences();
}

void Pathfinding_Destroy (void)
{
	delete routing::pathfinding;
}

void Pathfinding_ConstructMenu (GtkMenu* menu)
{
	create_check_menu_item_with_mnemonic(menu, _("Show pathfinding info"), "ShowPathfinding");
	menuItemShowIn2D = create_check_menu_item_with_mnemonic(menu, _("Show in 2D views"), "ShowPathfindingIn2D");
	menuItemShowLowerLevels = create_check_menu_item_with_mnemonic(menu, _("Show all lower levels"),
			"ShowPathfindingLowerLevels");
	//disabled items by default, enabled via show pathfinding
	gtk_widget_set_sensitive(GTK_WIDGET(menuItemShowIn2D), false);
	gtk_widget_set_sensitive(GTK_WIDGET(menuItemShowLowerLevels), false);
}
