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

/* FIXME: The client hunk isn't cleared in CL_ReadSinglePlayerData - so if a
 * player restarts the campaign a few times without restarting the game - the
 * hunk may overflow but we can't just call CL_ClientHunkClear because fonts,
 * shaders and other stuff (not reparsed) are stored, there, too
 */

/** @brief don't allow external access to this hunk */
static byte *clHunkData = NULL;
static byte *clHunkPointerPos;

/**
 * @brief Dumps the hunk to a file
 */
static void CL_ClientHunkDumpToFile_f (void)
{
	qFILE f;
	ptrdiff_t size = clHunkPointerPos - clHunkData;

	memset(&f, 0, sizeof(qFILE));
	FS_FOpenFileWrite(va("%s/hunk.dump", FS_Gamedir()), &f);
	if (!f.f) {
		Com_Printf("CL_ClientHunkDumpToFile_f: Error opening dump file for writing");
		return;
	}

	FS_Write(clHunkData, size, &f);

	FS_FCloseFile(&f);
}

/**
 * @brief
 */
static void CL_ClientHunkUsage_f (void)
{
	ptrdiff_t size = clHunkPointerPos - clHunkData;
	int allocated = cl_hunkmegs->integer * 1024 * 1024;

	Com_Printf("client hunk statistics:\n"
		"used:      %10ti bytes\n"
		"free:      %10ti bytes\n"
		"allocated: %10i bytes\n", size, allocated - size, allocated);
}

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
	Cmd_AddCommand("cl_hunkdump", CL_ClientHunkDumpToFile_f, "Dump the client hunk into a file");
	Cmd_AddCommand("cl_hunkstats", CL_ClientHunkUsage_f, "Displays some client hunk stats");

	clHunkData = malloc(cl_hunkmegs->integer * 1024 * 1024 * sizeof(char));
	if (!clHunkData)
		Sys_Error("Could not allocate client hunk with %i megabyte(s)\n", cl_hunkmegs->integer);
	clHunkPointerPos = clHunkData;
	Com_Printf("inited client hunk data with %i megabyte(s)\n", cl_hunkmegs->integer);
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
 * @brief Saves a string to client hunk
 * @sa CL_ClientHunkUse
 * @param[in] string String to store on hunk
 * @param[in] dst The offset or char pointer that points to the hunk space that
 * was used to store the string
 */
extern char *CL_ClientHunkStoreString (const char *string, char **dst)
{
	assert(dst);
	assert(string);
	*dst = (char*)CL_ClientHunkUse(string, (strlen(string) + 1) * sizeof(char));
	return *dst;
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
	assert(clHunkData);

	size = (size + 31) & ~31;  /* round to cacheline */

	if (clHunkPointerPos + size > clHunkData + (cl_hunkmegs->integer * 1024 * 1024))
		Sys_Error("Increase the cl_hunkmegs value\n");
	/* maybe we just want to alloc, but not copy the mem */
	if (data)
		memcpy(clHunkPointerPos, data, size);
	clHunkPointerPos = clHunkPointerPos + size;
	return hunkPos;
}
