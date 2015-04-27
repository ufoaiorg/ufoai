/**
 * @file
 * @note This file should fully support ipv6 and any other protocol that is
 * compatible with the getaddrinfo interface, with the exception of
 * NET_DatagramBroadcast() which must be amended for each protocol (and
 * currently supports only ipv4)
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <SDL_thread.h>
#ifdef _WIN32
#include "../ports/system.h"
#endif
#include "../shared/scopedmutex.h"

#define MAX_STREAMS 56
#define MAX_DATAGRAM_SOCKETS 7

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# if WINVER < 0x501
#  include <wspiapi.h>
# else
#  include <ws2spi.h>
# endif
# define netError WSAGetLastError()
# define netStringError netStringErrorWin
# define netCloseSocket closesocket
# define gai_strerrorA netStringErrorWin

#else /* WIN32 */

# include <sys/ioctl.h>
# include <sys/select.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <signal.h>
typedef int SOCKET;
# define INVALID_SOCKET (-1)
# define netError errno
# define netStringError strerror
# define netCloseSocket close
# define ioctlsocket ioctl
#endif /* WIN32 */

#ifdef EMSCRIPTEN
#define NI_NUMERICHOST 0
#define NI_NUMERICSERV 0
#define NI_DGRAM 0
#define AI_PASSIVE 0
#define AI_NUMERICHOST 0
#define INADDR_BROADCAST 0
#endif

/**
 * @todo Move this into the configure script
 * AI_ADDRCONFIG, AI_ALL, and AI_V4MAPPED are available since glibc 2.3.3.
 * AI_NUMERICSERV is available since glibc 2.3.4.
 */
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

/**
 * @brief use an admin local address per default so that
 * network admins can decide on how to handle traffic.
 */
#define NET_MULTICAST_IP6 "ff04::696f:7175:616b:6533"

#define dbuffer_len(dbuf) (dbuf ? (dbuf)->length() : 0)

static cvar_t* net_ipv4;
static SDL_mutex* netMutex;

struct net_stream {
	void* data;

	bool loopback;
	bool ready;
	bool closed;
	bool finished;		/**< finished but maybe not yet closed */
	SOCKET socket;
	int index;
	int family;
	int addrlen;

	/* these buffers must be used in a thread safe manner
	 * (lock netMutex) because the game thread can also write to it */
	dbufferptr inbound;
	dbufferptr outbound;

	stream_onclose_func* onclose;
	stream_callback_func* func;
	struct net_stream* loopback_peer;
};

struct datagram {
	int len;
	char* msg;
	char* addr;
	struct datagram* next;
};

struct datagram_socket {
	SOCKET socket;
	int index;
	int family;
	int addrlen;
	struct datagram* queue;
	struct datagram** queue_tail;
	datagram_callback_func* func;
};

static fd_set read_fds;
static fd_set write_fds;
static SOCKET maxfd;
static struct net_stream* streams[MAX_STREAMS];
static struct datagram_socket* datagram_sockets[MAX_DATAGRAM_SOCKETS];

static bool loopback_ready = false;
static bool server_running = false;
static stream_callback_func* server_func = nullptr;
static SOCKET server_socket = INVALID_SOCKET;
static int server_family, server_addrlen;

#ifdef _WIN32
static const char* netStringErrorWin (int code)
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

static inline int NET_StreamGetLength (struct net_stream* s)
{
	return s ? dbuffer_len(s->inbound) : 0;
}

/**
 * @brief
 * @sa NET_StreamNew
 * @sa NET_StreamClose
 * @sa NET_DatagramFindFreeSocket
 */
static int NET_StreamGetFree (void)
{
	static int start = 0;

	for (int i = 0; i < MAX_STREAMS; i++) {
		const int pos = (i + start) % MAX_STREAMS;
		if (streams[pos] == nullptr) {
			start = (pos + 1) % MAX_STREAMS;
			Com_DPrintf(DEBUG_SERVER, "New stream at index: %i\n", pos);
			return pos;
		}
	}
	return -1;
}

/**
 * @sa NET_StreamNew
 */
