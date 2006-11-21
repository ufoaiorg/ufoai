/**
 * @file master.c
 * @brief Master server implementation
 * @TODO: Adopt for UFO:AI
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

Original file from Quakeforge hw/source/master.c

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

#include <string.h>
#include <stdlib.h>

/* FIXME: */
#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/idparse.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"

#include "compat.h"
#include "netchan.h"

#define SV_TIMEOUT 450

/* FIXME: */
#define PORT_MASTER 26900
#define PORT_SERVER 26950

/**
 * M = master, S = server, C = client, A = any
 * the second character will allways be \n if the message isn't a single
 * byte long (?? not true anymore?)
 */

#define S2C_CHALLENGE       'c'
#define A2A_PING            'k'

#define S2M_HEARTBEAT       'a' // + serverinfo + userlist + fraglist
#define S2M_SHUTDOWN        'C'

typedef struct filter_s {
	netadr_t    from;
	netadr_t    to;
	struct filter_s *next;
	struct filter_s *previous;
} filter_t;

typedef struct server_s {
	netadr_t    ip;
	struct server_s *next;
	struct server_s *previous;
	double timeout;
} server_t;

static cvar_t *sv_console_plugin;
SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
	SERVER_PLUGIN_LIST
};

qboolean is_server = qtrue;

static cbuf_t *mst_cbuf;

static server_t *sv_list = NULL;
static filter_t *filter_list = NULL;

/**
 * @brief
 */
static void FL_Remove (filter_t * filter)
{
	if (filter->previous)
		filter->previous->next = filter->next;
	if (filter->next)
		filter->next->previous = filter->previous;
	filter->next = NULL;
	filter->previous = NULL;
	if (filter_list == filter)
		filter_list = NULL;
}

/**
 * @brief
 */
static void FL_Clear (void)
{
	filter_t   *filter;

	for (filter = filter_list; filter;) {
		if (filter) {
			filter_t   *next = filter->next;

			FL_Remove (filter);
			free (filter);
			filter = next;
		}
	}
	filter_list = NULL;
}

/**
 * @brief
 */
static filter_t * FL_New (netadr_t *adr1, netadr_t *adr2)
{
	filter_t   *filter;

	filter = (filter_t *) calloc (1, sizeof (filter_t));
	if (adr1)
		filter->from = *adr1;
	if (adr2)
		filter->to = *adr2;
	return filter;
}

/**
 * @brief
 */
static void FL_Add (filter_t * filter)
{
	filter->next = filter_list;
	filter->previous = NULL;
	if (filter_list)
		filter_list->previous = filter;
	filter_list = filter;
}

/**
 * @brief
 */
static filter_t * FL_Find (netadr_t adr)
{
	filter_t   *filter;

	for (filter = filter_list; filter; filter = filter->next) {
		if (NET_CompareBaseAdr (filter->from, adr))
			return filter;
	}
	return NULL;
}

/**
 * @brief
 */
static void Filter (void)
{
	int         hold_port;
	netadr_t    filter_adr;
	filter_t   *filter;

	hold_port = net_from.port;
	NET_StringToAdr ("127.0.0.1:26950", &filter_adr);
	if (NET_CompareBaseAdr (net_from, filter_adr)) {
		NET_StringToAdr ("0.0.0.0:26950", &filter_adr);
		if (!NET_CompareBaseAdr (net_local_adr, filter_adr)) {
			net_from = net_local_adr;
			net_from.port = hold_port;
		}
		return;
	}

	/* if no compare with filter list */
	if ((filter = FL_Find (net_from))) {
		net_from = filter->to;
		net_from.port = hold_port;
	}
}

/**
 * @brief
 */
static void SVL_Remove (server_t *sv)
{
	if (sv_list == sv)
		sv_list = sv->next;
	if (sv->previous)
		sv->previous->next = sv->next;
	if (sv->next)
		sv->next->previous = sv->previous;
	sv->next = NULL;
	sv->previous = NULL;
}

/**
 * @brief
 */
static void SVL_Clear (void)
{
	server_t   *sv;

	for (sv = sv_list; sv;) {
		if (sv) {
			server_t   *next = sv->next;

			SVL_Remove (sv);
			free (sv);
			sv = next;
		}
	}
	sv_list = NULL;
}

/**
 * @brief
 */
static server_t * SVL_New (netadr_t *adr)
{
	server_t   *sv;

	sv = (server_t *) calloc (1, sizeof (server_t));
	if (adr)
		sv->ip = *adr;
	return sv;
}

/**
 * @brief
 */
static void SVL_Add (server_t *sv)
{
	sv->next = sv_list;
	sv->previous = NULL;
	if (sv_list)
		sv_list->previous = sv;
	sv_list = sv;
}

/**
 * @brief
 */
static server_t * SVL_Find (netadr_t adr)
{
	server_t   *sv;

	for (sv = sv_list; sv; sv = sv->next) {
		if (NET_CompareAdr (sv->ip, adr))
			return sv;
	}
	return NULL;
}

