/**
 * @file sys_win.c
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

#include "../../qcommon/qcommon.h"
#include "../../game/game.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <float.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include "conproc.h"
#include <process.h>

#ifdef _MSC_VER
#include <dbghelp.h>
#endif

#if defined DEBUG && defined _MSC_VER
#include <intrin.h>
#endif

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

HMODULE hSh32 = NULL;
FARPROC procShell_NotifyIcon = NULL;
NOTIFYICONDATA pNdata;

qboolean s_win95;

int			starttime;
qboolean	ActiveApp;
qboolean	Minimized;
static qboolean	oldconsole = qfalse;

static HANDLE		hinput, houtput;

uint32_t	sys_msg_time;
uint32_t	sys_frame_time;

/* Main window handle for life of the client */
HWND        cl_hwnd;
/* Main server console handle for life of the server */
HWND		hwnd_Server;

int consoleBufferPointer = 0;
byte consoleFullBuffer[16384];

sizebuf_t	console_buffer;
byte console_buff[8192];

static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];

int SV_CountPlayers (void);

/*
===============================================================================
SYSTEM IO
===============================================================================
*/

/**
 * @brief
 * @sa Sys_DisableTray
 */
void Sys_EnableTray (void)
{
	memset (&pNdata, 0, sizeof(pNdata));

	pNdata.cbSize = sizeof(NOTIFYICONDATA);
#ifdef DEDICATED_ONLY
	pNdata.hWnd = hwnd_Server;
#else
	pNdata.hWnd = cl_hwnd;
#endif
	pNdata.uID = 0;
	pNdata.uCallbackMessage = WM_USER + 4;
	GetWindowText (pNdata.hWnd, pNdata.szTip, sizeof(pNdata.szTip)-1);
	pNdata.hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE(IDI_ICON2));
	pNdata.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

	hSh32 = LoadLibrary ("shell32.dll");
	procShell_NotifyIcon = GetProcAddress (hSh32, "Shell_NotifyIcon");

	procShell_NotifyIcon (NIM_ADD, &pNdata);

	Com_Printf("Minimize to tray enabled.\n");
}

/**
 * @brief
 * @sa Sys_EnableTray
 */
void Sys_DisableTray (void)
{
#ifdef DEDICATED_ONLY
	ShowWindow (hwnd_Server, SW_RESTORE);
#else
	ShowWindow (cl_hwnd, SW_RESTORE);
#endif
	procShell_NotifyIcon (NIM_DELETE, &pNdata);

	if (hSh32)
		FreeLibrary (hSh32);

	procShell_NotifyIcon = NULL;

	Com_Printf("Minimize to tray disabled.\n");
}

/**
 * @brief
 */
void Sys_Minimize (void)
{
#ifdef DEDICATED_ONLY
	SendMessage (hwnd_Server, WM_ACTIVATE, MAKELONG(WA_INACTIVE,1), 0);
#else
	SendMessage (cl_hwnd, WM_ACTIVATE, MAKELONG(WA_INACTIVE,1), 0);
#endif
}

/**
 * @brief
 */
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

#if defined DEBUG && defined _MSC_VER
	__debugbreak();	/* break execution before game shutdown */
#endif

	CL_Shutdown();
	Qcommon_Shutdown();

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	text[sizeof(text)-1] = 0;

	MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	if (qwclsemaphore)
		CloseHandle(qwclsemaphore);

	/* shut down QHOST hooks if necessary */
	DeinitConProc();

	exit (1);
}

/**
 * @brief
 */
void Sys_Quit (void)
{
	timeEndPeriod(1);

	CL_Shutdown();
	Qcommon_Shutdown();
	CloseHandle(qwclsemaphore);
	if (dedicated && dedicated->value && oldconsole)
		FreeConsole();

	if (procShell_NotifyIcon)
		procShell_NotifyIcon(NIM_DELETE, &pNdata);

	/* shut down QHOST hooks if necessary */
	DeinitConProc();

	exit(0);
}


/**
 * @brief
 */
static void WinError (void)
{
	LPVOID lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);

	/* Display the string. */
	MessageBox(NULL, (char*)lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION);

	/* Free the buffer. */
	LocalFree(lpMsgBuf);
}