static int NET_DatagramFindFreeSocket (void)
{
	static int start = 0;

	for (int i = 0; i < MAX_DATAGRAM_SOCKETS; i++) {
		const int pos = (i + start) % MAX_DATAGRAM_SOCKETS;
		if (datagram_sockets[pos] == nullptr) {
			start = (pos + 1) % MAX_DATAGRAM_SOCKETS;
			Com_DPrintf(DEBUG_SERVER, "New datagram at index: %i\n", pos);
			return pos;
		}
	}
	return -1;
}

/**
 * @sa NET_StreamGetFree
 * @sa NET_StreamClose
 */
static struct net_stream* NET_StreamNew (int index)
{
	net_stream* const s = Mem_PoolAllocType(net_stream, com_networkPool);
	s->data = nullptr;
	s->loopback_peer = nullptr;
	s->loopback = false;
	s->closed = false;
	s->finished = false;
	s->ready = false;
	s->socket = INVALID_SOCKET;
	s->inbound = dbufferptr();
	s->outbound = dbufferptr();
	s->index = index;
	s->family = 0;
	s->addrlen = 0;
	s->func = nullptr;
	if (streams[index])
		NET_StreamFree(streams[index]);
	streams[index] = s;
	return s;
}

static void NET_ShowStreams_f (void)
{
	char buf[256];
	int cnt = 0;

	for (int i = 0; i < MAX_STREAMS; i++) {
		if (streams[i] == nullptr)
			continue;
		Com_Printf("Steam %i is opened: %s on socket %i (closed: %i, finished: %i, outbound: " UFO_SIZE_T ", inbound: " UFO_SIZE_T ")\n", i,
			NET_StreamPeerToName(streams[i], buf, sizeof(buf), true),
			streams[i]->socket, streams[i]->closed, streams[i]->finished,
			dbuffer_len(streams[i]->outbound), dbuffer_len(streams[i]->inbound));
		cnt++;
	}
	Com_Printf("%i/%i streams opened\n", cnt, MAX_STREAMS);
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
		streams[i] = nullptr;
	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++)
		datagram_sockets[i] = nullptr;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	net_ipv4 = Cvar_Get("net_ipv4", "1", CVAR_ARCHIVE, "Only use ipv4");
	Cmd_AddCommand("net_showstreams", NET_ShowStreams_f, "Show opened streams");

	netMutex = SDL_CreateMutex();
}

/**
 * @sa NET_Init
 */
void NET_Shutdown (void)
{
#ifdef _WIN32
	WSACleanup();
#endif
	SDL_DestroyMutex(netMutex);
	netMutex = nullptr;
}

/**
 * @sa NET_StreamFinished
 * @sa NET_StreamNew
 */
static void NET_StreamClose (struct net_stream* s)
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
		streams[s->index] = nullptr;

	if (s->loopback_peer) {
		/* Detach the peer, so that it won't send us anything more */
		s->loopback_peer->outbound = dbufferptr();
		s->loopback_peer->loopback_peer = nullptr;
	}

	s->closed = true;
	Com_DPrintf(DEBUG_SERVER, "Close stream at index: %i\n", s->index);

	s->outbound = dbufferptr();
	s->socket = INVALID_SOCKET;

	/* Note that this is potentially invalid after the callback returns */
	if (s->finished) {
		s->inbound = dbufferptr();
		if (s->onclose != nullptr)
			s->onclose();
		Mem_Free(s);
		s = nullptr;
	} else if (s->func) {
		s->func(s);
	}

	if (s != nullptr && s->onclose != nullptr)
		s->onclose();
}

static void do_accept (SOCKET sock)
{
	const int index = NET_StreamGetFree();
	struct net_stream* s;
	if (index == -1) {
		Com_Printf("Too many streams open, rejecting inbound connection\n");
		netCloseSocket(sock);
		return;
	}

	s = NET_StreamNew(index);
	s->socket = sock;
	s->inbound = dbufferptr(new dbuffer(4096));
	s->outbound = dbufferptr(new dbuffer(4096));
	s->family = server_family;
	s->addrlen = server_addrlen;
	s->func = server_func;

	maxfd = std::max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	server_func(s);
	/** @todo close stream? */
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
		Sys_Sleep(timeout);
		ready = 0;
	} else
