/**
 * @file sv_clientstub.c
 * @brief This file can stub out the entire client system for pure dedicated servers
 *
 * @todo Calls to functions not required by the dedicated server should be surrounded by the DEDICATED_ONLY macro.
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

#include "../common/common.h"
#include "../ports/system.h"

void CL_Init (void)
{
}

void CL_InitAfter (void)
{
}

void CL_Drop (void)
{
}

void Con_Print (const char *txt)
{
}

void CL_Shutdown (void)
{
}

void CL_Frame (int now, void *data)
{
}

void CL_SlowFrame (int now, void *data)
{
}

qboolean CL_ParseClientData (const char *type, const char *name, const char **text)
{
	return qtrue;
}

void Cmd_ForwardToServer (void)
{
	const char *cmd;

	cmd = Cmd_Argv(0);
	Com_Printf("Unknown command \"%s\"\n", cmd);
}

void SCR_BeginLoadingPlaque (void)
{
}

void SCR_EndLoadingPlaque (void)
{
}

int CL_Milliseconds (void)
{
	return Sys_Milliseconds();
}

void Key_Init (void)
{
	Cmd_AddCommand("bind", Cmd_Dummy_f, NULL);
}
