#include "FileSystem.h"

#include "iarchive.h"

#include "AutoPtr.h"
#include "string/string.h"
#include "os/path.h"
#include "generic/callback.h"

#include "directory/DirectoryArchive.h"
#include "DirectoryListVisitor.h"
#include "FileListVisitor.h"

/**
 * @note sort pakfiles in reverse order. This ensures that
 * later pakfiles override earlier ones. This because the vfs module
 * returns a filehandle to the first file it can find (while it should
 * return the filehandle to the file in the most overriding pakfile, the
 * last one in the list that is).
 */
class PakLess
{
	public:
		/**
		 * This behaves identically to stricmp(a,b), except that ASCII chars
		 * [\]^`_ come AFTER alphabet chars instead of before. This is because
		 * it converts all alphabet chars to uppercase before comparison,
		 * while stricmp converts them to lowercase.
		 */
		bool operator() (const std::string& self, const std::string& other) const
		{
			const std::string aU = string::toUpper(self);
			const std::string bU = string::toUpper(other);
			return aU.compare(bU);
		}
};

typedef std::set<std::string, PakLess> Archives;

UFOFileSystem::UFOFileSystem () :
	g_bUsePak(true)
{
}

inline const _QERArchiveTable* GetArchiveTable (ArchiveModules& archiveModules, const std::string& ext)
{
	return archiveModules.findModule(string::toLower(ext).c_str());
}

ArchiveModules& FileSystemAPI_getArchiveModules ();

void UFOFileSystem::InitPK3File (const std::string& filename)
{
	ArchiveModules& archiveModules = FileSystemAPI_getArchiveModules();
	const _QERArchiveTable* table = GetArchiveTable(archiveModules, os::getExtension(filename));

	if (table != 0) {
		ArchiveEntry entry;
		entry.name = filename;

		entry.is_pakfile = true;
		entry.archive = table->m_pfnOpenArchive(filename);
		g_archives.push_back(entry);
		g_message("  pk3 file: %s\n", filename.c_str());
	}
}

void UFOFileSystem::initDirectory (const std::string& directory)
{
	ArchiveModules& archiveModules = FileSystemAPI_getArchiveModules();

	if (g_numDirs == (VFS_MAXDIRS - 1))
		return;

	strncpy(g_strDirs[g_numDirs], std::string(DirectoryCleaned(directory)).c_str(), PATH_MAX);
	g_strDirs[g_numDirs][PATH_MAX] = '\0';

	const char* path = g_strDirs[g_numDirs];

	g_numDirs++;

	{
		ArchiveEntry entry;
		entry.name = path;
		entry.archive = OpenDirArchive(path);
		entry.is_pakfile = false;
		g_archives.push_back(entry);
	}

	if (g_bUsePak) {
		GDir* dir = g_dir_open(path, 0, 0);

		if (dir != 0) {
			g_message("vfs directory: %s\n", path);

			Archives archives;
			Archives archivesOverride;
			for (;;) {
				const char* name = g_dir_read_name(dir);
				if (name == 0)
					break;

				const char *ext = strrchr(name, '.');
				if ((ext == 0) || *(++ext) == '\0' || GetArchiveTable(archiveModules, ext) == 0)
					continue;

				archives.insert(name);
			}

			g_dir_close(dir);

			// add the entries to the vfs
			for (Archives::iterator i = archivesOverride.begin(); i != archivesOverride.end(); ++i) {
				std::string filename = path + *i;
				InitPK3File(filename);
			}
			for (Archives::iterator i = archives.begin(); i != archives.end(); ++i) {
				std::string filename = path + *i;
				InitPK3File(filename);
			}
		} else {
			g_message("vfs directory not found: '%s'\n", path);
		}
	}
}

void UFOFileSystem::initialise ()
{
	g_message("filesystem initialised\n");
	g_observers.realise();
}