#endif
		ready = select(maxfd, &read_fds_out, &write_fds_out, nullptr, &tv);

	if (ready == -1) {
		Com_Printf("select failed: %s\n", netStringError(netError));
		return;
	}

	if (ready == 0 && !loopback_ready)
		return;

	if (server_socket != INVALID_SOCKET && FD_ISSET(server_socket, &read_fds_out)) {
		const SOCKET client_socket = accept(server_socket, nullptr, 0);
		if (client_socket == INVALID_SOCKET) {
			if (errno != EAGAIN)
				Com_Printf("accept on socket %d failed: %s\n", server_socket, netStringError(netError));
		} else
			do_accept(client_socket);
	}

	for (i = 0; i < MAX_STREAMS; i++) {
		struct net_stream* s = streams[i];

		if (!s)
			continue;

		if (s->loopback) {
			/* If the peer is gone and the buffer is empty, close the stream */
			if (!s->loopback_peer && NET_StreamGetLength(s) == 0) {
				NET_StreamClose(s);
			}
			/* Note that s is potentially invalid after the callback returns - we'll close dead streams on the next pass */
			else if (s->ready && s->func) {
				s->func(s);
			}

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
					NET_StreamClose(s);

				continue;
			}

			{
				const ScopedMutex scopedMutex(netMutex);
				len = s->outbound->get(buf, sizeof(buf));
				len = send(s->socket, buf, len, 0);

				s->outbound->remove(len);
			}

			if (len < 0) {
				Com_Printf("write on socket %d failed: %s\n", s->socket, netStringError(netError));
				NET_StreamClose(s);
				continue;
			}

			Com_DPrintf(DEBUG_SERVER, "wrote %d bytes to stream %d (%s)\n", len, i, NET_StreamPeerToName(s, buf, sizeof(buf), true));
		}

		if (FD_ISSET(s->socket, &read_fds_out)) {
			char buf[4096];
			const int len = recv(s->socket, buf, sizeof(buf), 0);
			if (len <= 0) {
				if (len == -1)
					Com_Printf("read on socket %d failed: %s\n", s->socket, netStringError(netError));
				NET_StreamClose(s);
				continue;
			} else {
				if (s->inbound) {
					SDL_LockMutex(netMutex);
					s->inbound->add(buf, len);
					SDL_UnlockMutex(netMutex);

					Com_DPrintf(DEBUG_SERVER, "read %d bytes from stream %d (%s)\n", len, i, NET_StreamPeerToName(s, buf, sizeof(buf), true));

					/* Note that s is potentially invalid after the callback returns */
					if (s->func)
						s->func(s);

					continue;
				}
			}
		}
	}

	for (i = 0; i < MAX_DATAGRAM_SOCKETS; i++) {
		struct datagram_socket* s = datagram_sockets[i];

		if (!s)
			continue;

		if (FD_ISSET(s->socket, &write_fds_out)) {
			if (s->queue) {
				struct datagram* dgram = s->queue;
				const int len = sendto(s->socket, dgram->msg, dgram->len, 0, (struct sockaddr* )dgram->addr, s->addrlen);
				if (len == -1)
					Com_Printf("sendto on socket %d failed: %s\n", s->socket, netStringError(netError));
				/* Regardless of whether it worked, we don't retry datagrams */
				s->queue = dgram->next;
				Mem_Free(dgram->msg);
				Mem_Free(dgram->addr);
				Mem_Free(dgram);
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
			const int len = recvfrom(s->socket, buf, sizeof(buf), 0, (struct sockaddr* )addrbuf, &addrlen);
			if (len == -1)
				Com_Printf("recvfrom on socket %d failed: %s\n", s->socket, netStringError(netError));
			else
				s->func(s, buf, len, (struct sockaddr* )addrbuf);
		}
	}

	loopback_ready = false;
}

static bool NET_SocketSetNonBlocking (SOCKET socketNum)
{
	unsigned long t = 1;
	if (ioctlsocket(socketNum, FIONBIO, &t) == -1) {
		Com_Printf("ioctl FIONBIO failed: %s\n", strerror(errno));
		return false;
	}
	return true;
}

static struct net_stream* NET_DoConnect (const char* node, const char* service, const struct addrinfo* addr, int i, stream_onclose_func* onclose)
{
	struct net_stream* s;
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", netStringError(netError));
		return nullptr;
	}

	if (!NET_SocketSetNonBlocking(sock)) {
		netCloseSocket(sock);
		return nullptr;
	}

	if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		const int err = netError;
