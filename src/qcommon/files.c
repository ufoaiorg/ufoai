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


//
// in memory
//

typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
cvar_t	*fs_cddir;
cvar_t	*fs_gamedirvar;

typedef struct filelink_s
{
	struct filelink_s	*next;
	char	*from;
	int		fromlength;
	char	*to;
} filelink_t;

filelink_t	*fs_links;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*fs_searchpaths;
searchpath_t	*fs_base_searchpaths;	// without gamedirs


/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/

/*
** wildcard string comparing
*/
qboolean strwildcomp (const char *string, const char *pattern)
{
	const char *s = 0;
	char c = '\0';

	s = string;

	while (true) {
		switch (c = *pattern++) {
		case 0:
			return !*s ? true : false;
		case '?':
			if (*s == '\0') return false;
			s++;
			break;
		case '*':
			c = *pattern;
			while (c == '*')
				c = *++pattern;

			if (!c)
				return true;

			while (*s) {
				if (strwildcomp (s, pattern)) return true;
				s++;
			}
			return false;
		default:
			if (c != *s) return false;
			++s;
			break;
		}
	}
}

/*
** write paths in a normal form
*/
void normPath( char *path )
{
	char *tmp = path;

	while ( *tmp )
	{
		if ( *tmp == '\\' )
			*tmp = '/';
		else
			*tmp = tolower(*tmp);
		tmp++;
	}
}

/*
================
FS_filelength
================
*/
int FS_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void	FS_CreatePath (char *path)
{
	char	*ofs;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = '/';
		}
	}
}

/*
==============
FS_FCloseFile

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile (FILE *f)
{
	fclose (f);
}

/*
==============
Developer_searchpath
==============
*/
int Developer_searchpath (int who)
{
	return (0);
}

/*
===========
FS_FOpenFileSingle

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
int file_from_pak = 0;
int FS_FOpenFileSingle (const char *filename, FILE **file)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	int				i;
	filelink_t		*link;

	file_from_pak = 0;

	// check for links first
	for (link = fs_links ; link ; link=link->next)
	{
		if (!Q_strncmp ((char*)filename, link->from, link->fromlength))
		{
			Com_sprintf (netpath, sizeof(netpath), "%s%s",link->to, filename+link->fromlength);
			*file = fopen (netpath, "rb");
			if (*file)
			{
				Com_DPrintf ("link file: %s\n",netpath);
				return FS_filelength (*file);
			}
			return -1;
		}
	}

	//
	// search through the path, one element at a time
	//
	for (search = fs_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
				if (!Q_strcasecmp (pak->files[i].name, (char*)filename))
				{	// found it!
					file_from_pak = 1;
					Com_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);
					// open a new file on the pakfile
					*file = fopen (pak->filename, "rb");
					if (!*file)
						Com_Error (ERR_FATAL, "Couldn't reopen %s", pak->filename);
					fseek (*file, pak->files[i].filepos, SEEK_SET);
					return pak->files[i].filelen;
				}
		}
		else
		{
			// check a file in the directory tree

			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);

			*file = fopen (netpath, "rb");
			if (!*file)
				continue;

			Com_DPrintf ("FindFile: %s\n",netpath);

			return FS_filelength (*file);
		}

	}

	*file = NULL;
	return -1;
}

/*
=================
FS_Seek

=================
*/
int FS_Seek( FILE* f, long offset, int origin )
{
	int _origin;

	switch( origin )
	{
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
			Sys_Error( "Bad origin in FS_Seek\n" );
			break;
	}
	return fseek( f, offset, _origin );
}

int FS_FOpenFile (const char *filename, FILE **file)
{
	int		result, len;
	len = strlen( filename );

	// open file
	result =  FS_FOpenFileSingle( filename, file );

	// nothing corresponding found
	if ( result == -1 )
		Com_DPrintf ("FindFile: can't find %s\n", filename);

	return result;
}

