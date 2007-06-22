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

/* FIXME: The client hunk isn't cleared in CL_ReadSinglePlayerData - so if a
 * player restarts the campaign a few times without restarting the game - the
 * hunk may overflow but we can't just call a clear function because fonts,
 * shaders and other stuff (not reparsed) are stored, there, too
 * we have to free the tags - and we have to introduce tags for different parseing
 * stages
 */

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
	*dst = Mem_PoolStrDup(string, cl_genericPool, CL_TAG_NONE);
	return *dst;
}

/**
 * @brief Copy data to hunk and return the position in current hunk
 * @sa CL_ParseScriptFirst
 * @sa CL_ParseScriptSecond
 * @note Everything that gets parsed in CL_ParseScriptFirst or
 * CL_ParseScriptSecond may use the hunk - it's cleared on every restart or
 * reload of a singleplayer game
 */
extern void *CL_ClientHunkUse (const void *data, size_t size)
{
	void *pos = Mem_PoolAlloc(size, cl_genericPool, 0);
	if (pos)
		memcpy(pos, data, size);
	return pos;
}
