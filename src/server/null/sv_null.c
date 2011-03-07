/**
 * @file sv_null.c
 * @brief Stub out the entire server system for pure net-only clients.
 *
 * NOTE: This code is currently not in use and is not compiled.
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_null.c
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

#include "server.h"

void SV_Init(void)
{
}

void SV_Clear(void)
{
}

void SV_Shutdown(const char *finalmsg, qboolean reconnect)
{
}

void SV_ShutdownWhenEmpty(void)
{
}

void SV_Frame(int now, void *unused)
{
}

mapData_t* SV_GetMapData(void)
{
	return NULL;
}

mapTiles_t* SV_GetMapTiles(void)
{
	return NULL;
}

static server_state_t s_state = ss_dead;
server_state_t SV_GetServerState(void)
{
	return s_state;
}

void SV_SetServerState(server_state_t state)
{
	s_state = state;
}
