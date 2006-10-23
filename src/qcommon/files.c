/**
 * @file files.c
 * @brief All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.
 * The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
 * only used during filesystem initialization.
 * The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.
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

#include "qcommon.h"
#include "unzip.h"

/* in memory */

typedef struct {
	char name[MAX_QPATH];
	unsigned long filepos;
	unsigned long filelen;
} packfile_t;

typedef struct pack_s {
	char filename[MAX_OSPATH];
	FILE *handle;
	int numfiles;
	packfile_t *files;
} pack_t;

char fs_gamedir[MAX_OSPATH];
cvar_t *fs_basedir;
cvar_t *fs_gamedirvar;

typedef struct filelink_s {
	struct filelink_s *next;
	char *from;
	int fromlength;
	char *to;
} filelink_t;

filelink_t *fs_links;

typedef struct searchpath_s {
	char filename[MAX_OSPATH];
	pack_t *pack;				/* only one of filename / pack will be used */
	struct searchpath_s *next;
} searchpath_t;

searchpath_t *fs_searchpaths;
searchpath_t *fs_base_searchpaths;	/* without gamedirs */


/**
 * @brief
 */
qboolean strwildcomp(const char *string, const char *pattern)
{
	const char *s = 0;
	char c = '\0';

	s = string;

	while (qtrue) {
		switch (c = *pattern++) {
		case 0:
			return !*s ? qtrue : qfalse;
		case '?':
			if (*s == '\0')
				return qfalse;
			s++;
			break;
		case '*':
			c = *pattern;
			while (c == '*')
				c = *++pattern;

			if (!c)
				return qtrue;

			while (*s) {
				if (strwildcomp(s, pattern))
					return qtrue;
				s++;
			}
			return qfalse;
		default:
			if (c != *s)
				return qfalse;
			++s;
			break;
		}
	}
}

/**
 * @brief
 * @sa Sys_NormPath
 */
void FS_NormPath(char *path)
{
	Sys_NormPath(path);
}

/**
 * @brief
 */
int FS_filelength(FILE * f)
{
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

/**
 * @brief Creates any directories needed to store the given filename
 * @sa Sys_Mkdir
 */
void FS_CreatePath(char *path)
{
	char *ofs;

	for (ofs = path + 1; *ofs; ofs++) {
		/* create the directory */
		if (*ofs == '/') {
			*ofs = 0;
			Sys_Mkdir(path);
			*ofs = '/';
		}
	}
}

/**
 * @brief For some reason, other dll's can't just call fclose()
 * on files returned by FS_FOpenFile...
 */
void FS_FCloseFile(FILE * f)
{
	fclose(f);
}

/**
 * @brief
 */
int Developer_searchpath(int who)
{
	return (0);
}

/**
 * @brief Finds the file in the search path.
 * @return filesize and an open FILE *
 * @note Used for streaming data out of either a pak file or a seperate file.
 */
int file_from_pak = 0;
int FS_FOpenFileSingle(const char *filename, FILE ** file)
{
	searchpath_t *search;
	char netpath[MAX_OSPATH];
	pack_t *pak;
	int i;
	filelink_t *link;

	file_from_pak = 0;

	/* check for links first */
	for (link = fs_links; link; link = link->next)
		if (!Q_strncmp((char *) filename, link->from, link->fromlength)) {
			Com_sprintf(netpath, sizeof(netpath), "%s%s", link->to, filename + link->fromlength);
			*file = fopen(netpath, "rb");
			if (*file) {
				Com_DPrintf("link file: %s\n", netpath);
				return FS_filelength(*file);
			}
			return -1;
		}

	/* search through the path, one element at a time */
	for (search = fs_searchpaths; search; search = search->next) {
		/* is the element a pak file? */
		if (search->pack) {
			/* look through all the pak file elements */
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				/* found it! */
				if (!Q_strcasecmp(pak->files[i].name, (char *) filename)) {
					file_from_pak = 1;
					Com_DPrintf("PackFile: %s : %s\n", pak->filename, filename);
					/* open a new file on the pakfile */
					*file = fopen(pak->filename, "rb");
					if (!*file)
						Com_Error(ERR_FATAL, "Couldn't reopen %s", pak->filename);
					fseek(*file, pak->files[i].filepos, SEEK_SET);
					return pak->files[i].filelen;
				}
		} else {
			/* check a file in the directory tree */
			Com_sprintf(netpath, sizeof(netpath), "%s/%s", search->filename, filename);

			*file = fopen(netpath, "rb");
			if (!*file)
				continue;

			Com_DPrintf("FindFile: %s\n", netpath);
			return FS_filelength(*file);
		}
	}

	*file = NULL;
	return -1;
}

/**
 * @brief
 */
int FS_Seek(FILE * f, long offset, int origin)
{
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
		_origin = SEEK_CUR;
		Sys_Error("Bad origin in FS_Seek\n");
		break;
	}
	return fseek(f, offset, _origin);
}

