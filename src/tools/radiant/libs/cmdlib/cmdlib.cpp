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

#define OUTPUTBUFSIZE 8192

#if defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)

#include <unistd.h>

char *Q_Exec (const char *cmd, const char *cmdline, const char *, bool)
{
	FILE *pipe;
	char *cbuff;
	char fullcmd[1024];
	char temp[128];
	int read = 0;

	snprintf(fullcmd, sizeof(fullcmd) - 1, "%s %s", cmd, cmdline);
	fullcmd[sizeof(fullcmd) - 1] = '\0';

	pipe = popen(fullcmd, "r");
	if (!pipe)
		return strdup("Could not open pipe\n");

	cbuff = (char *)malloc(OUTPUTBUFSIZE + 1);
	if (!cbuff) {
		pclose(pipe);
		return strdup("Could not alocate memory\n");
	}

	cbuff[0] = '\0';

	while (fgets(temp, sizeof(temp), pipe)) {
		strncat(cbuff, temp, OUTPUTBUFSIZE - 1);
		read++;
	}

	pclose(pipe);
	if (!read) {
		free(cbuff);
		cbuff = NULL;
	}
	return cbuff;
}

#elif defined(WIN32)

#include <windows.h>

// NOTE TTimo windows is VERY nitpicky about the syntax in CreateProcess
char *Q_Exec (const char *cmd, const char *cmdline, const char *execdir, bool bCreateConsole)
{
	PROCESS_INFORMATION ProcessInformation;
	STARTUPINFO startupinfo = {0};
	DWORD dwCreationFlags;
	SECURITY_ATTRIBUTES sattr;
	HANDLE readfh;
	char *cbuff;
    char cmdlineBuf[1024];

    strncpy(cmdlineBuf, cmdline, sizeof(cmdlineBuf) - 1);
    cmdlineBuf[sizeof(cmdlineBuf) - 1] = '\0';

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
	pCmdline = cmdlineBuf;
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