/**
 * @brief
 */
static void SV_InitNet (void)
{
	const char *str;
	int         port, p;
	QFile      *filters;

	port = PORT_MASTER;
	p = COM_CheckParm ("-port");
	if (p && p < com_argc) {
		port = atoi (com_argv[p + 1]);
		Com_Printf ("Port: %i\n", port);
	}
	NET_Init (port);

	/* Add filters */
	if ((filters = Qopen ("filters.ini", "rt"))) {
		while ((str = Qgetline (filters))) {
			Cbuf_AddText (mst_cbuf, "filter add ");
			Cbuf_AddText (mst_cbuf, str);
			Cbuf_AddText (mst_cbuf, "\n");
		}
		Qclose (filters);
	}
}

/**
 * @brief
 */
static void AnalysePacket (void)
{
	byte        buf[16];
	byte       *p, *data;
	int         i, size, rsize;

	Com_Printf ("%s >> unknown packet:\n", NET_AdrToString (net_from));

	data = net_message->message->data;
	size = net_message->message->cursize;

	for (p = data; (rsize = min (size - (p - data), 16)); p += rsize) {
		Com_Printf ("%04X:", (unsigned) (p - data));
		memcpy (buf, p, rsize);
		for (i = 0; i < rsize; i++) {
			Com_Printf (" %02X", buf[i]);
			if (buf[i] < ' ' || buf [i] > '~')
				buf[i] = '.';
		}
		Com_Printf ("%*.*s\n", 1 + (16 - rsize) * 3 + rsize, rsize, buf);
	}
}

/**
 * @brief
 */
static void Mst_SendList (void)
{
	byte        buf[MAX_DATAGRAM];
	sizebuf_t   msg;
	server_t   *sv;
	short int   sv_num = 0;

	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;
	msg.allowoverflow = qtrue;
	msg.overflowed = qfalse;

	/* number of servers: */
	for (sv = sv_list; sv; sv = sv->next)
		sv_num++;
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 'd');
	MSG_WriteByte (&msg, '\n');
	if (sv_num > 0)
		for (sv = sv_list; sv; sv = sv->next) {
			MSG_WriteByte (&msg, sv->ip.ip[0]);
			MSG_WriteByte (&msg, sv->ip.ip[1]);
			MSG_WriteByte (&msg, sv->ip.ip[2]);
			MSG_WriteByte (&msg, sv->ip.ip[3]);
			MSG_WriteShort (&msg, sv->ip.port);
		}
	NET_SendPacket (msg.cursize, msg.data, net_from);
}

/**
 * @brief
 */
static void Mst_Packet (void)
{
	char        msg;
	server_t   *sv;

#if 0
	Filter ();
#endif
	msg = net_message->message->data[1];
	if (msg == A2A_PING) {
		Filter ();
		Com_Printf ("%s >> A2A_PING\n", NET_AdrToString (net_from));
		if (!(sv = SVL_Find (net_from))) {
			sv = SVL_New (&net_from);
			SVL_Add (sv);
		}
		sv->timeout = Sys_DoubleTime ();
	} else if (msg == S2M_HEARTBEAT) {
		Filter ();
		Com_Printf ("%s >> S2M_HEARTBEAT\n", NET_AdrToString (net_from));
		if (!(sv = SVL_Find (net_from))) {
			sv = SVL_New (&net_from);
			SVL_Add (sv);
		}
		sv->timeout = Sys_DoubleTime ();
	} else if (msg == S2M_SHUTDOWN) {
		Filter ();
		Com_Printf ("%s >> S2M_SHUTDOWN\n", NET_AdrToString (net_from));
		if ((sv = SVL_Find (net_from))) {
			SVL_Remove (sv);
			free (sv);
		}
	} else if (msg == S2C_CHALLENGE) {
		Com_Printf ("%s >> ", NET_AdrToString (net_from));
		Com_Printf ("Gamespy server list request\n");
		Mst_SendList ();
	} else {
		byte       *p;

		p = net_message->message->data;
		if (p[0] == 0 && p[1] == 'y') {
			Com_Printf ("%s >> ", NET_AdrToString (net_from));
			Com_Printf ("Pingtool server list request\n");
			Mst_SendList ();
		} else {
			AnalysePacket ();
		}
	}
}

/**
 * @brief
 */
static void SV_ReadPackets (void)
{
	while (NET_GetPacket ()) {
		Mst_Packet ();
	}
}

/**
 * @brief
 */
static void FilterAdd (int arg)
{
	filter_t   *filter;
	netadr_t    to, from;

	if (Cmd_Argc () - arg != 2) {
		Com_Printf ("Invalid command parameters. "
					"Usage:\nfilter add x.x.x.x:port x.x.x.x:port\n\n");
		return;
	}
	NET_StringToAdr (Cmd_Argv (arg), &from);
	NET_StringToAdr (Cmd_Argv (arg + 1), &to);
	if (to.port == 0)
		from.port = BigShort (PORT_SERVER);
	if (from.port == 0)
		from.port = BigShort (PORT_SERVER);
	if (!(filter = FL_Find (from))) {
		Com_Printf ("Added filter %s\t\t%s\n", Cmd_Argv (arg),
					Cmd_Argv (arg + 1));
		filter = FL_New (&from, &to);
		FL_Add (filter);
	} else
		Com_Printf ("%s already defined\n\n", Cmd_Argv (arg));
}