/*
===========
FS_SV_FOpenFileRead
===========
*/
int FS_FOpenFileRead( const char *filename, FILE **f )
{
	char *ospath;
	int l;

	if ( ( l = FS_CheckFile( filename) ) > 0 )
	{
		// don't let sound stutter
// 		S_ClearBuffer();

		*f = fopen( ospath, "rb" );
	}
	else
		*f = NULL;

	return l;
}


/*
===========
FS_FOpenFileWrite
===========
*/
int FS_FOpenFileWrite( const char *filename, FILE** f )
{
	int len;

	len = FS_CheckFile( filename );
	if ( len <= 0 )
		*f = fopen( filename, "wb" );

	return len;
}


/*
=================
FS_CheckFile

Just returns the filelength and -1 if the file wasn't found
Won't print any errors
=================
*/
int FS_CheckFile (const char *filename)
{
	int		result;
	FILE	*file;

	result = FS_FOpenFileSingle( filename, &file );
	if ( result != -1 ) fclose( file );

	return result;
}

/*
=================
FS_Read

Properly handles partial reads
=================
*/
void CDAudio_Stop(void);
#define	MAX_READ	0x10000		// read in blocks of 64k
void FS_Read (void *buffer, int len, FILE *f)
{
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;
	tries = 0;
	while (remaining)
	{
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread (buf, 1, block, f);
		if (read == 0)
		{
			// we might have been trying to read from a CD
			if (!tries)
			{
				tries = 1;
				CDAudio_Stop();
			}
			else
				Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
		}

		if (read == -1)
			Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");

		// do some progress bar thing here...

		remaining -= read;
		buf += read;
	}
}

/*
============
FS_LoadFile

Filename are reletive to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile (char *path, void **buffer)
{
	FILE	*h;
	byte	*buf;
	int	len;

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = FS_FOpenFile (path, &h);
	if (!h)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}

	if (!buffer)
	{
		fclose (h);
		return len;
	}

	buf = Z_Malloc(len+1);
	*buffer = buf;

	FS_Read (buf, len, h);
	buf[len] = 0;

	fclose (h);

	return len;
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile (void *buffer)
{
	Z_Free (buffer);
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackFile (char *packfile)
{
	dpackheader_t header;
	dpak3header_t temp;
	int i, numpackfiles, len;
	packfile_t *newfiles;
	pack_t *pack;
	FILE *packhandle;
	dpackfile_t info[MAX_FILES_IN_PACK];
	unsigned checksum;

	packhandle = fopen(packfile, "rb");
	if (!packhandle)
		return NULL;
	len = strlen(packfile);

	if (!Q_strncmp(packfile+len-4, ".pak", 4))
	{
		// NeVo - we have a regular QII .pak file
		fread (&header, 1, sizeof(header), packhandle);
		if (LittleLong(header.ident) != IDPAKHEADER)
			Com_Error (ERR_FATAL, "%s is not a packfile", packfile);
		header.dirofs = LittleLong (header.dirofs);
		header.dirlen = LittleLong (header.dirlen);

		numpackfiles = header.dirlen / sizeof(dpackfile_t);

		if (numpackfiles > MAX_FILES_IN_PACK)
			Com_Error (ERR_FATAL, "%s has %i files", packfile, numpackfiles);

		newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));

		fseek (packhandle, header.dirofs, SEEK_SET);
		fread (info, 1, header.dirlen, packhandle);

		// crc the directory to check for modifications
		checksum = Com_BlockChecksum ((void *)info, header.dirlen);

		// parse the directory
		for (i=0 ; i<numpackfiles ; i++)
		{
			Q_strncpyz(newfiles[i].name, info[i].name, MAX_QPATH);
			newfiles[i].filepos = LittleLong(info[i].filepos);
			newfiles[i].filelen = LittleLong(info[i].filelen);
		}
	}
	else if (!Q_strncmp(packfile+len-4, ".pk3", 4) || !Q_strncmp(packfile+len-4, ".zip", 4))
	{
		// NeVo - We have a Quake III .pk3 or uncompressed .zip file
		/*
		NeVo - crcing the zip file is pointless
		// crc the directory to check for modifications
		checksum = Com_BlockChecksum ((void *)info, header.dirlen);

		*/
		for( i = 0; i < MAX_FILES_IN_PACK; ++i)
		{
			//-- Get the local header of the file.
			fread(&temp, sizeof(dpak3header_t), 1, packhandle);

			//-- Check if finished with pak file item collection.
			if (BigLong(temp.ident) == PAK3DIRHEADER)
				break;

			//-- Check if header signature is correct for the first item.
			if ((BigLong(temp.ident) != PAK3HEADER) && (i == 0))
				Com_Error (ERR_FATAL, "%s is not a packfile", packfile);

			//-- Check if compression is used or any flags are set.
			if ((temp.compression != 0) || (temp.flags != 0))
				Com_Error (ERR_FATAL, "%s contains errors or is compressed" , packfile );

			//-- Get length of data area
			info[i].filelen = temp.uncompressedSize;

			//-- Get the data areas filename and add \0 to the end
			fread( &info[i].name, temp.filenameLength, 1, packhandle);
			info[i].name[temp.filenameLength] = '\0';

			//-- Get the offset of the data area
			info[i].filepos = (ftell(packhandle) + temp.extraFieldLength);

			//-- Goto the next header
			fseek( packhandle, (info[i].filelen + info[i].filepos), SEEK_SET);
		}
		//-- Allocate space for array of packfile structures (filename, offset, length)
		newfiles = Z_Malloc (i * sizeof(packfile_t));

		//-- The save the number of items collected from the zip file.
		numpackfiles = i;

		for (i=0 ; i < numpackfiles; i++)
		{
			Q_strncpyz(newfiles[i].name, info[i].name, MAX_QPATH);
			newfiles[i].filepos = info[i].filepos;
			newfiles[i].filelen = info[i].filelen;
		}
	}
	else
	{
		// Unrecognized file type!
		Com_Printf ("Pack file type %s unrecognized\n", packfile+len-4);
		return NULL;
	}
	pack = Z_Malloc (sizeof (pack_t));
	Q_strncpyz(pack->filename, packfile, MAX_QPATH);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
