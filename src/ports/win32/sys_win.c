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
/* sys_win.h */

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

#if defined DEBUG && defined _MSC_VER
#include <intrin.h>
#endif

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

qboolean s_win95;

int			starttime;
qboolean	ActiveApp;
qboolean	Minimized;

static HANDLE		hinput, houtput;

unsigned	sys_msg_time;
unsigned	sys_frame_time;


static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];


/*
===============================================================================

SYSTEM IO

===============================================================================
*/


void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

#if defined DEBUG && defined _MSC_VER
	__debugbreak();	/* break execution before game shutdown */
#endif

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

	/* shut down QHOST hooks if necessary */
	DeinitConProc ();

	exit (1);
}

void Sys_Quit (void)
{
	timeEndPeriod( 1 );

	CL_Shutdown();
	Qcommon_Shutdown ();
	CloseHandle (qwclsemaphore);
	if (dedicated && dedicated->value)
		FreeConsole ();

	/* shut down QHOST hooks if necessary */
	DeinitConProc ();

	exit (0);
}


void WinError (void)
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
	MessageBox( NULL, lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION );

	/* Free the buffer. */
	LocalFree( lpMsgBuf );
}

char *Sys_GetCurrentUser( void )
{
	static char s_userName[1024];
	unsigned long size = sizeof( s_userName );

	if ( !GetUserName( s_userName, &size ) )
		Q_strncpyz( s_userName, "player", sizeof(s_userName) );

	if ( !s_userName[0] )
		Q_strncpyz( s_userName, "player", sizeof(s_userName) );

	return s_userName;
}

/*
==============
Sys_Cwd
==============
*/
char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	_getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

void Sys_NormPath(char* path)
{
	char *tmp = path;

	while ( *tmp ) {
		if ( *tmp == '\\' )
			*tmp = '/';
		else
			*tmp = tolower(*tmp);
		tmp++;
	}
}

/*
=================
Sys_GetHomeDirectory
=================
*/
char *Sys_GetHomeDirectory (void)
{
	TCHAR szPath[MAX_PATH];
	static char path[MAX_OSPATH];

	if(!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
		return NULL;

	Q_strncpyz(path, szPath, sizeof(path));
	Q_strcat(path, "\\UFOAI", sizeof(path));
	if (!CreateDirectory(path, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			Com_Printf("Unable to create directory \"%s\"\n", path);
			return NULL;
		}
	}
	return path;
}

/*
================
Sys_Init
================
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
		Sys_Error ("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

    qwclsemaphore = CreateSemaphore(
        NULL,         /* Security attributes */
        0,            /* Initial count       */
        1,            /* Maximum count       */
        "qwcl"); /* Semaphore name      */
#endif

	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4) /* at least win nt 4 */
		Sys_Error ("UFO: AI requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s) /* win3.x with win32 extensions */
		Sys_Error ("UFO: AI doesn't run on Win32s");
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ) /* win95, 98, me */
		s_win95 = qtrue;

	Cvar_Get("sys_os", "win", 0);

	if (dedicated->value) {
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

		/* let QHOST hook in */
		InitConProc (argc, argv);
	}
}


static char	console_text[256];
static int	console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	int		ch;
    DWORD	numread, numevents, dummy;

	if (!dedicated || !dedicated->value)
		return NULL;


	for ( ;; ) {
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

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


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	DWORD	dummy;
	char	text[256];

	if (!dedicated || !dedicated->value)
		return;

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
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
	MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)) {
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();
		sys_msg_time = msg.time;
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	/* grab frame time */
	sys_frame_time = timeGetTime();	/* FIXME: should this be at start? */
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 ) {
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 ) {
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != 0 ) {
				data = malloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
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

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
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
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	/* run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath (path);
		if (!path)
			break;		/* couldn't find one anywhere */
		Com_sprintf (name, sizeof(name), "%s/game.dll", path);
		game_library = LoadLibrary (name);
		if (game_library) {
			Com_DPrintf ("LoadLibrary (%s)\n",name);
			break;
		} else {
			Com_DPrintf ("LoadLibrary (%s) failed\n",name);
		}
	}

	/* check the current debug directory for development purposes */
	if (!game_library) {
		_getcwd (cwd, sizeof(cwd));
		Com_sprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
		game_library = LoadLibrary ( name );
		if (game_library)
			Com_DPrintf ("LoadLibrary (%s)\n", name);
#ifdef DEBUG
		else {
			Com_DPrintf ("LoadLibrary (%s) failed\n",name);
			/* check the current directory for other development purposes */
			Com_sprintf (name, sizeof(name), "%s/%s", cwd, gamename);
			game_library = LoadLibrary ( name );
			if (game_library)
				Com_DPrintf ("LoadLibrary (%s)\n", name);
			else
				Com_DPrintf ("LoadLibrary (%s) failed\n",name);
		}
#endif
	}

	if (!game_library) {
		Com_Printf("Could not find any valid game lib\n");
		return NULL;
	}

	GetGameAPI = (GetGameApi_t)GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI) {
			Sys_UnloadGame ();
		Com_Printf("Could not load game lib '%s'\n", name);
		return NULL;
	}

	return GetGameAPI (parms);
}

/*======================================================================= */


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
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

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG			msg;
	int			time, oldtime, newtime;
	float		timescale;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	Qcommon_Init (argc, argv);
	timescale = 1.0;
	oldtime = Sys_Milliseconds ();

	srand( oldtime );

	/* main window message loop */
	while (1) {
		/* if at a full screen console, don't update unless needed */
		if (Minimized || (dedicated && dedicated->value) )
			Sleep (1);

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!GetMessage (&msg, NULL, 0, 0))
				Com_Quit ();
			sys_msg_time = msg.time;
			TranslateMessage (&msg);
   			DispatchMessage (&msg);
		}

		do {
			newtime = Sys_Milliseconds ();
			time = timescale * (newtime - oldtime);
		} while (time < 1);

		_controlfp( _PC_24, _MCW_PC );

		timescale = Qcommon_Frame (time);
		oldtime = newtime;
	}

	/* never gets here */
	return TRUE;
}
