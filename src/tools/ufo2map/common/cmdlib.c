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

typedef struct {
	char name[MAX_QPATH];
	unsigned long filepos;
	unsigned long filelen;
} packfile_t;

typedef struct pack_s {
	char filename[MAX_OSPATH];
	qFILE handle;
	int numfiles;
	packfile_t *files;
} pack_t;

static pack_t *pak;

static pack_t *FS_LoadPackFile(const char *packfile);

#include <zlib.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#define	BASEDIR_ID_FILE	".gamedir"	/* This is the name of the "trunk" directory. */

#ifdef WIN_ERROR
#include <windows.h>
/**
 * @brief For abnormal program terminations in windowed apps
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[2048];
	char text2[2048];
	int err, len;

	err = GetLastError();

	va_start(argptr, error);
	len = _vsnprintf(text, sizeof(text), error, argptr);
	if (len != -1) {
		text[strlen(text)-1] = '\0';
		va_end(argptr);
		snprintf(text2, sizeof(text2), "%s\n\nGetLastError() = %i", text, err);
		MessageBox(NULL, text2, "Error", MB_OK|MB_ICONINFORMATION);
	} else {
		MessageBox(NULL, "Buffer too short to display error message", "Error", MB_OK|MB_ICONINFORMATION);
	}

	exit(1);
}

#else
/**
 * @brief For abnormal program terminations in console apps
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[2048];

	Com_Printf("\n************ ERROR ************\n");

	va_start(argptr, error);
	vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);
	Com_Printf("%s\n", text);

	exit(1);
}
#endif

/**
 * @brief
 */
static qboolean FileExists (const char *filename)
{
	qFILE f;

	SafeOpenRead(filename, &f);
	if (!f.f && !f.z)
		return qfalse;
	CloseFile(&f);
	return qtrue;
}

/**
 * @brief gamedir will hold the game directory (base, id1, id2, etc)
 */
char gamedir[1024];

/**
 * @brief
 */
void SetQdirFromPath (char *path)
{
	char temp[1024] = "";
	char c[1024] = "";
	int pathlength = 0;

	if (!(path[0] == '/' || path[0] == '\\' || path[1] == ':')) {	/* path is partial */
		Q_getwd(temp);
		strcat(temp, path);
		path = temp;
	}

	/* search for ".gamedir"-file in path */
	strncpy(c, path, strlen(path) - 1);
	for (pathlength = strlen(c) - 1; pathlength > 0; pathlength--) {
		c[pathlength] = 0;
		if ((!c[strlen(c)-1] == '/')
		 && (!c[strlen(c)-1] == '\\')
		 && (!c[strlen(c)-1] == ':'))
			continue;
		snprintf(temp, sizeof(temp), "%s/%s", c, BASEDIR_ID_FILE);
		if (FileExists(temp)) {
			strncpy(gamedir, c, strlen(c) - 1);
			strncat(gamedir, "/"BASEDIRNAME"/", sizeof(gamedir));
			Com_Printf("gamedir: %s\n", gamedir);
			snprintf(c, sizeof(c) - 1, "%s0pics.pk3", gamedir);
			pak = FS_LoadPackFile(c);
			if (!pak)
				Com_Printf("Could not load image pk3 (%s)\n", c);
			return;
		}
	}
	Sys_Error("SetQdirFromPath: No file '%s' found in any directory of the path %s", BASEDIR_ID_FILE, path);
}

/**
 * @brief
 */
char *ExpandArg (const char *path)
{
	static char full[1024];

	if (path[0] != '/' && path[0] != '\\' && path[1] != ':') {
		Q_getwd(full);
		strcat(full, path);
	} else
		strncpy(full, path, sizeof(full));

	return full;
}

/**
 * @brief
 */
char *copystring (const char *s)
{
	char *b;
	b = (char*)malloc(sizeof(char)*(strlen(s)+1));
	strcpy(b, s);
	return b;
}

/**
 * @brief
 */
double I_FloatTime (void)
{
	time_t t;
	time(&t);
	return t;
}

/**
 * @brief
 */
void Q_getwd (char *out)
{
#ifdef _WIN32
	_getcwd(out, 256);
	strcat(out, "\\");
#else
	if (getcwd(out, 256) == NULL)
		Com_Printf("Warning, getcwd failed\n");
	strcat(out, "/");
#endif
}

/**
 * @brief
 */
int Q_strncasecmp (const char *s1, const char *s2, int n)
{
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		/* strings are equal until end point */

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		/* strings not equal */
		}
	} while (c1);

	return 0;		/* strings are equal */
}

/**
 * @brief
 */
int Q_strcasecmp (const char *s1, const char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

/**
 * @brief
 */
char *strlower (char *start)
{
	char *in;
	in = start;
	while (*in) {
		*in = tolower(*in);
		in++;
	}
	return start;
}


/*
=============================================================================
MISC FUNCTIONS
=============================================================================
*/

/**
 * @brief
 */
int Q_filelength (qFILE *f)
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
qFILE *SafeOpenWrite (const char *filename, qFILE* f)
{
	memset(f, 0, sizeof(qFILE));
	f->f = fopen(filename, "wb");
	if (!f->f)
		Sys_Error("Error opening %s for writing: %s", filename, strerror(errno));

	return f;
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

	len = strlen(packfile);

	if (!Q_strncasecmp(packfile + len - 4, ".pk3", 4) || !Q_strncasecmp(packfile + len - 4, ".zip", 4)) {
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
			strlower(filename_inzip);

			unzGetCurrentFileInfoPosition(uf, &newfiles[i].filepos);
			strncpy(newfiles[i].name, filename_inzip, sizeof(newfiles[i].name) - 1);
			newfiles[i].filelen = file_info.compressed_size;
			unzGoToNextFile(uf);
		}
		pack->files = newfiles;

		Com_Printf("Added packfile %s (%li files)\n", packfile, gi.number_entry);
		return pack;
	} else {
		/* Unrecognized file type! */
		Com_Printf("Pack file type %s unrecognized\n", packfile + len - 4);
		return NULL;
	}
}

/**
 * @brief
 */
qFILE *SafeOpenRead (const char *filename, qFILE *f)
{
	char path[MAX_OSPATH];
	size_t len;

	memset(f, 0, sizeof(qFILE));

	f->f = fopen(filename, "rb");
	if (!f->f && pak) {
		len = strlen(gamedir);
		strncpy(path, filename, sizeof(path));
		path[len-1] = '\0';
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
void SafeRead (qFILE *f, void *buffer, int count)
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
	length = Q_filelength(&f);
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
	length = Q_filelength(&f);
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
void DefaultExtension (char *path, const char *extension)
{
	char *src;
	/* if path doesnt have a .EXT, append extension
	 * (extension should include the .) */
	src = path + strlen(path) - 1;

	while (*src != '/' && *src != '\\' && src != path) {
		if (*src == '.')
			return; /* it has an extension */
		src--;
	}

	strcat(path, extension);
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
	vsprintf(out_buffer, format, argptr);
	va_end(argptr);

	printf(out_buffer);
}

/**
 * @brief
 */
void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	vsprintf(out_buffer, format, argptr);
	va_end(argptr);

	printf(out_buffer);
}