#ifdef _WIN32
		if (err != WSAEWOULDBLOCK) {
#else
		if (err != EINPROGRESS) {
#endif
			Com_Printf("Failed to start connection to %s:%s: %s\n", node, service, netStringError(err));
			netCloseSocket(sock);
			return nullptr;
		}
	}

	s = NET_StreamNew(i);
	s->socket = sock;
	s->inbound = dbufferptr(new dbuffer(4096));
	s->outbound = dbufferptr(new dbuffer(4096));
	s->family = addr->ai_family;
	s->addrlen = addr->ai_addrlen;
	s->onclose = onclose;

	maxfd = std::max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	return s;
}

/**
 * @brief Try to connect to a given host on a given port
 * @param[in] node The host to connect to
 * @param[in] service The port to connect to
 * @param[in] onclose The callback that is called on closing the returned stream. This is useful if
 * you hold the pointer for the returned stream anywhere else and would like to get notified once
 * this pointer is invalid.
 * @sa NET_DoConnect
 * @sa NET_ConnectToLoopBack
 * @todo What about a timeout
 */
struct net_stream* NET_Connect (const char* node, const char* service, stream_onclose_func* onclose)
{
	struct addrinfo* res;
	struct addrinfo hints;
	int rc;
	struct net_stream* s = nullptr;
	int index;

	OBJZERO(hints);
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;
	/* force ipv4 */
	if (net_ipv4->integer)
		hints.ai_family = AF_INET;

	rc = getaddrinfo(node, service, &hints, &res);
	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node, service, gai_strerror(rc));
		return nullptr;
	}

	index = NET_StreamGetFree();
	if (index == -1) {
		Com_Printf("Failed to connect to host %s:%s, too many streams open\n", node, service);
		freeaddrinfo(res);
		return nullptr;
	}

	s = NET_DoConnect(node, service, res, index, onclose);

	freeaddrinfo(res);
	return s;
}

/**
 * @param[in] onclose The callback that is called on closing the returned stream. This is useful if
 * you hold the pointer for the returned stream anywhere else and would like to get notified once
 * this pointer is invalid.
 * @sa NET_Connect
 */
struct net_stream* NET_ConnectToLoopBack (stream_onclose_func* onclose)
{
	struct net_stream* client, *server;
	int server_index, client_index;

	if (!server_running)
		return nullptr;

	server_index = NET_StreamGetFree();
	client_index = NET_StreamGetFree();

	if (server_index == -1 || client_index == -1 || server_index == client_index) {
		Com_Printf("Failed to connect to loopback server, too many streams open\n");
		return nullptr;
	}

	client = NET_StreamNew(client_index);
	client->loopback = true;
	client->inbound = dbufferptr(new dbuffer(4096));
	client->outbound = dbufferptr(new dbuffer(4096));
	client->onclose = onclose;

	server = NET_StreamNew(server_index);
	server->loopback = true;
	server->inbound = client->outbound;
	server->outbound = client->inbound;
	server->func = server_func;
	server->onclose = nullptr;

	client->loopback_peer = server;
	server->loopback_peer = client;

	server_func(server);

	return client;
}

/**
 * @brief Enqueue a network message into a stream
 * @sa NET_StreamDequeue
 */
void NET_StreamEnqueue (struct net_stream* s, const char* data, int len)
{
	if (len <= 0 || !s || s->closed || s->finished)
		return;

	if (s->outbound) {
		const ScopedMutex scopedMutex(netMutex);
		s->outbound->add(data, len);
	}

	/* on linux, socket is int, and INVALID_SOCKET -1
	 * on windows it is unsigned and  INVALID_SOCKET (~0)
	 * Let's hope that checking for INVALID_SOCKET is good enough for linux. */
	//if (s->socket >= 0)
	if (s->socket != INVALID_SOCKET)
		FD_SET(s->socket, &write_fds);

	if (s->loopback_peer) {
		loopback_ready = true;
		s->loopback_peer->ready = true;
	}
}

/**
 * @brief Returns the length of the waiting inbound buffer
 */
static int NET_StreamPeek (struct net_stream* s, char* data, int len)
{
	if (len <= 0 || !s)
		return 0;

	dbufferptr& dbuf = s->inbound;
	if ((s->closed || s->finished) && dbuffer_len(dbuf) == 0)
		return 0;

	return dbuf->get(data, len);
}