#if 0
pack_t *FS_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info[MAX_FILES_IN_PACK];
	unsigned		checksum;

	packhandle = fopen(packfile, "rb");
	if (!packhandle)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (LittleLong(header.ident) != IDPAKHEADER)
		Com_Error (ERR_FATAL, "%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Com_Error (ERR_FATAL, "%s has %i files", packfile, numpackfiles);

	newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (info, 1, header.dirlen, packhandle);

	// crc the directory to check for modifications
	checksum = Com_BlockChecksum ((void *)info, header.dirlen);

	// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	pack = Z_Malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}
#endif /* 0 */


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
------------------------------------
NeVo - heavily updated this function
- Now loads up to 100 enumerated .pak files. (Pooy)
- Now loads any and all .pak files. (Void)
- Extended all functionality to include Quake III .pk3 and
uncompressed .zip files (NeVo/Nanook).
Note: Just as higher enumerated files overwrite
prior ones if they are the same, loose directory
files overwrite .zip's which overwrite .pk3's
which overwrite .pak's.
------------------------------------
================
*/
void FS_AddGameDirectory (char *dir)
{
	int i;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];
	//VoiD -S- *.pak support
	//char *path = NULL;
	char findname[1024];
	char **dirnames;
	int ndirs;
	char *tmp;
	//VoiD -E- *.pack support

	Q_strncpyz(fs_gamedir, dir, MAX_QPATH);

	Com_Printf("Adding game dir: %s\n", fs_gamedir );

	//
	// add the directory to the search path
	//
	search = Z_Malloc (sizeof(searchpath_t));
	Q_strncpyz(search->filename, dir, sizeof(search->filename)-1);
	search->filename[sizeof(search->filename)-1] = 0;
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (i=0; i<100; i++) // Pooy - paks can now go up to 100
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = FS_LoadPackFile (pakfile);
		if (!pak)
			continue;
		search = Z_Malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;
	}
	//
	// NeVo - pak3's!
	// add any pk3 files in the format pak0.pk3 pak1.pk3, ...
	//
	for ( i=0; i<100; i++ ) // Pooy - paks can now go up to 100
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pk3", dir, i);
		pak = FS_LoadPackFile (pakfile);
		if (!pak)
			continue;
		search = Z_Malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;
	}
	//
	// NeVo - pak3's!
	// add any pk3 files in the format pak0.pk3 pak1.pk3, ...
	//
	for ( i=0; i<100; i++ ) // Pooy - paks can now go up to 100
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.zip", dir, i);
		pak = FS_LoadPackFile (pakfile);
		if (!pak)
			continue;
		search = Z_Malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;
	}
	//VoiD -S- *.pack support
	for (i=0; i<3; i++)
	{
		// NeVo - Set filetype
		switch (i) {
			case 0:
				// Standard Quake II pack file '.pak'
				Com_sprintf( findname, sizeof(findname), "%s/%s", dir, ".pak" );
				break;
			case 1:
				// Quake III pack file '.pk3'
				Com_sprintf( findname, sizeof(findname), "%s/%s", dir, ".pk3" );
				break;
			case 2:
				// Uncompressed zip file '.zip'
				Com_sprintf( findname, sizeof(findname), "%s/%s", dir, ".zip" );
				break;
			default:
				// Standard Quake II pack file '.pak'
				Com_sprintf( findname, sizeof(findname), "%s/%s", dir, ".pak" );
				break;
		}

		tmp = findname;
		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' )
				*tmp = '/';
			tmp++;
		}
		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;
			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
				{
					pak = FS_LoadPackFile (dirnames[i]);
					if (!pak)
						continue;
					search = Z_Malloc (sizeof(searchpath_t));
					search->pack = pak;
					search->next = fs_searchpaths;
					fs_searchpaths = search;
				}
				free( dirnames[i] );
			}
			free( dirnames );
		}
		//VoiD -E- *.pack support
	}
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir (void)
{
	if (*fs_gamedir)
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

/*================
FS_AddHomeAsGameDirectory

Use ~/.ufoai/dir as fs_gamedir
================
*/
void FS_AddHomeAsGameDirectory (char *dir)
{
#ifndef _WIN32
	char gdir[MAX_OSPATH];
	char *homedir=getenv("HOME");
	if(homedir)
	{
		int len = snprintf(gdir,sizeof(gdir),"%s/.ufoai/%s/", homedir, dir);
		Com_Printf("using %s for writing\n",gdir);
		FS_CreatePath (gdir);
		FS_CreatePath (va("%s/save", gdir) );

		if ((len > 0) && (len < sizeof(gdir)) && (gdir[len-1] == '/'))
			gdir[len-1] = 0;

		Q_strncpyz(fs_gamedir,gdir,MAX_QPATH);
		fs_gamedir[sizeof(fs_gamedir)-1] = 0;

		FS_AddGameDirectory (gdir);
	}
#endif
}



/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec (void)
{
	char *dir;
	char name [MAX_QPATH];

	dir = Cvar_VariableString("gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir);
	else
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASEDIRNAME);
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText ("exec autoexec.cfg\n");
	Sys_FindClose();
}


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack)
		{
			fclose (fs_searchpaths->pack->handle);
			Z_Free (fs_searchpaths->pack->files);
			Z_Free (fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Z_Free (fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->value)
		Cbuf_AddText ("vid_restart\nsnd_restart\n");

	Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!Q_strncmp(dir,BASEDIRNAME, strlen(BASEDIRNAME)) || (*dir == 0) )
	{
		Cvar_FullSet ("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet ("gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);
		if (fs_cddir->string[0])
			FS_AddGameDirectory (va("%s/%s", fs_cddir->string, dir) );
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
 		FS_AddHomeAsGameDirectory(dir);
	}
}


