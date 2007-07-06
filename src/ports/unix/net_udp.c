/**
 * @file net_udp.c
 * @brief Unix IPv4 network code
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/linux/net_udp.c
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

#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>
#include <arpa/inet.h>

#ifdef NeXT
#  include <libc.h>
#endif

#ifdef __sun
#  include <sys/filio.h>
#endif

static netadr_t net_local_adr;

#define	LOOPBACK	0x7f000001

#define	MAX_LOOPBACK	4

typedef struct
{
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

static loopback_t	loopbacks[2];
static int			ip_sockets[2];
static int			ipx_sockets[2];

int NET_Socket(char *net_interface, int port);
char *NET_ErrorString(void);
static cvar_t *net_ignore_icmp;


#define	MAX_IPS		16
static	int		numIP;
static	byte	localIP[MAX_IPS][4];

static unsigned int net_inittime;

static unsigned long long net_total_in;
static unsigned long long net_total_out;
static unsigned long long net_packets_in;
static unsigned long long net_packets_out;

/**
 * @brief Print network statistics
 */
void Net_Stats_f (void)
{
	int now = time(0);
	int diff = now - net_inittime;

	Com_Printf("Network up for %i seconds.\n"
			"%llu bytes in %llu packets received (av: %i kbps)\n"
			"%llu bytes in %llu packets sent (av: %i kbps)\n", diff,
	net_total_in, net_packets_in, (int)(((net_total_in * 8) / 1024) / diff),
	net_total_out, net_packets_out, (int)((net_total_out * 8) / 1024) / diff);
}

/**
 * @brief Print the local IP addresses
 * @sa NET_GetLocalAddress
 */
void Sys_ShowIP (void)
{
	int i;

	for (i = 0; i < numIP; i++)
		Com_Printf("IP: %i.%i.%i.%i\n", localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3]);
}

/**
 * @brief Get our local IP addresses
 * @note The found addresses are stored in a global array localIP
 */
void NET_GetLocalAddress (void)
{
	char hostname[256];
	struct hostent *hostInfo;
	char *p;
	int ip, n;

	if (gethostname(hostname, 256) == -1)
		return;

	hostInfo = gethostbyname(hostname);
	if (!hostInfo)
		return;

	Com_Printf("Hostname: %s\n", hostInfo->h_name);
	n = 0;
	while ((p = hostInfo->h_aliases[n++]) != NULL)
		Com_Printf("Alias: %s\n", p);

	if (hostInfo->h_addrtype != AF_INET)
		return;

	numIP = 0;
	while ((p = hostInfo->h_addr_list[numIP++]) != NULL && numIP < MAX_IPS) {
		ip = ntohl(*(int *)p);
		localIP[numIP][0] = p[0];
		localIP[numIP][1] = p[1];
		localIP[numIP][2] = p[2];
		localIP[numIP][3] = p[3];
		Com_Printf("IP: %i.%i.%i.%i\n", ( ip >> 24 ) & 0xff, ( ip >> 16 ) & 0xff, ( ip >> 8 ) & 0xff, ip & 0xff);
	}
}

/**
 * @brief Convert a network address to a socket one
 * @param[in] a Pointer to network address which is to be converted
 * @param[out] s Pointer to where the socket address will be written to
 * @sa SockadrToNetadr
 */
void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset(s, 0, sizeof(*s));

	if (a->type == NA_BROADCAST) {
		s->sin_family = AF_INET;

		s->sin_port = a->port;
		*(int *)&s->sin_addr = PORT_ANY;
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
void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
	*(int *)&a->ip = *(int *)&s->sin_addr;
	a->port = s->sin_port;
	a->type = NA_IP;
}


/**
 * @brief Converts a socket address to a string representation
 * @param[in] s_ptr Pointer to the socket address
 * @returns A pointer to the string representation of this socket address
 * @post the returned pointer is not null
 * @sa SockadrToNetadr
 */
char *NET_SocketToString (void *s_ptr)
{
	netadr_t addr;
	struct sockaddr_in *s = (struct sockaddr_in*)s_ptr;

	if (!s)
		return "";
	memset(&addr, 0, sizeof(netadr_t));
	SockadrToNetadr(s, &addr);
	return NET_AdrToString(addr);
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

	switch (a.type) {
#ifndef DEDICATED_ONLY
	case NA_LOOPBACK:
		return qtrue;
#endif

	case NA_IP:
		if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
			return qtrue;
		return qfalse;

	case NA_IPX:
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
			return qtrue;
		return qfalse;
	default:
		return qfalse;
	}
	return qfalse;
}

/**
 * @brief convert a network address to a string - port included
 * @param[in] a The network address which is to be converted
 * @returns a pointer to the string representation of the address
 * @post the returned pointer is not null
 * @sa *NET_BaseAdrToString
 */
char *NET_AdrToString (netadr_t a)
{
	static	char	s[64];

	Com_sprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));

	return s;
}

/**
 * @brief convert a network address to a string - port excluded
 * @param[in] a The network address which is to be converted
 * @returns a pointer to the string representation of the address
 * @postcond the returned pointer is not null
 * @sa *NET_AdrToString
 */
