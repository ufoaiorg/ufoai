#include "FileSystemModule.h"

#include "ifilesystem.h"
#include "iradiant.h"

#include "FileSystem.h"

#include "modulesystem/singletonmodule.h"
#include "modulesystem/modulesmap.h"

class FileSystemDependencies: public GlobalRadiantModuleRef
{
		ArchiveModulesRef m_archive_modules;
	public:
		FileSystemDependencies () :
			m_archive_modules("*")
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
			m_filesystem = new FileSystem();
		}
		~FileSystemAPI ()
		{
			delete m_filesystem;
		}
		VirtualFileSystem* getTable ()
		{
			return m_filesystem;
		}
};

typedef SingletonModule<FileSystemAPI, FileSystemDependencies> FileSystemModule;

typedef Static<FileSystemModule> StaticFileSystemModule;
StaticRegisterModule staticRegisterFileSystem(StaticFileSystemModule::instance());

ArchiveModules& FileSystemAPI_getArchiveModules ()
{
	return StaticFileSystemModule::instance().getDependencies().getArchiveModules();
}
