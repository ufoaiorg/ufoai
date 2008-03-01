/**
 * @file net.c
 * @note This file should fully support ipv6 and any other protocol that is
 * compatible with the getaddrinfo interface, with the exception of
 * broadcast_datagram() which must be amended for each protocol (and
 * currently supports only ipv4)
 * This file includes partial implementation of freeaddrinfo, getaddrinfo and
 * getnameinfo for windows 2k
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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


#include "common.h"
#include "dbuffer.h"

#define MAX_STREAMS 56
#define MAX_DATAGRAM_SOCKETS 7

#ifdef _WIN32
# ifdef __MINGW32__
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
# ifndef FD_SETSIZE
#  define FD_SETSIZE (MAX_STREAMS + 1)
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
# define netError WSAGetLastError()
# define netStringError netStringErrorWin
# define netCloseSocket closesocket
# define gai_strerrorA netStringErrorWin
extern qboolean s_win95, s_win2k; /* from win_main.c */
static HINSTANCE netLib = NULL;
typedef void (*PFreeAddrInfo)(struct addrinfo* res);
typedef int (*PGetAddrInfo)(const char *node, const char *service,
	const struct addrinfo *hints,struct addrinfo **res);
typedef int (*PGetNameInfo)(const struct sockaddr *sa, socklen_t salen,
	char *host, size_t hostlen, char *serv, size_t servlen, int flags);

static PFreeAddrInfo FreeAddrInfo = NULL;
static PGetAddrInfo GetAddrInfo = NULL;
static PGetNameInfo GetNameInfo = NULL;
#else
# define Sys_GetAddrInfo getaddrinfo
# define Sys_FreeAddrInfo freeaddrinfo
# define Sys_GetNameInfo getnameinfo
# define INVALID_SOCKET (-1)
typedef int SOCKET;
# include <sys/select.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <signal.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/time.h>
# include <unistd.h>
# define netError errno
# define netStringError strerror
# define netCloseSocket close
#endif /* WIN32 */
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/**
 * AI_ADDRCONFIG, AI_ALL, and AI_V4MAPPED are available since glibc 2.3.3.
 * AI_NUMERICSERV is available since glibc 2.3.4.
 */
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif


static cvar_t* net_ipv4;

struct net_stream {
	void *data;

	qboolean loopback;
	qboolean ready;
	qboolean closed;
	qboolean finished;
	int socket;
	int index;
	int family;
	int addrlen;

	struct dbuffer *inbound;
	struct dbuffer *outbound;

	stream_callback_func *func;
	struct net_stream *loopback_peer;
};

struct datagram {
	int len;
	char *msg;
	char *addr;
	struct datagram *next;
};

struct datagram_socket {
	int socket;
	int index;
	int family;
	int addrlen;
	struct datagram *queue;
	struct datagram **queue_tail;
	datagram_callback_func *func;
};

static fd_set read_fds;
static fd_set write_fds;
static int maxfd;
static struct net_stream *streams[MAX_STREAMS];
static struct datagram_socket *datagram_sockets[MAX_DATAGRAM_SOCKETS];

static qboolean loopback_ready = qfalse;
static qboolean server_running = qfalse;
static stream_callback_func *server_func = NULL;
static int server_socket = INVALID_SOCKET;
static int server_family, server_addrlen;

#ifdef _WIN32
/**
 * @brief This function implements a freeaddrinfo function for
 * windows systems which don't have it (other than vista and XP)
 */
static inline void Sys_FreeAddrInfo (struct addrinfo *res)
{
	if (s_win95 || s_win2k) {
		struct addrinfo *list;
		for (list = res; list != NULL;) {
			if (list->ai_addr)
				free(list->ai_addr);
			if (list->ai_canonname)
				free(list->ai_canonname);
			res = list;
			list = list->ai_next;
			free(res);
		}
	} else {
		/* need to dynamicly get the function adress otherwise binary
		 * won't start under w2k and previous */
		if (!netLib);
			netLib = LoadLibrary("ws2_32.dll");
		if (!FreeAddrInfo)
			FreeAddrInfo = (PFreeAddrInfo)GetProcAddress(netLib, "freeaddrinfo");
		if (!FreeAddrInfo)
			Com_Printf("...couldn't find freeaddrinfo\n");
		FreeAddrInfo(res);
		return;
	}
}

/**
 * @brief This function implements a getaddrinfo function for
 * windows systems which don't have it (like win2k)
 */