/**
 * @sa NET_StreamEnqueue
 */
int NET_StreamDequeue (struct net_stream* s, char* data, int len)
{
	if (len <= 0 || !s || s->finished)
		return 0;

	return s->inbound->extract(data, len);
}

/**
 * @brief Reads messages from the network channel and adds them to the dbuffer
 * where you can use the NET_Read* functions to get the values in the correct
 * order
 * @sa NET_StreamDequeue
 */
dbuffer* NET_ReadMsg (struct net_stream* s)
{
	unsigned int v;
	const ScopedMutex scopedMutex(netMutex);

	if (NET_StreamPeek(s, (char*)&v, 4) < 4)
		return nullptr;

	int len = LittleLong(v);
	if (NET_StreamGetLength(s) < (4 + len))
		return nullptr;

	char tmp[256];
	const int size = sizeof(tmp);
	NET_StreamDequeue(s, tmp, 4);

	dbuffer* buf = new dbuffer();
	while (len > 0) {
		const int x = NET_StreamDequeue(s, tmp, std::min(len, size));
		buf->add(tmp, x);
		len -= x;
	}

	return buf;
}

void* NET_StreamGetData (struct net_stream* s)
{
	return s ? s->data : nullptr;
}

void NET_StreamSetData (struct net_stream* s, void* data)
{
	if (!s)
		return;
	s->data = data;
}

/**
 * @brief Call NET_StreamFree to dump the whole thing right now
 * @sa NET_StreamClose
 * @sa NET_StreamFinished
 */
void NET_StreamFree (struct net_stream* s)
{
	if (!s)
		return;
	s->finished = true;
	NET_StreamClose(s);
}

/**
 * @brief Call NET_StreamFinished to mark the stream as uninteresting, but to
 * finish sending any data in the buffer. The stream will appear
 * closed after this call, and at some unspecified point in the future
 * s will become an invalid pointer, so it should not be further
 * referenced.
 */
void NET_StreamFinished (struct net_stream* s)
{
	if (!s)
		return;

	s->finished = true;

	if (s->socket != INVALID_SOCKET)
		FD_CLR(s->socket, &read_fds);

	/* Stop the loopback peer from queueing stuff up in here */
	if (s->loopback_peer)
		s->loopback_peer->outbound = dbufferptr();

	const ScopedMutex scopedMutex(netMutex);
	s->inbound = dbufferptr();

	/* If there's nothing in the outbound buffer, any finished stream is
	 * ready to be closed */
	if (dbuffer_len(s->outbound) == 0)
		NET_StreamClose(s);
}

/**
 * @brief Returns the numerical representation of a @c net_stream
 * @note Not thread safe!
 */
const char* NET_StreamToString (struct net_stream* s)
{
	static char node[64];
	NET_StreamPeerToName(s, node, sizeof(node), false);
	return node;
}

/**
 * @param[in] s The network stream to get the name for
 * @param[out] dst The target buffer to store the ip and port in
 * @param[in] len The length of the target buffer
 * @param[in] appendPort Also append the port number to the target buffer
 */
const char* NET_StreamPeerToName (struct net_stream* s, char* dst, int len, bool appendPort)
{
	if (!s)
		return "(null)";

	if (NET_StreamIsLoopback(s))
		return "loopback connection";

	char buf[128];
	socklen_t addrlen = s->addrlen;
	if (getpeername(s->socket, (struct sockaddr* )buf, &addrlen) != 0)
		return "(error)";

	char node[64];
	char service[64];
	const int rc = getnameinfo((struct sockaddr* )buf, addrlen, node, sizeof(node), service, sizeof(service),
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (rc != 0) {
		Com_Printf("Failed to convert sockaddr to string: %s\n", gai_strerror(rc));
		return "(error)";
	}
	if (!appendPort) {
		Q_strncpyz(dst, node, len);
	} else {
		node[sizeof(node) - 1] = '\0';
		service[sizeof(service) - 1] = '\0';
		Com_sprintf(dst, len, "%s %s", node, service);
	}
	return dst;
}

void NET_StreamSetCallback (struct net_stream* s, stream_callback_func* func)
{
	if (!s)
		return;
	s->func = func;
}

bool NET_StreamIsLoopback (struct net_stream* s)
{
	return s && s->loopback;
}

static int NET_DoStartServer (const struct addrinfo* addr)
{
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;

	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", netStringError(netError));
		return INVALID_SOCKET;
	}

	if (!NET_SocketSetNonBlocking(sock)) {
		netCloseSocket(sock);
		return INVALID_SOCKET;
	}

#ifdef _WIN32
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*) &t, sizeof(t)) != 0) {
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

	maxfd = std::max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);
	server_family = addr->ai_family;
	server_addrlen = addr->ai_addrlen;

	return sock;
}

