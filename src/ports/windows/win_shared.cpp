/**
 * @file
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
#include <mmsystem.h>
#include <shellapi.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

HINSTANCE global_hInstance;

static void Sys_Utf8ToUtf16 (const char* source, LPWSTR dest, size_t destlen)
{
	const int len = MultiByteToWideChar(CP_UTF8, 0, source, -1, dest, destlen - 1);
	dest[len] = 0;
}

static void Sys_Utf16ToUtf8 (const LPWSTR source, char* dest, size_t destsize)
{
	const int len = WideCharToMultiByte(CP_UTF8, 0, source, -1, dest, destsize - 1, nullptr, nullptr);
	dest[len] = '\0';
}

int Sys_Milliseconds (void)
{
	static int base = 0;

	/* let base retain 16 bits of effectively random data */
	if (!base)
		base = timeGetTime() & 0xffff0000;

	return timeGetTime() - base;
}

void Sys_Mkdir (const char* path)
{
	WCHAR wpath[MAX_OSPATH];
	Sys_Utf8ToUtf16(path, wpath, lengthof(wpath));
	_wmkdir(wpath);
}

void Sys_Breakpoint (void)
{
#if defined(_MSC_VER)
	__debugbreak();
#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
	__asm__ __volatile__ ( "int $3\n\t" );
#endif
}

#ifdef COMPILE_UFO
/**
 * @brief Switch to one processor usage for windows system with more than
 * one processor
 */
void Sys_SetAffinityAndPriority (void)
{
	SYSTEM_INFO sysInfo;
	/* 1 - use core #0
	 * 2 - use core #1
	 * 3 - use cores #0 and #1 */
	HANDLE proc = GetCurrentProcess();

	if (sys_priority->modified) {
		if (sys_priority->integer < 0)
			Cvar_SetValue("sys_priority", 0);
		else if (sys_priority->integer > 2)
			Cvar_SetValue("sys_priority", 2);

		sys_priority->modified = false;

		switch (sys_priority->integer) {
		case 0:
			SetPriorityClass(proc, NORMAL_PRIORITY_CLASS);
			Com_Printf("Priority changed to NORMAL\n");
			break;
		case 1:
			SetPriorityClass(proc, HIGH_PRIORITY_CLASS);
			Com_Printf("Priority changed to HIGH\n");
			break;
		default:
			SetPriorityClass(proc, REALTIME_PRIORITY_CLASS);
			Com_Printf("Priority changed to REALTIME\n");
			break;
		}
	}

	if (sys_affinity->modified) {
		DWORD_PTR procAffinity = 0;
		GetSystemInfo(&sysInfo);
		Com_Printf("Found %i processors\n", (int)sysInfo.dwNumberOfProcessors);
		sys_affinity->modified = false;
		if (sysInfo.dwNumberOfProcessors >= 2) {
			switch (sys_affinity->integer) {
			case 0:
				Com_Printf("Use all cores\n");
				procAffinity = (1 << sysInfo.dwNumberOfProcessors) - 1;
				break;
			case 1:
				Com_Printf("Use two cores\n");
				procAffinity = 3;
				break;
			case 2:
				Com_Printf("Only use one core\n");
				procAffinity = 1;
				break;
			}
		} else {
			Com_Printf("...only found one processor\n");
		}
		SetProcessAffinityMask(proc, procAffinity);
	}

	CloseHandle(proc);
}
#endif

const char* Sys_SetLocale (const char* localeID)
{
	Sys_Setenv("LANG", localeID);
	Sys_Setenv("LANGUAGE", localeID);
	Sys_Setenv("LC_NUMERIC", "C");

	return localeID;
}

