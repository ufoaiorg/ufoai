#include "FileSystem.h"
#include "FileSystemModule.h"

#include "AutoPtr.h"
#include "string/string.h"
#include "os/path.h"
#include "generic/callback.h"

#include "directory/DirectoryArchive.h"
#include "FileVisitor.h"
#include "DirectoryVisitor.h"

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

FileSystem::FileSystem () :
	g_bUsePak(true)
{
}

inline ArchiveModule* GetArchiveTable (ArchiveModules& archiveModules, const std::string& ext)
{
	return archiveModules.findModule(string::toLower(ext));
}

void FileSystem::initPK3File (const std::string& filename)
{
	ArchiveModules& archiveModules = FileSystemAPI_getArchiveModules();
	ArchiveModule* table = GetArchiveTable(archiveModules, os::getExtension(filename));

	if (table != 0) {
		ArchiveEntry entry;
		entry.name = filename;

		entry.is_pakfile = true;
		entry.archive = table->openArchive(filename);
		g_archives.push_back(entry);
		g_message("  pk3 file: %s\n", filename.c_str());
	}
}

void FileSystem::initDirectory (const std::string& directory)
{
	ArchiveModules& archiveModules = FileSystemAPI_getArchiveModules();

	std::string path = DirectoryCleaned(directory);
	g_strDirs.push_back(path);

	{
		ArchiveEntry entry;
		entry.name = path;
		entry.archive = OpenDirArchive(path);
		entry.is_pakfile = false;
		g_archives.push_back(entry);
	}

	if (g_bUsePak) {
		GDir* dir = g_dir_open(path.c_str(), 0, 0);

		if (dir != 0) {
			g_message("vfs directory: %s\n", path.c_str());

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
				initPK3File(filename);
			}
			for (Archives::iterator i = archives.begin(); i != archives.end(); ++i) {
				std::string filename = path + *i;
				initPK3File(filename);
			}
		} else {
			g_message("vfs directory not found: '%s'\n", path.c_str());
		}
	}
}

void FileSystem::initialise ()
{
	g_message("filesystem initialised\n");
	g_observers.realise();
}

void FileSystem::shutdown ()
{
	g_observers.unrealise();
	g_message("filesystem shutdown\n");
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		delete i->archive;
	}
	g_archives.clear();
	g_strDirs.clear();
}

ArchiveFile* FileSystem::openFile (const std::string& filename)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveFile* file = (*i).archive->openFile(filename);
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

ArchiveTextFile* FileSystem::openTextFile (const std::string& filename)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveTextFile* file = i->archive->openTextFile(filename);
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

std::size_t FileSystem::loadFile (const std::string& filename, void **buffer)
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

void FileSystem::freeFile (void *p)
{
	free(p);
}

void FileSystem::forEachDirectory (const std::string& basedir, const FileNameCallback& callback, std::size_t depth)
{
	// Set of visited directories, to avoid name conflicts
	std::set<std::string> visitedDirs;

	// Wrap around the passed visitor
	DirectoryVisitor visitor2(callback, basedir, visitedDirs);

	// Visit each Archive, applying the DirectoryVisitor to each one (which in
	// turn calls the callback for each matching file.
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		i->archive->forEachFile(Archive::VisitorFunc(visitor2, Archive::eDirectories, depth), basedir);
	}
}

void FileSystem::forEachFile (const std::string& basedir, const std::string& extension,
		const FileNameCallback& callback, std::size_t depth)
{
	// Set of visited files, to avoid name conflicts
	std::set<std::string> visitedFiles;

	// Wrap around the passed visitor
	FileVisitor visitor2(callback, basedir, extension, visitedFiles);

	// Visit each Archive, applying the FileVisitor to each one (which in
	// turn calls the callback for each matching file.
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		i->archive->forEachFile(Archive::VisitorFunc(visitor2, Archive::eFiles, depth), basedir);
	}
}

std::string FileSystem::findFile (const std::string& relative)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (i->archive->containsFile(relative)) {
			return i->name;
		}
	}

	return "";
}

std::string FileSystem::findRoot (const std::string& absolute)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (path_equal_n(absolute, i->name, i->name.length())) {
			return i->name;
		}
	}

	return "";
}

std::string FileSystem::getRelative (const std::string& name)
{
	const std::string abolsoluteBasePath = findRoot(name);
	return path_make_relative(name, abolsoluteBasePath);
}

void FileSystem::attach (ModuleObserver& observer)
{
	g_observers.attach(observer);
}

void FileSystem::detach (ModuleObserver& observer)
{
	g_observers.detach(observer);
}

Archive* FileSystem::getArchive (const std::string& archiveName)
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

void FileSystem::forEachArchive (const ArchiveNameCallback& callback)
{
	for (ArchiveEntryList::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (i->is_pakfile) {
			callback(i->name);
		}
	}
}
