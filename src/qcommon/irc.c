/**
 * @file irc.c
 * @brief IRC client implementation for UFO:AI
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

#include "../client/client.h"
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
static cvar_t *irc_channel = NULL;
static cvar_t *irc_nick = NULL;
static cvar_t *irc_user = NULL;
static cvar_t *irc_password = NULL;
static cvar_t *irc_topic = NULL;
static cvar_t *irc_defaultChannel = NULL;
static cvar_t *irc_logConsole = NULL;
/* menu cvar */
static cvar_t *irc_send_buffer = NULL;


static qboolean irc_connected = qfalse;

static irc_socket_t irc_socket = -1;

static const qboolean IRC_INVISIBLE = qtrue;
static const char IRC_QUIT_MSG[] = "ufo.myexp.de";

static irc_channel_t *chan = NULL;

static char irc_buffer[4048];

#define IRC_MESSAGEMODE_BUFSIZE 256
static char	irc_messagemode_buf[IRC_MESSAGEMODE_BUFSIZE];
static int	irc_messagemode_buflen = 0;

/*
===============================================================
Common functions
===============================================================
*/

/**
 * @brief
 */
static qboolean Irc_IsChannel(const char *target)
{
	assert(target);
	return (*target == '#' || *target == '&');
}

/**
 * @brief
 */
void Irc_ParseName(const char *mask, char *nick, irc_nick_prefix_t *prefix)
{
	const char *emph;
	if (*mask == IRC_NICK_PREFIX_OP || *mask == IRC_NICK_PREFIX_VOICE) {
		*prefix = (irc_nick_prefix_t) *mask;	/* read prefix */
		++mask;									/* crop prefix from mask */
	} else
		*prefix = IRC_NICK_PREFIX_NONE;
	emph = strchr(mask, '!');
	if (emph) {
		/* complete hostmask, crop anything after ! */
		memcpy(nick, mask, emph - mask);
		nick[emph - mask] = '\0';
	} else
		/* just the nickname, use as is */
		strcpy(nick, mask);
}

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
 * @sa Irc_Proto_Disconnect
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
 * @sa Irc_Proto_Connect
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
 * @sa Irc_Net_Send
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Net_Send
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Proto_Enqueue
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
 * @sa Irc_Logic_ReadMessages
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
 * @brief Append the irc message to the buffer
 * @note the irc_buffer is linked to menuText array to display in the menu
 * @param[in] msg the complete irc message (with \n)
 */
static void Irc_AppendToBuffer (const char* const msg)
{
	char buf[IRC_RECV_BUF_SIZE];
	/* FIXME */
	if (strlen(irc_buffer) + strlen(msg) >= sizeof(irc_buffer) ) {
		Com_Printf("clear irc buffer\n");
		memset(irc_buffer, 0, sizeof(irc_buffer));
	}

	Com_sprintf(buf, sizeof(buf), "%s\n", msg);
	Q_strcat(irc_buffer, buf, sizeof(irc_buffer));
	if (irc_logConsole->value)
		Com_Printf("IRC: %s\n", msg);
	MN_TextScrollBottom("irc_data");
}

/**
 * @brief
 */
static void Irc_Client_CmdRplWhowasuser(const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *user = "", *host = "", *real_name = trailing;
	char *p;
	unsigned int i = 0;

	/* parse params "<nick> <user> <host> * :<real name>" */
	Q_strncpyz(buf, params, sizeof(buf));
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				user = p;
				break;
			case 3:
				host = p;
				break;
		}
	}
	Irc_AppendToBuffer(va("%s was %s@%s : %s", nick, user, host, real_name));
}

/**
 * @brief
 */
static void Irc_Client_CmdRplTopic (const char *params, const char *trailing)
{
	const char *chan = strchr(params, ' ');
	if (chan) {
		++chan;
		Irc_AppendToBuffer(va("%s | Topic is: \"%s\"", chan, trailing));
	}
}

/**
 * @brief
 */
static void Irc_Client_CmdRplNotopic (const char *params)
{
	const char *chan = strchr(params, ' ');
	if (chan) {
		++chan;
		Irc_AppendToBuffer(va("%s | No topic set.", chan));
	}
}

/**
 * @brief
 */