const char* Sys_GetLocale (void)
{
	if (getenv("LANGUAGE"))
		return getenv("LANGUAGE");
	else
		/* Setting to en will always work in every windows. */
		return "en";
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static char findname[MAX_OSPATH];
static WCHAR wfindpath[MAX_OSPATH];
static int findhandle;

static bool CompareAttributes (unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
		return false;
	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
		return false;
	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
		return false;
	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
		return false;
	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
		return false;

	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
		return false;
	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
		return false;
	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
		return false;
	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
		return false;
	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
		return false;

	return true;
}

/**
 * @sa Sys_FindNext
 * @sa Sys_FindClose
 */
char* Sys_FindFirst (const char* path, unsigned musthave, unsigned canthave)
{
	struct _wfinddata_t findinfo;

	if (findhandle)
		Sys_Error("Sys_BeginFind without close");
	findhandle = 0;

	Com_FilePath(path, findbase, sizeof(findbase));
	Sys_Utf8ToUtf16(path, wfindpath, lengthof(wfindpath));
	findhandle = _wfindfirst(wfindpath, &findinfo);
	while (findhandle != -1) {
		/* found one that matched */
		Sys_Utf16ToUtf8(findinfo.name, findname, sizeof(findname));
		if (!Q_streq(findname, ".") && !Q_streq(findname, "..") &&
			CompareAttributes(findinfo.attrib, musthave, canthave)) {
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findname);
			return findpath;
		/* doesn't match - try the next one */
		} else if (_wfindnext(findhandle, &findinfo) == -1) {
			/* ok, no further entries here - leave the while loop */
			_findclose(findhandle);
			findhandle = -1;
		}
	}

	/* none found */
	return nullptr;
}

/**
 * @sa Sys_FindFirst
 * @sa Sys_FindClose
 */
char* Sys_FindNext (unsigned musthave, unsigned canthave)
{
	struct _wfinddata_t findinfo;

	if (findhandle == -1)
		return nullptr;

	/* until we found the next entry */
	while (_wfindnext(findhandle, &findinfo) != -1) {
		Sys_Utf16ToUtf8(findinfo.name, findname, sizeof(findname));
		if (!Q_streq(findname, ".") && !Q_streq(findname, "..") &&
			CompareAttributes(findinfo.attrib, musthave, canthave)) {
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findname);
			return findpath;
		}
	}

	/* none found */
	return nullptr;
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

void Sys_ListFilteredFiles (const char* basedir, const char* subdirs, const char* filter, linkedList_t** list)
{
	char search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char filename[MAX_OSPATH];
	int findhandle;
	struct _wfinddata_t findinfo;

	if (subdirs[0] != '\0') {
		Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
	} else {
		Com_sprintf(search, sizeof(search), "%s\\*", basedir);
	}

	Sys_Utf8ToUtf16(search, wfindpath, lengthof(wfindpath));
	findhandle = _wfindfirst(wfindpath, &findinfo);
	if (findhandle == -1)
		return;

	do {
		Sys_Utf16ToUtf8(findinfo.name, findname, sizeof(findname));
		if (findinfo.attrib & _A_SUBDIR) {
			if (Q_strcasecmp(findname, ".") && Q_strcasecmp(findname, "..")) {
				if (subdirs[0] != '\0') {
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findname);
				} else {
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", findname);
				}
				Sys_ListFilteredFiles(basedir, newsubdirs, filter, list);
			}
		}
		Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, findname);
		if (!Com_Filter(filter, filename))
			continue;
		LIST_AddString(list, filename);
	} while (_wfindnext(findhandle, &findinfo) != -1);

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
/**
 * @brief Coloring Windows cli output
 * @param[in] text <display this text.>
 * @param[in] fgColor <bitmask for foreground color. FOREGROUND_BLUE, FOREGROUND_GREEN, FOREGROUND_RED, FOREGROUND_INTENSITY and combinations (yellow = green | red).>
 * @param[in] toStdOut <true write to stdout. false == stderr.>
 */
static void Sys_ColoredOutput (const char* text, unsigned int fgColor, bool toStdOut)
{
	/* paint this colors (gray on black). */
	unsigned int cliPaintColor = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
	/* store default current colors. */
	unsigned int cliCurColor = cliPaintColor;
	CONSOLE_SCREEN_BUFFER_INFO cliInfo;
	DWORD handle = toStdOut ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
	HANDLE console = GetStdHandle(handle);

	if (GetConsoleScreenBufferInfo(console, &cliInfo)) {
		/* store current colors. */
		cliCurColor = cliInfo.wAttributes;
		/* is BACKGROUND == FOREGROUND */
		if (cliCurColor == (fgColor << 4)) {
			/* your fgColor on black background. */
			cliPaintColor = fgColor;
		} else {
			/* remove foreground color. */
			cliPaintColor = cliCurColor >> 4;
			cliPaintColor <<= 4;
			/* set foreground color. */
			cliPaintColor |= fgColor;
		}
	}

	SetConsoleTextAttribute(console, cliPaintColor);
	if (toStdOut)
		fprintf(stdout, "%s\n", text);
	else
		fprintf(stderr, "Error: %s\n", text);
	SetConsoleTextAttribute(console, cliCurColor);

	CloseHandle(console);
}

