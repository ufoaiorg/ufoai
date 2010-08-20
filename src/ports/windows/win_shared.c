/**
 * @file win_shared.c
 * @brief Windows shared functions
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

#include "../../common/common.h"
#include "win_local.h"
#include <fcntl.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

HINSTANCE global_hInstance;

int Sys_Milliseconds (void)
{
	static int base = 0;

	/* let base retain 16 bits of effectively random data */
	if (!base)
		base = timeGetTime() & 0xffff0000;

	return timeGetTime() - base;
}

void Sys_Mkdir (const char *path)
{
	_mkdir(path);
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static int findhandle;

static qboolean CompareAttributes (unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
		return qfalse;
	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
		return qfalse;
	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
		return qfalse;
	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
		return qfalse;
	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
		return qfalse;

	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
		return qfalse;
	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
		return qfalse;
	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
		return qfalse;
	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
		return qfalse;
	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
		return qfalse;

	return qtrue;
}

/**
 * @sa Sys_FindNext
 * @sa Sys_FindClose
 */
char *Sys_FindFirst (const char *path, unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle)
		Sys_Error("Sys_BeginFind without close");
	findhandle = 0;

	Com_FilePath(path, findbase);
	findhandle = _findfirst(path, &findinfo);
	while (findhandle != -1) {
		/* found one that matched */
		if (strcmp(findinfo.name, ".") && strcmp(findinfo.name, "..") &&
			CompareAttributes(findinfo.attrib, musthave, canthave)) {
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		/* doesn't match - try the next one */
		} else if (_findnext(findhandle, &findinfo) == -1) {
			/* ok, no further entries here - leave the while loop */
			_findclose(findhandle);
			findhandle = -1;
		}
	}

	/* none found */
	return NULL;
}

/**
 * @sa Sys_FindFirst
 * @sa Sys_FindClose
 */
char *Sys_FindNext (unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	/* until we found the next entry */
	while (_findnext(findhandle, &findinfo) != -1) {
		if (strcmp(findinfo.name, ".") && strcmp(findinfo.name, "..") &&
			CompareAttributes(findinfo.attrib, musthave, canthave)) {
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}
	}

	/* none found */
	return NULL;
}

/**
 * @brief Closes the find handle
 * @note Call this before calling a new Sys_FindFirst
 * @sa Sys_FindNext
 * @sa Sys_FindFirst
 */
void Sys_FindClose (void)
{
	if (findhandle != -1)
		_findclose(findhandle);
	findhandle = 0;
}

#define MAX_FOUND_FILES 0x1000

void Sys_ListFilteredFiles (const char *basedir, const char *subdirs, const char *filter, linkedList_t **list)
{
	char search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char filename[MAX_OSPATH];
	int findhandle;
	struct _finddata_t findinfo;

	if (subdirs[0] != '\0') {
		Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
	} else {
		Com_sprintf(search, sizeof(search), "%s\\*", basedir);
	}

	findhandle = _findfirst(search, &findinfo);
	if (findhandle == -1)
		return;

	do {
		if (findinfo.attrib & _A_SUBDIR) {
			if (Q_strcasecmp(findinfo.name, ".") && Q_strcasecmp(findinfo.name, "..")) {
				if (subdirs[0] != '\0') {
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name);
				} else {
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
				}
				Sys_ListFilteredFiles(basedir, newsubdirs, filter, list);
			}
		}
		Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name);
		if (!Com_Filter(filter, filename))
			continue;
		LIST_AddString(list, filename);
	} while (_findnext(findhandle, &findinfo) != -1);

	_findclose(findhandle);
}

void Sys_Quit (void)
{
	timeEndPeriod(1);

#ifdef COMPILE_UFO
	CL_Shutdown();
	Qcommon_Shutdown();
#elif COMPILE_MAP
	Mem_Shutdown();
#endif

	/* exit(0) */
	ExitProcess(0);
}

#ifdef COMPILE_MAP
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	MessageBox(NULL, text, "Error!", MB_OK + MB_ICONEXCLAMATION);

	ExitProcess(1);
}
#endif

/**
 * @brief Get current user
 */
const char *Sys_GetCurrentUser (void)
{
	static char s_userName[1024];
	unsigned long size = sizeof(s_userName);

	if (!GetUserName(s_userName, &size))
		Q_strncpyz(s_userName, "", sizeof(s_userName));

	return s_userName;
}

/**
 * @brief Get current working dir
 */
char *Sys_Cwd (void)
{
	static char cwd[MAX_OSPATH];

	if (_getcwd(cwd, sizeof(cwd) - 1) == NULL)
		return NULL;
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/**
 * @brief Normalize path (remove all \\ )
 */
void Sys_NormPath (char* path)
{
	char *tmp = path;

	while (*tmp) {
		if (*tmp == '\\')
			*tmp = '/';
		else
			*tmp = tolower(*tmp);
		tmp++;
	}
}

/**
 * @brief Get the home directory in Application Data
 * @note Forced for Windows Vista
 * @note Use the cvar fs_usehomedir if you want to use the homedir for other
 * Windows versions, too
 */
char *Sys_GetHomeDirectory (void)
{
	TCHAR szPath[MAX_PATH];
	static char path[MAX_OSPATH];
	FARPROC qSHGetFolderPath;
	HMODULE shfolder;

	shfolder = LoadLibrary("shfolder.dll");

	if (shfolder == NULL) {
		Com_Printf("Unable to load SHFolder.dll\n");
		return NULL;
	}

	qSHGetFolderPath = GetProcAddress(shfolder, "SHGetFolderPathA");
	if (qSHGetFolderPath == NULL) {
		Com_Printf("Unable to find SHGetFolderPath in SHFolder.dll\n");
		FreeLibrary(shfolder);
		return NULL;
	}

	if (!SUCCEEDED(qSHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
		Com_Printf("Unable to detect CSIDL_APPDATA\n");
		FreeLibrary(shfolder);
		return NULL;
	}

	Q_strncpyz(path, szPath, sizeof(path));
	Q_strcat(path, "\\UFOAI", sizeof(path));
	FreeLibrary(shfolder);

	if (!CreateDirectory(path, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			Com_Printf("Unable to create directory \"%s\"\n", path);
			return NULL;
		}
	}
	return path;
}

/**
 * @brief Calls the win32 sleep function
 */
void Sys_Sleep (int milliseconds)
{
	if (milliseconds < 1)
		milliseconds = 1;
	Sleep(milliseconds);
}

/**
 * @brief set/unset environment variables (empty value removes it)
 */
int Sys_Setenv (const char *name, const char *value)
{
	/* Windows does not have setenv, but its putenv is safe to use.
	 * It does not keep a pointer to the string. */
	char str[256];
	Com_sprintf(str, sizeof(str), "%s=%s", name, value);
	return putenv(str);
}

void Sys_InitSignals (void)
{
}
