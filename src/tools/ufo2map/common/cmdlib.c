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

#include "cmdlib.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif

#define	BASEDIR_ID_FILE	".gamedir"	/* This is the name of the "trunk" directory. */
#define	BASEDIR	"base"	/* This is the directory the game-data is stored. */
#define PATHSEPERATOR   '/'

/* set these before calling CheckParm */
int myargc;
char **myargv;

static char com_token[1024];
qboolean com_eof, archive;
char archivedir[1024];


#define	MAX_EX_ARGC	1024
int ex_argc;
char *ex_argv[MAX_EX_ARGC];

#ifdef _WIN32
/**
 * @brief Mimic unix command line expansion
 */
void ExpandWildcards (int *argc, char ***argv)
{
	struct _finddata_t fileinfo;
	int		handle;
	int		i;
	char	filename[1024];
	char	filebase[1024];
	char	*path;

	ex_argc = 0;
	for (i=0 ; i<*argc ; i++) {
		path = (*argv)[i];
		if ( path[0] == '-' || ( !strstr(path, "*") && !strstr(path, "?") ) ) {
			ex_argv[ex_argc++] = path;
			continue;
		}

		handle = _findfirst (path, &fileinfo);
		if (handle == -1)
			return;

		ExtractFilePath (path, filebase);

		do {
			sprintf (filename, "%s%s", filebase, fileinfo.name);
			ex_argv[ex_argc++] = copystring (filename);
		} while (_findnext( handle, &fileinfo ) != -1);

		_findclose (handle);
	}

	*argc = ex_argc;
	*argv = ex_argv;
}
#else
void ExpandWildcards (int *argc, char ***argv)
{
}
#endif

#ifdef WIN_ERROR
#include <windows.h>
/**
 * @brief For abnormal program terminations in windowed apps
 */
void Error (char *error, ...)
{
	va_list argptr;
	char	text[1024];
	char	text2[1024];
	int		err;

	err = GetLastError ();

	va_start (argptr,error);
	vsprintf (text, error,argptr);
	va_end (argptr);

	snprintf (text2, sizeof(text2), "%s\nGetLastError() = %i", text, err);
	MessageBox(NULL, text2, "Error", 0 /* MB_OK */ );

	exit (1);
}

#else
/**
 * @brief For abnormal program terminations in console apps
 */
void Error (char *error, ...)
{
	va_list argptr;

	printf ("\n************ ERROR ************\n");

	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}
#endif

/* only printf if in verbose mode */
qboolean verbose = qfalse;
void qprintf (char *format, ...)
{
	va_list argptr;

	if (!verbose)
		return;

	va_start (argptr,format);
	vprintf (format,argptr);
	va_end (argptr);
}


/**
 * qdir will hold the path up to the quake directory, including the slash
 * f:\quake\
 * /raid/quake/
 * gamedir will hold qdir + the game directory (id1, id2, etc)
 */

char qdir[1024];
char gamedir[1024];

/**
 * @brief
 */
void SetQdirFromPath (char *path)
{
	char	temp[1024];
	char	c[1024];
	int pathlength = 0;

	if (!(path[0] == '/' || path[0] == '\\' || path[1] == ':')) {	/* path is partial */
		Q_getwd (temp);
		strcat (temp, path);
		path = temp;
	}

	/* search for ".gamedir"-file in path */
	strncpy (c, path, strlen(path)-1);
	for (pathlength=strlen(c)-1; pathlength > 0 ; pathlength--) {
		c[pathlength] = 0;
		if ( (!c[strlen(c)-1] == '/')
		&& (!c[strlen(c)-1] == '\\')
		&& (!c[strlen(c)-1] == ':') )
			continue;
		snprintf (temp, sizeof(temp), "%s/%s", c, BASEDIR_ID_FILE);
		if ( FileExists(temp) ) {
			strncpy (gamedir, c, strlen(c)-1);
			strncat (gamedir, "/"BASEDIR"/", sizeof(gamedir));
			qprintf ("gamedir: %s\n", gamedir);
			return;
		}
	}
	Error ("SetQdirFromPath: No file '%s' found in any directory of the path %s", BASEDIR_ID_FILE, path);
}

/**
 * @brief
 */
char *ExpandArg (char *path)
{
	static char full[1024];

	if (path[0] != '/' && path[0] != '\\' && path[1] != ':') {
		Q_getwd (full);
		strcat (full, path);
	} else
		strncpy (full, path, sizeof(full));
	return full;
}

/**
 * @brief
 */
char *ExpandPath (char *path)
{
	static char full[1024];
	if (!qdir)
		Error ("ExpandPath called without qdir set");
	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
		return path;
	snprintf (full, sizeof(full), "%s%s", qdir, path);
	return full;
}

/**
 * @brief
 */
