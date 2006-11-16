/**
 * @file ms_server.h
 * @brief Master server header file
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

Most of this stuff comes from Warsow

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

#define STRINGIFY(x) #x
#define DOUBLEQUOTE(x) STRINGIFY(x)

static cvar_t *irc_server = NULL;
static cvar_t *irc_port = NULL;
static qboolean irc_connected = qfalse;

static irc_socket_t irc_socket = -1;

/*
===============================================================
Protokoll functions
===============================================================
*/

cvar_t *irc_messageBucketSize = NULL;
cvar_t *irc_messageBucketBurst = NULL;
cvar_t *irc_messageBucketRate = NULL;
cvar_t *irc_characterBucketSize = NULL;
cvar_t *irc_characterBucketBurst = NULL;
cvar_t *irc_characterBucketRate = NULL;

typedef struct irc_bucket_message_s {
	char *msg;
	size_t msg_len;
	struct irc_bucket_message_s *next;
} irc_bucket_message_t;

typedef struct irc_bucket_s {
	irc_bucket_message_t *first_msg;	/* pointer to first message in queue */
	unsigned int message_size;			/* number of messages in bucket */
	unsigned int character_size;		/* number of characters in bucket */
	int last_refill;				/* last refill timestamp */
	double message_token;
	double character_token;
} irc_bucket_t;

static qboolean Irc_Proto_ParseServerMsg(const char *txt, size_t txt_len, irc_server_msg_t *msg);

static qboolean Irc_Proto_Enqueue(const char *msg, size_t msg_len);
static void Irc_Proto_RefillBucket(void);
static qboolean Irc_Proto_DrainBucket(void);

static irc_bucket_t irc_bucket;

/**
 * @brief
 */
qboolean Irc_Proto_Connect (const char *host, unsigned short port)
{
	const qboolean status = Irc_Net_Connect(host, port);
	if (!status) {
		if (!irc_messageBucketSize) {
			irc_messageBucketSize = Cvar_Get("irc_messageBucketSize", DOUBLEQUOTE(IRC_DEFAULT_MESSAGE_BUCKET_SIZE), CVAR_ARCHIVE, NULL);
			irc_messageBucketBurst = Cvar_Get("irc_messageBucketBurst", DOUBLEQUOTE(IRC_DEFAULT_MESSAGE_BUCKET_BURST), CVAR_ARCHIVE, NULL);
			irc_messageBucketRate = Cvar_Get("irc_messageBucketRate", DOUBLEQUOTE(IRC_DEFAULT_MESSAGE_BUCKET_RATE), CVAR_ARCHIVE, NULL);
			irc_characterBucketSize = Cvar_Get("irc_characterBucketSize", DOUBLEQUOTE(IRC_DEFAULT_CHARACTER_BUCKET_SIZE), CVAR_ARCHIVE, NULL);
			irc_characterBucketBurst = Cvar_Get("irc_characterBucketBurst", DOUBLEQUOTE(IRC_DEFAULT_CHARACTER_BUCKET_BURST), CVAR_ARCHIVE, NULL);
			irc_characterBucketRate = Cvar_Get("irc_characterBucketRate", DOUBLEQUOTE(IRC_DEFAULT_CHARACTER_BUCKET_RATE), CVAR_ARCHIVE, NULL);
		}
		irc_bucket.first_msg = NULL;
		irc_bucket.message_size = 0;
		irc_bucket.character_size = 0;
		irc_bucket.last_refill = Sys_Milliseconds();
		irc_bucket.message_token = (double)irc_messageBucketBurst->value;
		irc_bucket.character_token = (double)irc_characterBucketBurst->value;
	}
	return status;
}

/**
 * @brief
 */
qboolean Irc_Proto_Disconnect (void)
{
	const qboolean status = Irc_Net_Disconnect();
	if (!status) {
		irc_bucket_message_t *msg = irc_bucket.first_msg;
		irc_bucket_message_t *prev;
		while (msg) {
			prev = msg;
			msg = msg->next;
			Mem_Free(prev->msg);
			Mem_Free(prev);
		}
		irc_bucket.first_msg = NULL;
		irc_bucket.message_size = 0;
		irc_bucket.character_size = 0;
	}
	return status;
}

