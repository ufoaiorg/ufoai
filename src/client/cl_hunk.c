/**
 * @file cl_hunk.c
 * @brief Implement the client hunk memory management
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"

static cvar_t* cl_hunkmegs;


/** @brief don't allow external access to this hunk */
static byte *clHunkData = NULL;
static byte *clHunkPointerPos;

/**
 * @brief Inits some client hunk mem to store parsed data
 * @sa CL_ClientHunkShutdown
 * @sa CL_ClientHunkUse
 * @sa CL_Init
 * @note Don't free this before you shutdown the game
 */
extern void CL_ClientHunkInit (void)
{
	cl_hunkmegs = Cvar_Get("cl_hunkmegs", "1", 0, "Hunk megabytes for client parsed static data");

	clHunkData = malloc(cl_hunkmegs->integer * 1024 * 1024 * sizeof(char));
	if (!clHunkData)
		Sys_Error("Could not allocate client hunk with %i megabytes\n", cl_hunkmegs->integer);
	clHunkPointerPos = clHunkData;
	Com_Printf("inited client hunk data with %i megabytes\n", cl_hunkmegs->integer);
}

/**
 * @brief Clears the client hunk and reset the hunk to zero
 * @sa CL_ClientHunkInit
 * @sa CL_ClientHunkUse
 * @sa CL_ResetSinglePlayerData
 */
extern qboolean CL_ClientHunkClear (void)
{
	if (clHunkData) {
		memset(clHunkData, 0, cl_hunkmegs->integer * 1024 * 1024 * sizeof(char));
		clHunkPointerPos = clHunkData;
		return qtrue;
	}
	return qfalse; /* not inited yet */
}

/**
 * @brief Frees the client hunk
 * @sa CL_ClientHunkInit
 * @sa CL_ClientHunkUse
 * @sa CL_Shutdown
 */
extern void CL_ClientHunkShutdown (void)
{
	free(clHunkData);
}

/**
 * @brief Copy data to hunk and return the position in current hunk
 * @sa CL_ClientHunkInit
 * @sa CL_ClientHunkShutdown
 * @sa CL_ParseScriptFirst
 * @sa CL_ParseScriptSecond
 * @note Everything that gets parsed in CL_ParseScriptFirst or
 * CL_ParseScriptSecond may use the hunk - it's cleared on every restart or
 * reload of a singleplayer game
 */
extern void *CL_ClientHunkUse (const void *data, size_t size)
{
	void *hunkPos = clHunkPointerPos;
	if (clHunkPointerPos + size > clHunkData + (cl_hunkmegs->integer * 1024 * 1024))
		Sys_Error("Increase the cl_hunkmegs value\n");
	/* maybe we just want to alloc, but not copy the mem */
	if (data)
		memcpy(clHunkPointerPos, data, size);
	clHunkPointerPos = clHunkPointerPos + size;
	return hunkPos;
}