void Sys_Error (const char* error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	/* red text to stderr. */
	Sys_ColoredOutput(text, FOREGROUND_RED | FOREGROUND_INTENSITY, false);

	ExitProcess(1);
}
#endif

/**
 * @brief Get current user
 */
const char* Sys_GetCurrentUser (void)
{
	static char s_userName[1024];
	WCHAR w_userName[1024];
	unsigned long size = lengthof(w_userName);

	if (!GetUserNameW(w_userName, &size))
		Q_strncpyz(s_userName, "", sizeof(s_userName));
	else
		Sys_Utf16ToUtf8(w_userName, s_userName, sizeof(s_userName));

	return s_userName;
}

/**
 * @brief Get current working dir
 */
char* Sys_Cwd (void)
{
	static char cwd[MAX_OSPATH];
	WCHAR wcwd[MAX_OSPATH];

	if (_wgetcwd(wcwd, lengthof(wcwd)) == nullptr)
		return nullptr;
	else
		Sys_Utf16ToUtf8(wcwd, cwd, sizeof(cwd));

	return cwd;
}

/**
 * @brief Normalize path (remove all \\ )
 */
void Sys_NormPath (char* path)
{
	char* tmp = path;

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
char* Sys_GetHomeDirectory (void)
{
	static char path[MAX_PATH];
	HMODULE shfolder;

	shfolder = LoadLibrary("shfolder.dll");

	if (shfolder == nullptr) {
		Com_Printf("Unable to load SHFolder.dll\n");
		return nullptr;
	}

	typedef HRESULT WINAPI SHGetFolderPath_t(HWND, int, HANDLE, DWORD, LPWSTR);
	SHGetFolderPath_t* const qSHGetFolderPath = (SHGetFolderPath_t*)GetProcAddress(shfolder, "SHGetFolderPathW");
	if (qSHGetFolderPath == nullptr) {
		Com_Printf("Unable to find SHGetFolderPath in SHFolder.dll\n");
		FreeLibrary(shfolder);
		return nullptr;
	}

	WCHAR wpath[MAX_PATH];
	if (!SUCCEEDED(qSHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, wpath))) {
		Com_Printf("Unable to detect CSIDL_APPDATA\n");
		FreeLibrary(shfolder);
		return nullptr;
	}

	Sys_Utf16ToUtf8(wpath, path, sizeof(path));
	Q_strcat(path, sizeof(path), "\\UFOAI");
	FreeLibrary(shfolder);

	Sys_Utf8ToUtf16(path, wpath, MAX_PATH);
	if (!CreateDirectoryW(wpath, nullptr)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			Com_Printf("Unable to create directory \"%s\"\n", path);
			return nullptr;
		}
	}

	Sys_Utf16ToUtf8(wpath, path, sizeof(path));
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
 * @return nonzero if succeeds
 * @note gettext FAQ recommends using both functions to set environment variable(s), so we do
 */
int Sys_Setenv (const char* name, const char* value)
{
	const size_t n = strlen(name) + strlen(value) + 2;
	char* str = (char*)malloc(n); /* do not convert this to allocation from managed pool, using malloc is intentional */

	strcat(strcat(strcpy(str, name), "="), value);
	if (putenv(str))
		return 0;  /* putenv returns nonzero if fails */

	return SetEnvironmentVariable(name, value);
}

void Sys_InitSignals (void)
{
}

void Sys_Mkfifo (const char* ospath, qFILE* f)
{
}

void Sys_OpenURL (const char* url)
{
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

FILE* Sys_Fopen(const char *filename, const char *mode)
{
	WCHAR wname[MAX_OSPATH];
	WCHAR wmode[MAX_VAR];
	Sys_Utf8ToUtf16(filename, wname, lengthof(wname));
	Sys_Utf8ToUtf16(mode, wmode, MAX_VAR);
	return _wfopen(wname, wmode);
}