static inline int Sys_GetAddrInfo (const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	if (s_win95 || s_win2k) { /* dirty implementation for windows <= w2k */
		struct addrinfo Result; /*inet_addr*/
		struct sockaddr_in Saddr_in;

		/*check that parameters are right ie those handled */
		if (hints->ai_family != AF_INET) /*only IPV4 handled */
			return EAI_FAIL;

		/* General init */
		Result.ai_flags = 0;
		Result.ai_family = AF_INET;
		Result.ai_addrlen = 0;
		Result.ai_addr = NULL;
		Result.ai_canonname = NULL;
		Result.ai_next = NULL; /*only one address returned*/
		Saddr_in.sin_family = AF_INET;

		if (hints->ai_socktype == SOCK_STREAM) {
			Result.ai_socktype = SOCK_STREAM;
			Result.ai_protocol = IPPROTO_TCP;
		} else if (hints->ai_socktype == SOCK_DGRAM) {
			Result.ai_socktype = SOCK_DGRAM;
			Result.ai_protocol = IPPROTO_UDP;
		} else {
			Com_Printf("Sys_GetAddrInfo : bad socktype requested\n");
			return EAI_FAIL;
		}

		/* node address management */
		if (node == NULL) { /* No nodename/ip specified */
			if (hints->ai_flags & AI_PASSIVE)
				Saddr_in.sin_addr.s_addr = INADDR_ANY; /* all local ip */
			else
				Saddr_in.sin_addr.s_addr = INADDR_LOOPBACK; /* Or loopback */
		} else if (hints->ai_flags & AI_NUMERICHOST) { /* Node is ip address */
			Saddr_in.sin_addr.s_addr = inet_addr(node);
			if (INADDR_NONE == Saddr_in.sin_addr.s_addr) {
				Com_Printf("Sys_GetAddrInfo : bad ip address\n");
				return EAI_FAIL;
			}
		} else {/* node is an ip or nodename */
			/* inet_addr is checking if it's a legitimate ip */
			Saddr_in.sin_addr.s_addr = inet_addr(node);
			if (INADDR_NONE == Saddr_in.sin_addr.s_addr) {
				/* node is not an ip and inet_addr has failed */
				struct hostent* host_info = NULL;
				host_info = gethostbyname(node);
				if (host_info == NULL) {
					Com_Printf("Sys_GetAddrInfo : unable to resolve hostname %s\n",node);
					return EAI_FAIL;
				}
				if (host_info->h_addrtype != AF_INET) {
					/*paranoid check*/
					Com_Printf("Sys_GetAddrInfo: only IPV4 is handled !\n");
					return EAI_FAIL;
				}

				Saddr_in.sin_addr.s_addr = ((struct in_addr *)(host_info->h_addr_list[0]))->s_addr;
				/* don't touch host_info, it's allocated&freed by windows */
			}
		}

		/* Port management */
		if (service == NULL)
			Saddr_in.sin_port = 0; /* system will choose the port*/
		else { /* AI_NUMERICSERV flag is unknown in windows so we have to test */
			char * endpt = NULL;
			Saddr_in.sin_port = strtoul(service, &endpt, 10);
			if (endpt == NULL) /* should never happen */
				return EAI_FAIL;
			else if ((*endpt == '\0') && (Saddr_in.sin_port != 0))
				Saddr_in.sin_port = htons(Saddr_in.sin_port);
			else { /* service is a name */
				struct servent * servinfopt = NULL;
				if (hints->ai_socktype == SOCK_STREAM)
					servinfopt = getservbyname(service, "tcp");
				else if (hints->ai_socktype == SOCK_DGRAM)
					servinfopt = getservbyname(service, "udp");

				if (servinfopt == NULL) {
					Com_Printf("Sys_GetAddrInfo : unable to resolve port from service\n");
					return EAI_FAIL;
				} else
					Saddr_in.sin_port = servinfopt->s_port;
				/* don't touch servinfopt, it's allocated & freed by windows */
			}
		}

		/* Copy of results */
		*res = malloc(sizeof(struct addrinfo));
		memcpy(*res, &Result, sizeof(struct addrinfo));
		(*res)->ai_addrlen = sizeof(struct sockaddr_in);
		(*res)->ai_addr = malloc(sizeof(struct sockaddr_in));
		memcpy((*res)->ai_addr, &Saddr_in, sizeof(struct sockaddr_in));
		return 0;
	/* end of dirty implementation for windows <= w2k */
	} else {
		/* need to dynamicly get the function adress otherwise binary
		 * won't start under w2k and previous */
		if (!netLib);
			netLib = LoadLibrary("ws2_32.dll");
		if (!GetAddrInfo)
			GetAddrInfo = (PGetAddrInfo)GetProcAddress(netLib, "getaddrinfo");
		if (!GetAddrInfo) {
			Com_Printf("...couldn't find getaddrinfo\n");
			return EAI_FAIL;
		}
		return GetAddrInfo(node, service, hints, res);
	}
}

