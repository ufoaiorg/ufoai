/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

//
// start of shared cmdlib stuff
//

#include "cmdlib.h"

#include <string.h>
#include <stdio.h>

#include "string/string.h"
#include "os/path.h"
#include "container/array.h"


#if defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)

#include <unistd.h>

const char *Q_Exec (const char *cmd, char *cmdline, const char *, bool)
{
	char fullcmd[2048];
	char *pCmd;
#ifdef DEBUG
	printf("Q_Exec damnit\n");
#endif
	switch (fork()) {
	case - 1:
		return NULL; /* FIXME: true */
	case 0:
		/* always concat the command on linux */
		if (cmd) {
			strcpy(fullcmd, cmd);
		} else
			fullcmd[0] = '\0';
		if (cmdline) {
			strcat(fullcmd, " ");
			strcat(fullcmd, cmdline);
		}
		pCmd = fullcmd;
		while (*pCmd == ' ')
			pCmd++;
#ifdef DEBUG
		printf("Running system...\n");
		printf("Command: %s\n", pCmd);
#endif
		system(pCmd);
#ifdef DEBUG
		printf("system() returned\n");
#endif
		_exit(0);
		break;
	}
	return NULL;
}

#elif defined(WIN32)

#include <windows.h>

#define OUTPUTBUFSIZE 8192

// NOTE TTimo windows is VERY nitpicky about the syntax in CreateProcess
const char *Q_Exec (const char *cmd, char *cmdline, const char *execdir, bool bCreateConsole)
{
	PROCESS_INFORMATION ProcessInformation;
	STARTUPINFO startupinfo = {0};
	DWORD dwCreationFlags;
	SECURITY_ATTRIBUTES sattr;
	HANDLE readfh;
	char *cbuff;

	// get handles and
	GetStartupInfo(&startupinfo);

	// initialize the security struct - to get the output handle
	sattr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sattr.lpSecurityDescriptor = 0;
	sattr.bInheritHandle = TRUE;

	if (bCreateConsole)
		dwCreationFlags = CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS;
	else
		dwCreationFlags = DETACHED_PROCESS | NORMAL_PRIORITY_CLASS;
	const char *pCmd;
	char *pCmdline;
	pCmd = cmd;
	if (pCmd) {
		while (*pCmd == ' ')
			pCmd++;
	}
	pCmdline = cmdline;
	if (pCmdline) {
		while (*pCmdline == ' ')
			pCmdline++;
	}
	// create a pipe to read the system output from
	if (!CreatePipe(&readfh, &startupinfo.hStdOutput, &sattr, 0))
		return NULL;

	if (CreateProcess(pCmd, pCmdline, NULL, NULL, TRUE, dwCreationFlags,
		NULL, execdir, &startupinfo, &ProcessInformation)) {

		startupinfo.dwFlags = 0;
		cbuff = (char *)malloc(OUTPUTBUFSIZE + 1);

		// capture output
		while (readfh) {
			if (!ReadFile(readfh, cbuff + startupinfo.dwFlags, OUTPUTBUFSIZE - startupinfo.dwFlags, &ProcessInformation.dwProcessId, 0) || !ProcessInformation.dwProcessId) {
				if (GetLastError() != ERROR_BROKEN_PIPE && ProcessInformation.dwProcessId) {
					free(cbuff);
					return NULL;
				}

				// Close the pipe
				CloseHandle(readfh);
				readfh = 0;
			}

			startupinfo.dwFlags += ProcessInformation.dwProcessId;
		}
		return cbuff;
	}
	return NULL;
}

#endif

