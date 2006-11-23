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

#include "../../qcommon/qcommon.h"

#define SV_TIMEOUT 450

#define PORT_MASTER 26900

/**
 * M = master, S = server, C = client, A = any
 * the second character will allways be \n if the message isn't a single
 * byte long (?? not true anymore?)
 */

#define S2C_CHALLENGE       'c'
#define A2A_PING            'k'

/* + serverinfo + userlist + fraglist */
#define S2M_HEARTBEAT       'a'
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

qboolean is_server = qtrue;

extern sizebuf_t net_message;

static server_t *sv_list = NULL;
static filter_t *filter_list = NULL;

static netadr_t net_local_adr;

/**
 * @brief Dynamic length version of Qgets. DO NOT free the buffer
 */
char* Qgetline (FILE *file)
{
	static int  size = 256;
	static char *buf = 0;
	int         len;

	if (!buf)
		buf = malloc (size);

	if (!fgets (buf, size, file))
		return 0;

	len = strlen (buf);
	while (buf[len - 1] != '\n') {
		char       *t = realloc (buf, size + 256);

		if (!t)
			return 0;
		buf = t;
		size += 256;
		if (!fgets (buf + len, size - len, file))
			break;
		len = strlen (buf);
	}
	return buf;
}

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
	FILE       *filters;

	NET_Init ();

	/* Add filters */
	if ((filters = fopen ("filters.ini", "r"))) {
		while ((str = Qgetline (filters))) {
			Cbuf_AddText ("filter add ");
			Cbuf_AddText (str);
			Cbuf_AddText ("\n");
		}
		fclose (filters);
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

	printf ("%s >> unknown packet:\n", NET_AdrToString (net_from));

	data = net_message.data;
	size = net_message.cursize;

	for (p = data; (rsize = min (size - (p - data), 16)); p += rsize) {
		printf ("%04X:", (unsigned) (p - data));
		memcpy (buf, p, rsize);
		for (i = 0; i < rsize; i++) {
			printf (" %02X", buf[i]);
			if (buf[i] < ' ' || buf [i] > '~')
				buf[i] = '.';
		}
		printf ("%*.*s\n", 1 + (16 - rsize) * 3 + rsize, rsize, buf);
	}
}

/**
 * @brief
 */