/**
 * @brief This function implements a GetNameInfo function for
 * windows systems which don't have it (other than vista and XP)
 */
static inline int Sys_GetNameInfo (const struct sockaddr *sa, socklen_t salen,
	char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	/* dirty implementation for windows <= w2k */
	if (s_win95 || s_win2k) {
		struct sockaddr_in * ptsock = (struct sockaddr_in *)sa;

		/*we don't handle those flags (laziness issue)*/
		if ((flags & NI_NOFQDN) || (flags & NI_NAMEREQD)) {
			Com_Printf("Sys_GetNameInfo: flags not handled\n");
			return WSANO_RECOVERY;
		}

		if ((salen != sizeof(struct sockaddr_in)) || (ptsock->sin_family != AF_INET)) {
			Com_Printf("Sys_GetNameInfo: Error only IPV4 is handled\n");
			return WSANO_RECOVERY;
		}

		/* Build host string */
		if (host != NULL) {/* otherwise nothing to do */
			char * sipaddr = NULL;
			if ((flags & NI_NUMERICHOST)){
				/* we need to fill ip in host string */
				sipaddr = inet_ntoa(ptsock->sin_addr);
				if ((sipaddr == NULL) || (strlen(sipaddr) >= hostlen)) {
					Com_Printf("Sys_GetNameInfo: host string too small or bad ip given\n");
					return WSANO_RECOVERY;
				}
				strcpy(host,sipaddr);
			} else {
				/* we need the hostname */
				PHOSTENT phostinfo = NULL;
				phostinfo = gethostbyaddr((const char*) &(ptsock->sin_addr),
					sizeof(struct in_addr), AF_INET);
				if (phostinfo != NULL) {
					if (strlen(phostinfo->h_name)>= hostlen) {
						Com_Printf("Sys_GetNameInfo: host string too small\n");
						return WSANO_RECOVERY;
					}
					strcpy(host,phostinfo->h_name);
				} else {
					/* let's put the ip in host string */
					sipaddr = inet_ntoa(ptsock->sin_addr);
					if ((sipaddr == NULL) || (strlen(sipaddr)>= hostlen)) {
						Com_Printf("Sys_GetNameInfo: host string too small or bad ip given\n");
						return WSANO_RECOVERY;
					}
					strcpy(host,sipaddr);
				}
			}
		}/* End of Build host string */

		/* Build serv string */
		if (serv != NULL) {/* otherwise nothing to do */
			if (flags & NI_NUMERICSERV){
				/* we need to fill port number in serv string */
				if (servlen < 6) {/* too small */
					Com_Printf("Sys_GetNameInfo: service string too small\n");
					return WSANO_RECOVERY;
				}

				sprintf(serv, "%u", ntohs(ptsock->sin_port));
			} else {
				struct servent * pservinfo = NULL;
				if (NI_DGRAM & flags)
					pservinfo = getservbyport(ptsock->sin_port, "udp");
				else
					pservinfo = getservbyport(ptsock->sin_port, "tcp");

				if (pservinfo != NULL) {
					if (servlen <= strlen(pservinfo->s_name)) {/* too small */
						Com_Printf("Sys_GetNameInfo: service string too small\n");
						return WSANO_RECOVERY;
					}

					strcpy(serv, pservinfo->s_name);
				}
				else {
					/* we need to fill port number in serv string */
					if (servlen < 6) { /* too small */
						Com_Printf("Sys_GetNameInfo: service string too small\n");
						return WSANO_RECOVERY;
					}
					sprintf(serv, "%u", ntohs(ptsock->sin_port));
				}
			}
		}

		return 0;
	} else {
		/* need to dynamicly get the function adress otherwise binary
		 * won't start under w2k and previous */
		if (!netLib);
			netLib = LoadLibrary("ws2_32.dll");
		if (!GetNameInfo)
			GetNameInfo = (PGetNameInfo)GetProcAddress(netLib, "getnameinfo");
		if (!GetNameInfo) {
			Com_Printf("...couldn't find getnameinfo\n");
			return EAI_FAIL;
		}
		return GetNameInfo(sa, salen, host, hostlen, serv, servlen, flags);
	}
}