static void Irc_Client_CmdRplWhoisuser (const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *user = "", *host = "", *real_name = trailing;
	char *p;
	unsigned int i = 0;

	/* parse params "<nick> <user> <host> * :<real name>" */
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				user = p;
				break;
			case 3:
				host = p;
				break;
		}
	}
	Irc_AppendToBuffer(va("%s is %s@%s : %s", nick, user, host, real_name));
}

/**
 * @brief
 */
static void Irc_Client_CmdRplWhoisserver (const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *server = "", *server_info = trailing;
	char *p;
	unsigned int i = 0;

	/* parse params "<nick> <server> :<server info>" */
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				server = p;
				break;
		}
	}
	Irc_AppendToBuffer(va("%s using %s : %s", nick, server, server_info));
}

/**
 * @brief
 */
static void Irc_Client_CmdRplWhoisaccount (const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *account = "";
	char *p;
	unsigned int i = 0;

	/* parse params "<nick> <account> :is logged in as" */
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				account = p;
				break;
		}
	}
	Irc_AppendToBuffer(va("%s %s %s", nick, trailing, account));
}

/**
 * @brief
 */
static void Irc_Client_CmdRplWhoisidle (const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *idle = "";
	char *p;
	unsigned int i = 0;

	/* parse params "<nick> <integer> :seconds idle" */
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				idle = p;
				break;
		}
	}
	Irc_AppendToBuffer(va("%s is %s %s", nick, idle, trailing));
}

/**
 * @brief
 */
static void Irc_Client_CmdRplWhoreply (const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	const char *channel = "", *user = "", *host = "", *server = "", *nick = "", *hg = "";
	char *p;
	unsigned int i = 0;

	/* parse params "<channel> <user> <host> <server> <nick> <H|G>[*][@|+] :<hopcount> <real name>" */
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 0:
				channel = p;
				break;
			case 1:
				user = p;
				break;
			case 2:
				host = p;
				break;
			case 3:
				server = p;
				break;
			case 4:
				nick = p;
				break;
			case 5:
				hg = p;
				break;
		}
	}
	Irc_AppendToBuffer(va("%s %s %s %s %s %s : %s", channel, user, host, server, nick, hg, trailing));
}

/**
 * @brief
 */
static void Irc_Client_CmdMode (const char *prefix, const char *params, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t pfx;
	Irc_ParseName(prefix, nick, &pfx);
	Irc_AppendToBuffer(va("%s sets mode %s", nick, params));
}

/**
 * @brief
 */
static void Irc_Client_CmdJoin (const char *prefix, const char *params, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_AppendToBuffer(va("Joins: %s (%s)", nick, prefix));
}

/**
 * @brief
 */
static void Irc_Client_CmdPart (const char *prefix, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_AppendToBuffer(va("Parts: %s (%s)", nick, prefix));
}

/**
 * @brief
 */
static void Irc_Client_CmdQuit (const char *prefix, const char *params, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_AppendToBuffer(va("Quits: %s (%s)", nick, trailing));
}

/**
 * @brief
 */
static void Irc_Client_CmdKill (const char *prefix, const char *params, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_AppendToBuffer(va("Killed: %s (%s)", nick, trailing));
}

/**
 * @brief
 */
static void Irc_Client_CmdKick (const char *prefix, const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	const char *chan, *victim;
	Irc_ParseName(prefix, nick, &p);
	strcpy(buf, params);
	chan = strtok(buf, " ");
	victim = strtok(NULL, " ");
	if (!strcmp(victim, irc_nick->string)) {
		/* we have been kicked */
		Irc_AppendToBuffer(va("You were kicked from %s by %s (%s)", chan, nick, trailing));
	} else {
		/* someone else was kicked */
		Irc_AppendToBuffer(va("%s kicked %s (%s)", nick, victim, trailing));
	}
}

/**
 * @brief
 */
static void Irc_Client_CmdTopic (const char *prefix, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_AppendToBuffer(va("%s sets topic: \"%s\"", nick, trailing));
	Cvar_ForceSet("irc_topic", trailing);
}

/**
 * @brief Changes the cvar 'name' with the new name you set
 */
static void Irc_Client_CmdNick (const char *prefix, const char *params, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	if (!strcmp(irc_nick->string, nick))
		Cvar_ForceSet("name", trailing);
	Irc_AppendToBuffer(va("%s is now known as %s", nick, trailing));
}

