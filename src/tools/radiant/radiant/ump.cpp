/**
 * @file ump.cpp
 * @brief UMP API code
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

#include "ump.h"
#include "radiant_i18n.h"

#include "iradiant.h"
#include "AutoPtr.h"
#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"
#include "ui/umpeditor/UMPEditor.h"
#include "map/map.h"
#include "commands.h"
#include "generic/callback.h"
#include "ump/UMPFile.h"
#include "ump/UMPTile.h"
#include "gtkutil/dialog.h"
#include "os/path.h"

namespace
{
	class UMPCollector
	{
		private:
			std::set<std::string>& _list;

		public:
			typedef const std::string& first_argument_type;

			UMPCollector (std::set<std::string> &list) :
				_list(list)
			{
				GlobalFileSystem().forEachFile("maps/", "*", makeCallback1(*this), 0);
				std::size_t size = _list.size();
				globalOutputStream() << "Found " << string::toString(size) << " ump files\n";
			}

			// Functor operator needed for the forEachFile() call
			void operator() (const std::string& file)
			{
				std::string extension = os::getExtension(file);

				if (extension == "ump")
					_list.insert(file);
			}
	};
}

UMPSystem::UMPSystem ()
{
}

void UMPSystem::init ()
{
	UMPCollector collector(_umpFiles);
	// TODO: Register observer for base directory
}

void UMPSystem::editUMPDefinition ()
{
	const std::string umpFileName = getUMPFilename(GlobalRadiant().getMapName());
	if (umpFileName.empty()) {
		gtkutil::infoDialog(_("Could not find the map in any ump file"));
		return;
	}
	ui::UMPEditor editor(GlobalRadiant().getMapsPath() + umpFileName);
	editor.show();
}

const std::string UMPSystem::getUMPFilename (const std::string& map)
{
	if (map.empty())
		return "";
	for (UMPFilesIterator i = _umpFiles.begin(); i != _umpFiles.end(); i++) {
		try {
			map::ump::UMPFile umpFile(*i);
			if (!umpFile.load()) {
				globalErrorStream() << "Could not load the ump file " << (*i) << "\n";
				continue;
			}

			map::ump::UMPTile* tile = umpFile.getTileForMap(map);
			if (tile)
				return *i;
		} catch (map::ump::UMPException& e) {
			globalErrorStream() << e.getMessage() << "\n";
			gtkutil::errorDialog(e.getMessage());
		}
	}
	// not found in any ump file
	return "";
}

class UMPSystemAPI
{
	private:

		UMPSystem * _UMPSystem;
	public:
		typedef UMPSystem Type;
		STRING_CONSTANT(Name, "*")
		;

		UMPSystemAPI () :
			_UMPSystem(0)
		{
			_UMPSystem = new UMPSystem();
		}
		~UMPSystemAPI ()
		{
			delete _UMPSystem;
		}

		UMPSystem* getTable ()
		{
			return _UMPSystem;
		}
};

typedef SingletonModule<UMPSystemAPI> UMPSystemModule;
typedef Static<UMPSystemModule> StaticUMPSystemModule;
StaticRegisterModule staticRegisterUMP(StaticUMPSystemModule::instance());

void EditUMPDefinition ()
{
	GlobalUMPSystem()->editUMPDefinition();
}

void UMP_Construct ()
{
	GlobalRadiant().commandInsert("EditUMPDefinition", FreeCaller<EditUMPDefinition> (), accelerator_null());
	GlobalUMPSystem()->init();
}

void UMP_Destroy ()
{
}

#include "gtkutil/menu.h"
#include "gtkutil/container.h"
#include "gtkutil/pointer.h"
#include "map/map.h"
#include "qe3.h"
namespace map
{
	namespace ump
	{
		/**
		 * Callback function for menu items to load the specific file
		 * @param menuItem activated menu item
		 * @param name map name to load as pointer to g_quark
		 */
		void UMPMenuItem_activate (GtkMenuItem* menuItem, gpointer name)
		{
			Map_ChangeMap(_("Open Map"), g_quark_to_string(gpointer_to_int(name)));
		}

		/**
		 * visitor implementation that adds a menu item for each ump tile
		 */
		class UMPTileToMenuItemVisitor: public UMPTileVisitor
		{
			private:
				GtkMenu* _parent;
				UMPFile &_file;
				UMPMenuCreator& _creator;

				int _count;

			public:
				UMPTileToMenuItemVisitor (UMPMenuCreator &creator, GtkMenu *parent, UMPFile &file) :
					_parent(parent), _file(file), _creator(creator), _count(0)
				{
				}
				void visit (const UMPTile& tile)
				{
					std::string base = _file.getBase();
					const std::string name = tile.getTileName(base);
					const std::string relativeName = "maps/" + _file.getMapName(name) + ".map";
					const std::string filename = GlobalFileSystem().findFile(relativeName) + relativeName;
					GtkMenuItem* item = GTK_MENU_ITEM(gtk_menu_item_new_with_label(name.c_str()));
					gtk_widget_show(GTK_WIDGET(item));
					g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(UMPMenuItem_activate), gint_to_pointer(
							g_quark_from_string(filename.c_str())));
					container_add_widget(GTK_CONTAINER(_parent), GTK_WIDGET(item));
					_count++;
				}
				int getCount (void)
				{
					return _count;
				}

		};

		/**
		 * Method recreates rma tile menu adding entries for all tiles in current rma if available
		 */
		void UMPMenuCreator::updateMenu ()
		{
			std::string umpFilename = GlobalUMPSystem()->getUMPFilename(GlobalRadiant().getMapName());
			if (!umpFilename.empty()) {
				map::ump::UMPFile file = map::ump::UMPFile(umpFilename);
				if (!file.load())
					return;
				container_remove_all(GTK_CONTAINER(_menu));
				map::ump::UMPTileToMenuItemVisitor visitor(*this, _menu, file);
				file.forEachTile(visitor);
				if (visitor.getCount() > 0)
					gtk_widget_set_sensitive(GTK_WIDGET(_menuItem), true);
				else
					gtk_widget_set_sensitive(GTK_WIDGET(_menuItem), false);
			} else {
				gtk_widget_set_sensitive(GTK_WIDGET(_menuItem), false);
			}
		}
	}
}

void UMP_constructMenu (GtkMenuItem* menuItem, GtkMenu* menu)
{
	map::ump::UMPMenuCreator::getInstance()->setMenuEntry(menuItem, menu);
	map::ump::UMPMenuCreator::getInstance()->updateMenu();
}
