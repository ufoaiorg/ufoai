#include "ifilesystem.h"
#include <list>

#include "moduleobservers.h"

#define VFS_MAXDIRS 8

class UFOFileSystem: public VirtualFileSystem
{
	private:

		void ClearFileDirList (GSList **lst);

		class ArchiveEntry
		{
			public:
				std::string name;
				Archive* archive;
				bool is_pakfile;
		};
		typedef std::list<ArchiveEntry> ArchiveEntryList;
		ArchiveEntryList g_archives;

		char g_strDirs[VFS_MAXDIRS][PATH_MAX + 1];
		int g_numDirs;
		bool g_bUsePak;

		ModuleObservers g_observers;

		void InitPK3File (const std::string& filename);

		GSList
				* GetListInternal (const std::string& refdir, const std::string& ext, bool directories,
						std::size_t depth);

	public:
		UFOFileSystem ();

		/** @brief reads all pak files from a dir */
		void initDirectory (const std::string& path);

		void initialise ();

		/** @brief frees all memory that we allocated */
		void shutdown ();

		ArchiveFile* openFile (const std::string& filename);
		ArchiveTextFile* openTextFile (const std::string& filename);
		std::size_t loadFile (const std::string& filename, void **buffer);
		void freeFile (void *p);

		void forEachDirectory (const std::string& basedir, const FileNameCallback& callback, std::size_t depth);

		void forEachFile (const std::string& basedir, const std::string& extension, const FileNameCallback& callback,
				std::size_t depth);

		std::string findFile (const std::string& name);
		std::string findRoot (const std::string& name);
		std::string getRelative (const std::string& name);

		void attach (ModuleObserver& observer);
		void detach (ModuleObserver& observer);

		Archive* getArchive (const std::string& archiveName);
		void forEachArchive (const ArchiveNameCallback& callback);
};