/**
 * @brief
 */
qboolean Irc_Proto_Quit (const char *quitmsg)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "QUIT %s\r\n", quitmsg);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Net_Send(msg, msg_len);	/* send immediately */
}

/**
 * @brief
 */
qboolean Irc_Proto_Nick (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "NICK %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_User (const char *user, qboolean invisible, const char *name)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "USER %s %c * :%s\r\n", user, invisible ? '8' : '0', name);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Password (const char *password)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PASS %s\r\n", password);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Join (const char *channel, const char *password)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = password
		? snprintf(msg, sizeof(msg) - 1, "JOIN %s %s\r\n", channel, password)
		: snprintf(msg, sizeof(msg) - 1, "JOIN %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Part (const char *channel)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PART %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Mode (const char *target, const char *modes, const char *params)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = params
		? snprintf(msg, sizeof(msg) - 1, "MODE %s %s %s\r\n", target, modes, params)
		: snprintf(msg, sizeof(msg) - 1, "MODE %s %s\r\n", target, modes);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Topic (const char *channel, const char *topic)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = topic
		? snprintf(msg, sizeof(msg) - 1, "TOPIC %s :%s\r\n", channel, topic)
		: snprintf(msg, sizeof(msg) - 1, "TOPIC %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Msg (const char *target, const char *text)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PRIVMSG %s :%s\r\n", target, text);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Notice (const char *target, const char *text)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "NOTICE %s :%s\r\n", target, text);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Pong (const char *nick, const char *server, const char *cookie)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = cookie
		? snprintf(msg, sizeof(msg) - 1, "PONG %s %s :%s\r\n", nick, server, cookie)
		: snprintf(msg, sizeof(msg) - 1, "PONG %s %s\r\n", nick, server);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Net_Send(msg, msg_len);	/* send immediately */
}

/**
 * @brief
 */