/**
 * @brief
 */
static void ServerWindowProcCommandExecute (void)
{
	int			ret;
	char		buff[1024];

	*(DWORD *)&buff = sizeof(buff)-2;

	ret = (int)SendDlgItemMessage(hwnd_Server, IDC_COMMAND, EM_GETLINE, 1, (LPARAM)buff);
	if (!ret)
		return;

	buff[ret] = '\n';
	buff[ret+1] = '\0';
	Sys_ConsoleOutput(buff);
	Cbuf_AddText(buff);
	SendDlgItemMessage(hwnd_Server, IDC_COMMAND, WM_SETTEXT, 0, (LPARAM)"");
}

/**
 * @brief
 */
static LRESULT ServerWindowProcCommand (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT idItem = LOWORD(wParam);
	UINT wNotifyCode = HIWORD(wParam);

	switch (idItem) {
	case IDOK:
		switch (wNotifyCode) {
		case BN_CLICKED:
			ServerWindowProcCommandExecute();
			break;
		}
	}
	return FALSE;
}

/**
 * @brief
 */
static LRESULT CALLBACK ServerWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_COMMAND:
		return ServerWindowProcCommand(hwnd, message, wParam, lParam);
	case WM_ENDSESSION:
		Cbuf_AddText ("quit exiting due to Windows shutdown.\n");
		return TRUE;
	case WM_CLOSE:
		if (SV_CountPlayers()) {
			int ays = MessageBox (hwnd_Server, "There are still players on the server! Really shut it down?", "WARNING!", MB_YESNO + MB_ICONEXCLAMATION);
			if (ays == IDNO)
				return TRUE;
		}
		Cbuf_AddText ("quit terminated by local request.\n");
		return FALSE;
	case WM_CREATE:
		SetTimer(hwnd_Server, 1, 1000, NULL);
		break;
	case WM_ACTIVATE:
		{
			int minimized = (BOOL)HIWORD(wParam);

			if (Minimized && !minimized) {
				int len;
				SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)consoleFullBuffer);
				len = (int)SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0);
				SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_LINESCROLL, 0, len);
			}

			Minimized = minimized;
			if (procShell_NotifyIcon) {
				if (minimized && LOWORD(wParam) == WA_INACTIVE) {
					Minimized = qtrue;
					ShowWindow (hwnd_Server, SW_HIDE);
					return FALSE;
				}
			}
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	case WM_USER + 4:
		if (lParam == WM_LBUTTONDBLCLK) {
			ShowWindow(hwnd_Server, SW_RESTORE);
			SetForegroundWindow(hwnd_Server);
			SetFocus(GetDlgItem(hwnd_Server, IDC_COMMAND));
		}
		return FALSE;
	}

	return FALSE;
}

/**
 * @brief Get current user
 */
