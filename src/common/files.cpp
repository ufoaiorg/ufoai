/**
 * @file
 * @brief All of UFO's data access is through a hierarchical file system, but the
 * contents of the file system can be transparently merged from several sources.
 * The "base directory" is the path to the directory holding the ufo binary and the game directory (base).
 * The base directory is only used during filesystem initialization.
 * The "game directory" is the first tree on the search path and directory that all generated
 * files (savegames, screenshots, config files) will be saved to.
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "common.h"
#include "qfiles.h"
#include "unzip.h"
#include "../ports/system.h"
#include "../shared/defines.h"
#include "../shared/parse.h"
#include <unistd.h>

/** counter for opened files - used to check against missing close calls */
static int fs_openedFiles;
static filelink_t* fs_links;
static searchpath_t* fs_searchpaths;
#define MODS_DIR "mods"

void FS_CreateOpenPipeFile (const char* filename, qFILE* f)
{
	if (fs_searchpaths == nullptr) {
		Sys_Error("Filesystem call made without initialization");
	}

	OBJZERO(*f);

	Q_strncpyz(f->name, filename, sizeof(f->name));
	Sys_Mkfifo(va("%s/%s", FS_Gamedir(), filename), f);

	if (f->f != nullptr) {
		Com_Printf("created pipe %s\n", filename);
		fs_openedFiles++;
	}
}

/**
 * @brief Called to find where to write a file (savegames, etc)
 * @note We will use the searchpath that isn't a pack and has highest priority
 */
const char* FS_Gamedir (void)
{
	searchpath_t* search;

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->write)
			return search->filename;
	}

	return nullptr;
}

/**
 * @brief Convert operating systems path separators to ufo virtual filesystem
 * separators (/)
 * @sa Sys_NormPath
 */
void FS_NormPath (char* path)
{
	Sys_NormPath(path);
}

/**
 * @brief Returns the size of a given file or -1 if no file is opened
 */
