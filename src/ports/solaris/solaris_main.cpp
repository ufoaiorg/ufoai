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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/file.h>

#include <dlfcn.h>

#include "../../common/common.h"
#include "../system.h"

/* ======================================================================= */
/* General routines */
/* ======================================================================= */

void Sys_Init (void)
{
	sys_os = Cvar_Get("sys_os", "solaris", CVAR_SERVERINFO, NULL);
	sys_affinity = Cvar_Get("sys_affinity", "0", CVAR_ARCHIVE, NULL);
	sys_priority = Cvar_Get("sys_priority", "0", CVAR_ARCHIVE, "Process nice level");
}

int main (int argc, const char **argv)
{
	Sys_ConsoleInit();
	Qcommon_Init(argc, argv);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	while (1)
		Qcommon_Frame();
}