char *Sys_GetCurrentUser (void)
{
	static char s_userName[1024];
	unsigned long size = sizeof( s_userName );

	if (!GetUserName(s_userName, &size))
		Q_strncpyz(s_userName, "player", sizeof(s_userName));

	if (!s_userName[0])
		Q_strncpyz(s_userName, "player", sizeof(s_userName));

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
 * @note Currently disabled
 */
char *Sys_GetHomeDirectory (void)
{
#if 0
	TCHAR szPath[MAX_PATH];
	static char path[MAX_OSPATH];
	FARPROC qSHGetFolderPath;
	HMODULE shfolder = LoadLibrary("shfolder.dll");

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

	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
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
#else
	return NULL;
#endif
}

/**
 * @brief
 */
int Sys_FileLength (const char *path)
{
	WIN32_FILE_ATTRIBUTE_DATA	fileData;

	if (GetFileAttributesEx (path, GetFileExInfoStandard, &fileData))
		return fileData.nFileSizeLow;
	else
		return -1;
}

/**
 * @brief
 */
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

#if 0
	/* allocate a named semaphore on the client so the */
	/* front end can tell if it is alive */

	/* mutex will fail if semephore already exists */
	qwclsemaphore = CreateMutex(
		NULL,         /* Security attributes */
		0,            /* owner       */
		"qwcl"); /* Semaphore name      */
	if (!qwclsemaphore)
		Sys_Error("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

	qwclsemaphore = CreateSemaphore(
		NULL,         /* Security attributes */
		0,            /* Initial count       */
		1,            /* Maximum count       */
		"qwcl"); /* Semaphore name      */
#endif

	timeBeginPeriod(1);

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4) /* at least win nt 4 */
		Sys_Error("UFO: AI requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s) /* win3.x with win32 extensions */
		Sys_Error("UFO: AI doesn't run on Win32s");
	else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) /* win95, 98, me */
		s_win95 = qtrue;

	Cvar_Get("sys_os", "win", CVAR_SERVERINFO, NULL);

	if (dedicated->value) {
		oldconsole = Cvar_VariableInteger("oldconsole");
		if (oldconsole) {
			if (!AllocConsole()) {
				WinError();
				Sys_Error("Couldn't create dedicated server console");
			}
			hinput = GetStdHandle(STD_INPUT_HANDLE);
			houtput = GetStdHandle(STD_OUTPUT_HANDLE);

			/* let QHOST hook in */
			InitConProc(argc, argv);
		} else {
			HICON hIcon;
			hwnd_Server = CreateDialog(global_hInstance, MAKEINTRESOURCE(IDD_SERVER_GUI), NULL, (DLGPROC)ServerWindowProc);

			if (!hwnd_Server) {
				WinError();
				Sys_Error("Couldn't create dedicated server window. GetLastError() = %d", (int)GetLastError());
			}

			SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_SETREADONLY, TRUE, 0);

			SZ_Init(&console_buffer, console_buff, sizeof(console_buff));
			console_buffer.allowoverflow = qtrue;

			hIcon = (HICON)LoadImage(global_hInstance,
				MAKEINTRESOURCE(IDI_ICON2),
				IMAGE_ICON,
				GetSystemMetrics(SM_CXSMICON),
				GetSystemMetrics(SM_CYSMICON),
				0);

			if(hIcon)
				SendMessage(hwnd_Server, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

			UpdateWindow(hwnd_Server);
			SetForegroundWindow(hwnd_Server);
			SetFocus(GetDlgItem (hwnd_Server, IDC_COMMAND));
		}
	}
}


static char	console_text[256];
static int	console_textlen;

/**
 * @brief
 */
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	int		ch;
    DWORD	numread, numevents, dummy;

	if (!dedicated || !dedicated->value || !oldconsole)
		return NULL;

	for ( ;; ) {
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
			Sys_Error("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error("Error reading console input");

		if (numread != 1)
			Sys_Error("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT) {
			if (!recs[0].Event.KeyEvent.bKeyDown) {
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch) {
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);

						if (console_textlen) {
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}
						break;

					case '\b':
						if (console_textlen) {
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						}
						break;

					default:
						if (ch >= ' ') {
							if (console_textlen < sizeof(console_text)-2) {
								WriteFile(houtput, &ch, 1, &dummy, NULL);
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}
						break;
				}
			}
		}
	}

	return NULL;
}

/**
 * @brief
 */
void Sys_UpdateConsoleBuffer (void)
{
	if (console_buffer.cursize) {
		int len, buflen;

		buflen = console_buffer.cursize + 1024;

		if (consoleBufferPointer + buflen >= sizeof(consoleFullBuffer)) {
			int		moved;
			char	*p = consoleFullBuffer + buflen;
			char	*q;

			while (p[0] && p[0] != '\n')
				p++;
			p++;
			q = (consoleFullBuffer + buflen);
			moved = (buflen + (int)(p - q));
			memmove(consoleFullBuffer, consoleFullBuffer + moved, consoleBufferPointer - moved);
			consoleBufferPointer -= moved;
			consoleFullBuffer[consoleBufferPointer] = '\0';
		}

		memcpy(consoleFullBuffer+consoleBufferPointer, console_buffer.data, console_buffer.cursize);
		consoleBufferPointer += (console_buffer.cursize - 1);

		if (!Minimized) {
			SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)consoleFullBuffer);
			len = (int)SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0);
			SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_LINESCROLL, 0, len);
		}

		SZ_Clear (&console_buffer);
	}
}

