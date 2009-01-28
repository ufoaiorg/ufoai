/**
 * @file sv_send.c
 * @brief Event message handling?
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
void SV_ClientCommand (client_t *client, const char *fmt, ...)
{
	va_list ap;
	struct dbuffer *msg = new_dbuffer();

	NET_WriteByte(msg, svc_stufftext);

	va_start(ap, fmt);
	NET_VPrintf(msg, fmt, ap);
	va_end(ap);

	NET_WriteMsg(client->stream, msg);
}

/**
 * @brief Sends text across to be displayed if the level passes
 */
void SV_ClientPrintf (client_t * cl, int level, const char *fmt, ...)
{
	va_list argptr;
	struct dbuffer *msg;

	if (level > cl->messagelevel)
		return;

	msg = new_dbuffer();
	NET_WriteByte(msg, svc_print);
	NET_WriteByte(msg, level);

	va_start(argptr, fmt);
	NET_VPrintf(msg, fmt, argptr);
	va_end(argptr);

	NET_WriteMsg(cl->stream, msg);
}

/**
 * @brief Sends text to all active clients
 */
void SV_BroadcastPrintf (int level, const char *fmt, ...)
{
	va_list argptr;
	struct dbuffer *msg;
	client_t *cl;
	int i;

	msg = new_dbuffer();
	NET_WriteByte(msg, svc_print);
	NET_WriteByte(msg, level);

	va_start(argptr, fmt);
	NET_VPrintf(msg, fmt, argptr);
	va_end(argptr);

	/* echo to console */
	if (sv_dedicated->integer) {
		char copy[1024];

		va_start(argptr, fmt);
		Q_vsnprintf(copy, sizeof(copy), fmt, argptr);
		va_end(argptr);

		/* mask off high bits */
		for (i = 0; i < 1023 && copy[i]; i++)
			copy[i] = copy[i] & 127;
		copy[i] = 0;
		Com_Printf("%s", copy);
	}

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (level > cl->messagelevel)
			continue;
		if (cl->state < cs_connected)
			continue;
		NET_WriteConstMsg(cl->stream, msg);
	}

	free_dbuffer(msg);
}

/**
 * @brief Sends the contents of msg to a subset of the clients,then frees msg
 */
void SV_Multicast (int mask, struct dbuffer *msg)
{
	client_t *c;
	int j;

	/* send the data to all relevant clients */
	for (j = 0, c = svs.clients; j < sv_maxclients->integer; j++, c++) {
		if (c->state == cs_free)
			continue;
		if (!(mask & (1 << j)))
			continue;

		/* write the message */
		NET_WriteConstMsg(c->stream, msg);
	}

	free_dbuffer(msg);
}


/*
===============================================================================
FRAME UPDATES
===============================================================================
*/

/**
 * @brief Each entity can have eight independant sound sources, like voice, weapon, feet, etc.
 * If cahnnel & 8, the sound will be sent to everyone, not just things in the PHS.
 * Channel 0 is an auto-allocate channel, the others override anything already running on that entity/channel pair.
 * Timeofs can range from 0.0 to 0.1 to cause sounds to be started later in the frame than they normally would.
 * If origin is NULL, the origin is determined from the entity origin or the midpoint of the entity box for bmodels.
 */
void SV_StartSound (int mask, vec3_t origin, edict_t *entity, const char *sound, int channel, float volume)
{
	int flags;
	vec3_t origin_v;
	struct dbuffer *msg;

	if (volume < 0 || volume > 1.0)
		Com_Error(ERR_FATAL, "SV_StartSound: volume = %f", volume);

	flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;

	/**
	 * the client doesn't know that bmodels have weird origins
	 * the origin can also be explicitly set
	 */
	if (entity->solid == SOLID_BSP || origin)
		flags |= SND_POS;

	/* use the entity origin unless it is a bmodel or explicitly specified */
	if (!origin) {
		origin = origin_v;
		if (entity->solid == SOLID_BSP) {
			VectorCenterFromMinsMaxs(entity->mins, entity->maxs, origin_v);
			VectorAdd(entity->origin, origin_v, origin_v);
		} else {
			VectorCopy(entity->origin, origin_v);
		}
	}

	msg = new_dbuffer();

	NET_WriteByte(msg, svc_sound);
	NET_WriteByte(msg, flags);
	NET_WriteString(msg, sound);

	if (flags & SND_VOLUME)
		NET_WriteByte(msg, volume * 128);

	if (flags & SND_POS)
		NET_WritePos(msg, origin);

	SV_Multicast(mask, msg);
}
