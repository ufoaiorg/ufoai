/**
 * @file win_main.c
 * @brief Windows system functions
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
#include <mmsystem.h>

qboolean s_win95, s_win2k, s_winxp, s_vista;

#define MAX_NUM_ARGVS 128
static int argc;
static const char *argv[MAX_NUM_ARGVS];

void Sys_Init (void)
{
	OSVERSIONINFO vinfo;
#if 0
	MEMORYSTATUS mem;
#endif

	sys_affinity = Cvar_Get("sys_affinity", "0", CVAR_ARCHIVE, "Which core to use - 0 = all cores, 1 = two cores, 2 = one core");
	sys_priority = Cvar_Get("sys_priority", "0", CVAR_ARCHIVE, "Process priority - 0 = normal, 1 = high, 2 = realtime");

	timeBeginPeriod(1);

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx(&vinfo))
		Sys_Error("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4) /* at least win nt 4 */
		Sys_Error("UFO: AI requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s) /* win3.x with win32 extensions */
		Sys_Error("UFO: AI doesn't run on Win32s");
	else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) /* win95, 98, me */
		s_win95 = qtrue;
	else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) { /* win nt, xp */
		if (vinfo.dwMajorVersion == 5 && vinfo.dwMinorVersion == 0)
			s_win2k = qtrue;
		else if (vinfo.dwMajorVersion == 5)
			s_winxp = qtrue;
		else if (vinfo.dwMajorVersion == 6)
			s_vista = qtrue;
	}

	if (s_win95)
		sys_os = Cvar_Get("sys_os", "win95", CVAR_SERVERINFO, NULL);
	else if (s_win2k)
		sys_os = Cvar_Get("sys_os", "win2K", CVAR_SERVERINFO, NULL);
	else if (s_winxp)
		sys_os = Cvar_Get("sys_os", "winXP", CVAR_SERVERINFO, NULL);
	else if (s_vista)
		sys_os = Cvar_Get("sys_os", "winVista", CVAR_SERVERINFO, NULL);
	else
		sys_os = Cvar_Get("sys_os", "win", CVAR_SERVERINFO, NULL);

#if 0
	GlobalMemoryStatus(&mem);
	Com_Printf("Memory: %u MB\n", mem.dwTotalPhys >> 20);
#endif
}

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

static void FixWorkingDirectory (void)
{
	char *p;
	char curDir[MAX_PATH];

	GetModuleFileName(NULL, curDir, sizeof(curDir)-1);

	p = strrchr(curDir, '\\');
	p[0] = 0;

	if (strlen(curDir) > (MAX_OSPATH - MAX_QPATH))
		Sys_Error("Current path is too long. Please move your installation to a shorter path.");

	SetCurrentDirectory(curDir);
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;

	ParseCommandLine(lpCmdLine);

	Sys_ConsoleInit();

	/* always change to the current working dir */
	FixWorkingDirectory();

	Qcommon_Init(argc, argv);

	/* main window message loop */
	while (1)
		Qcommon_Frame();

	/* never gets here */
	return FALSE;
}