/**
 * @brief
 */
static void Irc_Client_CmdPrivmsg (const char *prefix, const char *params, const char *trailing)
{
	char nick[IRC_SEND_BUF_SIZE];
	char * const emph = strchr(prefix, '!');
	memset(nick, 0, sizeof(nick));
	if (emph)
		memcpy(nick, prefix, emph - prefix);
	else
		strcpy(nick, prefix);
	Irc_AppendToBuffer(va("<%s> %s", nick, trailing));
}

/**
 * @brief
 * @TODO: Implement me
 */
static void Irc_Client_CmdRplNamreply (const char *params, const char *trailing)
{
}

/**
 * @brief
 * @TODO: Implement me
 */
static void Irc_Client_CmdRplEndofnames (const char *params, const char *trailing)
{
}

/**
 * @brief
 * @sa Irc_Logic_ReadMessages
 */
qboolean Irc_Proto_ProcessServerMsg (const irc_server_msg_t *msg)
{
	irc_command_t cmd;
	const char *p = NULL;
	cmd.type = msg->type;

	/* some debug output */
	Com_DPrintf("pre: '%s', param: '%s', trail: '%s'\n", msg->prefix, msg->params, msg->trailing);

	switch (cmd.type) {
	case IRC_COMMAND_NUMERIC:
		cmd.id.numeric = msg->id.numeric;
#ifdef DEBUG
		Com_DPrintf("<%s> [%03d] %s : %s", msg->prefix, cmd.id.numeric, msg->params, msg->trailing);
#endif
		switch (cmd.id.numeric) {
		case RPL_HELLO:
		case RPL_WELCOME:
		case RPL_YOURHOST:
		case RPL_CREATED:
		case RPL_MYINFO:
		case RPL_MOTDSTART:
		case RPL_MOTD:
		case RPL_ENDOFMOTD:
		case RPL_LOCALUSERS:
		case RPL_GLOBALUSERS:
			Irc_AppendToBuffer(msg->trailing);
			return qtrue;

		case RPL_ISUPPORT:
		case RPL_LUSEROP:
		case RPL_LUSERUNKNOWN:
		case RPL_LUSERCHANNELS:
		case RPL_LUSERCLIENT:
		case RPL_LUSERME:
			p = strchr(msg->params, ' ');	/* skip first param (nick) */
			if (p) {
				++p;
				Irc_AppendToBuffer(va("%s %s", p, msg->trailing));
			} else
				Irc_AppendToBuffer(msg->trailing);
			return qtrue;

		case RPL_NAMREPLY:
			Irc_Client_CmdRplNamreply(msg->params, msg->trailing);
			return qtrue;
		case RPL_ENDOFNAMES:
			Irc_Client_CmdRplEndofnames(msg->params, msg->trailing);
			return qtrue;
		case RPL_TOPIC:
			Irc_Client_CmdRplTopic(msg->params, msg->trailing);
			return qtrue;
		case RPL_NOTOPIC:
			Irc_Client_CmdRplNotopic(msg->params);

		case RPL_WHOISUSER:
			Irc_Client_CmdRplWhoisuser(msg->params, msg->trailing);
			return qtrue;
		case RPL_WHOISSERVER:
			Irc_Client_CmdRplWhoisserver(msg->params, msg->trailing);
			return qtrue;
		case RPL_WHOISIDLE:
			Irc_Client_CmdRplWhoisidle(msg->params, msg->trailing);
			return qtrue;
		case RPL_WHOISACCOUNT:
			Irc_Client_CmdRplWhoisaccount(msg->params, msg->trailing);
			return qtrue;
		case RPL_WHOREPLY:
			Irc_Client_CmdRplWhoreply(msg->params, msg->trailing);
			return qtrue;
		case RPL_WHOWASUSER:
			Irc_Client_CmdRplWhowasuser(msg->params, msg->trailing);
			return qtrue;
		case RPL_ENDOFWHO:
		case RPL_WHOISCHANNELS:
		case RPL_WHOISOPERATOR:
		case RPL_ENDOFWHOIS:
		case RPL_ENDOFWHOWAS:
			p = strchr(msg->params, ' ');
			if (p) {
				++p;
				Irc_AppendToBuffer(va("%s %s", p, msg->trailing));
			}
			return qtrue;

		/* error codes */
		case ERR_NOSUCHNICK:
		case ERR_NOSUCHSERVER:
		case ERR_NOSUCHCHANNEL:
		case ERR_CANNOTSENDTOCHAN:
		case ERR_TOOMANYCHANNELS:
		case ERR_WASNOSUCHNICK:
		case ERR_TOOMANYTARGETS:
		case ERR_NOORIGIN:
		case ERR_NORECIPIENT:
		case ERR_NOTEXTTOSEND:
		case ERR_NOTOPLEVEL:
		case ERR_WILDTOPLEVEL:
		case ERR_UNKNOWNCOMMAND:
		case ERR_NOMOTD:
		case ERR_NOADMININFO:
		case ERR_FILEERROR:
		case ERR_NONICKNAMEGIVEN:
		case ERR_ERRONEUSNICKNAME:
		case ERR_NICKNAMEINUSE:
		case ERR_NICKCOLLISION:
		case ERR_BANNICKCHANGE:
		case ERR_NCHANGETOOFAST:
		case ERR_USERNOTINCHANNEL:
		case ERR_NOTONCHANNEL:
		case ERR_USERONCHANNEL:
		case ERR_NOLOGIN:
		case ERR_SUMMONDISABLED:
		case ERR_USERSDISABLED:
		case ERR_NOTREGISTERED:
		case ERR_NEEDMOREPARAMS:
		case ERR_ALREADYREGISTRED:
		case ERR_NOPERMFORHOST:
		case ERR_PASSWDMISMATCH:
		case ERR_YOUREBANNEDCREEP:
		case ERR_BADNAME:
		case ERR_KEYSET:
		case ERR_CHANNELISFULL:
		case ERR_UNKNOWNMODE:
		case ERR_INVITEONLYCHAN:
		case ERR_BANNEDFROMCHAN:
		case ERR_BADCHANNELKEY:
		case ERR_NOPRIVILEGES:
		case ERR_CHANOPRIVSNEEDED:
		case ERR_CANTKILLSERVER:
		case ERR_NOOPERHOST:
		case ERR_UMODEUNKNOWNFLAG:
		case ERR_USERSDONTMATCH:
		case ERR_GHOSTEDCLIENT:
		case ERR_LAST_ERR_MSG:
		case ERR_SILELISTFULL:
		case ERR_NOSUCHGLINE:
		case ERR_BADPING:
		case ERR_TOOMANYDCC:
		case ERR_LISTSYNTAX:
		case ERR_WHOSYNTAX:
		case ERR_WHOLIMEXCEED:
			Irc_AppendToBuffer(va("%s : %s", msg->params, msg->trailing));
			return qtrue;
		default:
			Irc_AppendToBuffer(msg->trailing);
			return qtrue;
		} /* switch */
		break;
	case IRC_COMMAND_STRING:
		cmd.id.string = msg->id.string;
#ifdef DEBUG
		Com_DPrintf("<%s> [%s] %s : %s", msg->prefix, cmd.id.string, msg->params, msg->trailing);
#endif
		if (!Q_strncmp(cmd.id.string, "NICK", 4))
			Irc_Client_CmdNick(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "QUIT", 4))
			Irc_Client_CmdQuit(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "NOTICE", 6))
			Irc_AppendToBuffer(msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "PRIVMSG", 7))
			Irc_Client_CmdPrivmsg(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "MODE", 4))
			Irc_Client_CmdMode(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "JOIN", 4))
			Irc_Client_CmdJoin(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "PART", 4))
			Irc_Client_CmdPart(msg->prefix, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "TOPIC", 5))
			Irc_Client_CmdTopic(msg->prefix, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "KILL", 4))
			Irc_Client_CmdKill(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "KICK", 4))
			Irc_Client_CmdKick(msg->prefix, msg->params, msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "PING", 4))
			Com_DPrintf("IRC: <PING> %s\n", msg->trailing);
		else if (!Q_strncmp(cmd.id.string, "ERROR", 5))
			Irc_Logic_Disconnect(msg->trailing);
		else
			Irc_AppendToBuffer(msg->trailing);
		break;
	} /* switch */
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
 * @sa Irc_Proto_Enqueue
 * @sa Irc_Proto_DrainBucket
 */