int FS_FileLength (qFILE*  f)
{
	if (f->f) {
		const int pos = ftell(f->f);
		int end;

		fseek(f->f, 0, SEEK_END);
		end = ftell(f->f);
		fseek(f->f, pos, SEEK_SET);

		return end;
	} else if (f->z) {
		unz_file_info info;
		if (unzGetCurrentFileInfo(f->z, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
			Sys_Error("Couldn't get size of %s", f->name);
		return info.uncompressed_size;
	}

	return -1;
}

/**
 * @brief Creates any directories needed to store the given filename
 * @sa Sys_Mkdir
 * @note Paths should already be normalized
 * @sa FS_NormPath
 */
void FS_CreatePath (const char* path)
{
	char pathCopy[MAX_OSPATH];

	Q_strncpyz(pathCopy, path, sizeof(pathCopy));

	for (char* ofs = pathCopy + 1; *ofs; ofs++) {
		/* create the directory */
		if (*ofs == '/') {
			*ofs = 0;
			Sys_Mkdir(pathCopy);
			*ofs = '/';
		}
	}
}

/**
 * @brief Closes a file handle
 * @sa FS_OpenFile
 */
void FS_CloseFile (qFILE*  f)
{
	if (f->f) {
		fclose(f->f);
		fs_openedFiles--;
	} else if (f->z) {
		unzCloseCurrentFile(f->z);
		fs_openedFiles--;
	} else {
		return;
	}
	assert(fs_openedFiles >= 0);

	f->f = nullptr;
	f->z = nullptr;
}

/**
 * @brief Finds and opens the file in the search path.
 * @param[in] filename
 * @param[out] file The file pointer
 * @param[in] mode read, write, append as an enum
 * @return the filesize or -1 in case of an error
 * @note Used for streaming data out of either a pak file or a separate file.
 */
int FS_OpenFile (const char* filename, qFILE* file, filemode_t mode)
{
	char netpath[MAX_OSPATH];

	file->z = file->f = nullptr;

	/* open for write or append in gamedir and return */
	if (mode == FILE_WRITE || mode == FILE_APPEND) {
		Com_sprintf(netpath, sizeof(netpath), "%s/%s", FS_Gamedir(), filename);
		FS_CreatePath(netpath);

		file->f = Sys_Fopen(netpath, (mode == FILE_WRITE ? "wb" : "ab"));
		if (file->f) {
			fs_openedFiles++;
			return 0;
		}

		return -1;
	}

	Q_strncpyz(file->name, filename, sizeof(file->name));

	/* check for links first */
	for (filelink_t* link = fs_links; link; link = link->next) {
		if (!strncmp(filename, link->from, link->fromlength)) {
			int length;
			Com_sprintf(netpath, sizeof(netpath), "%s%s", link->to, filename + link->fromlength);
			length = FS_OpenFile(netpath, file, mode);
			Q_strncpyz(file->name, filename, sizeof(file->name));
			if (length == -1)
				Com_Printf("linked file could not be opened: %s\n", netpath);
			return length;
		}
	}

	/* search through the path, one element at a time */
	for (searchpath_t* search = fs_searchpaths; search; search = search->next) {
		/* is the element a pak file? */
		if (search->pack) {
			/* look through all the pak file elements */
			pack_t* pak = search->pack;
			for (int i = 0; i < pak->numfiles; i++) {
				/* found it! */
				if (!Q_strcasecmp(pak->files[i].name, filename)) {
					/* open a new file on the pakfile */
					if (unzLocateFile(pak->handle.z, filename, 2) == UNZ_OK) {	/* found it! */
						if (unzOpenCurrentFile(pak->handle.z) == UNZ_OK) {
							unz_file_info info;
							if (unzGetCurrentFileInfo(pak->handle.z, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
								Sys_Error("Couldn't get size of %s in %s", filename, pak->filename);
							unzGetCurrentFileInfoPosition(pak->handle.z, &file->filepos);
							file->z = pak->handle.z;
							fs_openedFiles++;
							return info.uncompressed_size;
						}
					}
					return pak->files[i].filelen;
				}
			}
		} else {
			/* check a file in the directory tree */
			Com_sprintf(netpath, sizeof(netpath), "%s/%s", search->filename, filename);

			file->f = Sys_Fopen(netpath, "rb");
			if (!file->f)
				continue;

			fs_openedFiles++;
			return FS_FileLength(file);
		}
	}

	file->f = nullptr;
	file->z = nullptr;
	return -1;
}

#define PK3_SEEK_BUFFER_SIZE 65536
/**
 * @brief Sets the file position of the given file
 * @param[in] f The opened file handle
 * @param[in] origin fsOrigin_t
 * @param[in] offset The offset you want to do the
 * @sa FS_Read
 */
int FS_Seek (qFILE*  f, long offset, int origin)
{
	if (f->z) {
		byte buffer[PK3_SEEK_BUFFER_SIZE];
		int remainder = offset;

		if (offset < 0 || origin == FS_SEEK_END) {
			Sys_Error("Negative offsets and FS_SEEK_END not implemented "
					"for FS_Seek on pk3 file contents\n");
		}

		switch (origin) {
		case FS_SEEK_SET:
			unzSetCurrentFileInfoPosition(f->z, (unsigned long)f->filepos);
			unzOpenCurrentFile(f->z);
			/* fallthrough */
		case FS_SEEK_CUR:
			while (remainder > PK3_SEEK_BUFFER_SIZE) {
				FS_Read(buffer, PK3_SEEK_BUFFER_SIZE, f);
				remainder -= PK3_SEEK_BUFFER_SIZE;
			}
			FS_Read(buffer, remainder, f);
			return offset;

		default:
			Sys_Error("Bad origin in FS_Seek");
		}
	} else if (f->f) {
		int _origin;
		switch (origin) {
		case FS_SEEK_CUR:
			_origin = SEEK_CUR;
			break;
		case FS_SEEK_END:
			_origin = SEEK_END;
			break;
		case FS_SEEK_SET:
			_origin = SEEK_SET;
			break;
		default:
			Sys_Error("Bad origin in FS_Seek");
		}
		return fseek(f->f, offset, _origin);
	} else
		Sys_Error("FS_Seek: no file opened");
}

/**
 * @brief Just returns the filelength and -1 if the file wasn't found
 * @note Won't print any errors
 * @sa FS_FileExists
 */
int FS_CheckFile (const char* fmt, ...)
{
	va_list ap;
	char filename[MAX_QPATH];

	va_start(ap, fmt);
	Q_vsnprintf(filename, sizeof(filename), fmt, ap);
	va_end(ap);

	ScopedFile file;
	return FS_OpenFile(filename, &file, FILE_READ);
}

#define	MAX_READ	0x10000		/* read in blocks of 64k */
/**
 * @brief Read a file into a given buffer in memory.
 * @param[out] buffer Pointer to memory where file contents are written to.
 * @param[in] len The length of the supplied memory area.
 * @param[in] f The file which is to be read into the memory area.
 * @param[in] failOnEmptyRead idk Must have something to with certain readstates that can happen with removable media ?
 * @return The length of the file contents successfully read and written to
 * memory.
 * @note @c buffer is not null-terminated at the end of file reading
 * @note This function properly handles partial reads so check that the
 * returned length matches @c len.
 * @note Reads in blocks of 64k.
 * @sa FS_LoadFile
 * @sa FS_OpenFile
 */
int FS_Read2 (void* buffer, int len, qFILE* f, bool failOnEmptyRead)
{
	int read;
	byte* buf;
	int tries;

	buf = (byte*) buffer;

	if (f->z) {
		read = unzReadCurrentFile(f->z, buf, len);
		if (read == -1)
			Sys_Error("FS_Read (zipfile): -1 bytes read");

		return read;
	}

	int remaining = len;
	tries = 0;
	while (remaining) {
		int block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread(buf, 1, block, f->f);

		/* end of file reached */
		if (read != block && feof(f->f))
			return (len - remaining + read);

		if (read == 0) {
			/* we might have been trying to read from a CD */
			if (!tries)
				tries = 1;
			else if (failOnEmptyRead)
				Sys_Error("FS_Read: 0 bytes read");
			else
				return len - remaining;
		}

		if (read == -1)
			Sys_Error("FS_Read: -1 bytes read");

		/* do some progress bar thing here... */
		remaining -= read;
		buf += read;
	}
	return len;
}

int FS_Read (void* buffer, int len, qFILE* f)
{
	return FS_Read2(buffer, len, f, true);
}

/**
 * @brief Filenames are relative to the quake search path
 * @param[in] buffer a null buffer will just return the file length without loading
 * @param[in] path
 * @return a -1 length means that the file is not present
 * @sa FS_Read
 * @sa FS_OpenFile
 */
int FS_LoadFile (const char* path, byte** buffer)
{
	ScopedFile h;
	int len;

	/* look for it in the filesystem or pack files */
	len = FS_OpenFile(path, &h, FILE_READ);
	if (!h) {
		if (buffer)
			*buffer = nullptr;
		return -1;
	}

	if (!buffer) {
		return len;
	}

	byte* const buf = Mem_PoolAllocTypeN(byte, len + 1, com_fileSysPool);
	if (!buf)
		return -1;
	*buffer = buf;

	FS_Read(buf, len, &h);
	buf[len] = 0;

	return len;
}

void FS_FreeFile (void* buffer)
{
	_Mem_Free(buffer, "FS_FreeFile", 0);
}

/**
 * @brief Takes an explicit (not game tree related) path to a pak file.
 * Adding the files at the beginning of the list so they override previous pack files.
 * @param[in] packfile The pack filename
 * @note pk3 and zip are valid extensions
 */
static pack_t* FS_LoadPackFile (const char* packfile)
{
	const char* extension = Com_GetExtension(packfile);

	if (Q_streq(extension, "pk3") || Q_streq(extension, "zip")) {
		int i;
		unz_file_info file_info;
		unz_global_info gi;
		unzFile uf = unzOpen(packfile);
		unsigned int err = unzGetGlobalInfo(uf, &gi);
		char filenameInZip[MAX_QPATH];

		if (err != UNZ_OK) {
			Com_Printf("Could not load '%s'\n", packfile);
			return nullptr;
		}

		unzGoToFirstFile(uf);
		for (i = 0; i < gi.number_entry; i++) {
			err = unzGetCurrentFileInfo(uf, &file_info, filenameInZip, sizeof(filenameInZip), nullptr, 0, nullptr, 0);
			if (err != UNZ_OK) {
				break;
			}
			unzGoToNextFile(uf);
		}

		pack_t* const pack = Mem_PoolAllocType(pack_t, com_fileSysPool);
		Q_strncpyz(pack->filename, packfile, sizeof(pack->filename));
		pack->handle.z = uf;
		pack->handle.f = nullptr;
		pack->numfiles = gi.number_entry;
		unzGoToFirstFile(uf);

		/* Allocate space for array of packfile structures (filename, offset, length) */
		packfile_t* const newfiles = Mem_PoolAllocTypeN(packfile_t, i, com_fileSysPool);

		for (i = 0; i < gi.number_entry; i++) {
			err = unzGetCurrentFileInfo(uf, &file_info, filenameInZip, sizeof(filenameInZip), nullptr, 0, nullptr, 0);
			if (err != UNZ_OK)
				break;
			Q_strlwr(filenameInZip);

			unzGetCurrentFileInfoPosition(uf, &newfiles[i].filepos);
			Q_strncpyz(newfiles[i].name, filenameInZip, sizeof(newfiles[i].name));
			newfiles[i].filelen = file_info.compressed_size;
			unzGoToNextFile(uf);
		}
		pack->files = newfiles;

		/* Sort our list alphabetically - also rearrange the unsigned long values */
		qsort((void*)pack->files, i, sizeof(*newfiles), Q_StringSort);

		Com_Printf("Added packfile %s (%li files)\n", packfile, gi.number_entry);
		return pack;
	} else {
		/* Unrecognized file type! */
		Com_Printf("Pack file type %s unrecognized\n", extension);
		return nullptr;
	}
}

#define MAX_PACKFILES 1024

static char const* const pakFileExt[] = {
	"pk3", "zip", nullptr
};

/**
 * @brief Adds the directory to the head of the search path
 * @note No ending slash here
 * @param[in] dir The directory name relative to the game dir
 * @param[in] write Add this directory as writable (config files, save games)
 */
void FS_AddGameDirectory (const char* dir, bool write)
{
	int ndirs = 0, i;
	char pakfile_list[MAX_PACKFILES][MAX_OSPATH];
	int pakfile_count = 0;
	char pattern[MAX_OSPATH];

	for (searchpath_t* search = fs_searchpaths; search; search = search->next) {
		if (Q_streq(search->filename, dir))
			return;
		if (write && search->write) {
			Com_Printf("change writing directory to %s\n", dir);
			search->write = false;
		}
	}

	Com_Printf("Adding game dir: %s\n", dir);

	for (char const* const* extList = pakFileExt; *extList; ++extList) {
		Com_sprintf(pattern, sizeof(pattern), "%s/*.%s", dir, *extList);
		char** dirnames = nullptr;
		dirnames = FS_ListFiles(pattern, &ndirs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
		if (dirnames != nullptr) {
			for (i = 0; i < ndirs - 1; i++) {
				if (strrchr(dirnames[i], '/')) {
					Q_strncpyz(pakfile_list[pakfile_count], dirnames[i], sizeof(pakfile_list[pakfile_count]));
					pakfile_count++;
					if (pakfile_count >= MAX_PACKFILES) {
						Com_Printf("Warning: Max allowed pakfiles reached (%i) - skipping the rest\n", MAX_PACKFILES);
						break;
					}
				}
				Mem_Free(dirnames[i]);
			}
			Mem_Free(dirnames);
		}
	}

	/* Sort our list alphabetically */
	qsort((void*)pakfile_list, pakfile_count, MAX_OSPATH, Q_StringSort);

	for (i = 0; i < pakfile_count; i++) {
		pack_t* pak = FS_LoadPackFile(pakfile_list[i]);
		if (!pak)
			continue;

		searchpath_t* const search = Mem_PoolAllocType(searchpath_t, com_fileSysPool);
		search->pack = pak;
		search->next = fs_searchpaths;
		search->write = false;
		fs_searchpaths = search;
	}

	/* add the directory to the search path */
	searchpath_t* const search = Mem_PoolAllocType(searchpath_t, com_fileSysPool);
	Q_strncpyz(search->filename, dir, sizeof(search->filename));
	search->next = fs_searchpaths;
	search->write = write;
	fs_searchpaths = search;
}

/**
 * @brief Builds a qsorted filelist
 * @sa Sys_FindFirst
 * @sa Sys_FindNext
 * @sa Sys_FindClose
 * @note Don't forget to free the filelist array and the file itself
 */
char** FS_ListFiles (const char* findname, int* numfiles, unsigned musthave, unsigned canthave)
{
	int nfiles = 0;

	*numfiles = 0;

	char* s = Sys_FindFirst(findname, musthave, canthave);
	while (s) {
		if (s[strlen(s) - 1] != '.')
			nfiles++;
		s = Sys_FindNext(musthave, canthave);
	}
	Sys_FindClose();

	if (!nfiles)
		return nullptr;

	nfiles++; /* add space for a guard */
	*numfiles = nfiles;

	char** const list = Mem_PoolAllocTypeN(char*, nfiles, com_fileSysPool);
	char tempList[MAX_FILES][MAX_OSPATH];
	OBJZERO(tempList);

	s = Sys_FindFirst(findname, musthave, canthave);
	nfiles = 0;
	while (s) {
		if (s[strlen(s) - 1] != '.') {
			Q_strncpyz(tempList[nfiles], s, sizeof(tempList[nfiles]));
#ifdef _WIN32
			Q_strlwr(tempList[nfiles]);
#endif
			nfiles++;
			if (nfiles >= MAX_FILES)
				break;
		}
		s = Sys_FindNext(musthave, canthave);
	}
	Sys_FindClose();

	qsort(tempList, nfiles, MAX_OSPATH, Q_StringSort);
	for (int i = 0; i < nfiles; i++) {
		list[i] = Mem_PoolStrDup(tempList[i], com_fileSysPool, 0);
	}

	return list;
}

/**
 * @brief Allows enumerating all of the directories in the search path
 * @note ignore pk3 here
 */
const char* FS_NextPath (const char* prevpath)
{
	if (!prevpath)
		return FS_Gamedir();

	char* prev = nullptr;
	for (searchpath_t* s = fs_searchpaths; s; s = s->next) {
		if (s->pack)
			continue;
		if (prev && Q_streq(prevpath, prev))
			return s->filename;
		prev = s->filename;
	}

	return nullptr;
}

static bool FS_GetHomeDirectory (char* gdir, size_t length)
{
	const char* homedir = Sys_GetHomeDirectory();

	if (homedir) {
#ifdef _WIN32
		Com_sprintf(gdir, length, "%s/" UFO_VERSION, homedir);
#elif defined (__APPLE__) || defined (MACOSX)
		Com_sprintf(gdir, length, "%s/Documents/UFOAI-" UFO_VERSION, homedir);
#else
		Com_sprintf(gdir, length, "%s/.ufoai/" UFO_VERSION, homedir);
#endif
		return true;
	}
	Com_Printf("could not find the home directory\n");
	return false;
}

/**
 * @note e.g. *nix: Use ~/.ufoai/dir as gamedir
 * @param[in] dir The directory name relative to the game dir
 * @param[in] write Add this directory as writable (config files, save games)
 * @sa Sys_GetHomeDirectory
 */
void FS_AddHomeAsGameDirectory (const char* dir, bool write)
{
	char gdir[MAX_OSPATH];

	if (FS_GetHomeDirectory(gdir, sizeof(gdir))) {
		Q_strcat(gdir, sizeof(gdir), "/%s", dir);
		FS_CreatePath(va("%s/", gdir));
		FS_AddGameDirectory(gdir, write);
	}
}

/**
 * @brief Adds the directory to the head of the search path
 * @note No ending slash here
 */
int FS_GetModList (linkedList_t** mods)
{
	char gdir[MAX_OSPATH];
	const char* homedir;

	if (FS_GetHomeDirectory(gdir, sizeof(gdir))) {
		char const*  const append = "/" MODS_DIR;
		Q_strcat(gdir, sizeof(gdir), append);
		homedir = gdir;
	} else {
		homedir = nullptr;
	}

	char const*  searchpaths[] = {
#ifdef PKGDATADIR
			PKGDATADIR "/" MODS_DIR,
#endif
			"./" MODS_DIR,
			homedir,
			nullptr
	};

	LIST_AddString(mods, BASEDIRNAME);
	int numberMods = 1;
	/* it is likely that we have duplicate names now, which we will cleanup below */
	for (const char** path = searchpaths; *path; path++) {
		const char* pattern = *path;
		int ndirs = 0;
		char** dirnames = FS_ListFiles(va("%s/*", pattern), &ndirs, SFF_SUBDIR, SFF_HIDDEN | SFF_SYSTEM);
		if (dirnames != nullptr) {
			for (int i = 0; i < ndirs - 1; i++) {
				LIST_AddString(mods, dirnames[i] + (strlen(pattern) + 1));
				numberMods++;
				Mem_Free(dirnames[i]);
			}
			Mem_Free(dirnames);
		}
	}

	return numberMods;
}

#ifdef COMPILE_UFO

static void FS_Mod_f (void)
{
	if (Cmd_Argc() == 1) {
		linkedList_t* list = nullptr;
		FS_GetModList(&list);
		LIST_Foreach(list, const char, mod) {
			Com_Printf("mod: %s\n", mod);
		}
		LIST_Delete(&list);
	} else {
		FS_RestartFilesystem(Cmd_Argv(1));
	}
}
/**
 * @brief Adds the execution of the autoexec.cfg to the command buffer
 */
void FS_ExecAutoexec (void)
{
	/* search through all the paths for an autoexec.cfg file */
	for (searchpath_t* s = fs_searchpaths; s != nullptr; s = s->next) {
		char name[MAX_QPATH];
		snprintf(name, sizeof(name), "%s/autoexec.cfg", s->filename);

		if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM)) {
			Cbuf_AddText("exec autoexec.cfg\n");
			Sys_FindClose();
			break;
		}

		Sys_FindClose();
	}

	Cbuf_Execute();  /* execute it */
}

/**
 * @brief Creates a filelink_t
 */
static void FS_Link_f (void)
{
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <from> <to>\n", Cmd_Argv(0));
		return;
	}

	/* see if the link already exists */
	filelink_t** prev = &fs_links;
	for (filelink_t* l = fs_links; l; l = l->next) {
		if (Q_streq(l->from, Cmd_Argv(1))) {
			Mem_Free(l->to);
			if (!strlen(Cmd_Argv(2))) {	/* delete it */
				*prev = l->next;
				Mem_Free(l->from);
				Mem_Free(l);
				return;
			}
			l->to = Mem_PoolStrDup(Cmd_Argv(2), com_fileSysPool, 0);
			return;
		}
		prev = &l->next;
	}

	/* create a new link */
	filelink_t* const l = Mem_PoolAllocType(filelink_t, com_fileSysPool);
	l->next = fs_links;
	fs_links = l;
	l->from = Mem_PoolStrDup(Cmd_Argv(1), com_fileSysPool, 0);
	l->fromlength = strlen(l->from);
	l->to = Mem_PoolStrDup(Cmd_Argv(2), com_fileSysPool, 0);
}

/**
 * @brief Show the filesystem contents - also supports wildcarding
 * @sa FS_NextPath
 * @note No pk3 support
 */
static void FS_Dir_f (void)
{
	char const* wildcard = Cmd_Argc() != 1 ? Cmd_Argv(1) : "*.*";
	const char* path = nullptr;
	char findname[1024];
	int ndirs;

	while ((path = FS_NextPath(path)) != nullptr) {
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);
		FS_NormPath(findname);

		Com_Printf("Directory of %s\n", findname);
		Com_Printf("----\n");

		char** dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
		if (dirnames != nullptr) {
			for (int i = 0; i < ndirs - 1; i++) {
				char const* const slash = strrchr(dirnames[i], '/');
				Com_Printf("%s\n", slash ? slash + 1 : dirnames[i]);

				Mem_Free(dirnames[i]);
			}
			Mem_Free(dirnames);
		}
		Com_Printf("\n");
	}
}

static void FS_List_f (void)
{
	char const* wildcard = Cmd_Argc() == 2 ? Cmd_Argv(1) : "*.*";
	const char* filename;

	Com_Printf("Show files for '%s'\n", wildcard);
	FS_BuildFileList(wildcard);
	while ((filename = FS_NextFileFromFileList(wildcard)) != nullptr)
		Com_Printf("%s\n", filename);
	FS_NextFileFromFileList(nullptr);
}

/**
 * @brief Print all searchpaths
 */
static void FS_Info_f (void)
{
	Com_Printf("Filesystem information\n");
	Com_Printf("...write dir: '%s'\n", FS_Gamedir());

	for (searchpath_t* search = fs_searchpaths; search; search = search->next) {
		if (search->pack == nullptr)
			Com_Printf("...path: '%s'\n", search->filename);
		else
			Com_Printf("...pakfile: '%s' (%i files)\n", search->pack->filename, search->pack->numfiles);
	}

	for (filelink_t* l = fs_links; l; l = l->next)
		Com_Printf("...link: %s : %s\n", l->from, l->to);
}

static void FS_RestartFilesystem_f (void)
{
	if (Cmd_Argc() == 2)
		FS_RestartFilesystem(Cmd_Argv(1));
	else
		FS_RestartFilesystem(nullptr);
}

/**
 * @brief filesystem console commands
 * @sa FS_InitFilesystem
 * @sa FS_RestartFilesystem
 */
static const cmdList_t fs_commands[] = {
	{"fs_restart", FS_RestartFilesystem_f, "Reloads the file subsystem"},
	{"link", FS_Link_f, "Create file links"},
	{"dir", FS_Dir_f, "Show the filesystem contents per game dir - also supports wildcarding"},
	{"ls", FS_List_f, "Show the filesystem contents"},
	{"fs_info", FS_Info_f, "Show information about the virtual filesystem"},
	{"fs_mod", FS_Mod_f, "Show or activate mods"},

	{nullptr, nullptr, nullptr}
};

static void FS_RemoveCommands (void)
{
	Cmd_TableCheck();
	Cmd_TableRemoveList(fs_commands);
}

static void FS_InitCommandsAndCvars (void)
{
	Cmd_TableAddList(fs_commands);
}
#endif

/**
 * @sa FS_Shutdown
 * @sa FS_RestartFilesystem
 */
void FS_InitFilesystem (bool writeToHomeDir)
{
	Com_Printf("\n---- filesystem initialization -----\n");

#ifdef PKGDATADIR
	/* add the system search path */
	FS_AddGameDirectory(PKGDATADIR "/" BASEDIRNAME, false);
#endif

	FS_AddGameDirectory("./" BASEDIRNAME, !writeToHomeDir);
	FS_AddHomeAsGameDirectory(BASEDIRNAME, writeToHomeDir);
#ifdef COMPILE_UFO
	const char* fsGameDir = Cvar_GetString("fs_gamedir");
	if (Q_strvalid(fsGameDir)) {
		char path[MAX_QPATH];
		Com_sprintf(path, sizeof(path), "./%s", fsGameDir);
		if (!FS_FileExists("%s", path)) {
			Com_sprintf(path, sizeof(path), "./" MODS_DIR "/%s", fsGameDir);
		}
		FS_AddGameDirectory(path, !writeToHomeDir);
		FS_AddHomeAsGameDirectory(fsGameDir, writeToHomeDir);
	}
#endif

#ifdef COMPILE_UFO
	FS_InitCommandsAndCvars();
	Cbuf_AddText("exec filesystem.cfg\n");
#endif

	Com_Printf("using %s for writing\n", FS_Gamedir());
}

/** @todo This block list code is broken in terms of filename order
 * To see the bug reduce the FL_BLOCKSIZE to 1024 and verify the order of the
 * filenames FS_NextScriptHeader gives you - you will see that the last files
 * will be in reversed order
 */

typedef struct listBlock_s {
	char path[MAX_QPATH];
	linkedList_t* files;
	struct listBlock_s* next;
} listBlock_t;

static listBlock_t* fs_blocklist = nullptr;

/**
 * @brief Add one name to the filelist
 * @note also checks for duplicates
 * @sa FS_BuildFileList
 */
static void _AddToListBlock (linkedList_t** fl, const char* name, bool stripPath)
{
	const char* f;

	/* strip path */
	if (stripPath)
		f = Com_SkipPath(name);
	else
		f = name;

	if (LIST_ContainsString(*fl, f))
		return;

	/* add the new file */
	LIST_AddStringSorted(fl, f);
}

/**
 * @brief Build a filelist
 * @param[in] fileList e.g. *.cfg to get all config files in the gamedir/ dir
 */
int FS_BuildFileList (const char* fileList)
{
	char files[MAX_QPATH];

	/* bring it into normal form */
	Q_strncpyz(files, fileList, sizeof(files));
	FS_NormPath(files);

	/* check the blocklist for older searches
	 * and do a new one after deleting them */
	for (listBlock_t** anchor = &fs_blocklist; *anchor;) {
		listBlock_t* const block = *anchor;
		if (Q_streq(block->path, files)) {
			*anchor = block->next;

			LIST_Delete(&block->files);
			Mem_Free(block);
		} else {
			anchor = &block->next;
		}
	}

	/* allocate a new block and link it into the list */
	listBlock_t* const block = Mem_PoolAllocType(listBlock_t, com_fileSysPool);
	block->next = fs_blocklist;
	fs_blocklist = block;

	/* store the search string */
	Q_strncpyz(block->path, files, sizeof(block->path));

	/* search for the files */
	LIST_Delete(&block->files);

	/* search through the path, one element at a time */
	char findname[1024];
	for (searchpath_t* search = fs_searchpaths; search; search = search->next) {
		/* is the element a pak file? */
		if (search->pack) {
			const char* ext = strrchr(files, '.');
			const pack_t* pak = search->pack;
			size_t l = strlen(files);
			if (!ext)
				break;
			Q_strncpyz(findname, files, sizeof(findname));
			FS_NormPath(findname);
			l -= (strlen(ext) + 1);
			findname[l] = '\0';

			/* look through all the pak file elements */
			for (int i = 0; i < pak->numfiles; i++) {
				/* found it! */
				const char* fileNameEntry = pak->files[i].name;
				bool matchAlsoInSubDirs = (findname[0] == '*' || !strncmp(fileNameEntry, findname, l))
						 && (ext[0] == '*' || strstr(fileNameEntry, ext));
				if (matchAlsoInSubDirs) {
					bool add = false;
					if (strstr(findname, "**"))
						add = true;
					else {
						char pathName[MAX_QPATH];
						char pathNameEntry[MAX_QPATH];
						Com_FilePath(findname, pathName, sizeof(pathName));
						Com_FilePath(fileNameEntry, pathNameEntry, sizeof(pathNameEntry));
						if (Q_streq(pathNameEntry, pathName))
							add = true;
					}

					if (add)
						_AddToListBlock(&block->files, pak->files[i].name, true);
				}
			}
		} else if (strstr(files, "**")) {
			linkedList_t* list = nullptr;
			const char* wildcard = strstr(files, "**");
			const size_t l = strlen(files) - strlen(wildcard);

			Q_strncpyz(findname, files, sizeof(findname));
			FS_NormPath(findname);
			findname[l] = '\0';
			if (l > 0 && findname[l - 1] == '/')
				findname[l - 1] = '\0';

			Sys_ListFilteredFiles(search->filename, findname, &findname[l + 1], &list);

			LIST_Foreach(list, const char, name) {
				_AddToListBlock(&block->files, name, false);
			}

			LIST_Delete(&list);
		} else {
			int nfiles = 0;
			char** filenames;

			Com_sprintf(findname, sizeof(findname), "%s/%s", search->filename, files);
			FS_NormPath(findname);

			filenames = FS_ListFiles(findname, &nfiles, 0, SFF_HIDDEN | SFF_SYSTEM);
			if (filenames != nullptr) {
				for (int i = 0; i < nfiles - 1; i++) {
					_AddToListBlock(&block->files, filenames[i], true);
					Mem_Free(filenames[i]);
				}
				Mem_Free(filenames);
			}
		}
	}

	return LIST_Count(block->files);
}

/**
 * @brief Returns the next file that is found in the virtual filesystem identified by the given file pattern
 * @param[in] files The file pattern to search for. This can e.g. be "*.ogg" or "**.ufo" to also include
 * subdirectories.
 * @return The next found filename or @c NULL if the end of the list was reached.
 * @note Keep in mind that the list is cached and also the position in the list is not reset until you explicitly
 * want this by calling this function with a @c NULL parameter for the pattern.
 * @note If you have to rebuild a file list, use @c FS_BuildFileList manually. Following calls will then use the new
 * file list.
 */
const char* FS_NextFileFromFileList (const char* files)
{
	static linkedList_t* listEntry = nullptr;
	static listBlock_t* _block = nullptr;
	listBlock_t* block;
	const char* file = nullptr;

	/* restart the list? */
	if (files == nullptr) {
		_block = nullptr;
		return nullptr;
	}

	for (block = fs_blocklist; block; block = block->next) {
		if (!strncmp(files, block->path, MAX_QPATH))
			break;
	}

	if (!block) {
		FS_BuildFileList(files);
		for (block = fs_blocklist; block; block = block->next) {
			if (!strncmp(files, block->path, MAX_QPATH))
				break;
		}
		if (!block) {
			/* still no filelist */
			Com_Printf("FS_NextFileFromFileList: Could not create filelist for %s\n", files);
			return nullptr;
		}
	}

	/* everytime we switch between different blocks we get the
	 * first file again when we switch back */
	if (_block != block) {
		_block = block;
		listEntry = block->files;
	}

	if (listEntry) {
		file = (const char*)listEntry->data;
		listEntry = listEntry->next;
	}

	/* finished */
	return file;
}

/**
 * @brief Returns the buffer of a file
 * @param[in] files If nullptr, reset the filelist
 * If not nullptr it may be something like *.cfg to get a list
 * of all config files in base/. Calling @c FS_GetFileData("*.cfg");
 * until it returns @c nullptr is sufficient to get one buffer after another.
 * @note You don't have to free the file buffer on the calling side.
 * This is done in this function, too
 */
const char* FS_GetFileData (const char* files)
{
	listBlock_t* block;
	static linkedList_t* fileList = nullptr;
	static byte* buffer = nullptr;

	/* free the old file */
	if (buffer) {
		FS_FreeFile(buffer);
		buffer = nullptr;
	}

	if (!files) {
		fileList = nullptr;
		return nullptr;
	}

	for (block = fs_blocklist; block; block = block->next) {
		if (Q_streq(files, block->path))
			break;
	}

	if (!block) {
		/* didn't find any valid file list */
		fileList = nullptr;
		FS_BuildFileList(files);
		for (block = fs_blocklist; block; block = block->next) {
			if (Q_streq(files, block->path))
				break;
		}
		if (!block) {
			/* still no filelist */
			Com_Printf("FS_GetFileData: Could not create filelist for %s\n", files);
			return nullptr;
		}
	}

	if (!fileList)
		/* start the list */
		fileList = block->files;
	else
		/* search a new file */
		fileList = fileList->next;

	if (fileList) {
		char filename[MAX_QPATH];

		/* load a new file */
		Q_strncpyz(filename, block->path, sizeof(filename));
		strcpy(strrchr(filename, '/') + 1, (const char*)fileList->data);

		FS_LoadFile(filename, &buffer);
		return (const char*)buffer;
	}

	/* finished */
	return nullptr;
}

char* FS_NextScriptHeader (const char* files, const char** name, const char** text)
{
	static char lastList[MAX_QPATH];
	static listBlock_t* lBlock;
	static linkedList_t* lFile;
	static byte* lBuffer;

	static char headerType[MAX_VAR];
	static char headerName[512];
	listBlock_t* block;
	const char* token;

	if (!text) {
		*lastList = 0;

		/* free the old file */
		if (lBuffer) {
			FS_FreeFile(lBuffer);
			lBuffer = nullptr;
		}

		return nullptr;
	}

	if (!Q_streq(files, lastList)) {
		/* search for file lists */
		Q_strncpyz(lastList, files, sizeof(lastList));

		for (block = fs_blocklist; block; block = block->next) {
			if (Q_streq(files, block->path))
				break;
		}

		if (!block)
			/* didn't find any valid file list */
			return nullptr;

		lBlock = block;
		lFile = block->files;
	}

	while (lBlock) {
		if (lBuffer) {
			/* continue reading the current file */
			if (*text) {
				token = Com_Parse(text);
				if (*token == '{') {
					Com_SkipBlock(text);
					continue;
				}

				Q_strncpyz(headerType, token, sizeof(headerType));
				if (*text) {
					token = Com_Parse(text);
					Q_strncpyz(headerName, token, sizeof(headerName));
					*name = headerName;
					return headerType;
				}
			}

			/* search a new file */
			lFile = lFile->next;

			while (!lFile && lBlock) {
				/* it was the last file in the block, continue to next block */
				for (lBlock = lBlock->next; lBlock; lBlock = lBlock->next) {
					if (Q_streq(files, lBlock->path)) {
						lFile = lBlock->files;
						break;
					}
				}
			}
		}

		if (lFile) {
			char filename[MAX_QPATH];

			/* free the old file */
			if (lBuffer) {
				FS_FreeFile(lBuffer);
				lBuffer = nullptr;
			}

			/* load a new file */
			Q_strncpyz(filename, lBlock->path, sizeof(filename));
			strcpy(strrchr(filename, '/') + 1, (const char*)lFile->data);

			FS_LoadFile(filename, &lBuffer);
			/* skip a file that couldn't get loaded */
			if (!lBuffer) {
				lFile = lFile->next;
				continue;
			}
			*text = (char*)lBuffer;
		} else if (!lBuffer)
			break;
	}

	/* free the old file */
	if (lBuffer) {
		FS_FreeFile(lBuffer);
		lBuffer = nullptr;
	}

	/* finished */
	return nullptr;
}

/* global vars for maplisting */
char* fs_maps[MAX_MAPS];
int fs_numInstalledMaps = -1;
static bool fs_mapsInstalledInit = false;

/**
 * @sa Com_MapDefSort
 */
static int FS_MapDefSort (const void* map1, const void* map2)
{
	const char* mapStr1 = *(const char* const*)map1;
	const char* mapStr2 = *(const char* const*)map2;

	/* skip special map chars for rma and base attack */
	if (mapStr1[0] == '+')
		mapStr1++;
	if (mapStr2[0] == '+')
		mapStr2++;

	return Q_StringSort(mapStr1, mapStr2);
}

/**
 * @brief Checks for valid BSP-file
 *
 * @param[in] filename BSP-file to check
 *
 * @return 0 if valid
 * @return 1 could not open file
 * @return 2 if magic number is bad
 * @return 3 if version of bsp-file is bad
 */
static int CheckBSPFile (const char* filename)
{
	/* load the file */
	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "maps/%s.bsp", filename);

	ScopedFile file;
	FS_OpenFile(name, &file, FILE_READ);
	if (!file)
		return 1;

	int header[2];
	FS_Read(header, sizeof(header), &file);

	for (int i = 0; i < 2; i++)
		header[i] = LittleLong(header[i]);

	if (header[0] != IDBSPHEADER)
		return 2;
	if (header[1] != BSPVERSION)
		return 3;

	/* valid BSP-File */
	return 0;
}

