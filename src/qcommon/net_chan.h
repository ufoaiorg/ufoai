/*
==============================================================

NET

==============================================================
*/

/* net.h -- quake's interface to the networking layer */
#ifndef COMMON_NET_CHAN_H
#define COMMON_NET_CHAN_H

#include "common.h"

#define	PORT_ANY	-1

#define	MAX_MSGLEN		1400	/* max length of a message */
#define	PACKET_HEADER	10		/* two ints and a short */

#ifdef HAVE_IPV6
typedef enum { NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX, NA_IPV6, NA_MULTICAST6 } netadrtype_t;
#else							/* HAVE_IPV6 */
typedef enum { NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX } netadrtype_t;
#endif							/* HAVE_IPV6 */

typedef enum { NS_CLIENT, NS_SERVER } netsrc_t;

typedef struct {
	netadrtype_t type;
#ifdef HAVE_IPV6
	/* TODO: Use sockaddr_storage instead */
	byte ip[16];
	unsigned int scope_id;
#else							/* HAVE_IPV6 */
	byte ip[4];
#endif							/* HAVE_IPV6 */
	byte ipx[10];

	unsigned short port;
} netadr_t;

void NET_Init(void);
void NET_Shutdown(void);

void NET_Config(qboolean multiplayer);

qboolean NET_GetPacket(netsrc_t sock, netadr_t * net_from, sizebuf_t * net_message);
void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to);

qboolean NET_CompareAdr(netadr_t a, netadr_t b);
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
qboolean NET_IsLocalAddress(netadr_t adr);
char *NET_AdrToString(netadr_t a);
qboolean NET_StringToAdr(char *s, netadr_t * a);
void NET_Sleep(int msec);

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

qboolean Netchan_NeedReliable(netchan_t * chan);
void Netchan_Transmit(netchan_t * chan, int length, byte * data);
void Netchan_OutOfBand(int net_socket, netadr_t adr, int length, byte * data);
void Netchan_OutOfBandPrint(int net_socket, netadr_t adr, char *format, ...);
qboolean Netchan_Process(netchan_t * chan, sizebuf_t * msg);

qboolean Netchan_CanReliable(netchan_t * chan);

#endif /* COMMON_NET_CHAN_H */