/**
 * @brief Print text to the dedicated console
 */
void Sys_ConsoleOutput (char *string)
{
	DWORD	dummy;
	char text[2048];
	const char *p;
	char *s;

	if (!dedicated || !dedicated->value)
		return;

	if (oldconsole) {
		if (console_textlen) {
			text[0] = '\r';
			memset(&text[1], ' ', console_textlen);
			text[console_textlen+1] = '\r';
			text[console_textlen+2] = 0;
			WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
		}

		WriteFile(houtput, string, strlen(string), &dummy, NULL);

		if (console_textlen)
			WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
	} else {
		p = string;
		s = text;

		while (p[0]) {
			if (p[0] == '\n') {
				*s++ = '\r';
			}

			/* r1: strip high bits here */
			*s = (p[0]) & SCHAR_MAX;

			if (s[0] >= 32 || s[0] == '\n' || s[0] == '\t')
				s++;

			p++;

			if ((s - text) >= sizeof(text) - 2) {
				*s++ = '\n';
				break;
			}
		}
		s[0] = '\0';

		SZ_Print(&console_buffer, text);

		Sys_UpdateConsoleBuffer();
	}
}


/**
 * @brief Send Key_Event calls
 */
void Sys_SendKeyEvents (void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
		if (!GetMessage(&msg, NULL, 0, 0))
			Sys_Quit();
		sys_msg_time = msg.time;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* grab frame time */
	sys_frame_time = timeGetTime();	/* FIXME: should this be at start? */
}


/**
 * @brief
 */
char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard(NULL) != 0) {
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData(CF_TEXT)) != 0 ) {
			if ((cliptext = (char*)GlobalLock(hClipboardData)) != 0) {
				data = (char*)malloc( GlobalSize(hClipboardData) + 1);
				strcpy(data, cliptext);
				GlobalUnlock(hClipboardData);
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================
WINDOWS CRAP
==============================================================================
*/

/**
 * @brief
 */
void Sys_AppActivate (void)
{
#ifndef DEDICATED_ONLY
	ShowWindow(cl_hwnd, SW_RESTORE);
	SetForegroundWindow(cl_hwnd);
#endif
}

/*
========================================================================
GAME DLL
========================================================================
*/

static HINSTANCE	game_library;

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
	GetGameApi_t GetGameAPI;
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];

#if defined _M_IX86

#ifndef DEBUG
	const char *gamename = "game32.dll";
	const char *debugdir = "src/release/bin/win32";
#else
	const char *gamename = "game32d.dll";
	const char *debugdir = "src/debug/bin/win32";
#endif

#elif defined _M_X64

#ifndef DEBUG
	const char *gamename = "game64.dll";
	const char *debugdir = "src/release/bin/x64";
#else
	const char *gamename = "game64d.dll";
	const char *debugdir = "src/debug/bin/x64";
#endif

#elif defined _M_ALPHA

#error Update DEC ALPHA platform project configuration

#ifndef DEBUG
	const char *gamename = "game???.dll";
	const char *debugdir = "src/release/bin/alpha";
#else
	const char *gamename = "game???d.dll";
	const char *debugdir = "src/debug/bin/alpha";
#endif

#else

#error Unsupported platform!

#endif

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	/* run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath(path);
		if (!path)
			break;		/* couldn't find one anywhere */
		Com_sprintf(name, sizeof(name), "%s/game.dll", path);
		game_library = LoadLibrary(name);
		if (game_library) {
			Com_DPrintf("LoadLibrary (%s)\n",name);
			break;
		} else {
			Com_DPrintf("LoadLibrary (%s) failed\n",name);
		}
	}

	/* check the current debug directory for development purposes */
	if (!game_library) {
		_getcwd (cwd, sizeof(cwd));
		Com_sprintf(name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
		game_library = LoadLibrary(name);
		if (game_library)
			Com_DPrintf("LoadLibrary (%s)\n", name);
#ifdef DEBUG
		else {
			Com_DPrintf("LoadLibrary (%s) failed\n",name);
			/* check the current directory for other development purposes */
			Com_sprintf(name, sizeof(name), "%s/%s", cwd, gamename);
			game_library = LoadLibrary(name);
			if (game_library)
				Com_DPrintf("LoadLibrary (%s)\n", name);
			else
				Com_DPrintf("LoadLibrary (%s) failed\n",name);
		}
#endif
	}

	if (!game_library) {
		Com_Printf("Could not find any valid game lib\n");
		return NULL;
	}

	GetGameAPI = (GetGameApi_t)GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame();
		Com_Printf("Could not load game lib '%s'\n", name);
		return NULL;
	}

	return GetGameAPI (parms);
}