/**
 * @brief
 */
int FS_FOpenFile(const char *filename, FILE ** file)
{
	int result, len;

	len = strlen(filename);

	/* open file */
	result = FS_FOpenFileSingle(filename, file);

	/* nothing corresponding found */
	if (result == -1)
		Com_Printf("FindFile: can't find %s\n", filename);

	return result;
}

/**
 * @brief
 */
int FS_FOpenFileRead(const char *filename, FILE ** f)
{
	char *ospath = NULL;
	int l;

	if ((l = FS_CheckFile(filename)) > 0) {
		/* don't let sound stutter */
/* 		S_ClearBuffer(); */
		*f = fopen(ospath, "rb");
	} else
		*f = NULL;

	return l;
}


/**
 * @brief
 */
int FS_FOpenFileWrite(const char *filename, FILE ** f)
{
	int len;

	len = FS_CheckFile(filename);
	if (len <= 0)
		*f = fopen(filename, "wb");

	return len;
}


/**
 * @brief Just returns the filelength and -1 if the file wasn't found
 * @note Won't print any errors
 */
int FS_CheckFile(const char *filename)
{
	int result;
	FILE *file;

	result = FS_FOpenFileSingle(filename, &file);
	if (result != -1)
		fclose(file);

	return result;
}

void CDAudio_Stop(void);

#define	MAX_READ	0x10000		/* read in blocks of 64k */
/**
 * @brief Properly handles partial reads
 */
void FS_Read(void *buffer, int len, FILE * f)
{
	int block, remaining;
	int read;
	byte *buf;
	int tries;

	buf = (byte *) buffer;

	/* read in chunks for progress bar */
	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread(buf, 1, block, f);
		if (read == 0) {
			/* we might have been trying to read from a CD */
			if (!tries) {
				tries = 1;
				CDAudio_Stop();
			} else {
#if 0 /* Pk3 file support is not working atm */
				read = unzReadCurrentFile(f, buffer, len);
				if (read == 0)
					Com_Error(ERR_FATAL, "FS_Read: 0 bytes read");
				else
					break;
#endif
				Com_Error(ERR_FATAL, "FS_Read: 0 bytes read");
			}
		}

		if (read == -1)
			Com_Error(ERR_FATAL, "FS_Read: -1 bytes read");

		/* do some progress bar thing here... */

		remaining -= read;
		buf += read;
	}
}

/**
 * @briefFilename are reletive to the quake search path
 * a null buffer will just return the file length without loading
 * a -1 length means that the file is not present
 */
int FS_LoadFile(char *path, void **buffer)
{
	FILE *h;
	byte *buf;
	int len;

	buf = NULL;					/* quiet compiler warning */

	/* look for it in the filesystem or pack files */
	len = FS_FOpenFile(path, &h);
	if (!h) {
		if (buffer)
			*buffer = NULL;
		return -1;
	}

	if (!buffer) {
		fclose(h);
		return len;
	}

	buf = Mem_Alloc(len + 1);
	*buffer = buf;

	FS_Read(buf, len, h);
	buf[len] = 0;

	fclose(h);

	return len;
}

/**
 * @brief
 */
void FS_FreeFile(void *buffer)
{
	Mem_Free(buffer);
}

/**
 * @brief Takes an explicit (not game tree related) path to a pak file.
 * Adding the files at the beginning of the list so they override previous pack files.
 */