static const char *netStringErrorWin (int code)
{
	switch (code) {
	case WSAEINTR: return "WSAEINTR";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN: return "WSAENETDOWN";
	case WSAENETUNREACH: return "WSAENETUNREACH";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSASYSNOTREADY: return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "NO ERROR";
	}
}
#endif

/**
 * @brief
 * @sa new_stream
 * @sa close_stream
 * @sa find_free_datagram_socket
 */
static int find_free_stream (void)
{
	static int start = 0;
	int i;

	for (i = 0; i < MAX_STREAMS; i++) {
		int pos = (i + start) % MAX_STREAMS;
		if (streams[pos] == NULL) {
			start = (pos + 1) % MAX_STREAMS;
			Com_DPrintf(DEBUG_SERVER, "New stream at index: %i\n", pos);
			return pos;
		}
	}
	return -1;
}

/**
 * @sa new_stream
 */
static int find_free_datagram_socket (void)
{
	static int start = 0;
	int i;

	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++) {
		int pos = (i + start) % MAX_DATAGRAM_SOCKETS;
		if (datagram_sockets[pos] == NULL) {
			start = (pos + 1) % MAX_DATAGRAM_SOCKETS;
			Com_DPrintf(DEBUG_SERVER, "New datagram at index: %i\n", pos);
			return pos;
		}
	}
	return -1;
}

/**
 * @sa find_free_stream
 * @sa close_stream
 */
static struct net_stream *new_stream (int index)
{
	struct net_stream *s = malloc(sizeof(*s));
	s->data = NULL;
	s->loopback_peer = NULL;
	s->loopback = qfalse;
	s->closed = qfalse;
	s->finished = qfalse;
	s->ready = qfalse;
	s->socket = INVALID_SOCKET;
	s->inbound = NULL;
	s->outbound = NULL;
	s->index = index;
	s->family = 0;
	s->addrlen = 0;
	s->func = NULL;
	if (streams[index])
		free_stream(streams[index]);
	streams[index] = s;
	return s;
}

/**
 * @sa NET_Shutdown
 * @sa Qcommon_Init
 */
void NET_Init (void)
{
	int i;
#ifdef _WIN32
	WSADATA winsockdata;
#endif

	Com_Printf("\n----- network initialization -------\n");

#ifdef _WIN32
	if (WSAStartup(MAKEWORD(2, 0), &winsockdata) != 0)
		Com_Error(ERR_FATAL, "Winsock initialization failed.");
#endif

	maxfd = 0;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	for (i = 0; i < MAX_STREAMS; i++)
		streams[i] = NULL;
	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++)
		datagram_sockets[i] = NULL;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	net_ipv4 = Cvar_Get("net_ipv4", "1", CVAR_ARCHIVE, "Only use ipv4");
}

/**
 * @sa NET_Init
 */
void NET_Shutdown (void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}

/**
 * @sa stream_finished
 * @sa new_stream
 */
static void close_stream (struct net_stream *s)
{
	if (!s || s->closed)
		return;

	if (s->socket != INVALID_SOCKET) {
		if (dbuffer_len(s->outbound))
			Com_Printf("The outbound buffer for this socket (%d) is not empty\n", s->socket);
		else if (dbuffer_len(s->inbound))
			Com_Printf("The inbound buffer for this socket (%d) is not empty\n", s->socket);

		FD_CLR(s->socket, &read_fds);
		FD_CLR(s->socket, &write_fds);
		netCloseSocket(s->socket);
		s->socket = INVALID_SOCKET;
	}
	if (s->index >= 0)
		streams[s->index] = NULL;

	if (s->loopback_peer) {
		/* Detach the peer, so that it won't send us anything more */
		s->loopback_peer->outbound = NULL;
		s->loopback_peer->loopback_peer = NULL;
	}

	s->closed = qtrue;
	Com_DPrintf(DEBUG_SERVER, "Close stream at index: %i\n", s->index);

	/* If we have a loopback peer, don't free the outbound buffer,
	 * because it's our peer's inbound buffer */
	if (!s->loopback_peer)
		free_dbuffer(s->outbound);

	s->outbound = NULL;
	s->socket = INVALID_SOCKET;

	/* Note that s is potentially invalid after the callback returns */
	if (s->finished) {
		free_dbuffer(s->inbound);
		free(s);
	} else if (s->func)
		s->func(s);
}

static void do_accept (int sock)
{
	int index = find_free_stream();
	struct net_stream *s;
	if (index == -1) {
		Com_Printf("Too many streams open, rejecting inbound connection\n");
		netCloseSocket(sock);
		return;
	}

	s = new_stream(index);
	s->socket = sock;
	s->inbound = new_dbuffer();
	s->outbound = new_dbuffer();
	s->family = server_family;
	s->addrlen = server_addrlen;
	s->func = server_func;

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	server_func(s);
}