qboolean Irc_Proto_Flush (void)
{
	Irc_Proto_RefillBucket();		/* first refill token */
	return Irc_Proto_DrainBucket();	/* then send messages (if allowed) */
}

/**
 * @brief
 * @sa Irc_Proto_DrainBucket
 * @sa Irc_Proto_RefillBucket
 * @sa Irc_Proto_Flush
 */
static qboolean Irc_Proto_Enqueue (const char *msg, size_t msg_len)
{
	/* create message node */
	const int messageBucketSize = (int)irc_messageBucketSize->value;
	const int characterBucketSize = (int)irc_characterBucketSize->value;
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
 * @sa Irc_Proto_Enqueue
 * @sa Irc_Proto_DrainBucket
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
 * @brief Send all enqueued packets
 * @sa Irc_Proto_Enqueue
 * @sa Irc_Proto_RefillBucket
 */
static qboolean Irc_Proto_DrainBucket (void)
{
	const double characterBucketBurst = irc_characterBucketBurst->value;
	qboolean status = qfalse;
	irc_bucket_message_t *msg;
#if 0
	Com_DPrintf("Irc_Proto_DrainBucket: Queue send\n");
#endif
	/* remove messages whose size exceed our burst size (we can not send them) */
	for (msg = irc_bucket.first_msg; msg && msg->msg_len > characterBucketBurst; msg = irc_bucket.first_msg) {
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
Logic functions
===============================================================
*/

/**
 * @brief
 * @sa Irc_Logic_Frame
 */
static void Irc_Logic_SendMessages (void)
{
	if (Irc_Proto_Flush()) {
		/* flush failed, server closed connection */
		Com_Printf("Irc_Proto_Flush failed\n");
		irc_connected = qfalse;
	}
}

/**
 * @brief
 * @sa Irc_Logic_Frame
 * @sa Irc_Logic_Disconnect
 * @sa Irc_Proto_ProcessServerMsg
 * @sa Irc_Proto_PollServerMsg
 */
static void Irc_Logic_ReadMessages(void)
{
	qboolean msg_complete;
	do {
		irc_server_msg_t msg;
		if (!Irc_Proto_PollServerMsg(&msg, &msg_complete)) {
			/* success */
			if (msg_complete)
				Irc_Proto_ProcessServerMsg(&msg);
		} else
			/* failure */
			Irc_Logic_Disconnect("Server closed connection");
	} while (msg_complete);
}

/**
 * @brief
 * @sa Irc_Logic_ReadMessages
 * @sa Irc_Logic_SendMessages
 */
extern void Irc_Logic_Frame(int frame)
{
	if (irc_connected && frame > 0) {
		Irc_Logic_SendMessages();
		Irc_Logic_ReadMessages();
	}
}

/**
 * @brief
 */
void Irc_Logic_Connect(const char *server, unsigned short port)
{
	if (!Irc_Proto_Connect(server, port)) {
		/* connected to server, send NICK and USER commands */
		const char * const pass = irc_password->string;
		const char * const user = irc_user->string;
		if (strlen(pass))
			Irc_Proto_Password(pass);
		Irc_Proto_Nick(irc_nick->string);
		Irc_Proto_User(user, IRC_INVISIBLE, user);
		irc_connected = !Irc_Proto_Flush();
	}
}

/**
 * @brief
 */
void Irc_Logic_Disconnect(const char *reason)
{
	if (irc_connected) {
		char buf[1024];
		Com_Printf("Irc_Disconnect: %s\n", reason);
		Irc_Proto_Quit(buf);
		Irc_Proto_Disconnect();
		irc_connected = qfalse;
		Cvar_ForceSet("irc_defaultChannel", "");
		Cvar_ForceSet("irc_topic", "");
		Irc_Input_Deactivate();
	} else
		Com_Printf("Irc_Disconnect: not connected\n");
}


/**
 * @brief
 */
const char *Irc_Logic_GetChannelName(const irc_channel_t *channel)
{
	assert(channel);
	return channel->name;
}

/**
 * @brief
 */
const char *Irc_Logic_GetChannelTopic(const irc_channel_t *channel)
{
	assert(channel);
	return channel->topic;
}

/**
 * @brief
 */
irc_channel_t *Irc_Logic_GetChannel(const char *name)
{
	return chan;
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
			Com_Printf("Irc_Net_Connect: IP: %s\n", NET_SocketToString(&addr));
			status = connect(irc_socket, (const struct sockaddr*) &addr, sizeof(addr));
			if (!status) {
				/* connection successful */
				failed = qfalse;
				Com_Printf("Irc_Net_Connect: Connection successful\n");
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
static void Irc_Connect_f (void)
{
	const int argc = Cmd_Argc();
	if (!irc_port || !irc_server)
		return;
	if (argc >= 2 && argc <= 4) {
#if 0
		if (irc_connected)
			Irc_Logic_Disconnect("reconnect");
#endif
		if (!irc_connected) {
			/* not connected yet */
			if (argc >= 2)
				Cvar_Set("irc_server", Cmd_Argv(1));
			if (argc >= 3)
				Cvar_Set("irc_port", Cmd_Argv(2));
			Com_Printf("Connect to %s:%s\n", irc_server->string, irc_port->string);
			Irc_Logic_Connect(irc_server->string, (int)irc_port->value);
			if (argc >= 4)
				Cbuf_AddText(va("irc_join %s\n", Cmd_Argv(3)));
		} else
			Com_Printf("Already connected.\n");
	} else if (*irc_server->string && irc_port->value) {
		if (!irc_connected)
			Cbuf_AddText(va("irc_connect %s %s %s\n", irc_server->string, irc_port->string, irc_channel->string));
		else
			Com_Printf("Already connected.\n");
	} else
			Com_Printf("usage: irc_connect [<server>] [<port>] [<channel>]");
}

/**
 * @brief
 */
static void Irc_Disconnect_f (void)
{
	Irc_Logic_Disconnect("normal exit");
}

/**
 * @brief
 */
static void Irc_Client_Join_f (void)
{
	const int argc = Cmd_Argc();
	if (argc >= 2 && argc <= 3) {
		char * const channel = Cmd_Argv(1);
		char * const channel_pass = argc == 3	/* password is optional */
			? Cmd_Argv(2)
			: NULL;
		if (!Irc_IsChannel(channel)) {
			Com_Printf("No valid channel name\n");
			return;
		}
		Irc_Proto_Join(channel, channel_pass);	/* join desired channel */
		Cvar_ForceSet("irc_defaultChannel", channel);
	} else
		Com_Printf("usage: irc_join <channel> [<password>]\n");
}

/**
 * @brief
 */
static void Irc_Client_Part_f(void)
{
	const int argc = Cmd_Argc();
	if (argc == 2) {
		char * const channel = Cmd_Argv(1);
		Irc_Proto_Part(channel);
	} else
		Com_Printf("usage: irc_part <channel>\n");
}

/**
 * @brief Send a message from menu or commandline
 * @note This function uses the irc_send_buffer cvar to handle the menu input for irc messages
 *       See menu_irc.ufo for more information
 */
static void Irc_Client_Msg_f(void)
{
	char cropped_msg[IRC_SEND_BUF_SIZE];
	const char *msg = NULL;
	char *channel = irc_defaultChannel->string;
	if (Cmd_Argc() >= 2)
		msg = Cmd_Args();
	else if (*irc_send_buffer->string)
		msg = irc_send_buffer->string;

	if (msg && *msg) {
		if (*channel) {
			if (*msg == '"') {
				size_t msg_len = strlen(msg);
				memcpy(cropped_msg, msg + 1, msg_len - 2);
				cropped_msg[msg_len - 2] = '\0';
				msg = cropped_msg;
			}
			Irc_Proto_Msg(channel, msg);
			/* local echo */
			Irc_AppendToBuffer(va("<%s> %s", irc_nick->string, msg));
			if (*irc_send_buffer->string)
				Cvar_ForceSet("irc_send_buffer", "");
		} else
			Com_Printf("Join a channel first.\n");
	} else
		Com_Printf("usage: irc_chanmsg {<msg>}\n");
}

/**
 * @brief
 */
static void Irc_Client_PrivMsg_f(void)
{
	if (Cmd_Argc() >= 3) {
		char cropped_msg[IRC_SEND_BUF_SIZE];
		const char * const target = Cmd_Argv(1);
		const char *msg = Cmd_Args() + strlen(target) + 1;
		if (*msg == '"') {
			size_t msg_len = strlen(msg);
			memcpy(cropped_msg, msg + 1, msg_len - 2);
			cropped_msg[msg_len - 2] = '\0';
			msg = cropped_msg;
		}
		Irc_Proto_Msg(target, msg);
	} else
		Com_Printf("usage: irc_privmsg <target> {<msg>}\n");
}

/**
 * @brief
 */
static void Irc_Client_Mode_f(void)
{
	const int argc = Cmd_Argc();
	if (argc >= 3) {
		const char * const target = Cmd_Argv(1);
		const char * const modes = Cmd_Argv(2);
		const char * const params = argc >= 4
			? Cmd_Args() + strlen(target) + strlen(modes) + 2
			: NULL;
		Irc_Proto_Mode(target, modes, params);
	} else
		Com_Printf("usage: irc_mode <target> <modes> {<param>}\n");
}

/**
 * @brief
 */
static void Irc_Client_Topic_f(void)
{
	const int argc = Cmd_Argc();
	if (argc >= 2) {
		char * const channel = Cmd_Argv(1);
		irc_channel_t *chan = Irc_Logic_GetChannel(channel);
		if (chan) {
			if (argc >= 3) {
				char buf[1024];
				const char *in = Cmd_Args();
				char *out = buf;
				if (*in == '"')
					in += 2;
				in += strlen(channel) + 1;
				Q_strncpyz(out, in, sizeof(out));
				if (*out == '"') {
					size_t out_len;
					++out;
					out_len = strlen(out);
					assert(out_len >= 1);
					out[out_len - 1] = '\0';
				}
				Irc_Proto_Topic(channel, out);
			} else
				Com_Printf("%s topic: \"%s\"\n", channel, Irc_Logic_GetChannelTopic(chan));
		} else
			Com_Printf("Not joined: %s\n", channel);
	} else
		Com_Printf("usage: irc_topic <channel> [<topic>]\n");
}

/**
 * @brief
 */
static void Irc_Client_Names_f(void)
{
	const int argc = Cmd_Argc();
	if (argc == 2) {
		char * const channel = Cmd_Argv(1);
		irc_channel_t *chan = Irc_Logic_GetChannel(channel);
		if (chan) {
			/* TODO */
		} else
			Com_Printf("Not joined: %s\n", channel);
	} else
		Com_Printf("usage: irc_names <channel>\n");
}

/**
 * @brief
 */
static void Irc_Client_Kick_f(void)
{
	const int argc = Cmd_Argc();
	if (argc >= 3) {
		char *channel = Cmd_Argv(1);
		if (Irc_Logic_GetChannel(channel)) {
			const char * const nick = Cmd_Argv(2);
			const char *reason;
			if (argc >= 4)
				reason = Cmd_Args() + strlen(nick) + strlen(channel) + 2;
			else
				reason = NULL;
			Irc_Proto_Kick(channel, nick, reason);
		} else
			Com_Printf("Not joined: %s.", channel);
	} else
		Com_Printf("usage: irc_kick <channel> <nick> [<reason>]\n");
}

/**
 * @brief
 */
static void Irc_Client_Who_f(void)
{
	if (Cmd_Argc() == 2) {
		Irc_Proto_Who(Cmd_Argv(1));
	} else
		Com_Printf("usage: irc_who <usermask>");
}

/**
 * @brief
 */
static void Irc_Client_Whois_f(void)
{
	if (Cmd_Argc() == 2) {
		Irc_Proto_Whois(Cmd_Argv(1));
	} else
		Com_Printf("usage: irc_whois <nick>");
}

/**
 * @brief
 */
static void Irc_Client_Whowas_f(void)
{
	if (Cmd_Argc() == 2) {
		Irc_Proto_Whowas(Cmd_Argv(1));
	} else
		Com_Printf("usage: irc_whowas <nick>");
}


/*
===============================================================
Init and Shutdown functions
===============================================================
*/

/**
 * @brief
 */
extern void Irc_Init(void)
{
	/* commands */
	Cmd_AddCommand("irc_join", Irc_Client_Join_f, "Join an irc channel");
	Cmd_AddCommand("irc_connect", Irc_Connect_f, "Connect to the irc network");
	Cmd_AddCommand("irc_disconnect", Irc_Disconnect_f, "Disconnect from the irc network");
	Cmd_AddCommand("irc_part", Irc_Client_Part_f, NULL);
	Cmd_AddCommand("irc_privmsg", Irc_Client_PrivMsg_f, NULL);
	Cmd_AddCommand("irc_mode", Irc_Client_Mode_f, NULL);
	Cmd_AddCommand("irc_who", Irc_Client_Who_f, NULL);
	Cmd_AddCommand("irc_whois", Irc_Client_Whois_f, NULL);
	Cmd_AddCommand("irc_whowas", Irc_Client_Whowas_f, NULL);
	Cmd_AddCommand("irc_chanmsg", Irc_Client_Msg_f, NULL);
	Cmd_AddCommand("irc_topic", Irc_Client_Topic_f, NULL);
	Cmd_AddCommand("irc_names", Irc_Client_Names_f, NULL);
	Cmd_AddCommand("irc_kick", Irc_Client_Kick_f, NULL);

	Cmd_AddCommand("irc_activate", Irc_Input_Activate, "IRC init when entering the menu");
	Cmd_AddCommand("irc_deactivate", Irc_Input_Deactivate, "IRC deactivate when leaving the irc menu");

	/* cvars */
	irc_server = Cvar_Get("irc_server", "irc.freenode.org", CVAR_ARCHIVE, "IRC server to connect to");
	irc_channel = Cvar_Get("irc_channel", "#ufo:ai", CVAR_ARCHIVE, "IRC channel to join into");
	irc_port = Cvar_Get("irc_port", "6667", CVAR_ARCHIVE, "IRC port to connect to");
	irc_user = Cvar_Get("irc_user", "UfoAIPlayer", CVAR_ARCHIVE, NULL);
	irc_password = Cvar_Get("irc_password", "", CVAR_ARCHIVE, NULL);
	irc_topic = Cvar_Get("irc_topic", "", CVAR_NOSET, NULL);
	irc_defaultChannel = Cvar_Get("irc_defaultChannel", "", CVAR_NOSET, NULL);
	irc_logConsole = Cvar_Get("irc_logConsole", "0", CVAR_ARCHIVE, NULL);
	irc_send_buffer = Cvar_Get("irc_send_buffer", "", 0, NULL);
	irc_nick = Cvar_Get("name", "", 0, NULL);

	/* reset buffer */
	memset(irc_buffer, 0, sizeof(irc_buffer));
}

/**
 * @brief
 */
extern void Irc_Shutdown(void)
{
	if (irc_connected)
		Irc_Logic_Disconnect("shutdown");
}

/**
 * @brief
 * @sa Irc_Input_Deactivate
 */
extern void Irc_Input_Activate(void)
{
	menuText[TEXT_STANDARD] = irc_buffer;
	if (irc_connected && *irc_defaultChannel->string) {
		cls.key_dest = key_irc;
	} else
		Com_Printf("Warning: IRC not connected\n");
}

/**
 * @brief
 * @sa Irc_Input_Activate
 */
extern void Irc_Input_Deactivate(void)
{
	if (cls.key_dest == key_irc)
		cls.key_dest = key_game;
	else
		Com_DPrintf("Note: No in irc input mode - thus we can not leave it\n");
}

/**
 * @brief
 * @sa Irc_Input_Activate
 * @sa Irc_Input_Deactivate
 */
extern void Irc_Input_KeyEvent(int key)
{
	switch (key) {
		case K_ENTER:
		case K_KP_ENTER:
			if (irc_messagemode_buflen > 0) {
				Cbuf_AddText("irc_chanmsg \"");
				Cbuf_AddText(irc_messagemode_buf);
				Cbuf_AddText("\"\n");
				irc_messagemode_buflen = 0;
				irc_messagemode_buf[0] = '\0';
			}
			break;
		case K_BACKSPACE:
			if (irc_messagemode_buflen) {
				--irc_messagemode_buflen;
				irc_messagemode_buf[irc_messagemode_buflen] = '\0';
			}
			break;
		case K_ESCAPE:
			irc_messagemode_buflen = 0;
			irc_messagemode_buf[0] = '\0';
			break;
		case 12:
			irc_messagemode_buflen = 0;
			irc_messagemode_buf[0] = '\0';
			break;
	}
}