char *ExpandPathAndArchive (char *path)
{
	char *expanded;
	char archivename[1024];

	expanded = ExpandPath (path);

	if (archive) {
		snprintf (archivename, sizeof(archivename), "%s/%s", archivedir, path);
		QCopyFile (expanded, archivename);
	}
	return expanded;
}


/**
 * @brief
 */
char *copystring(char *s)
{
	char	*b;
	b = (char*)malloc(sizeof(char)*(strlen(s)+1));
	strcpy (b, s);
	return b;
}

/**
 * @brief
 */
double I_FloatTime (void)
{
	time_t t;
	time (&t);
	return t;
#if 0
	/* more precise, less portable */
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
#endif
}

/**
 * @brief
 */
void Q_getwd (char *out)
{
#ifdef _WIN32
   _getcwd (out, 256);
   strcat (out, "\\");
#else
   getcwd( out, 256 );
   strcat (out, "/");
#endif
}


/**
 * @brief
 */
void Q_mkdir (char *path)
{
#ifdef _WIN32
	if (_mkdir (path) != -1)
		return;
#else
	if (mkdir (path, 0777) != -1)
		return;
#endif
	if (errno != EEXIST)
		Error ("mkdir %s: %s",path, strerror(errno));
}

/**
 * @brief
 * @return -1 if not present
 */
int	FileTime (char *path)
{
	struct stat buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
}

/**
 * @brief Parse a token out of a string
 */
char *COM_Parse (char *data)
{
	int c, len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

	/* skip whitespace */
skipwhite:
	while ( (c = *data) <= ' ') {
		if (c == 0) {
			com_eof = qtrue;
			return NULL;			/* end of file; */
		}
		data++;
	}

	/* skip // comments */
	if (c=='/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


	/* handle quoted strings specially */
	if (c == '\"') {
		data++;
		do {
			c = *data++;
			if (c=='\"') {
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		} while (1);
	}

	/* parse single characters */
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':') {
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

	/* parse a regular word */
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
			break;
	} while (c>32);

	com_token[len] = 0;
	return data;
}

/**
 * @brief
 */
int Q_strncasecmp (char *s1, char *s2, int n)
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
int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}


/**
 * @brief
 */