/**
 * @brief File the fs_maps array with valid maps
 * @param[in] reset If true the directory is scanned every time for new maps (useful for dedicated servers).
 * If false we only use the maps array (for clients e.g.)
 */
void FS_GetMaps (bool reset)
{
	char filename[MAX_QPATH];
	int i;
	const char* baseMapName = nullptr;
	char** dirnames;
	int ndirs;

	/* force a reread */
	if (!reset && fs_mapsInstalledInit)
		return;
	else if (fs_mapsInstalledInit) {
		for (i = 0; i <= fs_numInstalledMaps; i++)
			Mem_Free(fs_maps[i]);
	}

	fs_numInstalledMaps = -1;

	/* search through the path, one element at a time */
	char findname[MAX_OSPATH];
	for (searchpath_t* search = fs_searchpaths; search; search = search->next) {
		/* is the element a pak file? */
		if (search->pack) {
			/* look through all the pak file elements */
			pack_t* pak = search->pack;
			for (i = 0; i < pak->numfiles; i++) {
				/* found it! */
				baseMapName = strchr(pak->files[i].name, '/');
				if (baseMapName) {
					/** @todo paths are normalized here? */
					baseMapName = strchr(baseMapName + 1, '/');
					/* ugly hack - only show the maps in base/maps - not in base/maps/b and so on */
					if (baseMapName)
						continue;
				} else
					continue;

				if (strstr(pak->files[i].name, ".bsp") || strstr(pak->files[i].name, ".ump") ) {
					if (fs_numInstalledMaps + 1 >= MAX_MAPS) {
						Com_Printf("FS_GetMaps: Max maps limit hit\n");
						break;
					}
					fs_maps[fs_numInstalledMaps + 1] = Mem_PoolAllocTypeN(char, MAX_QPATH, com_fileSysPool);
					if (fs_maps[fs_numInstalledMaps + 1] == nullptr) {
						Com_Printf("Could not allocate memory in FS_GetMaps\n");
						continue;
					}
					Q_strncpyz(findname, pak->files[i].name, sizeof(findname));
					FS_NormPath(findname);
					baseMapName = Com_SkipPath(findname);
					Com_StripExtension(baseMapName, filename, sizeof(filename));
					fs_numInstalledMaps++;
					if (strstr(findname, ".ump"))
						Com_sprintf(fs_maps[fs_numInstalledMaps], MAX_QPATH, "+%s", filename);
					else
						Q_strncpyz(fs_maps[fs_numInstalledMaps], filename, MAX_QPATH);
				}
			}
		} else {
			Com_sprintf(findname, sizeof(findname), "%s/maps/*.bsp", search->filename);
			FS_NormPath(findname);

			dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM);
			if (dirnames != nullptr) {
				for (i = 0; i < ndirs - 1; i++) {
					baseMapName = Com_SkipPath(dirnames[i]);
					Com_StripExtension(baseMapName, filename, sizeof(filename));
					int status = CheckBSPFile(filename);
					if (!status) {
						if (fs_numInstalledMaps + 1 >= MAX_MAPS) {
							Com_Printf("FS_GetMaps: Max maps limit hit\n");
							break;
						}
						fs_maps[fs_numInstalledMaps + 1] = Mem_PoolAllocTypeN(char, MAX_QPATH, com_fileSysPool);
						if (fs_maps[fs_numInstalledMaps + 1] == nullptr) {
							Com_Printf("Could not allocate memory in FS_GetMaps\n");
							Mem_Free(dirnames[i]);
							continue;
						}
						fs_numInstalledMaps++;
						Q_strncpyz(fs_maps[fs_numInstalledMaps], filename, MAX_QPATH);
					} else
						Com_Printf("invalid mapstatus: %i (%s)\n", status, dirnames[i]);
					Mem_Free(dirnames[i]);
				}
				Mem_Free(dirnames);
			}
			/* +RMA to maplisting */
			Com_sprintf(findname, sizeof(findname), "%s/maps/*.ump", search->filename);
			FS_NormPath(findname);

			dirnames = FS_ListFiles(findname, &ndirs, 0, SFF_HIDDEN | SFF_SYSTEM);
			if (dirnames != nullptr) {
				for (i = 0; i < ndirs - 1; i++) {
					baseMapName = Com_SkipPath(dirnames[i]);
					Com_StripExtension(baseMapName, filename, sizeof(filename));
					if (fs_numInstalledMaps + 1 >= MAX_MAPS) {
						Com_Printf("FS_GetMaps: Max maps limit hit\n");
						break;
					}
					fs_maps[fs_numInstalledMaps + 1] = Mem_PoolAllocTypeN(char, MAX_QPATH, com_fileSysPool);
					if (fs_maps[fs_numInstalledMaps + 1] == nullptr) {
						Com_Printf("Could not allocate memory in FS_GetMaps\n");
						Mem_Free(dirnames[i]);
						continue;
					}
					fs_numInstalledMaps++;
					Com_sprintf(fs_maps[fs_numInstalledMaps], MAX_QPATH, "+%s", filename);
					Mem_Free(dirnames[i]);
				}
				Mem_Free(dirnames);
			}
		}
	}

	fs_mapsInstalledInit = true;

	qsort(fs_maps, fs_numInstalledMaps + 1, sizeof(char*), FS_MapDefSort);
}