pack_t *FS_LoadPackFile(char *packfile)
{
	int i, len, err;
	packfile_t *newfiles;
	pack_t *pack;
	FILE *packhandle;
	unz_file_info file_info;
	unzFile uf;
	unz_global_info gi;
	char filename_inzip[MAX_QPATH];

	packhandle = fopen(packfile, "rb");
	if (!packhandle)
		return NULL;
	len = strlen(packfile);

	if (!Q_strncmp(packfile + len - 4, ".pk3", 4) || !Q_strncmp(packfile + len - 4, ".zip", 4)) {
		uf = unzOpen(packfile);
		err = unzGetGlobalInfo (uf,&gi);

		if (err != UNZ_OK)
			return NULL;

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

		pack = Mem_Alloc(sizeof(pack_t));
		Q_strncpyz(pack->filename, packfile, MAX_QPATH);
		pack->handle = uf;
		pack->numfiles = gi.number_entry;
		unzGoToFirstFile(uf);

		/* Allocate space for array of packfile structures (filename, offset, length) */
		newfiles = Mem_Alloc(i * sizeof(packfile_t));

		for (i = 0; i < gi.number_entry; i++) {
			err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
			if (err != UNZ_OK)
				break;
			Q_strlwr(filename_inzip);

			unzGetCurrentFileInfoPosition(uf, &newfiles[i].filepos);
			Q_strncpyz(newfiles[i].name, filename_inzip, MAX_QPATH);
			newfiles[i].filelen = file_info.compressed_size;
			unzGoToNextFile(uf);
		}
		pack->files = newfiles;

		Com_Printf("Added packfile %s (%i files) (NOTE: support for pk3 is not fully implemented)\n", packfile, gi.number_entry);
		return pack;
	} else {
		/* Unrecognized file type! */
		Com_Printf("Pack file type %s unrecognized\n", packfile + len - 4);
		return NULL;
	}
}


/**
  * @brief Sets fs_gamedir, adds the directory to the head of the path
  */
void FS_AddGameDirectory(char *dir)
{
	searchpath_t *search;
	pack_t *pak;
	char **dirnames = NULL;
	int ndirs = 0, i;

	Q_strncpyz(fs_gamedir, dir, MAX_QPATH);

	Com_Printf("Adding game dir: %s\n", fs_gamedir);

	/* add the directory to the search path */
	search = Mem_Alloc(sizeof(searchpath_t));
	Q_strncpyz(search->filename, dir, sizeof(search->filename) - 1);
	search->filename[sizeof(search->filename) - 1] = 0;
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	if ((dirnames = FS_ListFiles(va("%s/*.pk3", dir), &ndirs, 0, 0)) != 0) {
		for (i = 0; i < ndirs - 1; i++) {
			if (strrchr(dirnames[i], '/')) {
				pak = FS_LoadPackFile(dirnames[i]);
				if (!pak)
					continue;
				search = Mem_Alloc(sizeof(searchpath_t));
				search->pack = pak;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
			free(dirnames[i]);
		}
		free(dirnames);
	}
}

/**
 * @brief Called to find where to write a file (demos, savegames, etc)
 */
char *FS_Gamedir(void)
{
	if (*fs_gamedir)
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

/**
 * @brief
 * @note e.g. *nix: Use ~/.ufoai/dir as fs_gamedir
 * @sa Sys_GetHomeDirectory
 */
void FS_AddHomeAsGameDirectory(char *dir)
{
	char gdir[MAX_OSPATH];
	char *homedir = Sys_GetHomeDirectory();

	if (homedir) {
		Com_sprintf(gdir, MAX_OSPATH, "%s/.ufoai/%s", homedir, dir);
		Com_Printf("using %s for writing\n", gdir);
		FS_CreatePath(va("%s/", gdir));

		Q_strncpyz(fs_gamedir, gdir, MAX_QPATH);

		FS_AddGameDirectory(gdir);
	}
}

/**
 * @brief
 */
void FS_ExecAutoexec(void)
{
	char *dir;
	char name[MAX_QPATH];

	dir = Cvar_VariableString("gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir);
	else
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASEDIRNAME);
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText("exec autoexec.cfg\n");
	Sys_FindClose();
}


/**
 * @brief Sets the gamedir and path to a different directory.
 */
void FS_SetGamedir(char *dir)
{
	searchpath_t *next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":")) {
		Com_Printf("Gamedir should be a single filename, not a path\n");
		return;
	}

	/* free up any current game dir info */
	while (fs_searchpaths != fs_base_searchpaths) {
		if (fs_searchpaths->pack) {
			fclose(fs_searchpaths->pack->handle);
			Mem_Free(fs_searchpaths->pack->files);
			Mem_Free(fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Mem_Free(fs_searchpaths);
		fs_searchpaths = next;
	}

	/* flush all data, so it will be forced to reload */
	if (dedicated && !dedicated->value)
		Cbuf_AddText("vid_restart\nsnd_restart\n");

	Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!Q_strncmp(dir, BASEDIRNAME, strlen(BASEDIRNAME)) || (*dir == 0)) {
		Cvar_FullSet("gamedir", "", CVAR_SERVERINFO | CVAR_NOSET);
		Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	} else {
		Cvar_FullSet("gamedir", dir, CVAR_SERVERINFO | CVAR_NOSET);
		FS_AddGameDirectory(va("%s/%s", fs_basedir->string, dir));
		FS_AddHomeAsGameDirectory(dir);
	}
}


/**
 * @brief Creates a filelink_t
 */
void FS_Link_f(void)
{
	filelink_t *l, **prev;

	if (Cmd_Argc() != 3) {
		Com_Printf("usage: link <from> <to>\n");
		return;
	}

	/* see if the link already exists */
	prev = &fs_links;
	for (l = fs_links; l; l = l->next) {
		if (!Q_strcmp(l->from, Cmd_Argv(1))) {
			Mem_Free(l->to);
			if (!strlen(Cmd_Argv(2))) {	/* delete it */
				*prev = l->next;
				Mem_Free(l->from);
				Mem_Free(l);
				return;
			}
			l->to = CopyString(Cmd_Argv(2));
			return;
		}
		prev = &l->next;
	}

	/* create a new link */
	l = Mem_Alloc(sizeof(*l));
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1));
	l->fromlength = strlen(l->from);
	l->to = CopyString(Cmd_Argv(2));
}