/**
 * @sa Qcommon_Frame
 */
void NET_Wait (int timeout)
{
	struct timeval tv;
	int ready;
	int i;

	fd_set read_fds_out;
	fd_set write_fds_out;

	memcpy(&read_fds_out, &read_fds, sizeof(read_fds_out));
	memcpy(&write_fds_out, &write_fds, sizeof(write_fds_out));

	/* select() won't notice that loopback streams are ready, so we'll
	 * eliminate the delay directly */
	if (loopback_ready)
		timeout = 0;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = 1000 * (timeout % 1000);
#ifdef _WIN32
	if (read_fds_out.fd_count == 0) {
		Sleep(timeout);
		ready = 0;
	} else
#endif
		ready = select(maxfd, &read_fds_out, &write_fds_out, NULL, &tv);

	if (ready == -1) {
		Com_Printf("select failed: %s\n", netStringError(netError));
		return;
	}

	if (ready == 0 && !loopback_ready)
		return;

	if (server_socket != INVALID_SOCKET && FD_ISSET(server_socket, &read_fds_out)) {
		int client_socket = accept(server_socket, NULL, 0);
		if (client_socket == INVALID_SOCKET) {
			if (errno != EAGAIN)
				Com_Printf("accept on socket %d failed: %s\n", server_socket, netStringError(netError));
		} else
			do_accept(client_socket);
	}

	for (i = 0; i < MAX_STREAMS; i++) {
		struct net_stream *s = streams[i];

		if (!s)
			continue;

		if (s->loopback) {
			/* Note that s is potentially invalid after the callback returns - we'll close dead streams on the next pass */
			if (s->ready && s->func)
				s->func(s);
			/* If the peer is gone and the buffer is empty, close the stream */
			else if (!s->loopback_peer && stream_length(s) == 0)
				close_stream(s);

			continue;
		}

		if (s->socket == INVALID_SOCKET)
			continue;

		if (FD_ISSET(s->socket, &write_fds_out)) {
			char buf[4096];
			int len;

			if (dbuffer_len(s->outbound) == 0) {
				FD_CLR(s->socket, &write_fds);

				/* Finished streams are closed when their outbound queues empty */
				if (s->finished)
					close_stream(s);

				continue;
			}

			len = dbuffer_get(s->outbound, buf, sizeof(buf));
			len = send(s->socket, buf, len, 0);

			if (len < 0) {
				Com_Printf("write on socket %d failed: %s\n", s->socket, netStringError(netError));
				close_stream(s);
				continue;
			}

			Com_DPrintf(DEBUG_SERVER, "wrote %d bytes to stream %d (%s)\n", len, i, stream_peer_name(s, buf, sizeof(buf), qfalse));

			dbuffer_remove(s->outbound, len);
		}

		if (FD_ISSET(s->socket, &read_fds_out)) {
			char buf[4096];
			int len = recv(s->socket, buf, sizeof(buf), 0);
			if (len <= 0) {
				if (len == -1)
					Com_Printf("read on socket %d failed: %s\n", s->socket, netStringError(netError));
				close_stream(s);
				continue;
			} else {
				if (s->inbound) {
					dbuffer_add(s->inbound, buf, len);

					Com_DPrintf(DEBUG_SERVER, "read %d bytes from stream %d (%s)\n", len, i, stream_peer_name(s, buf, sizeof(buf), qfalse));

					/* Note that s is potentially invalid after the callback returns */
					if (s->func)
						s->func(s);

					continue;
				}
			}
		}
	}

	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++) {
		struct datagram_socket *s = datagram_sockets[i];

		if (!s)
			continue;

		if (FD_ISSET(s->socket, &write_fds_out)) {
			if (s->queue) {
				struct datagram *dgram = s->queue;
				int len = sendto(s->socket, dgram->msg, dgram->len, 0, (struct sockaddr *)dgram->addr, s->addrlen);
				if (len == -1)
					Com_Printf("sendto on socket %d failed: %s\n", s->socket, netStringError(netError));
				/* Regardless of whether it worked, we don't retry datagrams */
				s->queue = dgram->next;
				free(dgram->msg);
				free(dgram->addr);
				free(dgram);
				if (!s->queue)
					s->queue_tail = &s->queue;
			} else {
				FD_CLR(s->socket, &write_fds);
			}
		}

		if (FD_ISSET(s->socket, &read_fds_out)) {
			char buf[256];
			char addrbuf[256];
			socklen_t addrlen = sizeof(addrbuf);
			int len = recvfrom(s->socket, buf, sizeof(buf), 0, (struct sockaddr *)addrbuf, &addrlen);
			if (len == -1)
				Com_Printf("recvfrom on socket %d failed: %s\n", s->socket, netStringError(netError));
			else
				s->func(s, buf, len, (struct sockaddr *)addrbuf);
		}
	}

	loopback_ready = qfalse;
}