/**
 * @brief Can print chunks for 1024 chars into a file.
 * @note The file must already be opened and may not be a zip file handle
 */
int FS_Printf (qFILE* f, const char* msg, ...)
{
	va_list ap;
	int len;
	char buf[1024];

	va_start(ap, msg);
	Q_vsnprintf(buf, sizeof(buf), msg, ap);
	len = fprintf(f->f, "%s", buf);
	va_end(ap);

	return len;
}

/**
 * @brief Properly handles partial writes
 */
int FS_Write (const void* buffer, int len, qFILE*  f)
{
	if (!f->f)
		return 0;

	const byte* buf = (const byte*) buffer;

	int remaining = len;
	int tries = 0;
	while (remaining) {
		int block = remaining;
		int written = fwrite(buf, 1, block, f->f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Printf("FS_Write: 0 bytes written\n");
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf("FS_Write: -1 bytes written\n");
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	return len;
}


int FS_WriteFile (const void* buffer, size_t len, const char* filename)
{
	ScopedFile f;
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f)
		return 0;

	const int c = FS_Write(buffer, len, &f);
	const int lencheck = FS_FileLength(&f);

	/* if file write failed (file is incomplete) then delete it */
	if (c != len || lencheck != len) {
		Com_Printf("FS_WriteFile: failed to finish writing '%s'\n", filename);
		if (remove(va("%s/%s", FS_Gamedir(), filename)))
			Com_Printf("FS_WriteFile: could not remove file: %s\n", filename);
		return 0;
	}

	return c;
}

/**
 * @brief Return current working dir
 */
const char* FS_GetCwd (void)
{
	static char buf[MAX_OSPATH];
	Q_strncpyz(buf, Sys_Cwd(), sizeof(buf));
	FS_NormPath(buf);
	return buf;
}

/**
 * @brief Checks whether a file exists (not in virtual filesystem)
 * @sa FS_CheckFile
 * @param[in] filename Full filesystem path to the file
 */
bool FS_FileExists (const char* filename, ...)
{
	char path[MAX_OSPATH];
	va_list ap;

	va_start(ap, filename);
	Q_vsnprintf(path, sizeof(path), filename, ap);
	va_end(ap);

#ifdef _WIN32
	return (_access(path, 00) == 0);
#else
	return (access(path, R_OK) == 0);
#endif
}

/**
 * @brief Cleanup function
 * @sa FS_InitFilesystem
 * @sa FS_RestartFilesystem
 */
void FS_Shutdown (void)
{
	searchpath_t* p, *next;

	if (fs_openedFiles != 0) {
		Com_Printf("There are still %i opened files\n", fs_openedFiles);
	}

	/* free everything */
	for (p = fs_searchpaths; p; p = next) {
		next = p->next;

		if (p->pack) {
			unzClose(p->pack->handle.z);
			Mem_Free(p->pack->files);
			Mem_Free(p->pack);
		}
		Mem_Free(p);
	}

	/* any FS_ calls will now be an error until reinitialized */
	fs_searchpaths = nullptr;
	fs_links = nullptr;
	fs_mapsInstalledInit = false;
	fs_numInstalledMaps = -1;
	fs_blocklist = nullptr;

#ifdef COMPILE_UFO
	FS_RemoveCommands();
#endif

	Mem_FreePool(com_fileSysPool);
}

/**
 * @brief Restart the filesystem (reload all pk3 files)
 * @note Call this after you finished a download
 * @sa FS_Shutdown
 * @sa FS_InitFilesystem
 */
void FS_RestartFilesystem (const char* gamedir)
{
	if (gamedir != nullptr)
		Com_Printf("restarting with gamedir set to %s\n", gamedir);
	throw comRestart_t(gamedir);
}

/**
 * @brief Copy a fully specified file from one place to another
 * @todo Allow copy of pk3 file content
 */
void FS_CopyFile (const char* fromOSPath, const char* toOSPath)
{
	FILE* f;
	int len;

	if (!fs_searchpaths)
		Sys_Error("Filesystem call made without initialization");

	Com_Printf("FS_CopyFile: copy %s to %s\n", fromOSPath, toOSPath);

	f = Sys_Fopen(fromOSPath, "rb");
	if (!f)
		return;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	byte* const buf = Mem_PoolAllocTypeN(byte, len, com_fileSysPool);
	if (fread(buf, 1, len, f) != len)
		Sys_Error("Short read in FS_CopyFile");
	fclose(f);

	FS_CreatePath(toOSPath);

	f = Sys_Fopen(toOSPath, "wb");
	if (!f) {
		Mem_Free(buf);
		return;
	}

	if (fwrite(buf, 1, len, f) != len)
		Sys_Error("Short write in FS_CopyFile");

	fclose(f);
	Mem_Free(buf);
}

/**
 * @sa FS_CopyFile
 */
void FS_RemoveFile (const char* osPath)
{
	if (!fs_searchpaths)
		Sys_Error("Filesystem call made without initialization");

	Com_Printf("FS_RemoveFile: remove %s\n", osPath);
	remove(osPath);
}

/**
 * @brief Renames a file
 * @sa FS_RemoveFile
 * @sa FS_CopyFile
 * @param[in] from The source filename
 * @param[in] to The filename we want after the rename
 * @param[in] relative If relative is true we have to add the FS_Gamedir path for writing
 */
bool FS_RenameFile (const char* from, const char* to, bool relative)
{
	char from_buf[MAX_OSPATH];
	char to_buf[MAX_OSPATH];

	if (!fs_searchpaths)
		Sys_Error("Filesystem call made without initialization");

	if (relative) {
		Com_sprintf(from_buf, sizeof(from_buf), "%s/%s", FS_Gamedir(), from);
		Com_sprintf(to_buf, sizeof(to_buf), "%s/%s", FS_Gamedir(), to);
		from = from_buf;
		to = to_buf;
	}

	return rename(from, to) == 0;
}