/**
 * @brief Builds a qsorted filelist
 * @sa Sys_FindFirst
 * @sa Sys_FindNext
 * @sa Sys_FindClose
 */
char **FS_ListFiles(char *findname, int *numfiles, unsigned musthave, unsigned canthave)
{
	char *s;
	int nfiles = 0, i;
	char **list = NULL;
	char tempList[MAX_FILES][MAX_OSPATH];

	*numfiles = 0;

	s = Sys_FindFirst(findname, musthave, canthave);
	while (s) {
		if (s[strlen(s) - 1] != '.')
			nfiles++;
		s = Sys_FindNext(musthave, canthave);
	}
	Sys_FindClose();

	if (!nfiles)
		return NULL;

	nfiles++; /* add space for a guard */
	*numfiles = nfiles;

	list = malloc(sizeof(char*)*nfiles);
	memset(list, 0, sizeof(char*)*nfiles);
	memset(tempList, 0, sizeof(tempList));

	s = Sys_FindFirst(findname, musthave, canthave);
	nfiles = 0;
	while (s) {
		if (s[strlen(s) - 1] != '.') {
			Q_strncpyz(tempList[nfiles], s, MAX_OSPATH);
#ifdef _WIN32
			Q_strlwr(tempList[nfiles]);
#endif
			nfiles++;
		}
		s = Sys_FindNext(musthave, canthave);
	}
	Sys_FindClose();

	qsort(tempList, nfiles, MAX_OSPATH, Q_StringSort);
	for (i=0; i<nfiles; i++) {
		list[i] = Q_strdup(tempList[i]);
	}

	return list;
}

/**
 * @brief
 */