/**
 * @brief
 */
static void FilterRemove (int arg)
{
	filter_t   *filter;
	netadr_t    from;

	if (Cmd_Argc () - arg != 1) {
		Com_Printf ("Invalid command parameters. Usage:\n"
					"filter remove x.x.x.x:port\n\n");
		return;
	}
	NET_StringToAdr (Cmd_Argv (arg), &from);
	if ((filter = FL_Find (from))) {
		Com_Printf ("Removed %s\n\n", Cmd_Argv (arg));
		FL_Remove (filter);
		free (filter);
	} else
		Com_Printf ("Cannot find %s\n\n", Cmd_Argv (arg));
}

/**
 * @brief
 */
static void FilterList (void)
{
	filter_t   *filter;

	for (filter = filter_list; filter; filter = filter->next) {
		Com_Printf ("%s", NET_AdrToString (filter->from));
		Com_Printf ("\t\t%s\n", NET_AdrToString (filter->to));
	}
	if (filter_list == NULL)
		Com_Printf ("No filter\n");
	Com_Printf ("\n");
}

/**
 * @brief
 */
static void FilterClear (void)
{
	Com_Printf ("Removed all filters\n\n");
	FL_Clear ();
}

/**
 * @brief
 */
static void Filter_f (void)
{
	if (!strcmp (Cmd_Argv (1), "add"))
		FilterAdd (2);
	else if (!strcmp (Cmd_Argv (1), "remove"))
		FilterRemove (2);
	else if (!strcmp (Cmd_Argv (1), "clear"))
		FilterClear ();
	else if (Cmd_Argc () == 3) {
		FilterAdd (1);
	} else if (Cmd_Argc () == 2) {
		FilterRemove (1);
	} else
		FilterList ();
}

/**
 * @brief
 */
static void SV_WriteFilterList (void)
{
	FILE *f;
	filter_t *filter;

	if (filter_list == NULL)
		return;

	f = fopen("filters.ini", "w");
	if (!f) {
		Com_Printf("ERROR: couldn't open filters.ini.\n");
		return;
	}

	for (filter = filter_list; filter; filter = filter->next) {
		fprintf(f, "%s", NET_AdrToString (filter->from));
		fprintf(f, " %s\n", NET_AdrToString (filter->to));
	}
	fclose(f);
}

/**
 * @brief
 */
static void SV_Shutdown (void)
{
	NET_Shutdown ();

	/* write filter list */
	SV_WriteFilterList ();
	Con_Shutdown ();
}

/**
 * @brief
 */
static void SV_TimeOut (void)
{
	/* Remove listed servers that haven't sent a heartbeat for some time */
	double      time = Sys_DoubleTime ();
	server_t   *sv;
	server_t   *next;

	if (sv_list == NULL)
		return;

	for (sv = sv_list; sv;) {
		if (sv->timeout + SV_TIMEOUT < time) {
			next = sv->next;
			Com_Printf ("%s timed out\n", NET_AdrToString (sv->ip));
			SVL_Remove (sv);
			free (sv);
			sv = next;
		} else
			sv = sv->next;
	}
}

/**
 * @brief
 */
static void SV_Frame (void)
{
	Sys_CheckInput (1, net_socket);
	Con_ProcessInput ();
	Cbuf_Execute_Stack (mst_cbuf);
	SV_TimeOut ();
	SV_ReadPackets ();
}

/**
 * @brief
 */
static void MST_Quit_f (void)
{
	Com_Printf ("UFO:AI master shutdown\n");
	Sys_Quit ();
}

/**
 * @brief
 */
int main (int argc, const char **argv)
{
	COM_InitArgv (argc, argv);

	mst_cbuf = Cbuf_New (&id_interp);

	Sys_RegisterShutdown (SV_Shutdown);

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cvar_Init ();
	Sys_Init_Cvars ();
	Sys_Init ();
	Cmd_Init ();

	Cmd_AddCommand ("quit", MST_Quit_f, "Shut down the master server");
	Cmd_AddCommand ("clear", SVL_Clear, "Clear the server list");
	Cmd_AddCommand ("filter", Filter_f, "Manipulate filtering");

	Cmd_StuffCmds (mst_cbuf);
	Cbuf_Execute_Sets (mst_cbuf);

	SV_InitNet ();
	Com_Printf ("Exe: " __TIME__ " " __DATE__ "\n");
	Com_Printf ("======== UFO:AI master initialized ========\n\n");
	while (1) {
		SV_Frame ();
	}
	return 0;
}