/*
================
FS_Link_f

Creates a filelink_t
================
*/
void FS_Link_f (void)
{
	filelink_t	*l, **prev;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: link <from> <to>\n");
		return;
	}

	// see if the link already exists
	prev = &fs_links;
	for (l=fs_links ; l ; l=l->next)
	{
		if (!Q_strcmp (l->from, Cmd_Argv(1)))
		{
			Z_Free (l->to);
			if (!strlen(Cmd_Argv(2)))
			{	// delete it
				*prev = l->next;
				Z_Free (l->from);
				Z_Free (l);
				return;
			}
			l->to = CopyString (Cmd_Argv(2));
			return;
		}
		prev = &l->next;
	}

	// create a new link
	l = Z_Malloc(sizeof(*l));
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1));
	l->fromlength = strlen(l->from);
	l->to = CopyString(Cmd_Argv(2));
}


/*
** FS_ListFiles
*/
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = malloc( sizeof( char * ) * nfiles );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = strdup( s );
#ifdef _WIN32
			strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
** FS_Dir_f
*/
void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		Q_strncpyz( wildcard, Cmd_Argv( 1 ), sizeof(wildcard) );
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );
		normPath( findname );

		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				free( dirnames[i] );
			}
			free( dirnames );
		}
		Com_Printf( "\n" );
	};
}