void FS_Dir_f(void)
{
	char *path = NULL;
	char findname[1024];
	char wildcard[1024] = "*.*";
	char **dirnames;
	int ndirs;

	if (Cmd_Argc() != 1)
		Q_strncpyz(wildcard, Cmd_Argv(1), sizeof(wildcard));

	while ((path = FS_NextPath(path)) != NULL) {
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);
		FS_NormPath(findname);

		Com_Printf("Directory of %s\n", findname);
		Com_Printf("----\n");

		if ((dirnames = FS_ListFiles(findname, &ndirs, 0, 0)) != 0) {
			int i;

			for (i = 0; i < ndirs - 1; i++) {
				if (strrchr(dirnames[i], '/'))
					Com_Printf("%s\n", strrchr(dirnames[i], '/') + 1);
				else
					Com_Printf("%s\n", dirnames[i]);

				free(dirnames[i]);
			}
			free(dirnames);
		}
		Com_Printf("\n");
	};
}

/**
 * @brief
 */
void FS_Path_f(void)
{
	searchpath_t *s;
	filelink_t *l;

	Com_Printf("Current search path:\n");
	for (s = fs_searchpaths; s; s = s->next) {
		if (s == fs_base_searchpaths)
			Com_Printf("----------\n");
		if (s->pack)
			Com_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf("%s\n", s->filename);
	}

	Com_Printf("\nLinks:\n");
	for (l = fs_links; l; l = l->next)
		Com_Printf("%s : %s\n", l->from, l->to);
}

/**
 * @brief Allows enumerating all of the directories in the search path
 */
char *FS_NextPath(char *prevpath)
{
	searchpath_t *s;
	char *prev;

	if (!prevpath)
		return fs_gamedir;

	prev = NULL;				/*fs_gamedir; */
	for (s = fs_searchpaths; s; s = s->next) {
		if (s->pack)
			continue;
		if (prev && !Q_strcmp(prevpath, prev))
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}


/**
 * @brief
 */
void FS_InitFilesystem(void)
{
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("link", FS_Link_f);
	Cmd_AddCommand("dir", FS_Dir_f);

	/* basedir <path> */
	/* allows the game to run from outside the data tree */
	fs_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);

	/* start up with base by default */
	FS_AddGameDirectory(va("%s/" BASEDIRNAME, fs_basedir->string));

	/* then add a '.ufoai/base' directory in home directory by default */
	FS_AddHomeAsGameDirectory(BASEDIRNAME);

	/* any set gamedirs will be freed up to here */
	fs_base_searchpaths = fs_searchpaths;

	/* check for game override */
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	if (fs_gamedirvar->string[0])
		FS_SetGamedir(fs_gamedirvar->string);
}


#define FL_BLOCKSIZE	1024
typedef struct listBlock_s {
	char path[MAX_QPATH];
	char files[FL_BLOCKSIZE];
	struct listBlock_s *next;
} listBlock_t;

static listBlock_t *fs_blocklist = NULL;

/**
 * @brief Build a filelist
 * @param[in] fileList e.g. ufos\*.ufo to get all ufo files in the gamedir/ufos dir
 */
void FS_BuildFileList(char *fileList)
{
	listBlock_t *block, *tblock;
	char *path;
	char files[MAX_QPATH];
	char findname[1024];
	char **filenames;
	char *f, *fl, *tl;
	int nfiles;
	int len;

	/* bring it into normal form */
	Q_strncpyz(files, fileList, MAX_QPATH);
	FS_NormPath(files);

	/* check the blocklist for older searches */
	/* and do a new one after deleting them */
	for (block = fs_blocklist, tblock = NULL; block;) {
		if (!Q_strncmp(block->path, files, MAX_QPATH)) {
			/* delete old one */
			if (tblock)
				tblock->next = block->next;
			else
				fs_blocklist = block->next;

			Mem_Free(block);

			if (tblock)
				block = tblock->next;
			else
				block = fs_blocklist;
			continue;
		}

		tblock = block;
		block = block->next;
	}

	/* allocate a new block and link it into the list */
	block = Mem_Alloc(sizeof(listBlock_t));
	block->next = fs_blocklist;
	fs_blocklist = block;

	/* store the search string */
	Q_strncpyz(block->path, files, MAX_QPATH);

/*	Com_Printf( "pattern: '%s' path: '%s'\n", block->pattern, block->path ); */

	/* search for the files */
	path = NULL;
	fl = block->files;
	tl = 0;
	nfiles = 0;

	while ((path = FS_NextPath(path)) != NULL) {
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, files);
		FS_NormPath(findname);

		if ((filenames = FS_ListFiles(findname, &nfiles, 0, 0)) != 0) {
			int i;

			for (i = 0; i < nfiles - 1; i++) {
				f = strrchr(filenames[i], '/');
				if (!f)
					f = filenames[i];
				else
					f++;

				/* check for double occurences */
				for (tblock = block; tblock; tblock = tblock->next) {
					tl = tblock->files;
					while (*tl) {
						if (!Q_strcmp(tl, f))
							break;
						tl += strlen(tl) + 1;
					}
					if (*tl)
						break;
				}

				if (!*tl) {
					len = strlen(f);
					if (fl - block->files + len >= FL_BLOCKSIZE) {
						/* terminalize the last block */
						*fl = 0;

						/* allocate a new block */
						tblock = Mem_Alloc(sizeof(listBlock_t));
						tblock->next = block->next;
						block->next = tblock;

						Q_strncpyz(tblock->path, block->path, MAX_QPATH);
						fl = tblock->files;
						block = tblock;
					}

					/* add the new file */
					Q_strncpyz(fl, f, len + 1);
					fl += len + 1;
				}

				free(filenames[i]);
			}
			free(filenames);
		}
	}

	/* terminalize the list */
	*fl = 0;
}


