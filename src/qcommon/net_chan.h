/**
 * @file net_chan.h
 * @brief Quake's interface to the networking layer
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/qcommon/net_chan.c
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


#ifdef _WINDOWS
#include <winsock.h>
#endif

#ifdef __linux__
#include <arpa/inet.h>
#endif

#define	PORT_ANY	-1

#define	MAX_MSGLEN		7400	/* max length of a message */
#define	PACKET_HEADER	10		/* two ints and a short */

/**
 * @brief network address types
 */
#ifdef HAVE_IPV6
typedef enum { NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX, NA_IPV6, NA_MULTICAST6 } netadrtype_t;
#else							/* HAVE_IPV6 */
typedef enum { NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX } netadrtype_t;
#endif							/* HAVE_IPV6 */

typedef enum { NS_CLIENT, NS_SERVER } netsrc_t;

/**
 * @brief Structure describing an Internet address.
 */
typedef struct {
	netadrtype_t type;	/**< address type */
#ifdef HAVE_IPV6
	/* @todo: Use sockaddr_storage instead */
	byte ip[16];		/**< IP */
	unsigned int scope_id;	/**< IPv6 scope-id */
#else							/* HAVE_IPV6 */
	byte ip[4];		/**< IP number */
#endif							/* HAVE_IPV6 */
	byte ipx[10];		/**< IPX number */

	unsigned short port;	/**< port number */
} netadr_t;

void NET_Init(void);
void NET_Shutdown(void);

void NET_Config(qboolean multiplayer);

int NET_GetPacket(netsrc_t sock, netadr_t * net_from, sizebuf_t * net_message);
void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to);

qboolean NET_CompareAdr(netadr_t a, netadr_t b);
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
qboolean NET_IsLocalAddress(netadr_t adr);
char *NET_AdrToString(netadr_t a);
qboolean NET_StringToAdr(char *s, netadr_t * a);
void NET_Sleep(int msec);
char *NET_SocketToString(void *s_ptr);
#ifdef HAVE_IPV6
int NET_Socket(char *net_interface, int port, netsrc_t type, int family);
#else
int NET_Socket(char *net_interface, int port);
#endif
/*============================================================================ */

#define	OLD_AVG		0.99		/* total = oldtotal*OLD_AVG + new*(1-OLD_AVG) */

#define	MAX_LATENT	32

typedef struct {
	qboolean fatal_error;

	netsrc_t sock;

	int dropped;				/* between last packet and previous */

	int last_received;			/* for timeouts */
	int last_sent;				/* for retransmits */

	netadr_t remote_address;
	int qport;					/* qport value to write when transmitting */

	/* sequencing variables */
	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;	/* single bit */

	int incoming_reliable_sequence;	/* single bit, maintained local */

	int outgoing_sequence;
	int reliable_sequence;		/* single bit */
	int last_reliable_sequence;	/* sequence number of last send */

	/* reliable staging and holding areas */
	sizebuf_t message;			/* writing buffer to send to server */
	byte message_buf[MAX_MSGLEN - 16];	/* leave space for header */

	/* message is copied to this buffer when it is first transfered */
	int reliable_length;
	byte reliable_buf[MAX_MSGLEN - 16];	/* unacked reliable message */
} netchan_t;

extern netadr_t net_from;
extern sizebuf_t net_message;
extern byte net_message_buffer[MAX_MSGLEN];


void Netchan_Init(void);
void Netchan_Setup(netsrc_t sock, netchan_t * chan, netadr_t adr, int qport);

void Netchan_Transmit(netchan_t * chan, int length, byte * data);
void Netchan_OutOfBandPrint(int net_socket, netadr_t adr, char *format, ...) __attribute__((format(printf, 3, 4)));
qboolean Netchan_Process(netchan_t * chan, sizebuf_t * msg);

void Net_Stats_f(void);
