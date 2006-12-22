/**
 * @file master.c
 * @brief Master server for UFO:AI
 */

/*
Copyright (C) 2002-2003 r1ch.net

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

/**
 * gloommaster 0.0.3 (c) 2002-2003 r1ch.net
 * quake 2 compatible master server for gloom
 */

/* change this to the game name of servers you only wish to accept */
#define MASTER_SERVER "195.136.48.62" /* sponsored by NineX */
#define VERSION "0.0.3"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <malloc.h>
# include <winsock.h>
#else
# include <errno.h>
# include <arpa/inet.h>
# define SOCKET int
# define SOCKET_ERROR -1
# define TIMEVAL struct timeval
# define WSAGetLastError() errno
# define strnicmp strncasecmp
#endif

#ifndef DEBUG
#define dprintf printf
#else
#define dprintf printf
#endif

typedef struct server_s server_t;

struct server_s {
	server_t		*prev;
	server_t		*next;
	struct sockaddr_in	ip;
	unsigned short	port;
	unsigned int	queued_pings;
	unsigned int	heartbeats;
	unsigned long	last_heartbeat;
	unsigned long	last_ping;
	unsigned char	shutdown_issued;
	unsigned char	validated;
};

server_t servers;

struct sockaddr_in listenaddress;
SOCKET listener;
TIMEVAL delay;
#ifdef _WIN32
WSADATA ws;
#endif
fd_set set;
char incoming[150000];
int retval;
FILE *logfile;

#if 0
/**
 * @brief
 * @TODO: Call this on signals send to the server
 */
static void MS_ExitNicely (void)
{
	server_t	*server = &servers;
	server_t	*old = NULL;

	printf ("[I] shutting down.\n");

	while (server->next) {
		if (old)
			free (old);
		server = server->next;
		old = server;
	}

	if (old)
		free (old);
}
#endif

/**
 * @brief
 */
static void MS_DropServer (server_t *server)
{
	/* unlink */
	if (server->next)
		server->next->prev = server->prev;

	if (server->prev)
		server->prev->next = server->next;

	/* free */
	free (server);
}

/**
 * @brief
 */
static void MS_AddServer (struct sockaddr_in *from, int normal)
{
	server_t	*server = &servers;
	int			preserved_heartbeats = 0;

	while (server->next) {
		server = server->next;
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
			/* already exists - could be a pending shutdown (ie killserver, change of map, etc) */
			if (server->shutdown_issued) {
				dprintf ("[I] scheduled shutdown server %s sent another ping!\n", inet_ntoa (from->sin_addr));
				MS_DropServer (server);
				server = &servers;
				while (server->next)
					server = server->next;
				break;
			} else {
				dprintf ("[W] dupe ping from %s!! ignored.\n", inet_ntoa (server->ip.sin_addr));
				return;
			}
		}
	}

	server->next = (server_t *)malloc(sizeof(server_t));

	server->next->prev = server;
	server = server->next;

	server->heartbeats = preserved_heartbeats;
	memcpy (&server->ip, from, sizeof(server->ip));
	server->last_heartbeat = time(0);
	server->next = NULL;
	server->port = from->sin_port;
	server->shutdown_issued = server->queued_pings = server->last_ping = server->validated = 0;

	dprintf ("[I] server %s added to queue! (%d)\n", inet_ntoa (from->sin_addr), normal);
	/* write out to log file, I want to track this */
#if 0
	logfile = fopen(inet_ntoa(from->sin_addr), "w");
	fprintf(logfile, "%s\n", inet_ntoa(from->sin_addr));
	fclose(logfile);
#endif
	if (normal) {
		struct sockaddr_in addr;
		memcpy (&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));
		sendto (listener, "\xFF\xFF\xFF\xFFack", 7, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
}

/**
 * @brief
 */
