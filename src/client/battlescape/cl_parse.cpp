/**
 * @file cl_parse.c
 * @brief Parse a message (event) received from the server.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../client.h"
#include "cl_localentity.h"
#include "cl_parse.h"
#include "cl_hud.h"
#include "../cgame/cl_game.h"
#include "events/e_parse.h"

/**
 * @brief see also svc_ops_e in common.h
 * @note don't change the array size - a NET_ReadByte can
 * return values between 0 and UCHAR_MAX (-1 is not handled here)
 */
static char const* const svc_strings[] =
{
	"svc_bad",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_event"
};

/*
=====================================================================
SERVER CONNECTING MESSAGES
=====================================================================
*/

/**
 * @brief Written by SV_New_f in sv_user.c
 */
static void CL_ParseServerData (struct dbuffer *msg)
{
	char str[1024];
	int i;

	Com_DPrintf(DEBUG_CLIENT, "Serverdata packet received.\n");

	CL_SetClientState(ca_connected);

	/* parse protocol version number */
	i = NET_ReadLong(msg);
	/* compare versions */
	if (i != PROTOCOL_VERSION)
		Com_Error(ERR_DROP, "Server returned version %i, not %i", i, PROTOCOL_VERSION);

	/* parse player entity number */
	cl.pnum = NET_ReadShort(msg);

	/* get the full level name */
	NET_ReadString(msg, str, sizeof(str));

	Com_DPrintf(DEBUG_CLIENT, "serverdata: pnum %d, level %s\n", cl.pnum, str);

	if (cl.pnum >= 0) {
		/* need to prep refresh at next opportunity */
		refdef.ready = qfalse;
	}
}

/**
 * @brief Parses client names that are displayed on the targeting box for
 * multiplayer games
 * @sa CL_AddTargetingBoX
 */
static void CL_ParseClientinfo (unsigned int player)
{
	clientinfo_t *ci = &cl.clientinfo[player];
	const char *s = CL_GetConfigString(CS_PLAYERNAMES + player);

	/* isolate the player's name */
	Q_strncpyz(ci->name, s, sizeof(ci->name));
}

int CL_GetPlayerNum (void)
{
	return cls.team;
}

/**
 * @brief Get the player name
 * @param player The index of the player
 * @return The name of the player
 */
const char *CL_PlayerGetName (unsigned int player)
{
	const clientinfo_t *ci = &cl.clientinfo[player];
	return ci->name;
}

/**
 * @sa SV_Configstring
 */
static void CL_ParseConfigString (struct dbuffer *msg)
{
	const int i = NET_ReadShort(msg);
	const char *s = CL_SetConfigString(i, msg);

	Com_DPrintf(DEBUG_CLIENT, "configstring %d: %s\n", i, s);

	/* do something appropriate */
	if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
		if (refdef.ready) {
			const unsigned int index = i - CS_MODELS;
			assert(index != 0);
			cl.model_draw[index] = R_FindModel(s);
			/* inline models are marked with * as first char followed by the number */
			if (s[0] == '*')
				cl.model_clip[index] = CM_InlineModel(cl.mapTiles, s);
			else
				cl.model_clip[index] = NULL;
		}
	} else if (i >= CS_PLAYERNAMES && i < CS_PLAYERNAMES + MAX_CLIENTS) {
		const unsigned int index = i - CS_PLAYERNAMES;
		CL_ParseClientinfo(index);
	}
}


/*
=====================================================================
ACTION MESSAGES
=====================================================================
*/

/**
 * @brief Parses the server sent data from the given buffer.
 * @sa CL_ReadPackets
 * @param[in] cmd The action that should be parsed from the data
 * @param[in,out] msg The client stream message buffer to read from
 */
void CL_ParseServerMessage (svc_ops_t cmd, struct dbuffer *msg)
{
	char s[4096];
	int i;

	/* parse the message */
	if (cmd < svc_bad || cmd >= svc_oob)
		return;

	Com_DPrintf(DEBUG_CLIENT, "command: %s\n", svc_strings[cmd]);

	/* commands */
	switch (cmd) {
	case svc_nop:
/*		Com_Printf("svc_nop\n"); */
		break;

	case svc_ping: {
		struct dbuffer *ack = new_dbuffer();
		NET_WriteByte(ack, clc_ack);
		NET_WriteMsg(cls.netStream, ack);
		break;
	}

	case svc_disconnect:
		NET_ReadString(msg, s, sizeof(s));
		Com_Printf("%s\n", s);
		CL_Drop();	/* ensure the right menu cvars are set */
		break;

	case svc_reconnect:
		NET_ReadString(msg, s, sizeof(s));
		Com_Printf("%s\n", s);
		cls.reconnectTime = CL_Milliseconds() + 4000;
		break;

	case svc_print:
		i = NET_ReadByte(msg);
		NET_ReadString(msg, s, sizeof(s));
		switch (i) {
		case PRINT_CHAT:
			GAME_AddChatMessage(s);
			/* skip format strings */
			if (s[0] == '^')
				memmove(s, &s[2], sizeof(s) - 2);
			/* also print to console */
			break;
		case PRINT_HUD:
			/* all game lib messages or server messages should be printed
			 * untranslated with BroadcastPrintf or PlayerPrintf */
			/* see src/po/OTHER_STRINGS */
			HUD_DisplayMessage(_(s));
			break;
		default:
			break;
		}
		Com_DPrintf(DEBUG_CLIENT, "svc_print(%d): %s", i, s);
		Com_Printf("%s", s);
		break;

	case svc_stufftext:
		NET_ReadString(msg, s, sizeof(s));
		Com_DPrintf(DEBUG_CLIENT, "stufftext: %s\n", s);
		Cbuf_AddText(s);
		break;

	case svc_serverdata:
		Cbuf_Execute();		/* make sure any stuffed commands are done */
		CL_ParseServerData(msg);
		break;

	case svc_configstring:
		CL_ParseConfigString(msg);
		break;

	case svc_event:
		CL_ParseEvent(msg);
		break;

	case svc_bad:
		Com_Printf("CL_ParseServerMessage: bad server message %d\n", cmd);
		break;

	default:
		Com_Error(ERR_DROP,"CL_ParseServerMessage: Illegal server message %d", cmd);
	}
}