static struct addrinfo* NET_GetAddrinfoForNode (const char* node, const char* service)
{
	struct addrinfo* res;
	struct addrinfo hints;
	int rc;

	OBJZERO(hints);
	hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	/* force ipv4 */
	if (net_ipv4->integer)
		hints.ai_family = AF_INET;

	rc = getaddrinfo(node, service, &hints, &res);
	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node ? node : "*", service, gai_strerror(rc));
		return nullptr;
	}

	return res;
}

/**
 * @sa NET_DoStartServer
 * @param[in] node The node to start the server with
 * @param[in] service If this is nullptr we are in single player mode
 * @param[in] func The server callback function to read the packets
 * @sa SV_ReadPacket
 * @sa server_func
 * @sa SV_Stop
 */
bool SV_Start (const char* node, const char* service, stream_callback_func* func)
{
	if (!func)
		return false;

	if (server_running) {
		Com_Printf("SV_Start: Server is still running - call SV_Stop before\n");
		return false;
	}

	if (service) {
		struct addrinfo* res = NET_GetAddrinfoForNode(node, service);

		server_socket = NET_DoStartServer(res);
		if (server_socket == INVALID_SOCKET) {
			Com_Printf("Failed to start server on %s:%s\n", node ? node : "*", service);
		} else {
			server_running = true;
			server_func = func;
		}
		freeaddrinfo(res);
	} else {
		/* Loopback server only */
		server_running = true;
		server_func = func;
	}

	return server_running;
}

/**
 * @sa SV_Start
 */
void SV_Stop (void)
{
	server_running = false;
	server_func = nullptr;
	if (server_socket != INVALID_SOCKET) {
		FD_CLR(server_socket, &read_fds);
		netCloseSocket(server_socket);
	}
	server_socket = INVALID_SOCKET;
}

/**
 * @sa NET_DatagramSocketNew
 */
static struct datagram_socket* NET_DatagramSocketDoNew (const struct addrinfo* addr)
{
	SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	int t = 1;
	const int index = NET_DatagramFindFreeSocket();

	if (index == -1) {
		Com_Printf("Too many datagram sockets open\n");
		return nullptr;
	}

	if (sock == INVALID_SOCKET) {
		Com_Printf("Failed to create socket: %s\n", netStringError(netError));
		return nullptr;
	}

