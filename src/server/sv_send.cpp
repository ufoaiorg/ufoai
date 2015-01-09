/**
 * @file
 * @brief Event message handling?
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_send.c
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

/*
=============================================================================
EVENT MESSAGES
=============================================================================
*/

/**
 * @sa SV_BroadcastCommand
 */
void SV_ClientCommand (client_t* client, const char* fmt, ...)
{
	va_list ap;
	char str[MAX_SVC_STUFFTEXT];
	dbuffer msg;

	NET_WriteByte(&msg, svc_stufftext);

	va_start(ap, fmt);
	NET_VPrintf(&msg, fmt, ap, str, sizeof(str));
	va_end(ap);

	NET_WriteMsg(client->stream, msg);
}

/**
 * @brief Sends text across to be displayed if the level passes
 */
void SV_ClientPrintf (client_t* cl, int level, const char* fmt, ...)
{
	if (level > cl->messagelevel)
		return;

	dbuffer msg;
	NET_WriteByte(&msg, svc_print);
	NET_WriteByte(&msg, level);

	va_list argptr;
	va_start(argptr, fmt);
	char str[MAX_SVC_PRINT];
	NET_VPrintf(&msg, fmt, argptr, str, sizeof(str));
	va_end(argptr);

	NET_WriteMsg(cl->stream, msg);
}

/**
 * @brief Sends text to all active clients
 */
void SV_BroadcastPrintf (int level, const char* fmt, ...)
{
	dbuffer msg;
	NET_WriteByte(&msg, svc_print);
	NET_WriteByte(&msg, level);

	va_list argptr;
	va_start(argptr, fmt);
	char str[MAX_SVC_PRINT];
	NET_VPrintf(&msg, fmt, argptr, str, sizeof(str));
	va_end(argptr);

	/* echo to console */
	if (sv_dedicated->integer) {
		char copy[1024];
		int i;
		const int length = sizeof(copy) - 1;

		va_start(argptr, fmt);
		Q_vsnprintf(copy, sizeof(copy), fmt, argptr);
		va_end(argptr);

		/* mask off high bits */
		for (i = 0; i < length && copy[i]; i++)
			copy[i] = copy[i] & 127;
		copy[i] = '\0';
		if (level == PRINT_HUD)
			Com_Printf("%s\n", copy);
		else
			Com_Printf("%s", copy);
	}

	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (level > cl->messagelevel)
			continue;
		if (cl->state < cs_connected)
			continue;
		NET_WriteConstMsg(cl->stream, msg);
	}
}

/**
 * @brief Sends the contents of msg to a subset of the clients, then frees msg
 * @param[in] mask Bitmask of the players to send the multicast to
 * @param[in,out] msg The message to send to the clients
 */
void SV_Multicast (int mask, const dbuffer& msg)
{
	/* send the data to all relevant clients */
	client_t* cl = nullptr;
	int j = -1;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		j++;
		if (cl->state < cs_connected)
			continue;
		if (!(mask & (1 << j)))
			continue;

		/* write the message */
		NET_WriteConstMsg(cl->stream, msg);
	}
}
