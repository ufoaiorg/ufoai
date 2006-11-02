/**
 * @file sv_send.c
 * @brief Event message handling?
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

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
Com_Printf redirection
=============================================================================
*/

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect(int sv_redirected, char *outputbuf)
{
	if (sv_redirected == RD_PACKET) {
		Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", outputbuf);
	} else if (sv_redirected == RD_CLIENT) {
		MSG_WriteByte(&sv_client->netchan.message, svc_print);
		MSG_WriteByte(&sv_client->netchan.message, PRINT_HIGH);
		MSG_WriteString(&sv_client->netchan.message, outputbuf);
	}
}


/*
=============================================================================
EVENT MESSAGES
=============================================================================
*/


/**
 * @brief Sends text across to be displayed if the level passes
 */
void SV_ClientPrintf(client_t * cl, int level, char *fmt, ...)
{
	va_list argptr;
	char string[1024];

	if (level < cl->messagelevel)
		return;

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&cl->netchan.message, svc_print);
	MSG_WriteByte(&cl->netchan.message, level);
	MSG_WriteString(&cl->netchan.message, string);
}

/**
 * @brief Sends text to all active clients
 */
void SV_BroadcastPrintf(int level, char *fmt, ...)
{
	va_list argptr;
	char string[2048];
	client_t *cl;
	int i;

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	/* echo to console */
	if (dedicated->value) {
		char copy[1024];
		int i;

		/* mask off high bits */
		for (i = 0; i < 1023 && string[i]; i++)
			copy[i] = string[i] & 127;
		copy[i] = 0;
		Com_Printf("%s", copy);
	}

	for (i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++) {
		if (level < cl->messagelevel)
			continue;
		if (cl->state != cs_spawned)
			continue;
		MSG_WriteByte(&cl->netchan.message, svc_print);
		MSG_WriteByte(&cl->netchan.message, level);
		MSG_WriteString(&cl->netchan.message, string);
	}
}

/**
 * @brief Sends text to all active clients
 */
void SV_BroadcastCommand(char *fmt, ...)
{
	va_list argptr;
	char string[1024];

	if (!sv.state)
		return;
	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&sv.multicast, svc_stufftext);
#ifdef DEBUG
	Com_DPrintf("broadcast%s\n", string);
#endif
	MSG_WriteString(&sv.multicast, string);
	SV_Multicast(~0);
}


/**
 * @brief Sends the contents of sv.multicast to a subset of the clients,then clears sv.multicast.
 * @note MULTICAST_ALL same as broadcast (origin can be NULL)
 * @note MULTICAST_PVS send to clients potentially visible from org
 * @note MULTICAST_PHS send to clients potentially hearable from org
 */