static void MS_QueueShutdown (struct sockaddr_in *from, server_t *myserver)
{
	server_t	*server = &servers;

	if (!myserver) {
		while (server->next) {
			server = server->next;
			if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
				myserver = server;
				break;
			}
		}
	}

	if (myserver) {
		struct sockaddr_in addr;
		memcpy (&addr.sin_addr, &myserver->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));

		/* hack, server will drop in next minute IF it doesn't respond to our ping */
		myserver->shutdown_issued = 1;

		dprintf ("[I] %s shutdown queued\n", inet_ntoa (myserver->ip.sin_addr));
		sendto (listener, "\xFF\xFF\xFF\xFFping", 8, 0, (struct sockaddr *)&addr, sizeof(addr));
		return;
	} else {
		dprintf ("[W] shutdown issued from unregistered server %s!\n", inet_ntoa (from->sin_addr));
	}
}

/**
 * @brief
 */
static void MS_RunFrame (void)
{
	server_t		*server = &servers;
	unsigned int	curtime = time(0);

	while (server->next) {
		server = server->next;
		if (curtime - server->last_heartbeat > 30) {
			server_t *old = server;

			server = old->prev;

			if (old->shutdown_issued || old->queued_pings > 6) {
				dprintf ("[I] %s shut down.\n", inet_ntoa (old->ip.sin_addr));
				MS_DropServer (old);
				continue;
			}

			server = old;
			if (curtime - server->last_ping >= 10) {
				struct sockaddr_in addr;
				memcpy (&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
				addr.sin_family = AF_INET;
				addr.sin_port = server->port;
				memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));
				server->queued_pings++;
				server->last_ping = curtime;
				dprintf ("[I] ping %s\n", inet_ntoa (server->ip.sin_addr));
				sendto (listener, "\xFF\xFF\xFF\xFFping", 8, 0, (struct sockaddr *)&addr, sizeof(addr));
			}
		}
	}
}

/**
 * @brief Submit the server list to the requesting client
 */
static void MS_SendServerListToClient (struct sockaddr_in *from)
{
	/* unsigned short	port; */
	int				buflen;
	char			buff[0xFFFF];
	server_t		*server = &servers;
	struct sockaddr_in ufoservers;
	unsigned int addr;

	buflen = 0;
	memset (buff, 0, sizeof(buff));

	memcpy (buff, "\xFF\xFF\xFF\xFFservers ", 12);
	buflen += 12;

	while (server->next) {
		server = server->next;
		/*if (server->heartbeats >= 2 && !server->shutdown_issued && server->validated) { */
			memcpy (buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;
			/*port = ntohs(server->port); */
			memcpy (buff + buflen, &server->port, 2);
			buflen += 2;
		/*}*/
	}

	addr = inet_addr(MASTER_SERVER);

	ufoservers.sin_port = htons (27910);
	memcpy (buff + buflen, &addr, 4);
	buflen += 4;
	memcpy (buff + buflen, &ufoservers.sin_port, 4);
	buflen += 2;

	dprintf ("[I] query response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa (from->sin_addr), ntohs (from->sin_port));

	if ((sendto (listener, buff, buflen, 0, (struct sockaddr *)from, sizeof(*from))) == SOCKET_ERROR) {
		dprintf ("[E] socket error on send! code %d.\n", WSAGetLastError());
	}

	dprintf ("[I] sent server list to client %s\n", inet_ntoa (from->sin_addr));
}

/**
 * @brief
 */
static void MS_Ack (struct sockaddr_in *from)
{
	server_t	*server = &servers;

	/* iterate through known servers */
	while (server->next) {
		server = server->next;
		/* a match! */
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
			printf ("[I] ack from %s (%d).\n", inet_ntoa (server->ip.sin_addr), server->queued_pings);
			server->last_heartbeat = time(0);
			server->queued_pings = 0;
			server->heartbeats++;
			return;
		}
	}
}

/**
 * @brief
 */
static void MS_HeartBeat (struct sockaddr_in *from, char *data)
{
	server_t	*server = &servers;

	/* iterate through known servers */
	while (server->next) {
		server = server->next;
		/* a match! */
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
			struct sockaddr_in addr;

			memcpy (&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
			addr.sin_family = AF_INET;
			addr.sin_port = server->port;
			memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));

			server->validated = 1;
			server->last_heartbeat = time(0);
			dprintf ("[I] heartbeat from %s.\n", inet_ntoa (server->ip.sin_addr));
			sendto (listener, "\xFF\xFF\xFF\xFFack", 7, 0, (struct sockaddr *)&addr, sizeof(addr));
			return;
		}
	}

	MS_AddServer (from, 0);
}

