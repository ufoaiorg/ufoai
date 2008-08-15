/**
 * @file cmdlib.c
 * @brief
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

#include "shared.h"
#include "cmdlib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "../../../shared/defines.h"
#include "../../../shared/typedefs.h"

static pack_t *pak;

#include <zlib.h>

#ifdef _WIN32
# include <direct.h>
# include <io.h>
# include <windows.h>
#else
# include <unistd.h>
# include <dirent.h>
#endif

/**
 * @brief For abnormal program terminations in windowed apps
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[2048];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);
	/* removed #ifdef _WIN32 which showed modal dialog. console output is much preferred. blondandy. */
	Com_Printf("\n************ ERROR ************\n");
	Com_Printf("%s\n", text);

	exit(1);
}

/**
 * @brief Takes an explicit (not game tree related) path to a pak file.
 * Adding the files at the beginning of the list so they override previous pack files.
 */
static pack_t *FS_LoadPackFile (const char *packfile)
{
	unsigned int i, len, err;
	packfile_t *newfiles;
	pack_t *pack;
	unz_file_info file_info;
	unzFile uf;
	unz_global_info gi;
	char filename_inzip[MAX_QPATH];

	char const* const ext = strrchr(packfile, '.');
	if (!ext || (
	      Q_strcasecmp(ext, ".pk3") != 0 &&
	      Q_strcasecmp(ext, ".zip") != 0
	    )) {
		/* Unrecognized file type! */
		Com_Printf("Pack file type %s unrecognized\n", ext);
		return NULL;
	}

	uf = unzOpen(packfile);
	err = unzGetGlobalInfo(uf, &gi);

	if (err != UNZ_OK) {
		Sys_FPrintf(SYS_VRB, "Could not load '%s' into zlib\n", packfile);
		return NULL;
	}

	len = 0;
	unzGoToFirstFile(uf);
	for (i = 0; i < gi.number_entry; i++) {
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK) {
			break;
		}
		len += strlen(filename_inzip) + 1;
		unzGoToNextFile(uf);
	}

	pack = malloc(sizeof(pack_t));
	strncpy(pack->filename, packfile, sizeof(pack->filename) - 1);
	pack->handle.z = uf;
	pack->handle.f = NULL;
	pack->numfiles = gi.number_entry;
	unzGoToFirstFile(uf);

	/* Allocate space for array of packfile structures (filename, offset, length) */
	newfiles = malloc(i * sizeof(packfile_t));

	for (i = 0; i < gi.number_entry; i++) {
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK)
			break;
		Q_strlwr(filename_inzip);

		unzGetCurrentFileInfoPosition(uf, &newfiles[i].filepos);
		strncpy(newfiles[i].name, filename_inzip, sizeof(newfiles[i].name) - 1);
		newfiles[i].filelen = file_info.compressed_size;
		unzGoToNextFile(uf);
	}
	pack->files = newfiles;

	Com_Printf("Added packfile %s (%li files)\n", packfile, gi.number_entry);
	return pack;
}

/**
 * @brief gamedir will hold the game directory (base, etc)
 */
static char gamedir[MAX_OSPATH];

/**
 * @brief Get the current working dir and ensures that there is a trailing slash
 */
static inline void FS_getwd (char *out, size_t size)
{
#ifdef _WIN32
	_getcwd(out, size);
#else
	if (getcwd(out, size) == NULL)
		Com_Printf("Warning, getcwd failed\n");
#endif
	strcat(out, "/");
}

/**
 * @sa FS_Init
 */
const char* FS_GameDir (void)
{
	if (gamedir[0] == '\0') {
		FS_getwd(gamedir, sizeof(gamedir));
		strncat(gamedir, BASEDIRNAME"/", sizeof(gamedir));
		Com_Printf("gamedir: %s\n", gamedir);
	}
	return gamedir;
}

/**
 * @brief
 * @param[in] path The path to the map filename - relative or absolute
 */
void FS_Init (const char *path)
{
	char ufoPath[MAX_OSPATH];
	const char *fullPath = COM_ExpandRelativePath(path);

	COM_FilePath(fullPath, ufoPath);

	FS_GameDir();

	pak = FS_LoadPackFile(va("%s0pics.pk3", gamedir));
	if (!pak)
		Com_Printf("Could not load image pk3, searching in directories for images instead\n");
}

/**
 * @brief
 */
char *COM_ExpandRelativePath (const char *path)
{
	static char full[MAX_OSPATH];

	if (path[0] != '/' && path[0] != '\\' && path[1] != ':') {
		FS_getwd(full, sizeof(full));
		Q_strcat(full, path, sizeof(full));
	} else
		Q_strncpyz(full, path, sizeof(full));

	return full;
}