	if (!NET_SocketSetNonBlocking(sock)) {
		netCloseSocket(sock);
		return nullptr;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*) &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_REUSEADDR on socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return nullptr;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*) &t, sizeof(t)) != 0) {
		Com_Printf("Failed to set SO_BROADCAST on socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return nullptr;
	}

	if (bind(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
		Com_Printf("Failed to bind socket: %s\n", netStringError(netError));
		netCloseSocket(sock);
		return nullptr;
	}

	maxfd = std::max(sock + 1, maxfd);
	FD_SET(sock, &read_fds);

	datagram_socket* const s = Mem_PoolAllocType(datagram_socket, com_networkPool);
	s->family = addr->ai_family;
	s->addrlen = addr->ai_addrlen;
	s->socket = sock;
	s->index = index;
	s->queue = nullptr;
	s->queue_tail = &s->queue;
	s->func = nullptr;
	datagram_sockets[index] = s;

	return s;
}

/**
 * @brief Opens a datagram socket (UDP)
 * @sa NET_DatagramSocketDoNew
 * @param[in] node The numeric address to resolv (might be nullptr)
 * @param[in] service The port number
 * @param[in] func Callback function for data handling
 */
struct datagram_socket* NET_DatagramSocketNew (const char* node, const char* service, datagram_callback_func* func)
{
	struct datagram_socket* s;
	struct addrinfo* res;
	struct addrinfo hints;
	int rc;

	if (!service || !func)
		return nullptr;

	OBJZERO(hints);
	hints.ai_flags = AI_NUMERICHOST | AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;
	/* force ipv4 */
	if (net_ipv4->integer)
		hints.ai_family = AF_INET;

	rc = getaddrinfo(node, service, &hints, &res);

	if (rc != 0) {
		Com_Printf("Failed to resolve host %s:%s: %s\n", node ? node : "*", service, gai_strerror(rc));
		return nullptr;
	}

	s = NET_DatagramSocketDoNew(res);
	if (s)
		s->func = func;

	freeaddrinfo(res);
	return s;
}

/**
 * @sa NET_DatagramSocketNew
 */
void NET_DatagramSend (struct datagram_socket* s, const char* buf, int len, struct sockaddr* to)
{
	if (!s || len <= 0 || !buf || !to)
		return;

	datagram* const dgram = Mem_PoolAllocType(datagram, com_networkPool);
	dgram->msg  = Mem_PoolAllocTypeN(char, len,        com_networkPool);
	dgram->addr = Mem_PoolAllocTypeN(char, s->addrlen, com_networkPool);
	memcpy(dgram->msg, buf, len);
	memcpy(dgram->addr, to, len);
	dgram->len = len;
	dgram->next = nullptr;

	*s->queue_tail = dgram;
	s->queue_tail = &dgram->next;

	FD_SET(s->socket, &write_fds);
}

/**
 * @sa NET_DatagramSend
 * @sa NET_DatagramSocketNew
 * @todo This is only sending on the first available device, what if we have several devices?
 */
void NET_DatagramBroadcast (struct datagram_socket* s, const char* buf, int len, int port)
{
	if (s->family == AF_INET) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		NET_DatagramSend(s, buf, len, (struct sockaddr* )&addr);
	} else if (s->family == AF_INET6) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET6;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		NET_DatagramSend(s, buf, len, (struct sockaddr* )&addr);
	} else {
		Com_Error(ERR_DROP, "Broadcast unsupported on address family %d\n", s->family);
	}
}

/**
 * @sa NET_DatagramSocketNew
 * @sa NET_DatagramSocketDoNew
 */
void NET_DatagramSocketClose (struct datagram_socket* s)
{
	if (!s)
		return;

	FD_CLR(s->socket, &read_fds);
	FD_CLR(s->socket, &write_fds);
	netCloseSocket(s->socket);

	while (s->queue) {
		struct datagram* dgram = s->queue;
		s->queue = dgram->next;
		Mem_Free(dgram->msg);
		Mem_Free(dgram->addr);
		Mem_Free(dgram);
	}

	datagram_sockets[s->index] = nullptr;
	Mem_Free(s);
}

/**
 * @brief Convert sockaddr to string
 * @param[in] s The datagram socket type to get the addrlen from
 * @param[in] addr The socket address to convert into a string
 * @param[out] node The target node name buffer
 * @param[in] nodelen The length of the node name buffer
 * @param[out] service The target service name buffer
 * @param[in] servicelen The length of the service name buffer
 */
void NET_SockaddrToStrings (struct datagram_socket* s, struct sockaddr* addr, char* node, size_t nodelen, char* service, size_t servicelen)
{
	const int rc = getnameinfo(addr, s->addrlen, node, nodelen, service, servicelen,
			NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM);
	if (rc != 0) {
		Com_Printf("Failed to convert sockaddr to string: %s\n", gai_strerror(rc));
		Q_strncpyz(node, "(error)", nodelen);
		Q_strncpyz(service, "(error)", servicelen);
	}
}

static void NET_AddrinfoToString (const struct addrinfo* addr, char* buf, size_t bufLength)
{
	char* service = inet_ntoa(((struct sockaddr_in*)addr->ai_addr)->sin_addr);
	Q_strncpyz(buf, service, bufLength);
}

bool NET_ResolvNode (const char* node, char* buf, size_t bufLength)
{
	struct addrinfo* addrinfo = NET_GetAddrinfoForNode(node, nullptr);
	if (addrinfo == nullptr) {
		buf[0] = '\0';
		return false;
	}
	NET_AddrinfoToString(addrinfo, buf, bufLength);
	freeaddrinfo(addrinfo);
	return true;
}
