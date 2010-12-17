#include "ifilesystem.h"
#include <list>
#include <vector>
#include <string>

#include "moduleobservers.h"

#define VFS_MAXDIRS 8

class FileSystem: public VirtualFileSystem
{
	private:

		class ArchiveEntry
		{
			public:
				std::string name;
				Archive* archive;
				bool is_pakfile;
		};
		typedef std::list<ArchiveEntry> ArchiveEntryList;
		ArchiveEntryList g_archives;

		std::vector<std::string> g_strDirs;
		bool g_bUsePak;

		ModuleObservers g_observers;

		void initPK3File (const std::string& filename);

	public:
		FileSystem ();

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
