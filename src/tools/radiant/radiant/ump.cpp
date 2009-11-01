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
			typedef const char* first_argument_type;

			UMPCollector (std::set<std::string> &list) :
				_list(list)
			{
			}

			// Functor operator needed for the forEachFile() call
			void operator() (const char* file)
			{
				std::string rawPath(file);
				std::string extension = os::getExtension(rawPath);

				if (extension == "ump")
					_list.insert(rawPath);
			}
	};
}

UMPSystem::UMPSystem ()
{
}

void UMPSystem::init ()
{
	UMPCollector collector(_umpFiles);
	GlobalFileSystem().forEachFile("maps/", "*", makeCallback1(collector), 0);
	std::size_t size = _umpFiles.size();
	globalOutputStream() << "Found " << size << " ump files\n";
	// TODO: Register observer for base directory
}

void UMPSystem::editUMPDefinition ()
{
	const std::string umpFileName = getUMPFilename(GlobalRadiant().getMapName());
	if (umpFileName.length() == 0) {
		gtkutil::infoDialog(GlobalRadiant().getMainWindow(), _("Could not find the map in any ump file"));
		return;
	}
	ui::UMPEditor editor("maps/" + umpFileName);
	editor.show();
}

const std::string UMPSystem::getUMPFilename (const std::string& map)
{
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
			globalErrorStream() << e.getMessage().c_str() << "\n";
			gtkutil::errorDialog(GlobalRadiant().getMainWindow(), e.getMessage());
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
		STRING_CONSTANT(Name, "*");

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
