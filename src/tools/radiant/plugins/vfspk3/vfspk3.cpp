/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#include "vfspk3.h"

#include "iradiant.h"
#include "iarchive.h"
#include "ifilesystem.h"

#include "modulesystem/singletonmodule.h"
#include "modulesystem/modulesmap.h"

#include "vfs.h"

class FileSystemDependencies: public GlobalRadiantModuleRef
{
		ArchiveModulesRef m_archive_modules;
	public:
		FileSystemDependencies () :
			m_archive_modules(GlobalRadiant().getRequiredGameDescriptionKeyValue("archivetypes"))
		{
		}
		ArchiveModules& getArchiveModules ()
		{
			return m_archive_modules.get();
		}
};

class FileSystemAPI
{
		VirtualFileSystem* m_filesystem;
	public:
		typedef VirtualFileSystem Type;
		STRING_CONSTANT(Name, "*");

		FileSystemAPI ()
		{
			FileSystem_Init();
			m_filesystem = &GetFileSystem();
		}
		~FileSystemAPI ()
		{
			FileSystem_Shutdown();
		}
		VirtualFileSystem* getTable ()
		{
			return m_filesystem;
		}
};

typedef SingletonModule<FileSystemAPI, FileSystemDependencies> FileSystemModule;

FileSystemModule g_FileSystemModule;

ArchiveModules& FileSystemAPI_getArchiveModules ()
{
	return g_FileSystemModule.getDependencies().getArchiveModules();
}

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules (ModuleServer& server)
{
	initialiseModule(server);

	g_FileSystemModule.selfRegister();
}