/*
============
FS_Path_f

============
*/
void FS_Path_f (void)
{
	searchpath_t	*s;
	filelink_t		*l;

	Com_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}

	Com_Printf ("\nLinks:\n");
	for (l=fs_links ; l ; l=l->next)
		Com_Printf ("%s : %s\n", l->from, l->to);
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return fs_gamedir;

	prev = NULL; //fs_gamedir;
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
			continue;
		if ( prev && !Q_strcmp( prevpath, prev ) )
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}


/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem (void)
{
	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("link", FS_Link_f);
	Cmd_AddCommand ("dir", FS_Dir_f );

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get ("basedir", ".", CVAR_NOSET);

	//
	// cddir <path>
	// Logically concatenates the cddir after the basedir for
	// allows the game to run from outside the data tree
	//
	fs_cddir = Cvar_Get ("cddir", "", CVAR_NOSET);
	if (fs_cddir->string[0])
		FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_cddir->string) );

	//
	// start up with base by default
	//
	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_basedir->string) );

 	//
 	// then add a '.ufoai/base' directory in home directory by default
 	//
 	FS_AddHomeAsGameDirectory(BASEDIRNAME);

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	if (fs_gamedirvar->string[0])
		FS_SetGamedir (fs_gamedirvar->string);
}


/*
================
FS_BuildFileList
================
*/
#define FL_BLOCKSIZE	1024
typedef struct listBlock_s
{
	char	path[MAX_QPATH];
	char	files[FL_BLOCKSIZE];
	struct listBlock_s	*next;
} listBlock_t;

listBlock_t *fs_blocklist = NULL;