static qboolean set_non_blocking (int socket)
{
#ifdef _WIN32
	unsigned long t = 1;
	if (ioctlsocket(socket, FIONBIO, &t) == -1) {
		Com_Printf("ioctl FIONBIO failed: %s\n", strerror(errno));
		return qfalse;
	}
#else
	if (fcntl(socket, F_SETFL, O_NONBLOCK) == -1) {
		Com_Printf("fcntl F_SETFL failed: %s\n", strerror(errno));
		return qfalse;
	}
#endif
	return qtrue;
}

static struct net_stream *NET_DoConnect (const char *node, const char *service, const struct addrinfo *addr, int i)
{
	struct net_stream *s;
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", netStringError(netError));
		return NULL;
	}

	if (!set_non_blocking(sock)) {
		netCloseSocket(sock);
		return NULL;
	}

	if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		int err = netError;
#ifdef _WIN32
		if (err != WSAEWOULDBLOCK) {
#else
		if (err != EINPROGRESS) {
#endif
			Com_Printf("Failed to start connection to %s:%s: %s\n", node, service, netStringError(err));
			netCloseSocket(sock);
			return NULL;
		}
	}

	s = new_stream(i);
	s->socket = sock;
	s->inbound = new_dbuffer();
	s->outbound = new_dbuffer();
	s->family = addr->ai_family;
	s->addrlen = addr->ai_addrlen;

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	return s;
}

struct net_stream *NET_Connect (const char *node, const char *service)
{
	struct addrinfo *res;
	struct addrinfo hints;
	int rc;
	struct net_stream *s = NULL;
	int index;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;
	/* force ipv4 */
	if (net_ipv4->integer)
		hints.ai_family = AF_INET;

	rc = Sys_GetAddrInfo(node, service, &hints, &res);

	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node, service, gai_strerror(rc));
		return NULL;
	}

	index = find_free_stream();
	if (index == -1) {
		Com_Printf("Failed to connect to host %s:%s, too many streams open\n", node, service);
		return NULL;
	}

	s = NET_DoConnect(node, service, res, index);

	Sys_FreeAddrInfo(res);
	return s;
}

struct net_stream *NET_ConnectToLoopBack (void)
{
	struct net_stream *client, *server;
	int server_index, client_index;

	if (!server_running)
		return NULL;

	server_index = find_free_stream();
	client_index = find_free_stream();

	if (server_index == -1 || client_index == -1 || server_index == client_index) {
		Com_Printf("Failed to connect to loopback server, too many streams open\n");
		return NULL;
	}

	client = new_stream(client_index);
	client->loopback = qtrue;
	client->inbound = new_dbuffer();
	client->outbound = new_dbuffer();

	server = new_stream(server_index);
	server->loopback = qtrue;
	server->inbound = client->outbound;
	server->outbound = client->inbound;
	server->func = server_func;

	client->loopback_peer = server;
	server->loopback_peer = client;

	server_func(server);

	return client;
}

/**
 * @brief Enqueue a network message into a stream
 * @sa stream_dequeue
 * @sa dbuffer_add
 */
void stream_enqueue (struct net_stream *s, const char *data, int len)
{
	if (len <= 0 || !s || s->closed || s->finished)
		return;

	if (s->outbound)
		dbuffer_add(s->outbound, data, len);

	if (s->socket >= 0)
		FD_SET(s->socket, &write_fds);

	if (s->loopback_peer) {
		loopback_ready = qtrue;
		s->loopback_peer->ready = qtrue;
	}
}

qboolean stream_closed (struct net_stream *s)
{
	return s ? (s->closed || s->finished) : qtrue;
}

int stream_length (struct net_stream *s)
{
	return s ? dbuffer_len(s->inbound) : 0;
}

/**
 * @brief Returns the length of the waiting inbound buffer
 * @sa dbuffer_get
 */
int stream_peek (struct net_stream *s, char *data, int len)
{
	if (len <= 0 || !s)
		return 0;

	if ((s->closed || s->finished) && dbuffer_len(s->inbound) == 0)
		return 0;

	return dbuffer_get(s->inbound, data, len);
}