/**
 * @brief
 */
static void MS_ParseResponse (struct sockaddr_in *from, char *data, int dglen)
{
	char *cmd = data;
	char *line = data;

	while (*line && *line != '\n')
		line++;

	*(line++) = '\0';
	cmd += 4;

	/* dprintf ("[I] %s:%d : query (%d bytes) '%s'\n", inet_ntoa(from->sin_addr), htons(from->sin_port), dglen, data); */

	if (strnicmp (cmd, "ping", 4) == 0) {
		MS_AddServer (from, 1);
	} else if (strnicmp (cmd, "heartbeat", 9) == 0 || strnicmp (cmd, "print", 5) == 0) {
		MS_HeartBeat (from, line);
	} else if (strncmp (cmd, "ack", 3) == 0) {
		MS_Ack (from);
	} else if (strnicmp (cmd, "shutdown", 8) == 0) {
		MS_QueueShutdown (from, NULL);
	} else if (strnicmp (cmd, "getservers", 10) == 0) {
		MS_SendServerListToClient (from);
	} else {
		printf ("[W] Unknown command from %s!\n... cmd: '%s'", inet_ntoa (from->sin_addr), cmd);
	}
}

/**
 * @brief
 */
int main (int argc, char **argv)
{
	int len;
	size_t fromlen;
	struct sockaddr_in from;

	printf ("ufomaster %s\n", VERSION);

#ifdef _WIN32
	WSAStartup ((WORD)MAKEWORD (1,1), &ws);
#endif

	listener = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	memset (&listenaddress, 0, sizeof(listenaddress));

	listenaddress.sin_family = AF_INET;
	listenaddress.sin_port = htons(27900);
	listenaddress.sin_addr.s_addr = INADDR_ANY;

	if ((bind (listener, (struct sockaddr *)&listenaddress, sizeof(listenaddress))) == SOCKET_ERROR) {
		printf ("[E] Couldn't bind to port 27900 UDP (something is probably using it)\n");
		return 1;
	}

	delay.tv_sec = 1;
	delay.tv_usec = 0;

	/* listen (listener, SOMAXCONN); */

	memset (&servers, 0, sizeof(servers));

	printf ("listening on port 27900 (UDP)\n");

	while (1) {
		FD_ZERO(&set);
		FD_SET (listener, &set);
		delay.tv_sec = 1;
		delay.tv_usec = 0;

		retval = select(listener+1, &set, NULL, NULL, &delay);
		if (retval > 0) {
			if (FD_ISSET(listener, &set)) {
				fromlen = sizeof(from);
				len = recvfrom (listener, incoming, sizeof(incoming), 0, (struct sockaddr *)&from, &fromlen);
				if (len > 0) {
					if (len > 4) {
						/* parse this packet */
						MS_ParseResponse (&from, incoming, len);
					} else {
						dprintf ("[W] runt packet from %s:%d\n", inet_ntoa (from.sin_addr), ntohs(from.sin_port));
					}

					/* reset for next packet */
					memset (incoming, 0, sizeof(incoming));
				} else {
					dprintf ("[E] socket error during select from %s:%d (%d)\n", inet_ntoa (from.sin_addr), ntohs(from.sin_port), WSAGetLastError());
				}
			}
		}

		/* destroy old servers, etc */
		MS_RunFrame ();
	}
}