void SV_Multicast(int mask)
{
	client_t *c;
	int j;

	/* send the data to all relevant clients */
	for (j = 0, c = svs.clients; j < sv_maxclients->value; j++, c++) {
		if (c->state == cs_free || c->state == cs_zombie)
			continue;
		if (!(mask & (1 << j)))
			continue;

		/* get next reliable buffer, if needed */
		if (c->addMsg == c->curMsg || c->reliable[c->addMsg].cursize + sv.multicast.cursize > MAX_MSGLEN - 100) {
			c->addMsg = (c->addMsg + 1) % RELIABLEBUFFERS;
			if (c->addMsg == c->curMsg) {
				/* client overflowed */
				c->netchan.message.overflowed = qtrue;
				continue;
			}
			SZ_Clear(&c->reliable[c->addMsg]);
		}

		/* write the message */
		SZ_Write(&c->reliable[c->addMsg], sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear(&sv.multicast);
}


/*
===============================================================================
FRAME UPDATES
===============================================================================
*/


/**
 * @brief Returns true if the client is over its current bandwidth estimation and should not be sent another packet
 */
qboolean SV_RateDrop(client_t * c)
{
	int total;
	int i;

	/* never drop over the loopback */
	if (c->netchan.remote_address.type == NA_LOOPBACK)
		return qfalse;

	total = 0;

	for (i = 0; i < RATE_MESSAGES; i++) {
		total += c->message_size[i];
	}

	if (total > c->rate) {
		c->surpressCount++;
		c->message_size[sv.framenum % RATE_MESSAGES] = 0;
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief
 */
void SV_SendClientMessages(void)
{
	int i;
	client_t *c;

	/* send a message to each connected client */
	for (i = 0, c = svs.clients; i < sv_maxclients->value; i++, c++) {
		if (!c->state)
			continue;
		/* if the reliable message overflowed, */
		/* drop the client */
		if (c->netchan.message.overflowed) {
			SZ_Clear(&c->netchan.message);
			SZ_Clear(&c->datagram);
			SV_BroadcastPrintf(PRINT_HIGH, "%s overflowed\n", c->name);
			SV_DropClient(c);
		}

		/* just update reliable if needed */
		if (c->netchan.message.cursize || (c->curMsg != c->addMsg && !c->netchan.message.cursize)
			|| curtime - c->netchan.last_sent > 1000) {
/*			Com_Printf( "send %i %i\n", c->netchan.message.cursize, c->netchan.reliable_length ); */
			if (c->curMsg != c->addMsg && !c->netchan.message.cursize) {
				/* copy the next reliable message */
				c->curMsg = (c->curMsg + 1) % RELIABLEBUFFERS;
				SZ_Write(&c->netchan.message, c->reliable[c->curMsg].data, c->reliable[c->curMsg].cursize);
			}
			Netchan_Transmit(&c->netchan, 0, NULL);
		}
	}
}

/**
 * Each entity can have eight independant sound sources, like voice, weapon, feet, etc.
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
void SV_StartSound(vec3_t origin, edict_t *entity, int channel,
	int soundindex, float volume, float attenuation, float timeofs)
{
	#if 0
	int sendchan;
	int flags;
	int i;
	int ent;
	vec3_t origin_v;

	if (volume < 0 || volume > 1.0)
		Com_Error(ERR_FATAL, "SV_StartSound: volume = %f", volume);

	if (attenuation < 0 || attenuation > 4)
		Com_Error(ERR_FATAL, "SV_StartSound: attenuation = %f", attenuation);

	if (timeofs < 0 || timeofs > 0.255)
		Com_Error(ERR_FATAL, "SV_StartSound: timeofs = %f", timeofs);

	ent = NUM_FOR_EDICT(entity);

	if (channel & 8) { /* no PHS flag */
		channel &= 7;

	sendchan =(ent << 3) |(channel & 7);

	flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	/**
	 * the client doesn't know that bmodels have weird origins
	 * the origin can also be explicitly set
	 */
	if ((entity->svflags & SVF_NOCLIENT) || (entity->solid == SOLID_BSP) || origin)
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
				origin_v[i] = entity->s.origin[i] + 0.5 *(entity->mins[i] + entity->maxs[i]);
		} else {
			VectorCopy(entity->s.origin, origin_v);
		}
	}

	MSG_WriteByte(&sv.multicast, svc_sound);
	MSG_WriteByte(&sv.multicast, flags);
	MSG_WriteByte(&sv.multicast, soundindex);

	if (flags & SND_VOLUME)
		MSG_WriteByte(&sv.multicast, volume*255);
	if (flags & SND_ATTENUATION)
		MSG_WriteByte(&sv.multicast, attenuation*64);
	if (flags & SND_OFFSET)
		MSG_WriteByte(&sv.multicast, timeofs*1000);

	if (flags & SND_ENT)
		MSG_WriteShort(&sv.multicast, sendchan);

	if (flags & SND_POS)
		MSG_WritePos(&sv.multicast, origin);

	if (channel & CHAN_RELIABLE){
		SV_Multicast(origin, MULTICAST_ALL_R);
	} else {
		SV_Multicast(origin, MULTICAST_ALL);
	}
	#endif
}