void FS_BuildFileList( char *fileList )
{
	listBlock_t	*block, *tblock;
	char	*path;
	char	files[MAX_QPATH];
	char	findname[1024];
	char	**filenames;
	char	*f, *fl, *tl;
	int		nfiles;
	int		len;

	// bring it into normal form
	Q_strncpyz( files, fileList, MAX_QPATH );
	normPath( files );

	// check the blocklist for older searches
	// and do a new one after deleting them
	for ( block = fs_blocklist, tblock = NULL; block; )
	{
		if ( !Q_strncmp( block->path, files, MAX_QPATH ) )
		{
			// delete old one
			if ( tblock ) tblock->next = block->next;
			else fs_blocklist = block->next;

			Z_Free( block );

			if ( tblock ) block = tblock->next;
			else block = fs_blocklist;
			continue;
		}

		tblock = block;
		block = block->next;
	}

	// allocate a new block and link it into the list
	block = Z_Malloc (sizeof(listBlock_t));
	block->next = fs_blocklist;
	fs_blocklist = block;

	// store the search string
	Q_strncpyz( block->path, files, MAX_QPATH );

//	Com_Printf( "pattern: '%s' path: '%s'\n", block->pattern, block->path );

	// search for the files
	path = NULL;
	fl = block->files;
	tl = 0;
	nfiles = 0;

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		Com_sprintf( findname, sizeof(findname), "%s/%s", path, files );
		normPath( findname );

		if ( ( filenames = FS_ListFiles( findname, &nfiles, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < nfiles-1; i++ )
			{
				f = strrchr( filenames[i], '/' );
				if ( !f ) f = filenames[i];
				else f++;

				// check for double occurences
				for ( tblock = block; tblock; tblock = tblock->next )
				{
					tl = tblock->files;
					while ( *tl )
					{
						if ( !Q_strcmp( tl, f ) )
							break;
						tl += strlen( tl ) + 1;
					}
					if ( *tl ) break;
				}

				if ( !*tl )
				{
					len = strlen( f );
					if ( fl - block->files + len >= FL_BLOCKSIZE )
					{
						// terminalize the last block
						*fl = 0;

						// allocate a new block
						tblock = Z_Malloc (sizeof(listBlock_t));
						tblock->next = block->next;
						block->next = tblock;

						Q_strncpyz( tblock->path, block->path, MAX_QPATH );
						fl = tblock->files;
						block = tblock;
					}

					// add the new file
					strcpy( fl, f );
					fl += len+1;
				}

				free( filenames[i] );
			}
			free( filenames );
		}
	}

	// terminalize the list
	*fl = 0;

	// print out the file list
/*	for ( tblock = fs_blocklist; tblock; tblock = tblock->next )
		if ( !strcmp( tblock->path, files ) )
		{
			tl = tblock->files;
			while ( *tl )
			{
				Com_Printf( "%s\n", tl );
				tl += strlen( tl ) + 1;
			}
		}*/
}


/*
================
FS_SkipBlock
================
*/
void FS_SkipBlock( char **text )
{
	char	*token;
	int		depth;
	depth = 1;

	do {
		token = COM_Parse( text );
		if ( *token == '{' ) depth++;
		else if ( *token == '}' ) depth--;
	} while ( depth && *text );
}

/*
================
FS_NextScriptHeader
================
*/
char		lastList[MAX_QPATH] = "";
listBlock_t	*lBlock;
char		*lFile = NULL;
char		*lBuffer = NULL;

char		headerType[32];
char		headerName[32];

char *FS_NextScriptHeader( char *files, char **name, char **text )
{
	listBlock_t	*block;
	char		*token;

	if ( !text )
	{
		*lastList = 0;
		return NULL;
	}

	if ( Q_strncmp( files, lastList, MAX_QPATH ) )
	{
		// search for file lists
		Q_strncpyz( lastList, files, MAX_QPATH );

		for ( block = fs_blocklist; block; block = block->next )
			if ( !strcmp( files, block->path ) )
				break;

		if ( !block )
			// didn't find any valid file list
			return NULL;

		lBlock = block;
		lFile = block->files;
	}

	while ( lBlock )
	{
		if ( lBuffer )
		{
			// continue reading the current file
			if ( *text )
			{
				token = COM_Parse( text );
				if ( *token == '{' )
				{
					FS_SkipBlock( text );
					token = COM_Parse( text );
				}

				Q_strncpyz( headerType, token, sizeof(headerType) );
				if ( *text )
				{
					token = COM_Parse( text );
					Q_strncpyz( headerName, token, sizeof(headerName) );
					*name = headerName;
					return headerType;
				}
			}

			// search a new file
			lFile += strlen( lFile ) + 1;

			while ( !*lFile && lBlock )
				// it was the last file in the block, continue to next block
				for ( lBlock = lBlock->next; lBlock; lBlock = lBlock->next )
					if ( !strcmp( files, lBlock->path ) )
					{
						lFile = lBlock->files;
						break;
					}
		}

		if ( *lFile )
		{
			char	filename[MAX_QPATH];

			// free the old file
			if ( lBuffer )
			{
				FS_FreeFile( lBuffer );
				lBuffer = NULL;
			}

			// load a new file
			Q_strncpyz( filename, lBlock->path, MAX_QPATH );
			strcpy( strrchr( filename, '/' ) + 1, lFile );

			FS_LoadFile( filename, (void**)&lBuffer );
			*text = lBuffer;
		}
	}

	// free the old file
	if ( lBuffer )
	{
		FS_FreeFile( lBuffer );
		lBuffer = NULL;
	}

	// finished
	return NULL;
}