char *NET_BaseAdrToString (netadr_t a)
{
	static	char	s[64];

	Com_sprintf(s, sizeof(s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);

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
qboolean NET_StringToSockaddr (char *s, struct sockaddr *sadr)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	memset(sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;

	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpyz(copy, s, sizeof(copy));
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
qboolean NET_StringToAdr (char *s, netadr_t *a)
{
	struct sockaddr_in sadr;

#ifndef DEDICATED_ONLY
	if (!strcmp(s, "localhost")) {
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return qtrue;
	}
#endif

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

/**
 * @brief
 */
qboolean NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
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
void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

/**
 * @brief
 */
int NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret;
	struct sockaddr_in	from;
	socklen_t	fromlen;
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

		net_packets_in++;
		net_total_in += ret;

		SockadrToNetadr(&from, net_from);

		if (ret == -1) {
			err = errno;

			switch (err) {
			case ECONNREFUSED:
			case EHOSTUNREACH:
			case ENETUNREACH:
				Com_Printf("NET_GetPacket: %s from %s\n", strerror(err), NET_AdrToString(*net_from));
				if (net_ignore_icmp->integer)
					return 0;
				else
					return -1;
			default:
/*				Com_Printf("NET_GetPacket: %s from %s\n", strerror(err), NET_AdrToString(*net_from));*/
				continue;
			}
			if (err == EWOULDBLOCK || err == ECONNREFUSED)
				continue;
			Com_Printf("NET_GetPacket: %s from %s\n", NET_ErrorString(),
						NET_AdrToString(*net_from));
			continue;
		}

		if (ret == net_message->maxsize) {
			Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
			continue;
		}

		net_message->cursize = ret;
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
	int		net_socket = 0;

	switch (to.type) {
#ifndef DEDICATED_ONLY
	case NA_LOOPBACK:
		NET_SendLoopPacket(sock, length, data, to);
		return;
#endif
	case NA_BROADCAST:
	case NA_IP:
		net_socket = ip_sockets[sock];
		break;
	case NA_BROADCAST_IPX:
	case NA_IPX:
		net_socket = ipx_sockets[sock];
		break;
	default:
		Com_Error(ERR_FATAL, "NET_SendPacket: bad address type");
		return;
	}
	if (!net_socket)
		return;

	NetadrToSockadr(&to, &addr);

	ret = sendto(net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr) );
	if (ret == -1) {
		Com_Printf("NET_SendPacket Warning: %s to %s\n", NET_ErrorString(),
			NET_AdrToString(to));
	} else {
		net_packets_out++;
		net_total_out += ret;
	}
}


/**
 * @brief Open the server and client IP sockets
 * @sa NET_OpenIPX
 * @todo: should return a value to indicate success/failure
 */
void NET_OpenIP (void)
{
	cvar_t	*ip;
	int port;

	/* lookup the server port from cvar "ip_hostport",
	 * if it isn't set, just use PORT_SERVER */
	port = Cvar_Get("ip_hostport", va("%i", PORT_SERVER), CVAR_NOSET, NULL)->integer;
	/* for our purposes port 0 is invalid */
	/* if we didn't get a valid port from ip_hostport nor PORT_SERVER */
	if (!port) {
		/* try cvar "hostport" */
		port = Cvar_Get("hostport", "0", CVAR_NOSET, NULL)->integer;
		if (!port) {
			/* if that failed, try cvar "port" */
			port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET, NULL)->integer;
		}
	}
	/* at this point port can still be an invalid number */

	/* get our ip address from cvar "ip" or use "localhost" if it is not set */
	ip = Cvar_Get("ip", "localhost", CVAR_NOSET, NULL);

	if (!ip_sockets[NS_SERVER]) {
		/* there is no server socket, so let's create one */
		ip_sockets[NS_SERVER] = NET_Socket(ip->string, port);
		if (!ip_sockets[NS_SERVER] && dedicated) {
			/* if we couldn't create a socket and this is
			 * a dedicated server just die miserably */
			/* @todo: should be more robust than this */
			Com_Error(ERR_FATAL, "Couldn't allocate server IP port (%i) - use cvar ip_hostport", port);
		}
	}
	/* at this point we must have a valid server socket open */

	/* lookup the client port from cvar "clientport",
	 * if it isn't set, just use PORT_CLIENT */
	port = Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET, NULL)->integer;
	if (!ip_sockets[NS_CLIENT]) {
		/* if there is no client socket, create one */
		ip_sockets[NS_CLIENT] = NET_Socket(ip->string, port);
		if (!ip_sockets[NS_CLIENT]) {
			/* if we couldn't create a client socket with the found port,
			 * try creating it with _any_ port */
			ip_sockets[NS_CLIENT] = NET_Socket(ip->string, PORT_ANY);
		}
	}
	/* at this point we should have a client socket running
	 * with either a set port or random one, or no socket at all */

	/* reset connection counters */
	net_total_in = net_packets_in = net_total_out = net_packets_out = 0;
	net_inittime = (unsigned int)time(NULL);
}

/**
 * @brief Open the server and client IPX sockets
 * @note This does nothing.
 * @sa NET_OpenIP
 * @todo: should return a value to indicate success/failure
 */
void NET_OpenIPX (void)
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
	int i;

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


/**
 * @brief
 */
void NET_Init (void)
{
	net_ignore_icmp = Cvar_Get("net_ignore_icmp", "0", 0, NULL);

	NET_GetLocalAddress();
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
	qboolean _true = qtrue;
	int	i = 1;

	if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		Com_Printf("ERROR: UDP_OpenSocket: socket: %s", NET_ErrorString());
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

#if defined (__APPLE__) || defined (MACOSX)
	if (!net_interface || !net_interface[0] || !strcasecmp(net_interface, "localhost"))
#else
	if (!net_interface || !net_interface[0] || !stricmp(net_interface, "localhost"))
#endif /* __APPLE__ || MACOSX */
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

	if (!ip_sockets[NS_SERVER] || (dedicated && !dedicated->integer))
		return; /* we're not a server, just run full speed */

	FD_ZERO(&fdset);
	if (stdin_active)
		FD_SET(0, &fdset); /* stdin is processed too */
	FD_SET(ip_sockets[NS_SERVER], &fdset); /* network socket */
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(ip_sockets[NS_SERVER]+1, &fdset, NULL, NULL, &timeout);
}

