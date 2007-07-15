/**
 * @file net.c
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

#include "qcommon.h"
#include "dbuffer.h"

#define MAX_STREAMS 63

#ifdef _WIN32
# define FD_SETSIZE (MAX_STREAMS + 1)
# include <winsock2.h>
# include <ws2tcpip.h>
# define EINPROGRESS WSAEINPROGRESS
#else
# define INVALID_SOCKET (-1)
# include <sys/select.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <netinet/in.h>
#endif /* WIN32 */
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>


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

static fd_set read_fds;
static fd_set write_fds;
static int maxfd;
static struct net_stream *streams[MAX_STREAMS];

static qboolean loopback_ready = qfalse;
static qboolean server_running = qfalse;
static stream_callback_func *server_func = NULL;
static int server_socket = INVALID_SOCKET;
static int server_family, server_addrlen;

/**
 * @brief
 */
static int find_free_stream (void)
{
	static int start = 0;
	int i;

	for (i = 0; i < MAX_STREAMS; i++) {
		int pos = (i + start) % MAX_STREAMS;
		if (streams[pos] == NULL) {
			start = (pos + 1) % MAX_STREAMS;
			return pos;
		}
	}
	return -1;
}

/**
 * @brief
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

#ifdef _WIN32
/**
 * @brief
 */
static const char *estr (void)
{
	int		code;

	code = WSAGetLastError();
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
#else
/**
 * @brief
 */
static const char *estr(void)
{
	return strerror(errno);
}
#endif

/**
 * @brief
 */
void init_net (void)
{
	int i;

#ifdef _WIN32
	WSADATA winsockdata;
	if (WSAStartup(MAKEWORD(2, 0), &winsockdata) != 0)
		Com_Error(ERR_FATAL,"Winsock initialization failed.");
#endif

	maxfd = 0;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	for (i = 0; i < MAX_STREAMS; i++)
		streams[i] = NULL;
}

/**
 * @brief
 */
static void close_stream (struct net_stream *s)
{
	if (!s || s->closed)
		return;

	if (s->socket != INVALID_SOCKET) {
		FD_CLR(s->socket, &read_fds);
		FD_CLR(s->socket, &write_fds);
#ifdef _WIN32
		closesocket(s->socket);
#else
		close(s->socket);
#endif
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

/**
 * @brief
 */
static void do_accept (int sock)
{
	int index = find_free_stream();
	struct net_stream *s;
	if (index == -1) {
		Com_Printf("Too many streams open, rejecting inbound connection\n");
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
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
 * @brief
 */
void wait_for_net (int timeout)
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
	ready = select(maxfd, &read_fds_out, &write_fds_out, NULL, &tv);
	if (ready == -1)
		Sys_Error("select failed: %s\n", estr());

	if (ready == 0 && !loopback_ready)
		return;

	if (server_running && server_socket != INVALID_SOCKET && FD_ISSET(server_socket, &read_fds_out)) {
		int client_socket = accept(server_socket, NULL, 0);
		if (client_socket == INVALID_SOCKET) {
			if (errno != EAGAIN)
				Com_Printf("accept on socket %d failed: %s\n", server_socket, estr());
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
			else if (!s->loopback_peer && dbuffer_len(s->inbound) == 0)
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
				Com_Printf("write on socket %d failed: %s\n", s->socket, estr());
				close_stream(s);
				continue;
			}

			Com_Printf("wrote %d bytes to stream %d (%s)\n", len, i, stream_peer_name(s, buf, sizeof(buf)));

			dbuffer_remove(s->outbound, len);
		}

		if (FD_ISSET(s->socket, &read_fds_out)) {
			char buf[4096];
			int len = recv(s->socket, buf, sizeof(buf), 0);
			if (len <= 0) {
				if (len == -1)
				Com_Printf("read on socket %d failed: %s\n", s->socket, estr());
				close_stream(s);
				continue;
			} else {
				if (s->inbound) {
					dbuffer_add(s->inbound, buf, len);

					Com_Printf("read %d bytes from stream %d (%s)\n", len, i, stream_peer_name(s, buf, sizeof(buf)));

					/* Note that s is potentially invalid after the callback returns */
					if (s->func)
						s->func(s);

					continue;
				}
			}
		}
	}

	loopback_ready = qfalse;
}

/**
 * @brief
 */
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

/**
 * @brief
 */
static struct net_stream *do_connect (const char *node, const char *service, const struct addrinfo *addr, int i)
{
	struct net_stream *s;
	int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", estr());
		return NULL;
	}

	if (!set_non_blocking(sock)) {
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		return NULL;
	}

	if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0 && errno != EINPROGRESS) {
		Com_Printf("Failed to start connection to %s:%s: %s\n", node, service, estr());
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		return NULL;
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

/**
 * @brief
 */
struct net_stream *connect_to_host (const char *node, const char *service)
{
	struct addrinfo *res;
	struct addrinfo hints;
	int rc;
	struct net_stream *s = NULL;
	int index;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(node, service, &hints, &res);

	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node, service, gai_strerror(rc));
		return NULL;
	}

	index = find_free_stream();
	if (index == -1) {
		Com_Printf("Failed to connect to host %s:%s, too many streams open\n", node, service);
		return NULL;
	}

	s = do_connect(node, service, res, index);

	freeaddrinfo(res);
	return s;
}

/**
 * @brief
 */
struct net_stream *connect_to_loopback (void)
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
 * @brief
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

/**
 * @brief
 */
qboolean stream_closed (struct net_stream *s)
{
	return s ? (s->closed || s->finished) : qtrue;
}

/**
 * @brief
 */
int stream_length (struct net_stream *s)
{
	return (!s || s->finished || !s->inbound) ? 0 : dbuffer_len(s->inbound);
}

/**
 * @brief
 */
int stream_peek (struct net_stream *s, char *data, int len)
{
	if (len <= 0 || !s || s->closed || s->finished)
		return 0;

	return dbuffer_get(s->inbound, data, len);
}

/**
 * @brief
 */
int stream_dequeue (struct net_stream *s, char *data, int len)
{
	if (len <= 0 || !s || s->finished)
		return 0;

	return dbuffer_extract(s->inbound, data, len);
}

/**
 * @brief
 */
void *stream_data (struct net_stream *s)
{
	return s ? s->data : NULL;
}

/**
 * @brief
 */
void set_stream_data (struct net_stream *s, void *data)
{
	if (!s)
		return;
	s->data = data;
}

/**
 * @brief
 */
void free_stream (struct net_stream *s)
{
	if (!s)
		return;
	s->finished = qtrue;
	close_stream(s);
}

/**
 * @brief
 */
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
		ready to be closed */
	if (stream_length(s) == 0)
		close_stream(s);
}

