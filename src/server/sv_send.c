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
 * @brief Sends text across to be displayed if the level passes
 */
void SV_ClientPrintf (client_t * cl, int level, const char *fmt, ...)
{
	va_list argptr;
        struct dbuffer *msg;

	if (level < cl->messagelevel)
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
	if (dedicated->integer) {
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
		if (level < cl->messagelevel)
			continue;
		if (cl->state < cs_connected)
			continue;
                NET_WriteConstMsg(cl->stream, msg);
	}

        free_dbuffer(msg);
}

/**
 * @brief Sends text to all active clients
 */
void SV_BroadcastCommand (const char *fmt, ...)
{
	va_list argptr;
        struct dbuffer *msg;

	if (!sv.state)
		return;

        msg = new_dbuffer();
        NET_WriteByte(msg, svc_stufftext);

	va_start(argptr, fmt);
	NET_VPrintf(msg, fmt, argptr);
	va_end(argptr);

#ifdef DEBUG
        {
          char string[1024];
          va_start(argptr, fmt);
          Q_vsnprintf(string, sizeof(string), fmt, argptr);
          va_end(argptr);
          string[sizeof(string)-1] = 0;
          Com_DPrintf("broadcast%s\n", string);
        }
#endif
	SV_Multicast(~0, msg);
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
		if (c->state == cs_free || c->state == cs_zombie)
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
 * FIXME: if entity isn't in PHS, they must be forced to be sent or have the origin explicitly sent.
 *
 * Channel 0 is an auto-allocate channel, the others override anything already running on that entity/channel pair.
 *
 * An attenuation of 0 will play full volume everywhere in the level. Larger attenuations will drop off. (max 4 attenuation)
 *
 * Timeofs can range from 0.0 to 0.1 to cause sounds to be started later in the frame than they normally would.
 *
 * If origin is NULL, the origin is determined from the entity origin or the midpoint of the entity box for bmodels.
 */
void SV_BreakSound (vec3_t origin, edict_t *entity, int channel, edictMaterial_t material)
{
	float volume = 1.0;
	float attenuation = 1.0;
	float timeofs = 0.0;
	int sendchan;
	int flags;
	int i;
	int ent;
	vec3_t origin_v;
	struct dbuffer *msg;

	if (volume < 0 || volume > 1.0)
		Com_Error(ERR_FATAL, "SV_BreakSound: volume = %f", volume);

	if (attenuation < 0 || attenuation > 4)
		Com_Error(ERR_FATAL, "SV_BreakSound: attenuation = %f", attenuation);

	if (timeofs < 0 || timeofs > 0.255)
		Com_Error(ERR_FATAL, "SV_BreakSound: timeofs = %f", timeofs);

	ent = NUM_FOR_EDICT(entity);

	if (channel & 8) /* no PHS flag */
		channel &= 7;

	sendchan = (ent << 3) | (channel & 7);

	flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	/**
	 * the client doesn't know that bmodels have weird origins
	 * the origin can also be explicitly set
	 */
	if ((entity->solid == SOLID_BSP) || origin)
		flags |= SND_POS;

	/* always send the entity number for channel overrides */
	flags |= SND_ENT;

	if (timeofs)
		flags |= SND_OFFSET;

	/* use the entity origin unless it is a bmodel or explicitly specified */
	if (!origin){
		origin = origin_v;
		if (entity->solid == SOLID_BSP){
			for (i = 0; i < 3; i++)
				origin_v[i] = entity->origin[i] + 0.5 *(entity->mins[i] + entity->maxs[i]);
		} else {
			VectorCopy(entity->origin, origin_v);
		}
	}

	msg = new_dbuffer();

	NET_WriteByte(msg, svc_breaksound);
	NET_WriteByte(msg, flags);
	NET_WriteByte(msg, material);

	if (flags & SND_VOLUME)
		NET_WriteByte(msg, volume * 255);
	if (flags & SND_ATTENUATION)
		NET_WriteByte(msg, attenuation * 64);
	if (flags & SND_OFFSET)
		NET_WriteByte(msg, timeofs * 1000);

	if (flags & SND_ENT)
		NET_WriteShort(msg, sendchan);

	if (flags & SND_POS)
		NET_WritePos(msg, origin);

	SV_Multicast(~0, msg);
}