static void Mst_SendList (void)
{
	byte        buf[MAX_MSGLEN];
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
	NET_SendPacket (ip_sockets[NS_SERVER], msg.cursize, msg.data, net_from);
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
	msg = net_message.data[1];
	if (msg == A2A_PING) {
		Filter ();
		printf ("%s >> A2A_PING\n", NET_AdrToString (net_from));
		if (!(sv = SVL_Find (net_from))) {
			sv = SVL_New (&net_from);
			SVL_Add (sv);
		}
		sv->timeout = Sys_Milliseconds ();
	} else if (msg == S2M_HEARTBEAT) {
		Filter ();
		printf ("%s >> S2M_HEARTBEAT\n", NET_AdrToString (net_from));
		if (!(sv = SVL_Find (net_from))) {
			sv = SVL_New (&net_from);
			SVL_Add (sv);
		}
		sv->timeout = Sys_Milliseconds ();
	} else if (msg == S2M_SHUTDOWN) {
		Filter ();
		printf ("%s >> S2M_SHUTDOWN\n", NET_AdrToString (net_from));
		if ((sv = SVL_Find (net_from))) {
			SVL_Remove (sv);
			free (sv);
		}
	} else if (msg == S2C_CHALLENGE) {
		printf ("%s >> ", NET_AdrToString (net_from));
		printf ("Gamespy server list request\n");
		Mst_SendList ();
	} else {
		byte       *p;

		p = net_message.data;
		if (p[0] == 0 && p[1] == 'y') {
			printf ("%s >> ", NET_AdrToString (net_from));
			printf ("Pingtool server list request\n");
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
	while (NET_GetPacket(ip_sockets[NS_SERVER], &net_from, &net_message)) {
		Mst_Packet ();
	}
}

/**
 * @brief
 */
static void FilterAdd (char* arg)
{
	filter_t   *filter;
	netadr_t    to, from;
	char		*d;

	d = strstr(arg, " ");
	if (d)
		*d++ = '\0';
	else
		return; /* wrong parameters */

	NET_StringToAdr (arg, &from);
	NET_StringToAdr (d, &to);
	if (to.port == 0)
		from.port = BigShort (PORT_SERVER);
	if (from.port == 0)
		from.port = BigShort (PORT_SERVER);
	if (!(filter = FL_Find (from))) {
		printf ("Added filter %s\t\t%s\n", arg, d);
		filter = FL_New (&from, &to);
		FL_Add (filter);
	} else
		printf ("%s already defined\n\n", arg);
}

/**
 * @brief
 */
static void FilterRemove (char* arg)
{
	filter_t   *filter;
	netadr_t    from;

	NET_StringToAdr (arg, &from);
	if ((filter = FL_Find (from))) {
		printf ("Removed %s\n\n", arg);
		FL_Remove (filter);
		free (filter);
	} else
		printf ("Cannot find %s\n\n", arg);
}

/**
 * @brief
 */
static void FilterList (void)
{
	filter_t   *filter;

	for (filter = filter_list; filter; filter = filter->next) {
		printf ("%s", NET_AdrToString (filter->from));
		printf ("\t\t%s\n", NET_AdrToString (filter->to));
	}
	if (filter_list == NULL)
		printf ("No filter\n");
	printf ("\n");
}

/**
 * @brief
 */
static void FilterClear (void)
{
	printf ("Removed all filters\n\n");
	FL_Clear ();
}

/**
 * @brief
 */
static void Filter_f (char* action, char* param)
{
	if (!strcmp (action, "add"))
		FilterAdd (param);
	else if (!strcmp (action, "remove"))
		FilterRemove (param);
	else if (!strcmp (action, "clear"))
		FilterClear ();
	else if (action && param) {
		FilterAdd (action);
	} else if (action && !param) {
		FilterRemove (action);
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
		printf("ERROR: couldn't open filters.ini.\n");
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
static void MS_Shutdown (void)
{
	NET_Shutdown ();

	/* write filter list */
	SV_WriteFilterList ();
/*	Con_Shutdown ();*/
}

/**
 * @brief
 */
static void MS_TimeOut (void)
{
	/* Remove listed servers that haven't sent a heartbeat for some time */
	double      time = Sys_Milliseconds ();
	server_t   *sv;
	server_t   *next;

	if (sv_list == NULL)
		return;

	for (sv = sv_list; sv;) {
		if (sv->timeout + SV_TIMEOUT < time) {
			next = sv->next;
			printf ("%s timed out\n", NET_AdrToString (sv->ip));
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
int Sys_CheckInput (int idle, int net_socket)
{
	fd_set      fdset;
	int         res;
	struct timeval _timeout;
	struct timeval *timeout = 0;

#ifdef _WIN32
	int         sleep_msec;
	/* Now we want to give some processing time to other applications, */
	/* such as qw_client, running on this machine. */
	sleep_msec = 10;
	if (sleep_msec > 0) {
		if (sleep_msec > 13)
			sleep_msec = 13;
		Sleep (sleep_msec);
	}

	_timeout.tv_sec = 0;
	_timeout.tv_usec = net_socket < 0 ? 0 : 100;
#else
	_timeout.tv_sec = 0;
	_timeout.tv_usec = net_socket < 0 ? 0 : 10000;
#endif
	/**
	 * select on the net socket and stdin
	 * the only reason we have a timeout at all is so that if the last
	 * connected client times out, the message would not otherwise
	 * be printed until the next event.
	 */
	FD_ZERO (&fdset);

#if 0
	if (do_stdin)
		FD_SET (0, &fdset);
#endif

	if (net_socket >= 0)
		FD_SET (net_socket, &fdset);

	if (!idle)
		timeout = &_timeout;

	res = select (max (net_socket, 0) + 1, &fdset, NULL, NULL, timeout);
	if (res == 0 || res == -1)
		return 0;
#if 0
	stdin_ready = FD_ISSET (0, &fdset);
#endif
	return 1;
}

/**
 * @brief
 */
static void MS_Frame (void)
{
/*	Sys_CheckInput (1, ip_sockets[NS_SERVER]);
	Con_ProcessInput ();
	Cbuf_Execute();*/
	MS_TimeOut ();
	SV_ReadPackets ();
}

/**
 * @brief
 */
int main (int argc, const char **argv)
{
	Sys_Init ();

#if 0
	Cmd_AddCommand ("quit", MST_Quit_f, "Shut down the master server");
	Cmd_AddCommand ("clear", SVL_Clear, "Clear the server list");
	Cmd_AddCommand ("filter", Filter_f, "Manipulate filtering");

/*	Cmd_StuffCmds ();*/
	Cbuf_Execute ();

#endif
	SV_InitNet ();
	printf ("Exe: " __TIME__ " " __DATE__ "\n");
	printf ("======== UFO:AI master initialized ========\n\n");
	while (1) {
		MS_Frame ();
	}
	printf ("UFO:AI master shutdown\n");
	MS_Shutdown();
	return 0;
}