#ifdef _WIN32
const char *inet_ntop (int af, const void *src, char *dst, socklen_t cnt)
{
	if (af == AF_INET) {
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	} else if (af == AF_INET6) {
		struct sockaddr_in6 in;
		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	return NULL;
}
#endif

/**
 * @brief
 */
const char * stream_peer_name (struct net_stream *s, char *dst, int len)
{
	if (!s)
		return "(null)";
	else if (stream_is_loopback(s))
		return "loopback connection";
	else {
		char buf[s->addrlen];
		void *src;
		socklen_t addrlen = s->addrlen;
		if (getpeername(s->socket, (struct sockaddr *)buf, &addrlen) != 0)
			return "(error)";
		switch (s->family) {
		case AF_INET:
			src = &((struct sockaddr_in *)buf)->sin_addr;
			break;
		case AF_INET6:
			src = &((struct sockaddr_in6 *)buf)->sin6_addr;
			break;
		default:
			return "(unknown address family)";
		}
		return inet_ntop(s->family, src, dst, len);
	}
}

/**
 * @brief
 */
void stream_callback (struct net_stream *s, stream_callback_func *func)
{
	if (!s)
		return;
	s->func = func;
}

/**
 * @brief
 */
qboolean stream_is_loopback (struct net_stream *s)
{
	return s && s->loopback;
}

/**
 * @brief
 */
static int do_start_server (const struct addrinfo *addr)
{
	int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;
	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", estr());
		return INVALID_SOCKET;
	}

	if (!set_non_blocking(sock)) {
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		return INVALID_SOCKET;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_REUSEADDR on socket: %s\n", estr());
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		return INVALID_SOCKET;
	}

	if (bind(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		Com_Printf("Failed to bind socket: %s\n", estr());
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		return INVALID_SOCKET;
	}

	if (listen(sock, SOMAXCONN) != 0) {
		Com_Printf("Failed to listen on socket: %s\n", estr());
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		return INVALID_SOCKET;
	}

	maxfd = max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);
	server_family = addr->ai_family;
	server_addrlen = addr->ai_addrlen;

	return sock;
}


/**
 * @brief
 */
qboolean start_server (const char *node, const char *service, stream_callback_func *func)
{
	if (!func)
		return qfalse;

	stop_server();

	if (service) {
		struct addrinfo *res;
		struct addrinfo hints;
		int rc;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;

		rc = getaddrinfo(node, service, &hints, &res);

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

		freeaddrinfo(res);
	} else {
		/* Loopback server only */
		server_running = qtrue;
		server_func = func;
	}

	return server_running;
}

/**
 * @brief
 */
void stop_server (void)
{
	server_func = qfalse;
	server_running = qfalse;
	if (server_socket != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif
	}
	server_socket = INVALID_SOCKET;
}