/**
 * @brief
 */
void FS_SkipBlock(char **text)
{
	char *token;
	int depth;

	depth = 1;

	do {
		token = COM_Parse(text);
		if (*token == '{')
			depth++;
		else if (*token == '}')
			depth--;
	} while (depth && *text);
}

/**
 * @brief
 */
static char lastList[MAX_QPATH] = "";
listBlock_t *lBlock;
char *lFile = NULL;
char *lBuffer = NULL;

static char headerType[32];
static char headerName[32];

/**
 * @brief
 */
char *FS_NextScriptHeader(char *files, char **name, char **text)
{
	listBlock_t *block;
	char *token;

	if (!text) {
		*lastList = 0;
		return NULL;
	}

	if (Q_strncmp(files, lastList, MAX_QPATH)) {
		/* search for file lists */
		Q_strncpyz(lastList, files, MAX_QPATH);

		for (block = fs_blocklist; block; block = block->next)
			if (!Q_strncmp(files, block->path, MAX_QPATH))
				break;

		if (!block)
			/* didn't find any valid file list */
			return NULL;

		lBlock = block;
		lFile = block->files;
	}

	while (lBlock) {
		if (lBuffer) {
			/* continue reading the current file */
			if (*text) {
				token = COM_Parse(text);
				if (*token == '{') {
					FS_SkipBlock(text);
					token = COM_Parse(text);
				}

				Q_strncpyz(headerType, token, sizeof(headerType));
				if (*text) {
					token = COM_Parse(text);
					Q_strncpyz(headerName, token, sizeof(headerName));
					*name = headerName;
					return headerType;
				}
			}

			/* search a new file */
			lFile += strlen(lFile) + 1;

			while (!*lFile && lBlock)
				/* it was the last file in the block, continue to next block */
				for (lBlock = lBlock->next; lBlock; lBlock = lBlock->next)
					if (!strcmp(files, lBlock->path)) {
						lFile = lBlock->files;
						break;
					}
		}

		if (*lFile) {
			char filename[MAX_QPATH];

			/* free the old file */
			if (lBuffer) {
				FS_FreeFile(lBuffer);
				lBuffer = NULL;
			}

			/* load a new file */
			Q_strncpyz(filename, lBlock->path, MAX_QPATH);
			strcpy(strrchr(filename, '/') + 1, lFile);

			FS_LoadFile(filename, (void **) &lBuffer);
			*text = lBuffer;
		}
	}

	/* free the old file */
	if (lBuffer) {
		FS_FreeFile(lBuffer);
		lBuffer = NULL;
	}

	/* finished */
	return NULL;
}

/* global vars for maplisting */
char *maps[MAX_MAPS];
int anzInstalledMaps = -1;
static qboolean mapsInstalledInit = qfalse;
int mapInstalledIndex = 0;

