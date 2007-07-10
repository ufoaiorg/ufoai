/**
 * @file solaris/net_udp.c
 * @brief Solaris network udp functions
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/solaris/net_udp.c
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

#include "../../qcommon/qcommon.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/filio.h>

#ifdef NeXT
#include <libc.h>
#endif

netadr_t	net_local_adr;

#define	LOOPBACK	0x7f000001

#define	MAX_LOOPBACK	4

typedef struct {
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

#ifndef DEDICATED_ONLY
loopback_t loopbacks[2];
#endif
int			ip_sockets[2];
int			ipx_sockets[2];

int NET_Socket (char *net_interface, int port);
char *NET_ErrorString (void);

/*============================================================================= */

/**
 * @brief Convert a network address to a socket one
 * @param[in] a Pointer to network address which is to be converted
 * @param[out] s Pointer to where the socket address will be written to
 * @sa SockadrToNetadr
 */
static void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset(s, 0, sizeof(*s));

	if (a->type == NA_BROADCAST) {
		s->sin_family = AF_INET;

		s->sin_port = a->port;
		*(int *)&s->sin_addr = -1;
	} else if (a->type == NA_IP) {
		s->sin_family = AF_INET;

		*(int *)&s->sin_addr = *(int *)&a->ip;
		s->sin_port = a->port;
	}
}

/**
 * @brief Convert a socket address to a network one
 * @param[in] s Pointer to the socket address which is to be converted
 * @param[out] a Pointer to where the network address will be written to
 * @sa NetadrToSockadr
 */
static void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
	*(int *)&a->ip = *(int *)&s->sin_addr;
	a->port = s->sin_port;
	a->type = NA_IP;
}


/**
 * @brief Compare two network addresses
 * @param[in] a The first address
 * @param[in] b The second address
 * @returns qtrue if their types are the same and
 * they are either loopback addresses,
 * or if both addresses' IPs and ports are the same.
 * Otherwise it returns false.
 */
qboolean NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
		return qtrue;
	return qfalse;
}

/**
 * @brief Compares two network addresses ignoring their ports
 * @param[in] a The first address
 * @param[in] b The second address
 * @returns qtrue if their types are the same and
 * they are either loopback addresses,
 * or if both addresses' IPs are the same.
 * Otherwise it returns false.
 */
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return qfalse;

	if (a.type == NA_LOOPBACK)
		return qtrue;

	if (a.type == NA_IP) {
		if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
			return qtrue;
		return qfalse;
	}

	if (a.type == NA_IPX) {
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
			return qtrue;
		return qfalse;
	}
}

/**
 * @brief convert a network address to a string - port included
 * @param[in] a The network address which is to be converted
 * @returns a pointer to the string representation of the address
 * @post the returned pointer is not null
 */
char *NET_AdrToString (netadr_t a)
{
	static	char	s[64];

	Com_sprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));

	return s;
}

/**
 * @brief Convert a string to a socket address
 * @param[out] sadr Pointer to where the resulting socket address will be written
 * @param[in] s String which is to be converted to a socket address
 * @returns qtrue if the supplied string was a valid socket address
 * and conversion succeeded. Otherwise returns qfalse.
 * @note valid string formats include: localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
static qboolean NET_StringToSockaddr (const char *s, struct sockaddr *sadr)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	memset(sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;

	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpyz(copy, s, 128);
	/* strip off a trailing :port if present */
	for (colon = copy; *colon; colon++)
		if (*colon == ':') {
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
		}

	if (copy[0] >= '0' && copy[0] <= '9') {
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	} else {
		if (! (h = gethostbyname(copy)) )
			return qfalse;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return qtrue;
}

/**
 * @brief Convert a string to a network address
 * @param[out] a Pointer to where the resulting network address will be written
 * @param[in] s String which is to be converted to a network address
 * @returns qtrue if the supplied string was a valid network address
 * and conversion succeeded. Otherwise returns qfalse.
 * @note valid string formats include: localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
qboolean NET_StringToAdr (const char *s, netadr_t *a)
{
	struct sockaddr_in sadr;

	if (!strcmp(s, "localhost")) {
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return qtrue;
	}

	if (!NET_StringToSockaddr(s, (struct sockaddr *)&sadr))
		return qfalse;

	SockadrToNetadr(&sadr, a);

	return qtrue;
}


/**
 * @brief Decide whether the address is local
 * @param[in] adr The address that is in question
 * @returns qtrue if the supplied address is local. Otherwise returns qfalse.
 */
qboolean NET_IsLocalAddress (netadr_t adr)
{
	return NET_CompareAdr(adr, net_local_adr);
}

/*
=============================================================================
LOOPBACK BUFFERS FOR LOCAL PLAYER
=============================================================================
*/

#ifndef DEDICATED_ONLY
/**
 * @brief
 */
static qboolean NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return qfalse;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	*net_from = net_local_adr;
	return qtrue;

}

/**
 * @brief
 */
static void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}
#endif

/**
 * @brief
 */
int NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret;
	struct sockaddr_in	from;
	int		fromlen;
	int		net_socket;
	int		protocol;
	int		err;

#ifndef DEDICATED_ONLY
	if (NET_GetLoopPacket(sock, net_from, net_message))
		return 1;
#endif

	for (protocol = 0; protocol < 2; protocol++) {
		if (protocol == 0)
			net_socket = ip_sockets[sock];
		else
			net_socket = ipx_sockets[sock];

		if (!net_socket)
			continue;

		fromlen = sizeof(from);
		ret = recvfrom(net_socket, net_message->data, net_message->maxsize
			, 0, (struct sockaddr *)&from, &fromlen);
		if (ret == -1) {
			err = errno;

			if (err == EWOULDBLOCK || err == ECONNREFUSED)
				continue;
			Com_Printf("NET_GetPacket: %s", NET_ErrorString());
			continue;
		}

		if (ret == net_message->maxsize) {
			Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
			continue;
		}

		net_message->cursize = ret;
		SockadrToNetadr(&from, net_from);
		return 1;
	}

	return 0;
}

