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

#include "ifilesystem.h"
#include "iradiant.h"
#include "ieventmanager.h"

#include "ui/umpeditor/UMPEditor.h"
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
class UMPSystem : public IUMPSystem {
	public:
		// Radiant Module stuff
		typedef IUMPSystem Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		IUMPSystem* getTable() {
			return this;
		}

	private:

		std::set<std::string> _umpFiles;
		typedef std::set<std::string>::iterator UMPFilesIterator;

	public:

		void editUMPDefinition () {
			const std::string umpFileName = getUMPFilename(GlobalRadiant().getMapName());
			if (umpFileName.empty()) {
				gtkutil::infoDialog(_("Could not find the map in any ump file"));
				return;
			}
			ui::UMPEditor editor(GlobalRadiant().getMapsPath() + umpFileName);
			editor.show();
		}

		/**
		 * @return A vector with ump filesnames
		 */
		const std::set<std::string> getFiles () const
		{
			return _umpFiles;
		}

		void init ()
		{
			UMPCollector collector(_umpFiles);
			// TODO: Register observer for base directory
		}

		/**
		 * @return The ump filename for the given map
		 */
		std::string getUMPFilename (const std::string& map)
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
};

void UMP_Construct ()
{
	GlobalUMPSystem().init();
	GlobalEventManager().addCommand("EditUMPDefinition", MemberCaller<IUMPSystem, &IUMPSystem::editUMPDefinition> (GlobalUMPSystem()));
}

void UMP_Destroy ()
{
}

/* Required code to register the module with the ModuleServer.
 */
#include "modulesystem/singletonmodule.h"

/* UMP dependencies class.
 */
class UMPSystemDependencies: public GlobalFileSystemModuleRef
{
};

typedef SingletonModule<UMPSystem, UMPSystemDependencies> UMPModule;

typedef Static<UMPModule> StaticUMPSystemModule;
StaticRegisterModule staticRegisterUMPSystem(StaticUMPSystemModule::instance());