// maplisting
char* maps[MAX_MAPS];
int anzInstalledMaps = 0;
qboolean mapsInstalledInit = false;
int mapInstalledIndex = 0;

/*
================
FS_GetMaps
================
*/
#define MAX_MAPNAME_LENGTH 256
void FS_GetMaps ( void )
{
	char	name[MAX_OSPATH];
	int	len, status;
	char	*found;
	char	*path = NULL;
	char	*baseMapName = NULL;

	if ( mapsInstalledInit )
		return;

	Com_sprintf (name, sizeof(name), "maps/*.bsp");//, FS_Gamedir());
	len = strlen(name);
	mapInstalledIndex = 0;
	while ( ( path = FS_NextPath( path ) ) )
	{
		found = Sys_FindFirst( va("%s/%s", path, name) , 0, 0 );
		while (found && anzInstalledMaps < MAX_MAPS )
		{
			baseMapName = COM_SkipPath ( found );
			COM_StripExtension ( baseMapName, found );
			status = CheckBSPFile( found );
			if ( ! status )
			{
				//searched a specific map?
				maps[anzInstalledMaps] = (char *) malloc ( MAX_MAPNAME_LENGTH * sizeof(char) );
				if ( maps[anzInstalledMaps] == NULL )
				{
					Com_Printf("Could not allocate memory in MN_GetMaps\n");
					return;
				}
				Q_strncpyz( maps[anzInstalledMaps], found, MAX_MAPNAME_LENGTH );
				anzInstalledMaps++;
			}
			else
				Com_Printf("invalid mapstatus: %i (%s)\n", status, found );
			found = Sys_FindNext( 0, 0 );
		}
	}

	mapsInstalledInit = true;

	Sys_FindClose ();
}

/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write( const void *buffer, int len, FILE* f )
{
	int		block, remaining;
	int		written;
	byte	*buf;
	int		tries;

	if ( !f )
		return 0;

	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining)
	{
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Printf( "FS_Write: 0 bytes written\n" );
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf( "FS_Write: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
// 	fflush( f );
	return len;
}


/*
=================
FS_WriteFile

Properly handles partial writes
=================
*/
int FS_WriteFile( const void *buffer, int len, const char* filename )
{
	int		block, remaining;
	int		written;
	byte	*buf;
	int		tries;
	FILE	*f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	// FIXME: Do we need more for security reasons?
	if ( ( filename[0] == '.' && filename[1] == '.' ) || filename[0] == '/' )
		return 0;

	f = fopen( filename, "wb" );
	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Printf( "FS_WriteFile: 0 bytes written\n" );
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf( "FS_WriteFile: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	fclose( f );
	return len;
}

/*
=================
FS_GetCwd

Return current working dir
=================
*/
char* FS_GetCwd( void )
{
	static char buf[MAX_QPATH];
	const char* path = getcwd(buf,sizeof(buf));
	if ( path != NULL )
	{
#ifdef _WIN32
		normPath( path );
#endif
		return buf;
	} else {
		return "";
	}
}

/*
==============
FS_FileExists
==============
*/
qboolean FS_FileExists (char *filename)
{
	FILE	*f;

	f = fopen (filename, "r");
	if (!f)
		return false;
	fclose (f);
	return true;
}
