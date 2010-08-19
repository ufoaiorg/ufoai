/**
 * @file cl_parse.c
 * @brief Parse a message (event) received from the server.
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../cl_game.h"
#include "../ui/ui_main.h"
#include "events/e_parse.h"
#include "../multiplayer/mp_chatmessages.h"

/**
 * @brief see also svc_ops_e in common.h
 * @note don't change the array size - a NET_ReadByte can
 * return values between 0 and UCHAR_MAX (-1 is not handled here)
 */
static const char *svc_strings[UCHAR_MAX + 1] =
{
	"svc_bad",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
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
static void CL_ParseClientinfo (int player)
{
	clientinfo_t *ci = &cl.clientinfo[player];
	const char *s = CL_PlayerGetName(player);

	Q_strncpyz(ci->cinfo, s, sizeof(ci->cinfo));

	/* isolate the player's name */
	Q_strncpyz(ci->name, s, sizeof(ci->name));
}

/**
 * @brief Get the player name
 * @param player The index of the player
 * @return The name of the player
 */
const char *CL_PlayerGetName (int player)
{
	assert(player >= 0);
	assert(player < MAX_CLIENTS);

	return cl.configstrings[CS_PLAYERNAMES + player];
}

/**
 * @sa SV_Configstring
 */
static void CL_ParseConfigString (struct dbuffer *msg)
{
	int i;
	char s[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

	/* which configstring? */
	i = NET_ReadShort(msg);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	/* value */
	NET_ReadString(msg, s, sizeof(s));

	Com_DPrintf(DEBUG_CLIENT, "configstring %d: %s\n", i, s);

	/* there may be overflows in i==CS_TILES - but thats ok */
	/* see definition of configstrings and MAX_TILESTRINGS */
	switch (i) {
	case CS_TILES:
	case CS_POSITIONS:
		Q_strncpyz(cl.configstrings[i], s, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		break;
	default:
		Q_strncpyz(cl.configstrings[i], s, MAX_TOKEN_CHARS);
		break;
	}

	/* do something appropriate */
	if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
		if (refdef.ready) {
			cl.model_draw[i - CS_MODELS] = R_RegisterModelShort(cl.configstrings[i]);
			/* inline models are marked with * as first char followed by the number */
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i - CS_MODELS] = CM_InlineModel(cl.configstrings[i]);
			else
				cl.model_clip[i - CS_MODELS] = NULL;
		}
	} else if (i >= CS_PLAYERNAMES && i < CS_PLAYERNAMES + MAX_CLIENTS) {
		CL_ParseClientinfo(i - CS_PLAYERNAMES);
	}
}


/*
=====================================================================
ACTION MESSAGES
=====================================================================
*/

/**
 * @brief Parsed a server send sound package
 * @sa svc_sound
 * @sa SV_StartSound
 */
static void CL_ParseStartSoundPacket (struct dbuffer *msg)
{
	vec3_t origin;
	char sound[MAX_QPATH];
	s_sample_t *sample;

	NET_ReadString(msg, sound, sizeof(sound));
	NET_ReadPos(msg, origin);

	if (sound[strlen(sound) - 1] == '+') {
		const int length = strlen(sound) - 1;
		char randomSound[MAX_QPATH];
		int i = 1;
		Com_sprintf(randomSound, sizeof(randomSound), "%s", sound);
		randomSound[length + 0] = '\0';
		for (;;) {
			if (i > 99)
				break;

			if (FS_CheckFile("sounds/%s%c%c", randomSound, i / 10 + '0', i % 10 + '0') == -1)
				break;
			i++;
		}

		sample = S_LoadSample(va("%s%02i", randomSound, (rand() % i) + 1));
	} else {
		sample = S_LoadSample(sound);
	}

	S_PlaySample(origin, sample, SOUND_ATTN_NORM, SND_VOLUME_DEFAULT);
}

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
	if (cmd == -1)
		return;

	Com_DPrintf(DEBUG_CLIENT, "command: %s\n", svc_strings[cmd]);

	/* commands */
	switch (cmd) {
	case svc_nop:
/*		Com_Printf("svc_nop\n"); */
		break;

	case svc_disconnect:
		NET_ReadString(msg, s, sizeof(s));
		Com_Printf("%s\n", s);
		CL_Drop();	/* ensure the right menu cvars are set */
		break;

	case svc_reconnect:
		NET_ReadString(msg, s, sizeof(s));
		Com_Printf("%s\n", s);
		CL_Disconnect();
		CL_SetClientState(ca_connecting);
		/* otherwise we would time out */
		cls.connectTime = CL_Milliseconds() - 1500;
		break;

	case svc_print:
		i = NET_ReadByte(msg);
		NET_ReadString(msg, s, sizeof(s));
		switch (i) {
		case PRINT_CHAT:
			S_StartLocalSample("misc/talk", SND_VOLUME_DEFAULT);
			MP_AddChatMessage(s);
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

	case svc_sound:
		CL_ParseStartSoundPacket(msg);
		break;

	case svc_event:
		CL_ParseEvent(msg);
		break;

	case svc_bad:
		Com_Printf("CL_ParseServerMessage: bad server message %d\n", cmd);
		break;

	default:
		Com_Error(ERR_DROP,"CL_ParseServerMessage: Illegible server message %d", cmd);
	}
}