/**
 * @brief
 */
static void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS)) {
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine) {
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine) {
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

}

HINSTANCE	global_hInstance;

/**
 * @brief
 */
static void FixWorkingDirectory (void)
{
	char	*p;
	char	curDir[MAX_PATH];

	GetModuleFileName(NULL, curDir, sizeof(curDir)-1);

	p = strrchr(curDir, '\\');
	p[0] = 0;

	if (strlen(curDir) > (MAX_OSPATH - MAX_QPATH))
		Sys_Error("Current path is too long. Please move your UFO:AI installation to a shorter path.");

	SetCurrentDirectory(curDir);
}

#if 0
/**
 * @brief
 */
static DWORD UFOAIExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo)
{
	HMODULE	hDbgHelp, hVersion;

	MINIDUMP_EXCEPTION_INFORMATION miniInfo;
	STACKFRAME64	frame = {0};
	CONTEXT			context = *exceptionInfo->ContextRecord;
	SYMBOL_INFO		*symInfo;
	DWORD64			fnOffset;
	CHAR			tempPath[MAX_PATH];
	CHAR			dumpPath[MAX_PATH];
	OSVERSIONINFOEX	osInfo;
	SYSTEMTIME		timeInfo;

	ENUMERATELOADEDMODULES64	fnEnumerateLoadedModules64;
	SYMSETOPTIONS				fnSymSetOptions;
	SYMINITIALIZE				fnSymInitialize;
	STACKWALK64					fnStackWalk64;
	SYMFUNCTIONTABLEACCESS64	fnSymFunctionTableAccess64;
	SYMGETMODULEBASE64			fnSymGetModuleBase64;
	SYMFROMADDR					fnSymFromAddr;
	SYMCLEANUP					fnSymCleanup;
	MINIDUMPWRITEDUMP			fnMiniDumpWriteDump;

	DWORD						ret, i;
	DWORD64						InstructionPtr;

	BOOL						wantUpload = TRUE;

	CHAR						searchPath[MAX_PATH], *p;

	if (win_disableexceptionhandler->intvalue == 2)
		return EXCEPTION_EXECUTE_HANDLER;
	else if (win_disableexceptionhandler->intvalue)
		return EXCEPTION_CONTINUE_SEARCH;

#ifndef DEDICATED_ONLY
	ShowCursor (TRUE);
	if (cl_hwnd)
		DestroyWindow (cl_hwnd);
#else
	if (cl_hwnd)
		EnableWindow (cl_hwnd, FALSE);
#endif

#ifdef DEBUG
	ret = MessageBox (NULL, "EXCEPTION_CONTINUE_SEARCH?", "Unhandled Exception", MB_ICONERROR | MB_YESNO);
	if (ret == IDYES)
		return EXCEPTION_CONTINUE_SEARCH;
#endif

#ifndef DEBUG
	if (IsDebuggerPresent ())
		return EXCEPTION_CONTINUE_SEARCH;
#endif

	hDbgHelp = LoadLibrary ("DBGHELP");
	hVersion = LoadLibrary ("VERSION");

	if (!hDbgHelp) {
		if (!win_silentexceptionhandler->intvalue)
			MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since R1Q2 failed to load DBGHELP.DLL. Please obtain DBGHELP.DLL and place it in your R1Q2 directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (hVersion) {
		fnVerQueryValue = (VERQUERYVALUE)GetProcAddress (hVersion, "VerQueryValueA");
		fnGetFileVersionInfo = (GETFILEVERSIONINFO)GetProcAddress (hVersion, "GetFileVersionInfoA");
		fnGetFileVersionInfoSize = (GETFILEVERSIONINFOSIZE)GetProcAddress (hVersion, "GetFileVersionInfoSizeA");
	}

	fnEnumerateLoadedModules64 = (ENUMERATELOADEDMODULES64)GetProcAddress (hDbgHelp, "EnumerateLoadedModules64");
	fnSymSetOptions = (SYMSETOPTIONS)GetProcAddress (hDbgHelp, "SymSetOptions");
	fnSymInitialize = (SYMINITIALIZE)GetProcAddress (hDbgHelp, "SymInitialize");
	fnSymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64)GetProcAddress (hDbgHelp, "SymFunctionTableAccess64");
	fnSymGetModuleBase64 = (SYMGETMODULEBASE64)GetProcAddress (hDbgHelp, "SymGetModuleBase64");
	fnStackWalk64 = (STACKWALK64)GetProcAddress (hDbgHelp, "StackWalk64");
	fnSymFromAddr = (SYMFROMADDR)GetProcAddress (hDbgHelp, "SymFromAddr");
	fnSymCleanup = (SYMCLEANUP)GetProcAddress (hDbgHelp, "SymCleanup");
	fnSymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress (hDbgHelp, "SymGetModuleInfo64");
	fnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress (hDbgHelp, "MiniDumpWriteDump");

	if (!fnEnumerateLoadedModules64 || !fnSymSetOptions || !fnSymInitialize || !fnSymFunctionTableAccess64 ||
		!fnSymGetModuleBase64 || !fnStackWalk64 || !fnSymFromAddr || !fnSymCleanup || !fnSymGetModuleInfo64)) {
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since the version of DBGHELP.DLL in use is too old. Please obtain an up-to-date DBGHELP.DLL and place it in your R1Q2 directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (!win_silentexceptionhandler->intvalue)
		ret = MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. Would you like to generate a crash report?", "Unhandled Exception", MB_ICONEXCLAMATION | MB_YESNO);
	else
		ret = IDYES;

	if (ret == IDNO) {
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	hProcess = GetCurrentProcess();

	fnSymSetOptions (SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_ANYTHING);

	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p) p[0] = 0;

	GetSystemTime (&timeInfo);

	i = 1;

	for (;;) {
		snprintf (tempPath, sizeof(tempPath)-1, "%s\\UFOCrashLog%.4d-%.2d-%.2d%_%d.txt", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
		if (Sys_FileLength (tempPath) == -1)
			break;
		i++;
	}

	fhReport = fopen (tempPath, "wb");

	if (!fhReport) {
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	fnSymInitialize (hProcess, searchPath, TRUE);

#ifdef DEBUG
	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p) p[0] = 0;
#endif


#ifdef _M_AMD64
	InstructionPtr = context.Rip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrPC.Offset = context.Rsp;
#else
	InstructionPtr = context.Eip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif

	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	symInfo = LocalAlloc (LPTR, sizeof(*symInfo) + 128);
	symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	symInfo->MaxNameLen = 128;
	fnOffset = 0;

	memset (&osInfo, 0, sizeof(osInfo));
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx ((OSVERSIONINFO *)&osInfo)) {
		osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx ((OSVERSIONINFO *)&osInfo);
	}

	strcpy (szModuleName, "<unknown>");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)InstructionPtr);

	strlwr (szModuleName);

	fprintf (fhReport,
		PRODUCTNAME " encountered an unhandled exception and has terminated. If you are able to\r\n"
		"reproduce this crash, please submit the crash report on the UFO:AI forums at\r\n"
		"include this .txt file and the .dmp file (if available)\r\n"
		"\r\n"
		"This crash appears to have occured in the '%s' module.\r\n\r\n", szModuleName);

	fprintf (fhReport, "**** UNHANDLED EXCEPTION: %x\r\nFault address: %I64p (%s)\r\n", exceptionCode, InstructionPtr, szModuleName);

	fprintf (fhReport, PRODUCTNAME " module: %s(%s) (Version: %s)\r\n", binary_name, R1BINARY, R1Q2_VERSION_STRING);
	fprintf (fhReport, "Windows version: %d.%d (Build %d) %s\r\n\r\n", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.szCSDVersion);

	fprintf (fhReport, "Symbol information:\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcSymInfo, (VOID *)fhReport);

	fprintf (fhReport, "\r\nEnumerate loaded modules:\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcDump, (VOID *)fhReport);

	fprintf (fhReport, "\r\nStack trace:\r\n");
	fprintf (fhReport, "Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n");
	while (fnStackWalk64 (IMAGE_FILE_MACHINE_I386, hProcess, GetCurrentThread(), &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL)) {
		strcpy (szModuleName, "<unknown>");
		fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)(DWORD)frame.AddrPC.Offset);

		p = strrchr (szModuleName, '\\');
		if (p) {
			p++;
		} else {
			p = szModuleName;
		}

		if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT)) {
			fprintf (fhReport, "%I64p %I64p %p %p %p %p %s!%s+0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, symInfo->Name, fnOffset, symInfo->Tag);
		} else {
			fprintf (fhReport, "%I64p %I64p %p %p %p %p %s!0x%I64p\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, frame.AddrPC.Offset);
		}
	}

	if (fnMiniDumpWriteDump) {
		HANDLE	hFile;

		GetTempPath (sizeof(dumpPath)-16, dumpPath);
		strcat (dumpPath, "R1Q2CrashDump.dmp");

		hFile = CreateFile (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			miniInfo.ClientPointers = TRUE;
			miniInfo.ExceptionPointers = exceptionInfo;
			miniInfo.ThreadId = GetCurrentThreadId ();
			if (fnMiniDumpWriteDump (hProcess, GetCurrentProcessId(), hFile, MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithDataSegs, &miniInfo, NULL, NULL)) {
				CHAR	zPath[MAX_PATH];

				CloseHandle (hFile);
				snprintf (zPath, sizeof(zPath)-1, "%s\\UFOCrashLog%.4d-%.2d-%.2d_%d.dmp", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
				CopyFile (dumpPath, zPath, FALSE);
				DeleteFile (dumpPath);
				strcpy (dumpPath, zPath);
				fprintf (fhReport, "\r\nA minidump was saved to %s.\r\nPlease include this file when posting a crash report.\r\n", dumpPath);
			} else {
				CloseHandle (hFile);
				DeleteFile (dumpPath);
			}
		}
	} else  {
		fprintf (fhReport, "\r\nA minidump could not be created. Minidumps are only available on Windows XP or later.\r\n");
	}

	fclose (fhReport);

	LocalFree (symInfo);

	fnSymCleanup (hProcess);

	if (!win_silentexceptionhandler->intvalue) {
		HMODULE shell;
		shell = LoadLibrary ("SHELL32");
		if (shell) {
			SHELLEXECUTEA fncOpen = (SHELLEXECUTEA)GetProcAddress (shell, "ShellExecuteA");
			if (fncOpen)
                fncOpen (NULL, NULL, tempPath, NULL, searchPath, SW_SHOWDEFAULT);

			FreeLibrary (shell);
		}
	}

	FreeLibrary (hDbgHelp);
	if (hVersion)
		FreeLibrary (hVersion);

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

/**
 * @brief
 */
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG			msg;
	int			time, oldtime, newtime;
	float		timescale;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;

	ParseCommandLine(lpCmdLine);

	/* always change to the current working dir */
	FixWorkingDirectory();

#if 0
	__try
	{
#endif
		Qcommon_Init(argc, argv);
		timescale = 1.0;
		oldtime = Sys_Milliseconds();

		srand( oldtime );

		/* main window message loop */
		while (1) {
			/* if at a full screen console, don't update unless needed */
			if (Minimized)
				Sys_Sleep(1);

			while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
				if (!GetMessage (&msg, NULL, 0, 0))
					Com_Quit();
				sys_msg_time = msg.time;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			do {
				newtime = Sys_Milliseconds();
				time = timescale * (newtime - oldtime);
			} while (time < 1);

#ifndef __MINGW32__
			_controlfp(_PC_24, _MCW_PC);
#endif
			timescale = Qcommon_Frame(time);
			oldtime = newtime;
		}
#if 0
	}
	__except (UFOAIExceptionHandler(GetExceptionCode(), GetExceptionInformation()))
	{
		return TRUE;
	}
#endif
	/* never gets here */
	return FALSE;
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
