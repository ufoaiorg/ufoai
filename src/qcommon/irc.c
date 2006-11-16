/**
 * @file ms_server.h
 * @brief Master server header file
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

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
#include "irc.h"

#ifdef _WIN32
#	include <winerror.h>
#else
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netdb.h>
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#endif


static cvar_t *irc_server = NULL;
static cvar_t *irc_port = NULL;
static qboolean irc_connected = qfalse;

static irc_socket_t irc_socket = -1;

/**
 * @brief
 */
qboolean Irc_Net_Connect(const char *host, unsigned short port)
{
	qboolean failed = qtrue;
	irc_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (irc_socket >= 0) {
		struct sockaddr_in addr;
		struct hostent *he;
		memset(&addr, 0, sizeof(addr));
		he = gethostbyname(host);		/* DNS lookup */
		if (he) {
			int status;
			/* convert host entry to sockaddr_in */
			addr.sin_port = htons(port);
			addr.sin_addr.s_addr = ((struct in_addr*) he->h_addr)->s_addr;
			addr.sin_family = AF_INET;
			status = connect(irc_socket, (const struct sockaddr*) &addr, sizeof(addr));
			if (!status) {
				/* connection successful */
				failed = qfalse;
			} else {
				Com_Printf("Irc_Net_Connect: Connection refused\n");
#ifdef _WIN32
				closesocket(irc_socket);
#else
				close(irc_socket);
#endif
			}
		} else {
			Com_Printf("Irc_Net_Connect: Unknown host\n");
#ifdef _WIN32
			closesocket(irc_socket);
#else
			close(irc_socket);
#endif
		}
	} else
		Com_Printf("Irc_Net_Connect: Could not create socket\n");

	if (!failed) {
		int status;
#ifdef _WIN32
		unsigned long one = 1;
		status = ioctlsocket(irc_socket, FIONBIO, &one);
#else
		status = fcntl(irc_socket, F_SETFL, O_NONBLOCK) == -1;
#endif
		if (status) {
			Com_Printf("Irc_Net_Connect: Could not set non-blocking socket mode\n");
			failed = qtrue;
		}
	}

	return failed;
}

/**
 * @brief
 */
qboolean Irc_Net_Disconnect(void)
{
#ifdef _WIN32
	return closesocket(irc_socket) < 0;
#else
	return close(irc_socket) == 0;
#endif
}

/**
 * @brief
 */
qboolean Irc_Net_Send(const char *msg, size_t msg_len)
{
	int sent;
	assert(msg);
	sent = send(irc_socket, msg, (int) msg_len, 0);
	if (sent >= 0)
		return qfalse;
	else {
		Com_Printf("Irc_Net_Send: send failed\n");
		return qtrue;
	}
}

/**
 * @brief
 */
qboolean Irc_Net_Receive(char *buf, size_t buf_len, int *recvd)
{
	assert(buf);
	assert(recvd);
	*recvd = recv(irc_socket, buf, (int) buf_len, 0);
#ifdef _WIN32
	if (*recvd < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
		*recvd = 0;
#else
	if (*recvd < 0 && errno == EAGAIN)
		*recvd = 0;
#endif
	if (*recvd >= 0)
		return qfalse;
	else {
		Com_Printf("Irc_Net_Receive: recv failed\n");
		return qtrue;
	}
}

/**
 * @brief
 */
extern void Irc_Connect_f(void)
{
	const int argc = Cmd_Argc();
	if (argc <= 3) {
		if (!irc_connected) {
			/* not connected yet */
			if (argc >= 2)
				Cvar_Set("irc_server", Cmd_Argv(1));
			if (argc >= 3)
				Cvar_Set("irc_port", Cmd_Argv(2));
			Irc_Net_Connect(irc_server->string, (int)irc_port->value);
			if (!irc_connected) {
				/* connect failed */
				Com_Printf("Could not connect to %s.\n", irc_server->string);
			}
		} else
			Com_Printf("Already connected.\n");
	} else
			Com_Printf("usage: irc_connect [<server>] [<port>]");
}

/**
 * @brief
 */
extern void Irc_Disconnect_f(void)
{
	if (irc_connected) {
		/* still connected, proceed */
		Irc_Net_Disconnect();
	} else
		Com_Printf("Not connected.\n");
}

/**
 * @brief
 */
extern void Irc_Init(void)
{
	/* commands */
	Cmd_AddCommand("irc_connect", Irc_Connect_f, "Connect to the irc network");
	Cmd_AddCommand("irc_disconnect", Irc_Disconnect_f, "Disconnect from the irc network");

	/* cvars */
	irc_server = Cvar_Get("irc_server", "irc.freenode.org", CVAR_ARCHIVE, "IRC server to connect to");
	irc_port = Cvar_Get("irc_port", "", CVAR_ARCHIVE, "IRC port to connect to");
}

/**
 * @brief
 */
extern void Irc_Shutdown(void)
{

}