/*
=============================================================================
MISC FUNCTIONS
=============================================================================
*/

/**
 * @brief
 */
static int FS_filelength (qFILE *f)
{
	int pos, end = 0;

	if (f->f) {
		pos = ftell(f->f);
		fseek(f->f, 0, SEEK_END);
		end = ftell (f->f);
		fseek(f->f, pos, SEEK_SET);
	} else if (f->z) {
		return f->size;
	}

	return end;
}

/**
 * @brief
 */
void SafeOpenWrite (const char *filename, qFILE* f)
{
	assert(!f->f);
	assert(!f->z);
	f->f = fopen(filename, "wb");
	if (!f->f)
		Sys_Error("Error opening %s for writing: %s", filename, strerror(errno));
}

/**
 * @brief
 */
static qFILE *SafeOpenRead (const char *filename, qFILE *f)
{
	char path[MAX_OSPATH];
	size_t len;

	memset(f, 0, sizeof(*f));

	f->f = fopen(filename, "rb");
	if (!f->f && pak) {
		len = strlen(gamedir);
		strncpy(path, filename, sizeof(path));
		path[len - 1] = '\0';
		filename = &path[len];
		if (unzLocateFile(pak->handle.z, filename, 2) == UNZ_OK) {	/* found it! */
			if (unzOpenCurrentFile(pak->handle.z) == UNZ_OK) {
				unz_file_info info;
				Com_Printf("PackFile: %s : %s\n", pak->filename, filename);
				if (unzGetCurrentFileInfo(pak->handle.z, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
					Sys_Error("Couldn't get size of %s in %s", filename, pak->filename);
				unzGetCurrentFileInfoPosition(pak->handle.z, &f->filepos);
				f->z = pak->handle.z;
				f->size = info.uncompressed_size;
			}
		}
	}

	strncpy(f->name, filename, sizeof(f->name));

	return f;
}


/**
 * @brief
 * @sa LoadFile
 */
static void SafeRead (qFILE *f, void *buffer, int count)
{
	if (f->f) {
		if (fread(buffer, 1, count, f->f) != (size_t)count)
			Sys_Error("SafeRead: File read failure");
	} else if (f->z) {
		if (unzReadCurrentFile(f->z, buffer, count) == -1)
			Sys_Error("SafeRead: unzReadCurrentFile error");
	} else
		Sys_Error("SafeRead: No filehandle given");
}


/**
 * @brief
 * @sa SafeRead
 */
void SafeWrite (qFILE *f, void *buffer, int count)
{
	if (f->z)
		Sys_Error("SafeWrite: Could not write into zip files");
	if (!f->f)
		Sys_Error("SafeWrite: No file handle given");
	if (fwrite(buffer, 1, count, f->f) != (size_t)count)
		Sys_Error("SafeWrite: File write failure");
}

/**
 * @brief
 */
void FreeFile (void *buffer)
{
	free(buffer);
}

/**
 * @brief
 * @sa SafeRead
 */
int LoadFile (const char *filename, void **bufferptr)
{
	qFILE f;
	int length;
	void *buffer;

	SafeOpenRead(filename, &f);
	if (!f.f && !f.z)
		Sys_Error("Could not load %s", filename);
	length = FS_filelength(&f);
	buffer = malloc(length + 1);
	((char *)buffer)[length] = 0;
	SafeRead(&f, buffer, length);
	CloseFile(&f);

	*bufferptr = buffer;
	return length;
}

/**
 * @brief
 */
void CloseFile (qFILE *f)
{
	if (f->f)
		fclose(f->f);
	else if (f->z)
		unzCloseCurrentFile(f->z);

	f->f = f->z = NULL;
}

/**
 * @brief
 * @note Allows failure
 * @sa LoadFile
 */
int TryLoadFile (const char *filename, void **bufferptr)
{
	qFILE f;
	int length;
	void *buffer;

	*bufferptr = NULL;

	SafeOpenRead(filename, &f);
	if (!f.f && !f.z)
		return -1;
	length = FS_filelength(&f);
	buffer = malloc(length + 1);
	((char *)buffer)[length] = 0;
	SafeRead(&f, buffer, length);
	CloseFile(&f);

	*bufferptr = buffer;
	return length;
}

/**
 * @brief
 */
void Sys_FPrintf (int flag, const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	if ((flag == SYS_VRB) && (config.verbose == qfalse))
		return;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}

/**
 * @brief
 */
void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}