/**
 * @sa stream_enqueue
 * @sa dbuffer_extract
 */
int stream_dequeue (struct net_stream *s, char *data, int len)
{
	if (len <= 0 || !s || s->finished)
		return 0;

	return dbuffer_extract(s->inbound, data, len);
}

void *stream_data (struct net_stream *s)
{
	return s ? s->data : NULL;
}

void set_stream_data (struct net_stream *s, void *data)
{
	if (!s)
		return;
	s->data = data;
}

/**
 * @sa close_stream
 * @sa stream_finished
 */
void free_stream (struct net_stream *s)
{
	if (!s)
		return;
	s->finished = qtrue;
	close_stream(s);
}

void stream_finished (struct net_stream *s)
{
	if (!s)
		return;

	s->finished = qtrue;

	if (s->socket >= 0)
		FD_CLR(s->socket, &read_fds);

	/* Stop the loopback peer from queueing stuff up in here */
	if (s->loopback_peer)
		s->loopback_peer->outbound = NULL;

	free_dbuffer(s->inbound);
	s->inbound = NULL;

	/* If there's nothing in the outbound buffer, any finished stream is
	 * ready to be closed */
	if (dbuffer_len(s->outbound) == 0)
		close_stream(s);
}

/* Any code which calls this function with ip_hack set to true is
 * considered broken - it should not make assumptions about the format
 * of the result, and this function is only really intended for
 * displaying data to the user
 */

const char * stream_peer_name (struct net_stream *s, char *dst, int len, qboolean ip_hack)
{
	if (!s)
		return "(null)";
	else if (stream_is_loopback(s))
		return "loopback connection";
	else {
		char buf[128];
		char node[64];
		char service[64];
		int rc;
		socklen_t addrlen = s->addrlen;
		if (getpeername(s->socket, (struct sockaddr *)buf, &addrlen) != 0)
			return "(error)";

		rc = Sys_GetNameInfo((struct sockaddr *)buf, addrlen, node, sizeof(node), service, sizeof(service),
				NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc != 0) {
			Com_Printf("Failed to convert sockaddr to string: %s\n", gai_strerror(rc));
			return "(error)";
		}
		if (ip_hack)
			Com_sprintf(dst, len, "%s", node);
		else {
			node[sizeof(node) - 1] = '\0';
			service[sizeof(service) - 1] = '\0';
			Com_sprintf(dst, len, "[%s]:%s", node, service);
		}
		return dst;
	}
}

void stream_callback (struct net_stream *s, stream_callback_func *func)
{
	if (!s)
		return;
	s->func = func;
}

qboolean stream_is_loopback (struct net_stream *s)
{
	return s && s->loopback;
}

static int do_start_server (const struct addrinfo *addr)
{
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;

	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", netStringError(netError));
		return INVALID_SOCKET;
	}

	if (!set_non_blocking(sock)) {
		netCloseSocket(sock);
		return INVALID_SOCKET;
	}