/**
 * @brief
 */
void NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		ret;
	struct sockaddr_in	addr;
	int		net_socket;

#ifndef DEDICATED_ONLY
	if (to.type == NA_LOOPBACK) {
		NET_SendLoopPacket(sock, length, data, to);
		return;
	}
#endif

	if (to.type == NA_BROADCAST) {
		net_socket = ip_sockets[sock];
		if (!net_socket)
			return;
	} else if (to.type == NA_IP) {
		net_socket = ip_sockets[sock];
		if (!net_socket)
			return;
	} else if (to.type == NA_IPX) {
		net_socket = ipx_sockets[sock];
		if (!net_socket)
			return;
	} else if (to.type == NA_BROADCAST_IPX) {
		net_socket = ipx_sockets[sock];
		if (!net_socket)
			return;
	} else
		Com_Error(ERR_FATAL, "NET_SendPacket: bad address type");

	NetadrToSockadr(&to, &addr);

	ret = sendto(net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr) );
	if (ret == -1)
	{
		Com_Printf("NET_SendPacket ERROR: %i\n", NET_ErrorString());
	}
}


/**
 * @brief Open the server and client IP sockets
 * @sa NET_OpenIPX
 * @todo: should return a value to indicate success/failure
 */
static void NET_OpenIP (void)
{
	cvar_t	*port, *ip;

	/* lookup the server port from cvar "port",
	 * if it isn't set, just use PORT_SERVER */
	port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET);

	/* get our ip address from cvar "ip" or use "localhost" if it is not set */
	ip = Cvar_Get("ip", "localhost", CVAR_NOSET);

	/* create a server socket if there is none yet */
	if (!ip_sockets[NS_SERVER])
		ip_sockets[NS_SERVER] = NET_Socket(ip->string, port->value);
	/* create a client socket if there is none yet */
	if (!ip_sockets[NS_CLIENT])
		ip_sockets[NS_CLIENT] = NET_Socket(ip->string, PORT_ANY);
}

/**
 * @brief Open the server and client IPX sockets
 * @note This does nothing.
 * @sa NET_OpenIP
 * @todo: should return a value to indicate success/failure
 */
static void NET_OpenIPX (void)
{
}


/**
 * @brief Configure the network connections
 * @param multiplayer Specify whether this is a single or a multi player game
 * @note A single player game will only use the loopback code
 * @todo: should return a value to indicate success/failure
 */
void NET_Config (qboolean multiplayer)
{
	int		i;

	if (!multiplayer) {	/* shut down any existing sockets */
		for (i = 0; i < 2; i++) {
			if (ip_sockets[i]) {
				close(ip_sockets[i]);
				ip_sockets[i] = 0;
			}
			if (ipx_sockets[i]) {
				close(ipx_sockets[i]);
				ipx_sockets[i] = 0;
			}
		}
	} else {	/* open sockets */
		NET_OpenIP();
		NET_OpenIPX();
	}
}


/*=================================================================== */


/**
 * @brief
 */
void NET_Init (void)
{
}


/**
 * @brief Create an IP socket
 * param[in] net_interface Interface for this socket
 * param[in] port Port for this socket
 * returns The created socket's descriptor upon success. Otherwise returns 0.
 */
int NET_Socket (char *net_interface, int port)
{
	int newsocket;
	struct sockaddr_in address;
	qboolean _true = true;
	int	i = 1;

	if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		Com_Printf("ERROR: UDP_OpenSocket: socket:", NET_ErrorString());
		return 0;
	}

	/* make it non-blocking */
	if (ioctl(newsocket, FIONBIO, &_true) == -1) {
		Com_Printf("ERROR: UDP_OpenSocket: ioctl FIONBIO:%s\n", NET_ErrorString());
		return 0;
	}

	/* make it broadcast capable */
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) == -1) {
		Com_Printf("ERROR: UDP_OpenSocket: setsockopt SO_BROADCAST:%s\n", NET_ErrorString());
		return 0;
	}

	if (!net_interface || !net_interface[0] || !stricmp(net_interface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr(net_interface, (struct sockaddr *)&address);

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);

	address.sin_family = AF_INET;

	if( bind(newsocket, (void *)&address, sizeof(address)) == -1) {
		Com_Printf("ERROR: UDP_OpenSocket: bind: %s\n", NET_ErrorString());
		close(newsocket);
		return 0;
	}

	return newsocket;
}


/**
 * @brief
 */
void NET_Shutdown (void)
{
	NET_Config(qfalse);	/* close sockets */
}


/**
 * @brief
 */
char *NET_ErrorString (void)
{
	int		code;

	code = errno;
	return strerror(code);
}

/**
 * @brief sleeps msec or until net socket is ready
 */
void NET_Sleep(int msec)
{
	struct timeval timeout;
	fd_set	fdset;
	extern cvar_t *dedicated;
	extern qboolean stdin_active;

	if (!ip_sockets[NS_SERVER] || (dedicated && !dedicated->value))
		return; /* we're not a server, just run full speed */

	FD_ZERO(&fdset);
	if (stdin_active)
		FD_SET(0, &fdset); /* stdin is processed too */
	FD_SET(ip_sockets[NS_SERVER], &fdset); /* network socket */
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(ip_sockets[NS_SERVER]+1, &fdset, NULL, NULL, &timeout);
}