char *strupr (char *start)
{
	char *in;
	in = start;
	while (*in) {
		*in = toupper(*in);
		in++;
	}
	return start;
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
 * @brief Checks for the given parameter in the program's command line arguments
 * @return the argument number (1 to argc-1) or 0 if not present
 */
int CheckParm (char *check)
{
	int i;

	for (i = 1;i<myargc;i++) {
		if ( !Q_strcasecmp(check, myargv[i]) )
			return i;
	}

	return 0;
}

/**
 * @brief
 */
int Q_filelength (FILE *f)
{
	int pos, end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/**
 * @brief
 */
FILE *SafeOpenWrite (char *filename)
{
	FILE	*f;

	f = fopen(filename, "wb");
	if (!f)
		Error ("Error opening %s: %s",filename,strerror(errno));

	return f;
}

/**
 * @brief
 */
FILE *SafeOpenRead (char *filename)
{
	FILE	*f;

	f = fopen(filename, "rb");
	if (!f)
		Error ("Error opening %s: %s",filename,strerror(errno));

	return f;
}


/**
 * @brief
 */
void SafeRead (FILE *f, void *buffer, int count)
{
	if ( fread (buffer, 1, count, f) != (size_t)count)
		Error ("File read failure");
}


/**
 * @brief
 */
void SafeWrite (FILE *f, void *buffer, int count)
{
	if (fwrite (buffer, 1, count, f) != (size_t)count)
		Error ("File write failure");
}


/**
 * @brief
 */
qboolean FileExists (char *filename)
{
	FILE	*f;

	f = fopen (filename, "r");
	if (!f)
		return qfalse;
	fclose (f);
	return qtrue;
}

/**
 * @brief
 * @sa SaveFile
 */
int LoadFile (char *filename, void **bufferptr)
{
	FILE *f;
	int length;
	void *buffer;

	f = SafeOpenRead (filename);
	length = Q_filelength (f);
	buffer = malloc (length+1);
	((char *)buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}


/**
 * @brief
 * @note Allows failure
 * @sa LoadFile
 */
int TryLoadFile (char *filename, void **bufferptr)
{
	FILE *f;
	int length;
	void *buffer;

	*bufferptr = NULL;

	f = fopen (filename, "rb");
	if (!f)
		return -1;
	length = Q_filelength (f);
	buffer = malloc (length+1);
	((char *)buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}

/**
 * @brief
 * @sa LoadFile
 */
void SaveFile (char *filename, void *buffer, int count)
{
	FILE *f;

	f = SafeOpenWrite (filename);
	SafeWrite (f, buffer, count);
	fclose (f);
}

/**
 * @brief
 */
void DefaultExtension (char *path, char *extension)
{
	char    *src;
	/* if path doesnt have a .EXT, append extension */
	/* (extension should include the .) */
	src = path + strlen(path) - 1;

	while (*src != PATHSEPERATOR && src != path) {
		if (*src == '.')
			return;                 /* it has an extension */
		src--;
	}

	strcat (path, extension);
}

/**
 * @brief
 */
void DefaultPath (char *path, char *basepath)
{
	char temp[128];

	if (path[0] == PATHSEPERATOR)
		return;                   /* absolute path location */
	strcpy(temp,path);
	strcpy(path,basepath);
	strcat(path,temp);
}


/**
 * @brief
 */
void StripFilename (char *path)
{
	int length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != PATHSEPERATOR)
		length--;
	path[length] = 0;
}

/**
 * @brief
 */
void StripExtension (char *path)
{
	int length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != '.') {
		length--;
		if (path[length] == '/')
			return;		/* no extension */
	}
	if (length)
		path[length] = 0;
}


/**
 * @brief
 * FIXME: should include the slash, otherwise
 * backing to an empty path will be wrong when appending a slash
 */
void ExtractFilePath (char *path, char *dest)
{
	char *src;

	src = path + strlen(path) - 1;

	/* back up until a \ or the start */
	while (src != path && *(src-1) != '\\' && *(src-1) != '/')
		src--;

	memcpy (dest, path, src-path);
	dest[src-path] = 0;
}

/**
 * @brief
 */
void ExtractFileBase (char *path, char *dest)
{
	char    *src;

	src = path + strlen(path) - 1;

	/* back up until a \ or the start */
	while (src != path && *(src-1) != PATHSEPERATOR)
		src--;

	while (*src && *src != '.') {
		*dest++ = *src++;
	}
	*dest = 0;
}

/**
 * @brief
 */
void ExtractFileExtension (char *path, char *dest)
{
	char    *src;

	src = path + strlen(path) - 1;

	/* back up until a . or the start */
	while (src != path && *(src-1) != '.')
		src--;
	if (src == path) {
		*dest = 0;	/* no extension */
		return;
	}

	strcpy (dest,src);
}


/**
 * @brief
 */
int ParseHex (char *hex)
{
	char    *str;
	int    num;

	num = 0;
	str = hex;

	while (*str) {
		num <<= 4;
		if (*str >= '0' && *str <= '9')
			num += *str-'0';
		else if (*str >= 'a' && *str <= 'f')
			num += 10 + *str-'a';
		else if (*str >= 'A' && *str <= 'F')
			num += 10 + *str-'A';
		else
			Error ("Bad hex number: %s",hex);
		str++;
	}

	return num;
}

/**
 * @brief
 */
int ParseNum (char *str)
{
	if (str[0] == '$')
		return ParseHex (str+1);
	if (str[0] == '0' && str[1] == 'x')
		return ParseHex (str+2);
	return atol (str);
}



/*
============================================================================
BYTE ORDER FUNCTIONS
============================================================================
*/

#ifdef _SGI_SOURCE
#define	__BIG_ENDIAN__
#endif

#ifdef __BIG_ENDIAN__

/**
 * @brief
 */
short LittleShort (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

/**
 * @brief
 */
short BigShort (short l)
{
	return l;
}

/**
 * @brief
 */
int LittleLong (int l)
{
	byte b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

/**
 * @brief
 */
int BigLong (int l)
{
	return l;
}


/**
 * @brief
 */
float LittleFloat (float l)
{
	union {byte b[4]; float f;} in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

/**
 * @brief
 */
float BigFloat (float l)
{
	return l;
}

#else

/**
 * @brief
 */
short BigShort (short l)
{
	byte b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

/**
 * @brief
 */
short LittleShort (short l)
{
	return l;
}


/**
 * @brief
 */
int BigLong (int l)
{
	byte b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

/**
 * @brief
 */
int LittleLong (int l)
{
	return l;
}

/**
 * @brief
 */
float BigFloat (float l)
{
	union {byte b[4]; float f;} in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

/**
 * @brief
 */
float LittleFloat (float l)
{
	return l;
}
#endif


/**
 * @brief
 */
void CreatePath (char *path)
{
	char	*ofs, c;

	if (path[1] == ':')
		path += 2;

	for (ofs = path+1 ; *ofs ; ofs++) {
		c = *ofs;
		if (c == '/' || c == '\\') {	/* create the directory */
			*ofs = 0;
			Q_mkdir (path);
			*ofs = c;
		}
	}
}


/**
 * @brief Used to archive source files
 */
void QCopyFile (char *from, char *to)
{
	void	*buffer;
	int		length;

	length = LoadFile (from, &buffer);
	CreatePath (to);
	SaveFile (to, buffer, length);
	free (buffer);
}