qboolean Irc_Proto_Kick (const char *channel, const char *nick, const char *reason)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = reason
		? snprintf(msg, sizeof(msg) - 1, "KICK %s %s :%s\r\n", channel, nick, reason)
		: snprintf(msg, sizeof(msg) - 1, "KICK %s %s :%s\r\n", channel, nick, nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Who (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHO %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Whois (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHOIS %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_Whowas (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHOWAS %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @brief
 */
qboolean Irc_Proto_PollServerMsg (irc_server_msg_t *msg, qboolean *msg_complete)
{
	static char buf[IRC_RECV_BUF_SIZE];
	static char *last = buf;
	int recvd;
	*msg_complete = qfalse;
	/* recv packet */
	if (Irc_Net_Receive(last, sizeof(buf) - (last - buf) - 1, &recvd)) {
		/* receive failed */
		return qtrue;
	} else {
		/* terminate buf string */
		const char * const begin = buf;
		last += recvd;
		*last = '\0';
		if (last != begin) {
			/* buffer not empty; */
			const char * const end = strstr(begin, "\r\n");
			if (end) {
				/* complete command in buffer, parse */
				const size_t cmd_len = end + 2 - begin;
				if (!Irc_Proto_ParseServerMsg(begin, cmd_len, msg)) {
					/* parsing successful */
					/* move succeeding commands to begin of buffer */
					memmove(buf, end + 2, sizeof(buf) - cmd_len);
					last -= cmd_len;
					*msg_complete = qtrue;
				} else {
					/* parsing failure, fatal */
					Com_Printf("Received invalid packet from server\n");
					return qtrue;
				}
			}
		} else
			*msg_complete = qfalse;
		return qfalse;
	}
}

/**
 * @brief
 */
qboolean Irc_Proto_ProcessServerMsg (const irc_server_msg_t *msg)
{
	irc_command_t cmd;
	cmd.type = msg->type;
	switch (cmd.type) {
		case IRC_COMMAND_NUMERIC:
			cmd.id.numeric = msg->id.numeric;
			break;
		case IRC_COMMAND_STRING:
			cmd.id.string = msg->id.string;
			break;
	}
/*	Irc_Proto_CallListeners(cmd, msg->prefix, msg->params, msg->trailing);*/
	return qfalse;
}

/**
 * @brief
 */
static qboolean Irc_Proto_ParseServerMsg (const char *txt, size_t txt_len, irc_server_msg_t *msg)
{
	const char *c = txt;
	const char *end = txt + txt_len;
	*(msg->prefix) = '\0';
	*(msg->params) = '\0';
	*(msg->trailing) = '\0';
	if (c < end && *c == ':') {
		/* parse prefix */
		char *prefix = msg->prefix;
		++c;
		while (c < end && *c != '\r' && *c != ' ') {
			*prefix = *c;
			++prefix;
			++c;
		}
		*prefix = '\0';
		++c;
	}
	if (c < end && *c != '\r') {
		/* parse command */
		if (c < end && *c >= '0' && *c <= '9') {
			/* numeric command */
			char command[4];
			int i;
			for (i = 0; i < 3; ++i) {
				if (c < end && *c >= '0' && *c <= '9') {
					command[i] = *c;
					++c;
				} else
					return qtrue;
			}
			command[3] = '\0';
			msg->type = IRC_COMMAND_NUMERIC;
			msg->id.numeric = atoi(command);
		} else if (c < end && *c != '\r') {
			/* string command */
			char *command = msg->id.string;
			while (c < end && *c != '\r' && *c != ' ') {
				*command = *c;
				++command;
				++c;
			}
			*command = '\0';
			msg->type = IRC_COMMAND_STRING;
		} else
			return qtrue;
		if (c < end && *c == ' ') {
			/* parse params and trailing */
			char *params = msg->params;
			++c;
			while (c < end && *c != '\r' && *c != ':') {
				/* parse params */
				while (c < end && *c != '\r' && *c != ' ') {
					/* parse single param */
					*params = *c;
					++params;
					++c;
				}
				if (c + 1 < end && *c == ' ' && *(c+1) != ':') {
					/* more params */
					*params = ' ';
					++params;
				}
				if (*c == ' ')
					++c;
			}
			*params = '\0';
			if (c < end && *c == ':') {
				/* parse trailing */
				char *trailing = msg->trailing;
				++c;
				while (c < end && *c != '\r') {
					*trailing = *c;
					++trailing;
					++c;
				}
				*trailing = '\0';
			}
		}
	}
	return qfalse;
}

/**
 * @brief
 */
qboolean Irc_Proto_Flush (void)
{
	Irc_Proto_RefillBucket();		/* first refill token */
	return Irc_Proto_DrainBucket();	/* then send messages (if allowed) */
}

/**
 * @brief
 */
static qboolean Irc_Proto_Enqueue (const char *msg, size_t msg_len)
{
	/* create message node */
	const double messageBucketSize = irc_messageBucketSize->value;
	const double characterBucketSize = irc_characterBucketSize->value;
	irc_bucket_message_t * const m = (irc_bucket_message_t*) Mem_Alloc(sizeof(irc_bucket_message_t));
	irc_bucket_message_t * n = irc_bucket.first_msg;
	if (irc_bucket.message_size + 1 <= messageBucketSize && irc_bucket.character_size + msg_len <= characterBucketSize) {
		m->msg = (char*) Mem_Alloc(msg_len);
		memcpy(m->msg, msg, msg_len);
		m->msg_len = msg_len;
		m->next = NULL;
		/* append message node */
		if (n) {
			while (n->next)
				n = n->next;
			n->next = m;
		} else
			irc_bucket.first_msg = m;
		/* update bucket sizes */
		++irc_bucket.message_size;
		irc_bucket.character_size += msg_len;
		return qfalse;
	} else {
		Com_Printf("Bucket(s) full. Could not enqueue message\n");
		return qtrue;
	}
}

/**
 * @brief
 */
static void Irc_Proto_RefillBucket (void)
{
	/* calculate token refill */
	const double messageBucketSize = irc_messageBucketSize->value;
	const double characterBucketSize = irc_characterBucketSize->value;
	const double messageBucketRate = irc_messageBucketRate->value;
	const double characterBucketRate = irc_characterBucketRate->value;
	const int micros = Sys_Milliseconds();
	const int micros_delta = micros - irc_bucket.last_refill;
	const double msg_delta = (micros_delta * messageBucketRate) / 1000000;
	const double msg_new = irc_bucket.message_token + msg_delta;
	const double char_delta = (micros_delta * characterBucketRate) / 1000000;
	const double char_new = irc_bucket.character_token + char_delta;
	/* refill token (but do not exceed maximum) */
	irc_bucket.message_token = min(msg_new, messageBucketSize);
	irc_bucket.character_token = min(char_new, characterBucketSize);
	/* set timestamp so next refill can calculate delta */
	irc_bucket.last_refill = micros;
}

/**
 * @brief
 */
static qboolean Irc_Proto_DrainBucket (void)
{
	const double characterBucketBurst = irc_characterBucketBurst->value;
	qboolean status = qfalse;
	irc_bucket_message_t *msg;
	/* remove messages whose size exceed our burst size (we can not send them) */
	for (msg = irc_bucket.first_msg;
		msg && msg->msg_len > characterBucketBurst;
		msg = irc_bucket.first_msg) {
		irc_bucket_message_t * const next = msg->next;
		/* update bucket sizes */
		--irc_bucket.message_size;
		irc_bucket.character_size -= msg->msg_len;
		/* free message */
		Mem_Free(msg->msg);
		/* dequeue message */
		irc_bucket.first_msg = next;
	}
	/* send burst of remaining messages */
	for (msg = irc_bucket.first_msg;
		msg && !status && irc_bucket.message_token >= 1.0 && msg->msg_len <= irc_bucket.character_token;
		msg = irc_bucket.first_msg) {
		/* send message */
		status = Irc_Net_Send(msg->msg, msg->msg_len);
		--irc_bucket.message_token;
		irc_bucket.character_token -= msg->msg_len;
		/* dequeue message */
		irc_bucket.first_msg = msg->next;
		/* update bucket sizes */
		--irc_bucket.message_size;
		irc_bucket.character_size -= msg->msg_len;
		/* free message */
		Mem_Free(msg->msg);
		Mem_Free(msg);
	}
	return status;
}

/*
===============================================================
Network functions
===============================================================
*/

/**
 * @brief
 */
qboolean Irc_Net_Connect (const char *host, unsigned short port)
{
	int status;
	qboolean failed = qtrue;

	irc_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (irc_socket >= 0) {
		struct sockaddr_in addr;
		struct hostent *he;
		memset(&addr, 0, sizeof(addr));
		he = gethostbyname(host);		/* DNS lookup */
		if (he) {
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
qboolean Irc_Net_Disconnect (void)
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
qboolean Irc_Net_Send (const char *msg, size_t msg_len)
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
qboolean Irc_Net_Receive (char *buf, size_t buf_len, int *recvd)
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

/*
===============================================================
Bindings
===============================================================
*/

/**
 * @brief
 */
extern void Irc_Connect_f (void)
{
	const int argc = Cmd_Argc();
	if (argc <= 3) {
		if (!irc_connected) {
			/* not connected yet */
			if (argc >= 2)
				Cvar_Set("irc_server", Cmd_Argv(1));
			if (argc >= 3)
				Cvar_Set("irc_port", Cmd_Argv(2));
			Com_Printf("Connect to %s:%s\n", irc_server->string, irc_port->string);
			if (Irc_Proto_Connect(irc_server->string, (int)irc_port->value)) {
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