/**
 * @brief
 * @param[in] reset If true the directory is scanned everytime for new maps (useful for dedicated servers).
 * If false we only use the maps array (for clients e.g.)
 */
void FS_GetMaps(qboolean reset)
{
	char findname[MAX_OSPATH];
	char filename[MAX_QPATH];
	int status, i;
	char *baseMapName = NULL;
	char **dirnames;
	char *path = NULL;
	int ndirs;

	/* force a reread */
	if (!reset && mapsInstalledInit)
		return;

	mapInstalledIndex = 0;
	anzInstalledMaps = -1;

	while ((path = FS_NextPath(path)) != NULL) {
		Com_sprintf(findname, sizeof(findname), "%s/maps/*.bsp", path);
		FS_NormPath(findname);

		if ((dirnames = FS_ListFiles(findname, &ndirs, 0, 0)) != 0) {
			for (i = 0; i < ndirs - 1; i++) {
				anzInstalledMaps++;
				Com_DPrintf("... found map: '%s' (pos %i out of %i)\n", dirnames[i], i+1, ndirs);
				baseMapName = COM_SkipPath(dirnames[i]);
				COM_StripExtension(baseMapName, filename);
				status = CheckBSPFile(filename);
				if (!status) {
					/*searched a specific map? */
					maps[anzInstalledMaps] = (char *) malloc(MAX_QPATH * sizeof(char));
					if (maps[anzInstalledMaps] == NULL) {
						Com_Printf("Could not allocate memory in MN_GetMaps\n");
						free(dirnames[i]);
						continue;
					}
					Q_strncpyz(maps[anzInstalledMaps], filename, MAX_QPATH);
				} else
					Com_Printf("invalid mapstatus: %i (%s)\n", status, dirnames[i]);
				free(dirnames[i]);
			}
			free(dirnames);
		}
	}

	mapsInstalledInit = qtrue;
}

/**
 * @brief Properly handles partial writes
 */
int FS_Write(const void *buffer, int len, FILE * f)
{
	int block, remaining;
	int written;
	byte *buf;
	int tries;

	if (!f)
		return 0;

	buf = (byte *) buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite(buf, 1, block, f);
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
/* 	fflush( f ); */
	return len;
}


/**
 * @brief
 */
int FS_WriteFile(const void *buffer, int len, const char *filename)
{
	FILE *f;
	int c, lencheck;

	FS_CreatePath((char *) filename);

	f = fopen(filename, "wb");

	if (f)
		c = fwrite(buffer, 1, len, f);
	else
		return 0;

	lencheck = FS_filelength(f);
	fclose(f);

	/* if file write failed (file is incomplete) then delete it */
	if (c != len || lencheck != len) {
		Com_Printf("FS_WriteFile: failed to finish writing '%s'\n", filename);
		Com_Printf("FS_WriteFile: deleting this incomplete file\n");
		if (remove(filename))
			Com_Printf("FS_WriteFile: could not remove file: %s\n", filename);
		return 0;
	}

	return c;
}

/**
 * @brief Return current working dir
 */
char *FS_GetCwd(void)
{
	static char buf[MAX_QPATH];
	char *path = Q_getcwd(buf, sizeof(buf));

	if (path != NULL) {
		FS_NormPath(path);
		return buf;
	} else {
		return "";
	}
}

/**
 * @brief
 */
qboolean FS_FileExists(char *filename)
{
#ifdef _WIN32
	return (_access(filename, 00) == 0);
#else
	return (access(filename, R_OK) == 0);
#endif
}

/**
 * @brief Extract the path from a given filename
 * @param[in] filename The complete filename
 * @return pointer to start location of the base path
 */
char* FS_GetBasePath(char* filename)
{
	char* pathSep = filename;
	char* endPos;

	FS_NormPath(filename);
	while ((pathSep = strstr(pathSep, "/")) != NULL) {
		/* set to next / */
		endPos = pathSep;
	}
	if (endPos)
		endPos = '\0';

	return filename;
}

/**
 * @brief Cleanup function
 */
void FS_Shutdown(void)
{
	int i;

	/* free malloc'ed space for maplist */
	if (mapsInstalledInit) {
		for (i=0; i<=anzInstalledMaps;i++)
			free(maps[i]);
	}
}