#ifdef _WIN32
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &t, sizeof(t)) != 0) {
#else
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)) != 0) {
#endif
		Com_Printf("Failed to set SO_REUSEADDR on socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return INVALID_SOCKET;
	}

	if (bind(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		Com_Printf("Failed to bind socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return INVALID_SOCKET;
	}

	if (listen(sock, SOMAXCONN) != 0) {
		Com_Printf("Failed to listen on socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return INVALID_SOCKET;
	}

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);
	server_family = addr->ai_family;
	server_addrlen = addr->ai_addrlen;

	return sock;
}

qboolean SV_Start (const char *node, const char *service, stream_callback_func *func)
{
	if (!func)
		return qfalse;

	if (server_running) {
		Com_Printf("SV_Start: Server is still running - call SV_Stop before\n");
		return qfalse;
	}

	if (service) {
		struct addrinfo *res;
		struct addrinfo hints;
		int rc;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		/* force ipv4 */
		if (net_ipv4->integer)
			hints.ai_family = AF_INET;

		rc = Sys_GetAddrInfo(node, service, &hints, &res);

		if (rc != 0) {
			Com_Printf("Failed to resolve host %s:%s: %s\n", node ? node : "*", service, gai_strerror(rc));
			return qfalse;
		}

		server_socket = do_start_server(res);
		if (server_socket == INVALID_SOCKET) {
			Com_Printf("Failed to start server on %s:%s\n", node ? node : "*", service);
		} else {
			server_running = qtrue;
			server_func = func;
		}
		Sys_FreeAddrInfo(res);
	} else {
		/* Loopback server only */
		server_running = qtrue;
		server_func = func;
	}

	return server_running;
}

void SV_Stop (void)
{
	server_running = qfalse;
	server_func = NULL;
	if (server_socket != INVALID_SOCKET) {
		FD_CLR(server_socket, &read_fds);
		netCloseSocket(server_socket);
	}
	server_socket = INVALID_SOCKET;
}

/**
 * @sa new_datagram_socket
 */
static struct datagram_socket *do_new_datagram_socket (const struct addrinfo *addr)
{
	struct datagram_socket *s;
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;
	int index = find_free_datagram_socket();

	if (index == -1) {
		Com_Printf("Too many datagram sockets open\n");
		return NULL;
	}

	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", netStringError(netError));
		return NULL;
	}

	if (!set_non_blocking(sock)) {
		netCloseSocket(sock);
		return NULL;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_REUSEADDR on socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return NULL;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *) &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_BROADCAST on socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return NULL;
	}

	if (bind(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		Com_Printf("Failed to bind socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return NULL;
	}

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	s = malloc(sizeof(*s));
	s->family = addr->ai_family;
	s->addrlen = addr->ai_addrlen;
	s->socket = sock;
	s->index = index;
	s->queue = NULL;
	s->queue_tail = &s->queue;
	s->func = NULL;
	datagram_sockets[index] = s;

	return s;
}

/**
 * @brief Opens a datagram socket (UDP)
 * @sa do_new_datagram_socket
 */
struct datagram_socket *new_datagram_socket (const char *node, const char *service, datagram_callback_func *func)
{
	struct datagram_socket *s;
	struct addrinfo *res;
	struct addrinfo hints;
	int rc;

	if (!service || !func)
		return NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;
	/* force ipv4 */
	if (net_ipv4->integer)
		hints.ai_family = AF_INET;

	rc = Sys_GetAddrInfo(node, service, &hints, &res);

	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node ? node : "*", service, gai_strerror(rc));
		return qfalse;
	}

	s = do_new_datagram_socket(res);
	if (s)
		s->func = func;

	Sys_FreeAddrInfo(res);
	return s;
}

/**
 * @sa new_datagram_socket
 */
void send_datagram (struct datagram_socket *s, const char *buf, int len, struct sockaddr *to)
{
	struct datagram *dgram;
	if (!s || len <= 0 || !buf || !to)
		return;

	dgram = malloc(sizeof(*dgram));
	dgram->msg = malloc(len);
	dgram->addr = malloc(s->addrlen);
	memcpy(dgram->msg, buf, len);
	memcpy(dgram->addr, to, len);
	dgram->len = len;
	dgram->next = NULL;

	*s->queue_tail = dgram;
	s->queue_tail = &dgram->next;

	FD_SET(s->socket, &write_fds);
}

/**
 * @sa send_datagram
 * @sa new_datagram_socket
 */
void broadcast_datagram (struct datagram_socket *s, const char *buf, int len, int port)
{
	if (s->family == AF_INET) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		send_datagram(s, buf, len, (struct sockaddr *)&addr);
	} else if (s->family == AF_INET6) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET6;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		send_datagram(s, buf, len, (struct sockaddr *)&addr);
	} else {
		Sys_Error("Broadcast unsupported on address family %d\n", s->family);
	}
}

/**
 * @sa new_datagram_socket
 * @sa do_new_datagram_socket
 */
void close_datagram_socket (struct datagram_socket *s)
{
	if (!s)
		return;

	FD_CLR(s->socket, &read_fds);
	FD_CLR(s->socket, &write_fds);
	netCloseSocket(s->socket);

	while (s->queue) {
		struct datagram *dgram = s->queue;
		s->queue = dgram->next;
		free(dgram->msg);
		free(dgram->addr);
		free(dgram);
	}

	datagram_sockets[s->index] = NULL;
	free(s);
}

/**
 * @brief Convert sockaddr to string
 * @note uses getnameinfo
 */
void sockaddr_to_strings (struct datagram_socket *s, struct sockaddr *addr, char *node, size_t nodelen, char *service, size_t servicelen)
{
	int rc = Sys_GetNameInfo(addr, s->addrlen, node, nodelen, service, servicelen,
			NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM);
	if (rc != 0) {
		Com_Printf("Failed to convert sockaddr to string: %s\n", gai_strerror(rc));
		Q_strncpyz(node, "(error)", nodelen);
		Q_strncpyz(service, "(error)", servicelen);
	}
}