void UFOFileSystem::shutdown ()
{
	g_observers.unrealise();
	g_message("filesystem shutdown\n");
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		delete i->archive;
	}
	g_archives.clear();

	g_numDirs = 0;
}

ArchiveFile* UFOFileSystem::openFile (const std::string& filename)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveFile* file = (*i).archive->openFile(filename);
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

ArchiveTextFile* UFOFileSystem::openTextFile (const std::string& filename)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveTextFile* file = i->archive->openTextFile(filename);
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

std::size_t UFOFileSystem::loadFile (const std::string& filename, void **buffer)
{
	AutoPtr<ArchiveFile> file(openFile(DirectoryCleaned(filename)));
	if (file) {
		*buffer = malloc(file->size() + 1);
		// we need to end the buffer with a 0
		((char*) (*buffer))[file->size()] = 0;

		std::size_t length = file->getInputStream().read((InputStream::byte_type*) *buffer, file->size());
		return length;
	}

	*buffer = 0;
	return 0;
}

void UFOFileSystem::freeFile (void *p)
{
	free(p);
}

void UFOFileSystem::ClearFileDirList (GSList **lst)
{
	while (*lst) {
		g_free((*lst)->data);
		*lst = g_slist_remove(*lst, (*lst)->data);
	}
}

GSList* UFOFileSystem::GetListInternal (const std::string& refdir, const std::string& ext, bool directories,
		std::size_t depth)
{
	GSList* files = 0;

	if (directories) {
		for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
			DirectoryListVisitor visitor(files, refdir);
			i->archive->forEachFile(Archive::VisitorFunc(visitor, Archive::eDirectories, depth), refdir);
		}
	} else {
		for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
			FileListVisitor visitor(files, refdir, ext);
			i->archive->forEachFile(Archive::VisitorFunc(visitor, Archive::eFiles, depth), refdir);
		}
	}

	files = g_slist_reverse(files);

	return files;
}

void UFOFileSystem::forEachDirectory (const std::string& basedir, const FileNameCallback& callback, std::size_t depth)
{
	GSList* list = GetListInternal(basedir, "", true, depth);

	for (GSList* i = list; i != 0; i = g_slist_next(i)) {
		const std::string directory = reinterpret_cast<const char*> (i->data);
		callback(directory);
	}

	ClearFileDirList(&list);
}

void UFOFileSystem::forEachFile (const std::string& basedir, const std::string& extension,
		const FileNameCallback& callback, std::size_t depth)
{
	GSList* list = GetListInternal(basedir, extension, false, depth);

	for (GSList* i = list; i != 0; i = g_slist_next(i)) {
		const std::string file = reinterpret_cast<const char*> (i->data);
		if (extension_equal(os::getExtension(file), extension)) {
			callback(file);
		}
	}

	ClearFileDirList(&list);
}

std::string UFOFileSystem::findFile (const std::string& relative)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (i->archive->containsFile(relative)) {
			return i->name;
		}
	}

	return "";
}

std::string UFOFileSystem::findRoot (const std::string& absolute)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (path_equal_n(absolute, i->name, i->name.length())) {
			return i->name;
		}
	}

	return "";
}

std::string UFOFileSystem::getRelative (const std::string& name)
{
	const std::string abolsoluteBasePath = findRoot(name);
	return path_make_relative(name, abolsoluteBasePath);
}

void UFOFileSystem::attach (ModuleObserver& observer)
{
	g_observers.attach(observer);
}

void UFOFileSystem::detach (ModuleObserver& observer)
{
	g_observers.detach(observer);
}

Archive* UFOFileSystem::getArchive (const std::string& archiveName)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (i->is_pakfile) {
			if (path_equal(i->name, archiveName)) {
				return i->archive;
			}
		}
	}
	return 0;
}

void UFOFileSystem::forEachArchive (const ArchiveNameCallback& callback)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (i->is_pakfile) {
			callback(i->name);
		}
	}
}
