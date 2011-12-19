/**
 * @file cl_irc.c
 * @brief IRC client implementation for UFO:AI
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "cl_irc.h"
#include "client.h"
#include "ui/ui_main.h"
#include "ui/ui_nodes.h"
#include "ui/ui_popup.h"
#include "battlescape/cl_hud.h"
#include "cgame/cl_game.h"

#ifdef _WIN32
#	include <winerror.h>
#else
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <fcntl.h>
#endif

static cvar_t *irc_server;
static cvar_t *irc_port;
static cvar_t *irc_channel;
static cvar_t *irc_nick;
static cvar_t *irc_user;
static cvar_t *irc_password;
static cvar_t *irc_topic;
static cvar_t *irc_defaultChannel;
static cvar_t *irc_logConsole;
static cvar_t *irc_showIfNotInMenu;
/* menu cvar */
static cvar_t *irc_send_buffer;
static memPool_t *cl_ircSysPool;

static qboolean irc_connected;

#define IRC_SEND_BUF_SIZE 512
#define IRC_RECV_BUF_SIZE 1024

typedef struct irc_user_s {
	char key[MAX_VAR];
	struct irc_user_s *next;
} irc_user_t;

typedef struct irc_channel_s {
	char name[MAX_VAR];
	char topic[256];
	int users;
	irc_user_t *user;
} irc_channel_t;

/* numeric commands as specified by RFC 1459 - Internet Relay Chat Protocol */
typedef enum irc_numeric_e {
	/* command replies */
	RPL_WELCOME = 1,				/* ":Welcome to the Internet Relay Network <nick>!<user>@<host>" */
	RPL_YOURHOST = 2,				/* ":Your host is <servername>, running version <ver>" */
	RPL_CREATED = 3,				/* ":This server was created <date>" */
	RPL_MYINFO = 4,					/* "<servername> <version> <available user modes> <available channel modes>" */
	RPL_ISUPPORT = 5,				/* "<nick> <parameter> * :are supported by this server" */
	RPL_HELLO = 20,					/* ":Please wait while we process your connection" */
	RPL_NONE = 300,
	RPL_USERHOST = 302,				/* ":[<reply>{<space><reply>}]" */
	RPL_ISON = 303,					/* ":[<nick> {<space><nick>}]" */
	RPL_AWAY = 301,					/* "<nick> :<away message>" */
	RPL_UNAWAY = 305,				/* ":You are no longer marked as being away" */
	RPL_NOWAWAY = 306,				/* ":You have been marked as being away" */
	RPL_WHOISUSER = 311,			/* "<nick> <user> <host> * :<real name>" */
	RPL_WHOISSERVER = 312,			/* "<nick> <server> :<server info>" */
	RPL_WHOISOPERATOR = 313,		/* "<nick> :is an IRC operator" */
	RPL_WHOISIDLE = 317,			/* "<nick> <integer> :seconds idle" */
	RPL_ENDOFWHOIS = 318,			/* "<nick> :End of /WHOIS list" */
	RPL_WHOISCHANNELS = 319,		/* "<nick> :{[@|+]<channel><space>}" */
	RPL_WHOWASUSER = 314,			/* "<nick> <user> <host> * :<real name>" */
	RPL_ENDOFWHOWAS = 369,			/* "<nick> :End of WHOWAS" */
	RPL_WHOISACCOUNT = 330,			/* "<nick> <account> :is logged in as" */

	RPL_LISTSTART = 321,			/* "Channel :Users  Name" */
	RPL_LIST = 322,					/* "<channel> <# visible> :<topic>" */
	RPL_LISTEND = 323,				/* ":End of /LIST" */
	RPL_CHANNELMODEIS = 324,		/* "<channel> <mode> <mode params>" */
	RPL_NOTOPIC = 331,				/* "<channel> :No topic is set" */
	RPL_TOPIC = 332,				/* "<channel> :<topic>" */
	RPL_TOPICWHOTIME = 333,			/* "<channel> <nick> <time>" */
	RPL_INVITING = 341,				/* "<channel> <nick>" */
	RPL_SUMMONING = 342,			/* "<user> :Summoning user to IRC" */
	RPL_VERSION = 351,				/* "<version>.<debuglevel> <server> :<comments>" */
	RPL_WHOREPLY = 352,				/* "<channel> <user> <host> <server> <nick> <H|G>[*][@|+] :<hopcount> <real name>" */
	RPL_ENDOFWHO = 315,				/* "<name> :End of /WHO list" */
	RPL_NAMREPLY = 353,				/* "<channel> :[[@|+]<nick> [[@|+]<nick> [...]]]" */
	RPL_ENDOFNAMES = 366,			/* "<channel> :End of /NAMES list" */
	RPL_LINKS = 364,				/* "<mask> <server> :<hopcount> <server info>" */
	RPL_ENDOFLINKS = 365,			/* "<mask> :End of /LINKS list" */
	RPL_BANLIST = 367,				/* "<channel> <banid>" */
	RPL_ENDOFBANLIST = 368,			/* "<channel> :End of channel ban list" */
	RPL_INFO = 371,					/* ":<string>" */
	RPL_ENDOFINFO = 374,			/* ":End of /INFO list" */
	RPL_MOTDSTART = 375,			/* ":- <server> Message of the day - " */
	RPL_MOTD = 372,					/* ":- <text>" */
	RPL_ENDOFMOTD = 376,			/* ":End of /MOTD command" */
	RPL_YOUREOPER = 381,			/* ":You are now an IRC operator" */
	RPL_REHASHING = 382,			/* "<config file> :Rehashing" */
	RPL_TIME = 391,					/* "<server> :<string showing server's local time>" */
	RPL_USERSSTART = 392,			/* ":UserID   Terminal  Host" */
	RPL_USERS = 393,				/* ":%-8s %-9s %-8s" */
	RPL_ENDOFUSERS = 394,			/* ":End of users" */
	RPL_NOUSERS = 395,				/* ":Nobody logged in" */
	RPL_TRACELINK = 200,			/* "Link <version & debug level> <destination> <next server>" */
	RPL_TRACECONNECTING = 201,		/* "Try. <class> <server>" */
	RPL_TRACEHANDSHAKE = 202,		/* "H.S. <class> <server>" */
	RPL_TRACEUNKNOWN = 203,			/* "???? <class> [<client IP address in dot form>]" */
	RPL_TRACEOPERATOR = 204,		/* "Oper <class> <nick>" */
	RPL_TRACEUSER = 205,			/* "User <class> <nick>" */
	RPL_TRACESERVER = 206,			/* "Serv <class> <int>S <int>C <server> <nick!user|*!*>@<host|server>" */
	RPL_TRACENEWTYPE = 208,			/* "<newtype> 0 <client name>" */
	RPL_TRACELOG = 261,				/* "File <logfile> <debug level>" */
	RPL_STATSLINKINFO = 211,		/* "<linkname> <sendq> <sent messages> <sent bytes> <received messages> <received bytes> <time open>" */
	RPL_STATSCOMMANDS = 212,		/* "<command> <count>" */
	RPL_STATSCLINE = 213,			/* "C <host> * <name> <port> <class>" */
	RPL_STATSNLINE = 214,			/* "N <host> * <name> <port> <class>" */
	RPL_STATSILINE = 215,			/* "I <host> * <host> <port> <class>" */
	RPL_STATSKLINE = 216,			/* "K <host> * <username> <port> <class>" */
	RPL_STATSYLINE = 218,			/* "Y <class> <ping frequency> <connectfrequency> <max sendq>" */
	RPL_ENDOFSTATS = 219,			/* "<stats letter> :End of /STATS report" */
	RPL_STATSLLINE = 241,			/* "L <hostmask> * <servername> <maxdepth>" */
	RPL_STATSUPTIME = 242,			/* ":Server Up %d days %d:%02d:%02d" */
	RPL_STATSOLINE = 243,			/* "O <hostmask> * <name>" */
	RPL_STATSHLINE = 244,			/* "H <hostmask> * <servername>" */
	RPL_UMODEIS = 221,				/* "<user mode string>" */
	RPL_LUSERCLIENT = 251,			/* ":There are <integer> users and <integer> invisible on <integer> servers" */
	RPL_LUSEROP = 252,				/* "<integer> :operator(s) online" */
	RPL_LUSERUNKNOWN = 253,			/* "<integer> :unknown connection(s)" */
	RPL_LUSERCHANNELS = 254,		/* "<integer> :channels formed" */
	RPL_LUSERME = 255,				/* ":I have <integer> clients and <integer> servers" */
	RPL_ADMINME = 256,				/* "<server> :Administrative info" */
	RPL_ADMINLOC1 = 257,			/* ":<admin info>" */
	RPL_ADMINLOC2 = 258,			/* ":<admin info>" */
	RPL_ADMINEMAIL = 259,			/* ":<admin info>" */
	RPL_LOCALUSERS = 265,
	RPL_GLOBALUSERS = 266,

	/* error codes */
	ERR_NOSUCHNICK = 401,			/* "<nickname> :No such nick/channel" */
	ERR_NOSUCHSERVER = 402,			/* "<server name> :No such server" */
	ERR_NOSUCHCHANNEL = 403,		/* "<channel name> :No such channel" */
	ERR_CANNOTSENDTOCHAN = 404,		/* "<channel name> :Cannot send to channel" */
	ERR_TOOMANYCHANNELS = 405,		/* "<channel name> :You have joined too many channels" */
	ERR_WASNOSUCHNICK = 406,		/* "<nickname> :There was no such nickname" */
	ERR_TOOMANYTARGETS = 407,		/* "<target> :Duplicate recipients. No message delivered" */
	ERR_NOORIGIN = 409,				/* ":No origin specified" */
	ERR_NORECIPIENT = 411,			/* ":No recipient given (<command>)" */
	ERR_NOTEXTTOSEND = 412,			/* ":No text to send" */
	ERR_NOTOPLEVEL = 413,			/* "<mask> :No toplevel domain specified" */
	ERR_WILDTOPLEVEL = 414,			/* "<mask> :Wildcard in toplevel domain" */
	ERR_UNKNOWNCOMMAND = 421,		/* "<command> :Unknown command" */
	ERR_NOMOTD = 422,				/* ":MOTD File is missing" */
	ERR_NOADMININFO = 423,			/* "<server> :No administrative info available" */
	ERR_FILEERROR = 424,			/* ":File error doing <file op> on <file>" */
	ERR_NONICKNAMEGIVEN = 431,		/* ":No nickname given" */
	ERR_ERRONEUSNICKNAME = 432,		/* "<nick> :Erroneus nickname" */
	ERR_NICKNAMEINUSE = 433,		/* "<nick> :Nickname is already in use" */
	ERR_NICKCOLLISION = 436,		/* "<nick> :Nickname collision KILL" */
	ERR_BANNICKCHANGE = 437,
	ERR_NCHANGETOOFAST = 438,
	ERR_USERNOTINCHANNEL = 441,		/* "<nick> <channel> :They aren't on that channel" */
	ERR_NOTONCHANNEL = 442,			/* "<channel> :You're not on that channel" */
	ERR_USERONCHANNEL = 443,		/* "<user> <channel> :is already on channel" */
	ERR_NOLOGIN = 444,				/* "<user> :User not logged in" */
	ERR_SUMMONDISABLED = 445,		/* ":SUMMON has been disabled" */
	ERR_USERSDISABLED = 446,		/* ":USERS has been disabled" */
	ERR_NOTREGISTERED = 451,		/* ":You have not registered" */
	ERR_NEEDMOREPARAMS = 461,		/* "<command> :Not enough parameters" */
	ERR_ALREADYREGISTRED = 462,		/* ":You may not reregister" */
	ERR_NOPERMFORHOST = 463,		/* ":Your host isn't among the privileged" */
	ERR_PASSWDMISMATCH = 464,		/* ":Password incorrect" */
	ERR_YOUREBANNEDCREEP = 465,		/* ":You are banned from this server" */
	ERR_BADNAME = 468,				/* ":Your username is invalid" */
	ERR_KEYSET = 467,				/* "<channel> :Channel key already set" */
	ERR_CHANNELISFULL = 471,		/* "<channel> :Cannot join channel (+l)" */
	ERR_UNKNOWNMODE = 472,			/* "<char> :is unknown mode char to me" */
	ERR_INVITEONLYCHAN = 473,		/* "<channel> :Cannot join channel (+i)" */
	ERR_BANNEDFROMCHAN = 474,		/* "<channel> :Cannot join channel (+b)" */
	ERR_BADCHANNELKEY = 475,		/* "<channel> :Cannot join channel (+k)" */
	ERR_NOPRIVILEGES = 481,			/* ":Permission Denied- You're not an IRC operator" */
	ERR_CHANOPRIVSNEEDED = 482,		/* "<channel> :You're not channel operator" */
	ERR_CANTKILLSERVER = 483,		/* ":You cant kill a server!" */
	ERR_NOOPERHOST = 491,			/* ":No O-lines for your host" */
	ERR_UMODEUNKNOWNFLAG = 501,		/* ":Unknown MODE flag" */
	ERR_USERSDONTMATCH = 502,		/* ":Cant change mode for other users" */
	ERR_GHOSTEDCLIENT = 503,
	ERR_LAST_ERR_MSG = 504,
	ERR_SILELISTFULL = 511,
	ERR_NOSUCHGLINE = 512,
/*	ERR_TOOMANYWATCH = 512,*/
	ERR_BADPING = 513,
	ERR_TOOMANYDCC = 514,
	ERR_LISTSYNTAX = 521,
	ERR_WHOSYNTAX = 522,
	ERR_WHOLIMEXCEED = 523
} irc_numeric_t;

typedef enum irc_command_type_e {
	IRC_COMMAND_NUMERIC,
	IRC_COMMAND_STRING
} irc_command_type_t;

typedef enum irc_nick_prefix_e {
	IRC_NICK_PREFIX_NONE = ' ',
	IRC_NICK_PREFIX_OP = '@',
	IRC_NICK_PREFIX_VOICE = '+'
} irc_nick_prefix_t;

/* commands can be numeric's or string */
typedef struct irc_command_s {
	union {
		/* tagged union */
		irc_numeric_t	numeric;
		const char *	string;
	} id;
	irc_command_type_t	type;
} irc_command_t;

/* server <- client messages */
typedef struct irc_server_msg_s {
	union {
		char string[IRC_SEND_BUF_SIZE];
		irc_numeric_t numeric;
	} id;
	irc_command_type_t type;
	char prefix[IRC_SEND_BUF_SIZE];
	char params[IRC_SEND_BUF_SIZE];
	char trailing[IRC_SEND_BUF_SIZE];
} irc_server_msg_t;

static struct net_stream *irc_stream;

static const char IRC_INVITE_FOR_A_GAME[] = "UFOAIINVITE;";

static irc_channel_t ircChan;
static irc_channel_t *chan;

static char irc_buffer[4096];

static void Irc_Logic_RemoveChannelName(irc_channel_t *channel, const char *nick);
static void Irc_Logic_AddChannelName(irc_channel_t *channel, irc_nick_prefix_t prefix, const char *nick);
static void Irc_Client_Names_f(void);
static qboolean Irc_Client_Join(const char *channel, const char *password);
static void Irc_Logic_Disconnect(const char *reason);

static qboolean Irc_AppendToBuffer(const char* const msg, ...) __attribute__((format(__printf__, 1, 2)));
static qboolean Irc_Proto_ParseServerMsg(const char *txt, size_t txt_len, irc_server_msg_t *msg);
static qboolean Irc_Proto_Enqueue(const char *msg, size_t msg_len);

static qboolean Irc_Net_Connect(const char *host, const char *port);
static qboolean Irc_Net_Disconnect(void);
static void Irc_Net_Send(const char *msg, size_t msg_len);

static void Irc_Connect_f(void);
static void Irc_Disconnect_f(void);
static void Irc_Input_Deactivate_f(void);

/*
===============================================================
Common functions
===============================================================
*/

static inline qboolean Irc_IsChannel (const char *target)
{
	assert(target);
	return (target[0] == '#' || target[0] == '&');
}

static void Irc_ParseName (const char *mask, char *nick, size_t size, irc_nick_prefix_t *prefix)
{
	const char *emph;
	if (mask[0] == IRC_NICK_PREFIX_OP || mask[0] == IRC_NICK_PREFIX_VOICE) {
		*prefix = (irc_nick_prefix_t) *mask;	/* read prefix */
		++mask;									/* crop prefix from mask */
	} else
		*prefix = IRC_NICK_PREFIX_NONE;
	emph = strchr(mask, '!');
	if (emph) {
		size_t length = emph - mask;
		if (length >= size - 1)
			length = size - 1;
		/* complete hostmask, crop anything after ! */
		memcpy(nick, mask, length);
		nick[length] = '\0';
	} else
		/* just the nickname, use as is */
		Q_strncpyz(nick, mask, size);
}

/*
===============================================================
Protocol functions
===============================================================
*/

static cvar_t *irc_messageBucketSize;
static cvar_t *irc_messageBucketBurst;
static cvar_t *irc_characterBucketSize;
static cvar_t *irc_characterBucketBurst;
static cvar_t *irc_characterBucketRate;

typedef struct irc_bucket_message_s {
	char *msg;
	size_t msg_len;
	struct irc_bucket_message_s *next;
} irc_bucket_message_t;

typedef struct irc_bucket_s {
	irc_bucket_message_t *first_msg;	/**< pointer to first message in queue */
	unsigned int message_size;			/**< number of messages in bucket */
	unsigned int character_size;		/**< number of characters in bucket */
	int last_refill;				/**< last refill timestamp */
	double character_token;
} irc_bucket_t;

static irc_bucket_t irc_bucket;

/**
 * @sa Irc_Proto_Disconnect
 */
static qboolean Irc_Proto_Connect (const char *host, const char *port)
{
	const qboolean status = Irc_Net_Connect(host, port);
	if (!status) {
		irc_bucket.first_msg = NULL;
		irc_bucket.message_size = 0;
		irc_bucket.character_size = 0;
		irc_bucket.last_refill = CL_Milliseconds();
		irc_bucket.character_token = (double)irc_characterBucketBurst->value;
	}
	return status;
}

/**
 * @sa Irc_Proto_Connect
 */
static qboolean Irc_Proto_Disconnect (void)
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
 * @sa Irc_Net_Send
 */
static qboolean Irc_Proto_Quit (const char *quitmsg)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "QUIT %s\r\n", quitmsg);
	msg[sizeof(msg) - 1] = '\0';
	Irc_Net_Send(msg, msg_len);	/* send immediately */
	return qfalse;
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Nick (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "NICK %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_User (const char *user, qboolean invisible, const char *name)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "USER %s %c * :%s\r\n", user, invisible ? '8' : '0', name);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Password (const char *password)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PASS %s\r\n", password);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Join (const char *channel, const char *password)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = password
		? snprintf(msg, sizeof(msg) - 1, "JOIN %s %s\r\n", channel, password)
		: snprintf(msg, sizeof(msg) - 1, "JOIN %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';

	/* only one channel allowed */
	if (chan) {
		Com_Printf("Already in a channel\n");
		return qfalse;
	}

	chan = &ircChan;
	OBJZERO(*chan);
	Q_strncpyz(chan->name, channel, sizeof(chan->name));
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Part (const char *channel)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PART %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Mode (const char *target, const char *modes, const char *params)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = params
		? snprintf(msg, sizeof(msg) - 1, "MODE %s %s %s\r\n", target, modes, params)
		: snprintf(msg, sizeof(msg) - 1, "MODE %s %s\r\n", target, modes);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Topic (const char *channel, const char *topic)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = topic
		? snprintf(msg, sizeof(msg) - 1, "TOPIC %s :%s\r\n", channel, topic)
		: snprintf(msg, sizeof(msg) - 1, "TOPIC %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 * @return qtrue on failure
 * @sa Irc_Client_CmdPrivmsg
 */
static qboolean Irc_Proto_Msg (const char *target, const char *text)
{
	if (*text == '/') {
		Com_DPrintf(DEBUG_CLIENT, "Don't send irc commands as PRIVMSG\n");
		Cbuf_AddText(va("%s\n", &text[1]));
		return qtrue;
	} else {
		char msg[IRC_SEND_BUF_SIZE];
		const int msg_len = snprintf(msg, sizeof(msg) - 1, "PRIVMSG %s :%s\r\n", target, text);
		msg[sizeof(msg) - 1] = '\0';
		return Irc_Proto_Enqueue(msg, msg_len);
	}
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Notice (const char *target, const char *text)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "NOTICE %s :%s\r\n", target, text);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Net_Send
 */
static void Irc_Proto_Pong (const char *nick, const char *server, const char *cookie)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = cookie
		? snprintf(msg, sizeof(msg) - 1, "PONG %s %s :%s\r\n", nick, server, cookie)
		: snprintf(msg, sizeof(msg) - 1, "PONG %s %s\r\n", nick, server);
	msg[sizeof(msg) - 1] = '\0';
	Irc_Net_Send(msg, msg_len);	/* send immediately */
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Kick (const char *channel, const char *nick, const char *reason)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = reason
		? snprintf(msg, sizeof(msg) - 1, "KICK %s %s :%s\r\n", channel, nick, reason)
		: snprintf(msg, sizeof(msg) - 1, "KICK %s %s :%s\r\n", channel, nick, nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Who (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHO %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Whois (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHOIS %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Proto_Enqueue
 */
static qboolean Irc_Proto_Whowas (const char *nick)
{
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHOWAS %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

/**
 * @sa Irc_Logic_ReadMessages
 */
static qboolean Irc_Proto_PollServerMsg (irc_server_msg_t *msg, qboolean *msg_complete)
{
	static char buf[IRC_RECV_BUF_SIZE];
	static char *last = buf;
	int recvd;
	*msg_complete = qfalse;
	/* recv packet */
	recvd = NET_StreamDequeue(irc_stream, last, sizeof(buf) - (last - buf) - 1);
	if (recvd >= 0) {
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
	return qtrue;
}

/**
 * @brief Append the irc message to the buffer
 * @note the irc_buffer is linked to mn.menuText array to display in the menu
 * @param[in] msg the complete irc message (without \n)
 * @return true if the message was added to the chatbuffer, too
 */
static qboolean Irc_AppendToBuffer (const char* const msg, ...)
{
	char buf[IRC_RECV_BUF_SIZE];
	va_list ap;
	char appendString[2048];

	va_start(ap, msg);
	Q_vsnprintf(appendString, sizeof(appendString), msg, ap);
	va_end(ap);

	while (strlen(irc_buffer) + strlen(appendString) + 1 >= sizeof(irc_buffer)) {
		const char *n;
		if (!(n = strchr(irc_buffer, '\n'))) {
			irc_buffer[0] = '\0';
			break;
		}
		memmove(irc_buffer, n + 1, strlen(n));
	}

	Com_sprintf(buf, sizeof(buf), "%s\n", appendString);
	Q_strcat(irc_buffer, buf, sizeof(irc_buffer));
	if (irc_logConsole->integer)
		Com_Printf("IRC: %s\n", appendString);

	UI_RegisterText(TEXT_IRCCONTENT, irc_buffer);
	UI_TextScrollEnd("irc.irc_data");

	if (irc_showIfNotInMenu->integer && !Q_streq(UI_GetActiveWindowName(), "irc")) {
		S_StartLocalSample("misc/talk", SND_VOLUME_DEFAULT);
		GAME_AddChatMessage(appendString);
		return qtrue;
	}
	return qfalse;
}

static void Irc_Client_CmdRplWhowasuser (const char *params, const char *trailing)
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
	Irc_AppendToBuffer("^B%s was %s@%s : %s", nick, user, host, real_name);
}

static inline void Irc_Client_CmdTopic (const char *prefix, const char *trailing)
{
	Cvar_ForceSet("irc_topic", trailing);
}

static void Irc_Client_CmdRplTopic (const char *params, const char *trailing)
{
	const char *channel = strchr(params, ' ');
	if (channel) {
		++channel;
		Irc_Client_CmdTopic(params, trailing);
	}
}

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
	Irc_AppendToBuffer("^B%s is %s@%s : %s", nick, user, host, real_name);
}

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
	Irc_AppendToBuffer("^B%s using %s : %s", nick, server, server_info);
}

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
	Irc_AppendToBuffer("^B%s %s %s", nick, trailing, account);
}

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
	Irc_AppendToBuffer("^B%s is %s %s", nick, idle, trailing);
}

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
	Irc_AppendToBuffer("%s %s %s %s %s %s : %s", channel, user, host, server, nick, hg, trailing);
}

static void Irc_Client_CmdMode (const char *prefix, const char *params, const char *trailing)
{
	char nick[MAX_VAR];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	Irc_AppendToBuffer("^B%s sets mode %s", nick, params);
}

static void Irc_Client_CmdJoin (const char *prefix, const char *params, const char *trailing)
{
	char nick[MAX_VAR];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	Irc_AppendToBuffer("^BJoined: %s", nick);
	Irc_Logic_AddChannelName(chan, p, nick);
}

static void Irc_Client_CmdPart (const char *prefix, const char *trailing)
{
	char nick[MAX_VAR];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	Irc_AppendToBuffer("^BLeft: %s (%s)", nick, prefix);
	Irc_Logic_RemoveChannelName(chan, nick);
}

static void Irc_Client_CmdQuit (const char *prefix, const char *params, const char *trailing)
{
	char nick[MAX_VAR];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	Irc_AppendToBuffer("^BQuits: %s (%s)", nick, trailing);
	Irc_Logic_RemoveChannelName(chan, nick);
}

static void Irc_Client_CmdKill (const char *prefix, const char *params, const char *trailing)
{
	char nick[MAX_VAR];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	Irc_AppendToBuffer("^BKilled: %s (%s)", nick, trailing);
	Irc_Logic_RemoveChannelName(chan, nick);
}

static void Irc_Client_CmdKick (const char *prefix, const char *params, const char *trailing)
{
	char buf[IRC_SEND_BUF_SIZE];
	char nick[MAX_VAR];
	irc_nick_prefix_t p;
	const char *channel, *victim;
	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	strcpy(buf, params);
	channel = strtok(buf, " ");
	victim = strtok(NULL, " ");
	if (Q_streq(victim, irc_nick->string)) {
		/* we have been kicked */
		Irc_AppendToBuffer("^BYou were kicked from %s by %s (%s)", channel, nick, trailing);
	} else {
		/* someone else was kicked */
		Irc_AppendToBuffer("^B%s kicked %s (%s)", nick, victim, trailing);
	}
	Irc_Logic_RemoveChannelName(chan, nick);
}

/**
 * @brief Changes the cvar 'name' with the new name you set
 */
static void Irc_Client_CmdNick (const char *prefix, const char *params, const char *trailing)
{
	char nick[MAX_VAR];
	irc_nick_prefix_t p;

	/* not connected */
	if (!chan)
		return;

	Irc_ParseName(prefix, nick, sizeof(nick), &p);
	if (Q_streq(irc_nick->string, nick))
		Cvar_ForceSet("cl_name", trailing);
	Irc_AppendToBuffer("%s is now known as %s", nick, trailing);
	Irc_Logic_RemoveChannelName(chan, nick);
	Irc_Logic_AddChannelName(chan, p, trailing);
}

#define IRC_CTCP_MARKER_CHR '\001'
#define IRC_CTCP_MARKER_STR "\001"

/**
 * @sa Irc_Client_Invite_f
 */
static void Irc_Client_CmdPrivmsg (const char *prefix, const char *params, const char *trailing)
{
	char nick[MAX_VAR];
	char * const emph = strchr(prefix, '!');
	char * ctcp = strchr(trailing, IRC_CTCP_MARKER_CHR);
	OBJZERO(nick);
	if (emph)
		memcpy(nick, prefix, emph - prefix);
	else
		strcpy(nick, prefix);

	if (ctcp) {
		if (Q_streq(trailing + 1, "VERSION" IRC_CTCP_MARKER_STR)) {
			/* response with the game version */
			Irc_Proto_Msg(irc_defaultChannel->string, Cvar_GetString("version"));
			/*Irc_Proto_Notice(nick, IRC_CTCP_MARKER_STR "VERSION " UFO_VERSION " " CPUSTRING " " __DATE__ " " BUILDSTRING);*/
			Com_DPrintf(DEBUG_CLIENT, "Irc_Client_CmdPrivmsg: Response to version query\n");
		} else if (!strncmp(trailing + 1, "PING", 4)) {
			/* Used to measure the delay of the IRC network between clients. */
			char response[IRC_SEND_BUF_SIZE];
			strcpy(response, trailing);
			response[2] = 'O'; /* PING => PONG */
			Irc_Proto_Notice(nick, response);
		} else if (Q_streq(trailing + 1, "TIME" IRC_CTCP_MARKER_STR)) {
			const time_t t = time(NULL);
			char response[IRC_SEND_BUF_SIZE];
			const size_t response_len = sprintf(response, IRC_CTCP_MARKER_STR "TIME :%s" IRC_CTCP_MARKER_STR, ctime(&t));
			response[response_len - 1] = '\0';	/* remove trailing \n */
			Irc_Proto_Notice(nick, response);
		} else {
			Com_Printf("Irc_Client_CmdPrivmsg: Unknown ctcp command: '%s'\n", trailing);
		}
	} else {
		if (!strncmp(trailing, IRC_INVITE_FOR_A_GAME, strlen(IRC_INVITE_FOR_A_GAME))) {
			char serverIPAndPort[128];
			char *port;
			char *version;
			Q_strncpyz(serverIPAndPort, trailing + strlen(IRC_INVITE_FOR_A_GAME), sizeof(serverIPAndPort));
			/* values are splitted by ; */
			port = strstr(serverIPAndPort, ";");
			if (port == NULL) {
				Com_DPrintf(DEBUG_CLIENT, "Invalid irc invite message received\n");
				return;
			}

			/* split ip and port */
			*port++ = '\0';

			/* the version is optional */
			version = strstr(port, ";");
			if (version != NULL) {
				/* split port and version */
				*version++ = '\0';
				if (!Q_streq(version, UFO_VERSION)) {
					Com_DPrintf(DEBUG_CLIENT, "irc invite message from different game version received: %s (versus our version: "UFO_VERSION")\n",
							version);
					return;
				}
			}

			/** get the ip and port into the menu */
			UI_ExecuteConfunc("multiplayer_invite_server_info %s %s", serverIPAndPort, port);

			UI_PushWindow("multiplayer_invite", NULL, NULL);
		} else if (!Irc_AppendToBuffer("<%s> %s", nick, trailing)) {
			/* check whether this is no message to the channel - but to the user */
			if (params && !Q_streq(params, irc_defaultChannel->string)) {
				S_StartLocalSample("misc/lobbyprivmsg", SND_VOLUME_DEFAULT);
				GAME_AddChatMessage(va("<%s> %s\n", nick, trailing));
			} else if (strstr(trailing, irc_nick->string)) {
				S_StartLocalSample("misc/lobbyprivmsg", SND_VOLUME_DEFAULT);
				GAME_AddChatMessage(va("<%s> %s\n", nick, trailing));
			}
		}

		if (UI_GetActiveWindow() && !Q_streq(UI_GetActiveWindowName(), "irc")) {
			Com_Printf(S_COLOR_GREEN "<%s@lobby> %s\n", nick, trailing);
		}
	}
}

static void Irc_Client_CmdRplNamreply (const char *params, const char *trailing)
{
	char *parseBuf, *pos;
	char *space;
	char nick[MAX_VAR];
	size_t len = strlen(trailing) + 1;
	irc_nick_prefix_t p;

	if (!chan)
		return;

	parseBuf = (char *)Mem_PoolAlloc(len * sizeof(char), cl_ircSysPool, 0);
	if (!parseBuf)
		return;

	Q_strncpyz(parseBuf, trailing, len);
	pos = parseBuf;

	do {
		/* names are space separated */
		space = strstr(pos, " ");
		if (space)
			*space = '\0';
		Irc_ParseName(pos, nick, sizeof(nick), &p);
		Irc_Logic_AddChannelName(chan, p, nick);
		if (space)
			pos = space + 1;
	} while (space && *pos);

	Mem_Free(parseBuf);
}

/**
 * @todo Implement me
 */
static void Irc_Client_CmdRplEndofnames (const char *params, const char *trailing)
{
}

/**
 * @sa Irc_Logic_ReadMessages
 */
static qboolean Irc_Proto_ProcessServerMsg (const irc_server_msg_t *msg)
{
	irc_command_t cmd;
	const char *p = NULL;
	cmd.type = msg->type;

	/** @todo Skip non printable chars here */

	switch (cmd.type) {
	case IRC_COMMAND_NUMERIC:
		cmd.id.numeric = msg->id.numeric;
		switch (cmd.id.numeric) {
		case RPL_HELLO:
		case RPL_WELCOME:
		case RPL_YOURHOST:
		case RPL_CREATED:
		case RPL_MYINFO:
		case RPL_MOTDSTART:
		case RPL_MOTD:
		case RPL_LOCALUSERS:
		case RPL_GLOBALUSERS:
		case RPL_ISUPPORT:
		case RPL_LUSEROP:
		case RPL_LUSERUNKNOWN:
		case RPL_LUSERCHANNELS:
		case RPL_LUSERCLIENT:
		case RPL_LUSERME:
			return qtrue;

		/* read our own motd */
		case RPL_ENDOFMOTD:
			{
				byte *fbuf;
				int size;
				size = FS_LoadFile("irc_motd.txt", &fbuf);
				if (size) {
					Irc_AppendToBuffer("%s", (char *)fbuf);
					FS_FreeFile(fbuf);
				}
			}
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
			return qtrue;
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
				Irc_AppendToBuffer("%s %s", p, msg->trailing);
			}
			return qtrue;

		case ERR_NICKNAMEINUSE:
		case ERR_NOSUCHNICK:
		case ERR_NONICKNAMEGIVEN:
		case ERR_ERRONEUSNICKNAME:
		case ERR_NICKCOLLISION:
			Irc_AppendToBuffer("%s : %s", msg->params, msg->trailing);
			UI_PushWindow("irc_changename", NULL, NULL);
			break;
		/* error codes */
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
			Irc_AppendToBuffer("%s : %s", msg->params, msg->trailing);
			return qtrue;
		default:
			Com_DPrintf(DEBUG_CLIENT, "<%s> [%s] %s : %s\n", msg->prefix, cmd.id.string, msg->params, msg->trailing);
			return qtrue;
		} /* switch */
		break;
	case IRC_COMMAND_STRING:
		cmd.id.string = msg->id.string;
#ifdef DEBUG
		Com_DPrintf(DEBUG_CLIENT, "<%s> [%s] %s : %s\n", msg->prefix, cmd.id.string, msg->params, msg->trailing);
#endif
		if (!strncmp(cmd.id.string, "NICK", 4))
			Irc_Client_CmdNick(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "QUIT", 4))
			Irc_Client_CmdQuit(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "NOTICE", 6)) {
			if (irc_logConsole->integer)
				Com_Printf("%s\n", msg->trailing);
		} else if (!strncmp(cmd.id.string, "PRIVMSG", 7))
			Irc_Client_CmdPrivmsg(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "MODE", 4))
			Irc_Client_CmdMode(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "JOIN", 4))
			Irc_Client_CmdJoin(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "PART", 4))
			Irc_Client_CmdPart(msg->prefix, msg->trailing);
		else if (!strncmp(cmd.id.string, "TOPIC", 5))
			Irc_Client_CmdTopic(msg->prefix, msg->trailing);
		else if (!strncmp(cmd.id.string, "KILL", 4))
			Irc_Client_CmdKill(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "KICK", 4))
			Irc_Client_CmdKick(msg->prefix, msg->params, msg->trailing);
		else if (!strncmp(cmd.id.string, "PING", 4))
			Irc_Proto_Pong(irc_nick->string, msg->params, msg->trailing[0] ? msg->trailing : NULL);
		else if (!strncmp(cmd.id.string, "ERROR", 5)) {
			Irc_Logic_Disconnect(msg->trailing);
			Q_strncpyz(popupText, msg->trailing, sizeof(popupText));
			UI_Popup(_("Error"), popupText);
		} else
			Irc_AppendToBuffer("%s", msg->trailing);
		break;
	} /* switch */
	return qfalse;
}

static qboolean Irc_Proto_ParseServerMsg (const char *txt, size_t txt_len, irc_server_msg_t *msg)
{
	const char *c = txt;
	const char *end = txt + txt_len;
	msg->prefix[0] = '\0';
	msg->params[0] = '\0';
	msg->trailing[0] = '\0';
	if (c < end && *c == ':') {
		/* parse prefix */
		char *prefix = msg->prefix;
		int i = 1;
		const size_t size = sizeof(msg->prefix);
		++c;
		while (c < end && *c != '\r' && *c != ' ') {
			if (i++ >= size)
				return qtrue;
			*prefix++ = *c++;
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
				if (c < end && *c >= '0' && *c <= '9')
					command[i] = *c++;
				else
					return qtrue;
			}
			command[3] = '\0';
			msg->type = IRC_COMMAND_NUMERIC;
			msg->id.numeric = atoi(command);
		} else if (c < end && *c != '\r') {
			/* string command */
			int i = 1;
			const size_t size = sizeof(msg->id.string);
			char *command = msg->id.string;
			while (c < end && *c != '\r' && *c != ' ') {
				if (i++ >= size)
					return qtrue;
				*command++ = *c++;
			}
			*command = '\0';
			msg->type = IRC_COMMAND_STRING;
		} else
			return qtrue;
		if (c < end && *c == ' ') {
			/* parse params and trailing */
			char *params = msg->params;
			int i = 1;
			const size_t size = sizeof(msg->params);
			++c;

			/* parse params */
			while (c < end && *c != '\r' && *c != ':') {
				/* parse single param */
				while (c < end && *c != '\r' && *c != ' ') {
					if (i++ >= size)
						return qtrue;
					*params++ = *c++;
				}
				/* more params */
				if (c + 1 < end && *c == ' ' && *(c + 1) != ':') {
					if (i++ >= size)
						return qtrue;
					*params++ = ' ';
				}
				if (*c == ' ')
					++c;
			}
			*params = '\0';
			/* parse trailing */
			if (c < end && *c == ':') {
				char *trailing = msg->trailing;
				int i = 1;
				const size_t size = sizeof(msg->trailing);
				++c;
				while (c < end && *c != '\r') {
					if (i++ >= size)
						return qtrue;
					*trailing++ = *c++;
				}
				*trailing = '\0';
			}
		}
	}
	return qfalse;
}

/**
 * @sa Irc_Proto_DrainBucket
 * @sa Irc_Proto_RefillBucket
 */
static qboolean Irc_Proto_Enqueue (const char *msg, size_t msg_len)
{
	irc_bucket_message_t *m;
	irc_bucket_message_t *n;
	const int messageBucketSize = irc_messageBucketSize->integer;
	const int characterBucketSize = irc_characterBucketSize->integer;

	if (!irc_connected) {
		Com_Printf("Irc_Proto_Enqueue: not connected\n");
		return qtrue;
	}

	/* create message node */
	m = Mem_AllocType(irc_bucket_message_t);
	n = irc_bucket.first_msg;
	if (irc_bucket.message_size + 1 <= messageBucketSize && irc_bucket.character_size + msg_len <= characterBucketSize) {
		/** @todo strip high bits - or unprintable chars */
		m->msg = (char*) Mem_Alloc(msg_len);
		memcpy(m->msg, msg, msg_len);
		m->msg_len = msg_len;
		m->next = NULL;
		/* append message node */
		if (n) {
			while (n->next)
				n = n->next;
			n->next = m;
		} else {
			irc_bucket.first_msg = m;
		}
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
 * @sa Irc_Proto_Enqueue
 * @sa Irc_Proto_DrainBucket
 */
static void Irc_Proto_RefillBucket (void)
{
	/* calculate token refill */
	const double characterBucketSize = irc_characterBucketSize->value;
	const double characterBucketRate = irc_characterBucketRate->value;
	const int ms = CL_Milliseconds();
	const int ms_delta = ms - irc_bucket.last_refill;
	const double char_delta = (ms_delta * characterBucketRate) / 1000;
	const double char_new = irc_bucket.character_token + char_delta;
	/* refill token (but do not exceed maximum) */
	irc_bucket.character_token = min(char_new, characterBucketSize);
	/* set timestamp so next refill can calculate delta */
	irc_bucket.last_refill = ms;
}

/**
 * @brief Send all enqueued packets
 * @sa Irc_Proto_Enqueue
 * @sa Irc_Proto_RefillBucket
 */
static void Irc_Proto_DrainBucket (void)
{
	const double characterBucketBurst = irc_characterBucketBurst->value;
	irc_bucket_message_t *msg;

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
	for (msg = irc_bucket.first_msg; msg; msg = irc_bucket.first_msg) {
		/* send message */
		Irc_Net_Send(msg->msg, msg->msg_len);
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
}

/*
===============================================================
Logic functions
===============================================================
*/

/**
 * @sa Irc_Logic_Frame
 */
static void Irc_Logic_SendMessages (void)
{
	Irc_Proto_RefillBucket();		/* first refill token */
	Irc_Proto_DrainBucket();	/* then send messages (if allowed) */
}

/**
 * @sa Irc_Logic_Frame
 * @sa Irc_Logic_Disconnect
 * @sa Irc_Proto_ProcessServerMsg
 * @sa Irc_Proto_PollServerMsg
 */
static void Irc_Logic_ReadMessages (void)
{
	qboolean msg_complete;
	do {
		irc_server_msg_t msg;
		if (!Irc_Proto_PollServerMsg(&msg, &msg_complete)) {
			/* success */
			if (msg_complete)
				Irc_Proto_ProcessServerMsg(&msg);
		} else {
			/* failure */
			UI_Popup(_("Error"), _("Server closed connection"));
			Irc_Logic_Disconnect("Server closed connection");
		}
	} while (msg_complete);
}

static void Irc_Logic_Connect (const char *server, const char *port)
{
	if (!Irc_Proto_Connect(server, port)) {
		/* connected to server, send NICK and USER commands */
		const char * const pass = irc_password->string;
		const char * const user = irc_user->string;
		irc_connected = qtrue;
		if (pass[0] != '\0')
			Irc_Proto_Password(pass);
		Irc_Proto_Nick(irc_nick->string);
		Irc_Proto_User(user, qtrue, user);
	} else {
		Com_Printf("Could not connect to the irc server %s:%s\n", server, port);
	}
}

static void Irc_Logic_Disconnect (const char *reason)
{
	if (irc_connected) {
		Com_Printf("Irc_Disconnect: %s\n", reason);
		Irc_Proto_Quit(reason);
		Irc_Proto_Disconnect();
		irc_connected = qfalse;
		chan = NULL;
		Cvar_ForceSet("irc_defaultChannel", "");
		Cvar_ForceSet("irc_topic", "Connecting (please wait)...");
		UI_ResetData(TEXT_IRCUSERS);
		Irc_Input_Deactivate_f();
	} else
		Com_Printf("Irc_Disconnect: not connected\n");
}

/**
 * @sa Irc_Logic_ReadMessages
 * @sa Irc_Logic_SendMessages
 */
void Irc_Logic_Frame (void)
{
	if (irc_connected) {
		if (irc_channel->modified) {
			/** @todo do this without disconnect, connect */
			Irc_Logic_Disconnect("Switched to another channel");
			Irc_Logic_Connect(irc_server->string, irc_port->string);
			if (irc_connected)
				Irc_Client_Join(irc_channel->string, NULL);
		}
		/* could have changed in the meantime */
		if (irc_connected) {
			Irc_Logic_SendMessages();
			Irc_Logic_ReadMessages();
		}
	}
	irc_channel->modified = qfalse;
}

static const char *Irc_Logic_GetChannelTopic (const irc_channel_t *channel)
{
	assert(channel);
	return channel->topic;
}

/**
 * @brief Adds a new username to the channel username list
 * @sa Irc_Logic_RemoveChannelName
 */
static void Irc_Logic_AddChannelName (irc_channel_t *channel, irc_nick_prefix_t prefix, const char *nick)
{
	int i;
	/* first one */
	irc_user_t* user = channel->user;
	for (i = 0; user && i < channel->users; i++, user = user->next) {
		if (!strncmp(&(user->key[1]), nick, MAX_VAR - 1))
			return;
	}
	user = (irc_user_t *)Mem_PoolAlloc(sizeof(*user), cl_ircSysPool, 0);
	user->next = channel->user;
	channel->user = user;

	Com_sprintf(user->key, sizeof(user->key), "%c%s", prefix, nick);
	channel->users++;
	Com_DPrintf(DEBUG_CLIENT, "Add user '%s' to userlist at pos %i\n", user->key, channel->users);
	Irc_Client_Names_f();
}

/**
 * @brief Removes a username from the channel username list
 * @sa Irc_Logic_AddChannelName
 */
static void Irc_Logic_RemoveChannelName (irc_channel_t *channel, const char *nick)
{
	int i;
	/* first one */
	irc_user_t* user = channel->user;
	irc_user_t* predecessor = NULL;
	for (i = 0; user && i < channel->users; i++, user = user->next) {
		if (!strncmp(&(user->key[1]), nick, sizeof(user->key))) {
			/* delete the first user from the list */
			if (!predecessor)
				channel->user = user->next;
			/* point to the descendant */
			else
				predecessor->next = user->next;
			Mem_Free(user);
			chan->users--;
			Irc_Client_Names_f();
			return;
		}
		predecessor = user;
	}
}

/*
===============================================================
Network functions
===============================================================
*/

static void Irc_Net_StreamClose (void)
{
	irc_stream = NULL;
}

/**
 * @return qtrue if successful - qfalse otherwise
 * @sa Irc_Net_Disconnect
 */
static qboolean Irc_Net_Connect (const char *host, const char *port)
{
	if (irc_stream)
		NET_StreamFree(irc_stream);
	irc_stream = NET_Connect(host, port, Irc_Net_StreamClose);
	return irc_stream ? qfalse : qtrue;
}

/**
 * @sa Irc_Net_Connect
 */
static qboolean Irc_Net_Disconnect (void)
{
	NET_StreamFree(irc_stream);
	return qtrue;
}

static void Irc_Net_Send (const char *msg, size_t msg_len)
{
	assert(msg);
	NET_StreamEnqueue(irc_stream, msg, msg_len);
}

/*
===============================================================
Bindings
===============================================================
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
			Com_Printf("Connecting to %s:%s\n", irc_server->string, irc_port->string);
			Irc_Logic_Connect(irc_server->string, irc_port->string);
			if (irc_connected && argc >= 4)
				Irc_Client_Join(Cmd_Argv(3), NULL);
		} else
			Com_Printf("Already connected.\n");

	} else if (irc_server->string[0] != '\0' && irc_port->integer) {
		if (!irc_connected)
			Cbuf_AddText(va("irc_connect %s %i %s\n", irc_server->string, irc_port->integer, irc_channel->string));
		else
			Com_Printf("Already connected.\n");
	} else
		Com_Printf("Usage: %s [<server>] [<port>] [<channel>]\n", Cmd_Argv(0));
}

static void Irc_Disconnect_f (void)
{
	Irc_Logic_Disconnect("normal exit");
}

static qboolean Irc_Client_Join (const char *channel, const char *password)
{
	if (!Irc_IsChannel(channel)) {
		Com_Printf("No valid channel name\n");
		return qfalse;
	}
	/* join desired channel */
	if (!Irc_Proto_Join(channel, password)) {
		Cvar_ForceSet("irc_defaultChannel", channel);
		Com_Printf("Joined channel: '%s'\n", channel);
		return qtrue;
	} else {
		Com_Printf("Could not join channel: '%s'\n", channel);
		return qfalse;
	}
}

static void Irc_Client_Join_f (void)
{
	const int argc = Cmd_Argc();
	if (argc >= 2 && argc <= 3) {
		const char * const channel = Cmd_Argv(1);
		/* password is optional */
		const char * const channel_pass = (argc == 3) ? Cmd_Argv(2) : NULL;
		Irc_Client_Join(channel, channel_pass);
	} else
		Com_Printf("Usage: %s <channel> [<password>]\n", Cmd_Argv(0));
}

static void Irc_Client_Part_f (void)
{
	const int argc = Cmd_Argc();
	if (argc == 2) {
		const char * const channel = Cmd_Argv(1);
		Irc_Proto_Part(channel);
	} else
		Com_Printf("Usage: %s <channel>\n", Cmd_Argv(0));
}

/**
 * @brief Send a message from menu or commandline
 * @note This function uses the irc_send_buffer cvar to handle the menu input for irc messages
 * See menu_irc.ufo for more information
 */
static void Irc_Client_Msg_f (void)
{
	char cropped_msg[IRC_SEND_BUF_SIZE];
	const char *msg;

	if (Cmd_Argc() >= 2)
		msg = Cmd_Args();
	else if (irc_send_buffer->string[0] != '\0')
		msg = irc_send_buffer->string;
	else
		return;

	if (msg[0] != '\0') {
		if (irc_defaultChannel->string[0] != '\0') {
			if (msg[0] == '"') {
				const size_t msg_len = strlen(msg);
				memcpy(cropped_msg, msg + 1, msg_len - 2);
				cropped_msg[msg_len - 2] = '\0';
				msg = cropped_msg;
			}
			if (!Irc_Proto_Msg(irc_defaultChannel->string, msg)) {
				/* local echo */
				Irc_AppendToBuffer("<%s> %s", irc_nick->string, msg);
			}
			Cvar_ForceSet("irc_send_buffer", "");
		} else
			Com_Printf("Join a channel first.\n");
	}
}

static void Irc_Client_PrivMsg_f (void)
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
		Com_Printf("Usage: %s <target> {<msg>}\n", Cmd_Argv(0));
}

static void Irc_Client_Mode_f (void)
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
		Com_Printf("Usage: %s <target> <modes> {<param>}\n", Cmd_Argv(0));
}

static void Irc_Client_Topic_f (void)
{
	const int argc = Cmd_Argc();
	if (argc >= 2) {
		const char * const channel = Cmd_Argv(1);
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
		Com_Printf("Usage: %s <channel> [<topic>]\n", Cmd_Argv(0));
}

#define IRC_MAX_USERLIST 512
static char irc_userListOrdered[IRC_MAX_USERLIST][MAX_VAR];

static void Irc_Client_Names_f (void)
{
	irc_user_t* user;
	if (chan) {
		linkedList_t *irc_names_buffer = NULL;
		int i;
		user = chan->user;
		for (i = 0; i < chan->users; i++) {
			if (i >= IRC_MAX_USERLIST)
				break;
			Q_strncpyz(irc_userListOrdered[i], user->key, MAX_VAR);
			user = user->next;
		}
		if (i > 0) {
			qsort((void *)irc_userListOrdered, i, MAX_VAR, Q_StringSort);
			while (i--)
				LIST_AddString(&irc_names_buffer, irc_userListOrdered[i]);
		}
		UI_RegisterLinkedListText(TEXT_IRCUSERS, irc_names_buffer);
	} else
		Com_Printf("Not joined\n");
}

static void Irc_Client_Kick_f (void)
{
	const int argc = Cmd_Argc();
	if (argc >= 3) {
		const char *channel = Cmd_Argv(1);
		if (chan) {
			const char * const nick = Cmd_Argv(2);
			const char *reason;
			if (argc >= 4)
				reason = Cmd_Args() + strlen(nick) + strlen(channel) + 2;
			else
				reason = NULL;
			Irc_Proto_Kick(channel, nick, reason);
		} else
			Com_Printf("Not joined: %s.\n", channel);
	} else
		Com_Printf("Usage: %s <channel> <nick> [<reason>]\n", Cmd_Argv(0));
}

static void Irc_GetExternalIP (const char *externalIP)
{
	const irc_user_t *user;
	char buf[128];

	if (!externalIP) {
		Com_Printf("Could not query masterserver\n");
		return;
	}
	Com_sprintf(buf, sizeof(buf), "%s%s;%s;%s", IRC_INVITE_FOR_A_GAME, externalIP, port->string, UFO_VERSION);

	user = chan->user;
	while (user) {
		/** @todo Maybe somehow check the version of the client with ctcp VERSION and only send
		 * to those, that are connected with ufoai and have a correct version */
		/* the first character is a prefix, skip it when checking
		 * against our own nickname */
		const char *name = &(user->key[1]);
		if (!Q_streq(name, irc_nick->string))
			Irc_Proto_Msg(name, buf);
		user = user->next;
	}
}

/**
 * @sa Irc_Client_CmdPrivmsg
 */
static void Irc_Client_Invite_f (void)
{
	if (!CL_OnBattlescape()) {
		Com_Printf("You must be connected to a running server to invite others\n");
		return;
	}

	if (!chan) {
		UI_PushWindow("irc_popup", NULL, NULL);
		return;
	}

	HTTP_GetURL(va("%s/ufo/masterserver.php?ip", masterserver_url->string), Irc_GetExternalIP);
}

static void Irc_Client_Who_f (void)
{
	if (Cmd_Argc() == 2) {
		Irc_Proto_Who(Cmd_Argv(1));
	} else
		Com_Printf("Usage: %s <usermask>\n", Cmd_Argv(0));
}

static void Irc_Client_Whois_f (void)
{
	if (Cmd_Argc() == 2) {
		Irc_Proto_Whois(Cmd_Argv(1));
	} else
		Com_Printf("Usage: %s <nick>\n", Cmd_Argv(0));
}

static void Irc_Client_Whowas_f (void)
{
	if (Cmd_Argc() == 2) {
		Irc_Proto_Whowas(Cmd_Argv(1));
	} else
		Com_Printf("Usage: %s <nick>\n", Cmd_Argv(0));
}

/*
===============================================================
Menu functions
===============================================================
*/

/**
 * @brief Adds the username you clicked to your input buffer
 * @sa Irc_UserRightClick_f
 */
static void Irc_UserClick_f (void)
{
	const char *name;
	int num, cnt;

	if (Cmd_Argc() != 2)
		return;

	if (!chan)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= chan->users || num >= IRC_MAX_USERLIST)
		return;

	cnt = min(chan->users, IRC_MAX_USERLIST);
	cnt -= num + 1;

	name = irc_userListOrdered[cnt];
	Cvar_Set("irc_send_buffer", va("%s%s: ", irc_send_buffer->string, &name[1]));
}

/**
 * @brief Performs a whois query for the username you clicked
 * @sa Irc_UserClick_f
 */
static void Irc_UserRightClick_f (void)
{
	const char *name;
	int num, cnt;

	if (Cmd_Argc() != 2)
		return;

	if (!chan)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= chan->users || num >= IRC_MAX_USERLIST)
		return;

	cnt = min(chan->users, IRC_MAX_USERLIST);
	cnt -= num + 1;

	name = irc_userListOrdered[cnt];
	Irc_Proto_Whois(&name[1]);
}

/**
 * @sa Irc_Input_Deactivate
 */
static void Irc_Input_Activate_f (void)
{
	/* in case of a failure we need this in UI_PopWindow */
	if (irc_connected && irc_defaultChannel->string[0] != '\0') {
		UI_RegisterText(TEXT_IRCCONTENT, irc_buffer);
	} else {
		Com_DPrintf(DEBUG_CLIENT, "Irc_Input_Activate: Warning - IRC not connected\n");
		UI_PopWindow(qfalse);
		UI_PushWindow("irc_popup", NULL, NULL);
		/* cancel any active editing */
		return;
	}
}

/**
 * @sa Irc_Input_Activate
 */
static void Irc_Input_Deactivate_f (void)
{
	irc_send_buffer->modified = qfalse;

	UI_ResetData(TEXT_IRCCONTENT);
}

/*
===============================================================
Init and Shutdown functions
===============================================================
*/

void Irc_Init (void)
{
	cl_ircSysPool = Mem_CreatePool("Client: IRC system");

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
	Cmd_AddCommand("irc_invite", Irc_Client_Invite_f, "Invite other players for a game");

	Cmd_AddCommand("irc_userlist_click", Irc_UserClick_f, "Menu function for clicking a user from the list");
	Cmd_AddCommand("irc_userlist_rclick", Irc_UserRightClick_f, "Menu function for clicking a user from the list");

	Cmd_AddCommand("irc_activate", Irc_Input_Activate_f, "IRC init when entering the menu");
	Cmd_AddCommand("irc_deactivate", Irc_Input_Deactivate_f, "IRC deactivate when leaving the irc menu");

	/* cvars */
	irc_server = Cvar_Get("irc_server", "irc.freenode.org", CVAR_ARCHIVE, "IRC server to connect to");
	irc_channel = Cvar_Get("irc_channel", "#ufoai-gamer", CVAR_ARCHIVE, "IRC channel to join into");
	irc_channel->modified = qfalse;
	irc_port = Cvar_Get("irc_port", "6667", CVAR_ARCHIVE, "IRC port to connect to");
	irc_user = Cvar_Get("irc_user", "UFOAIPlayer", CVAR_ARCHIVE, NULL);
	irc_password = Cvar_Get("irc_password", "", CVAR_ARCHIVE, NULL);
	irc_topic = Cvar_Get("irc_topic", "Connecting (please wait)...", CVAR_NOSET, NULL);
	irc_defaultChannel = Cvar_Get("irc_defaultChannel", "", CVAR_NOSET, NULL);
	irc_logConsole = Cvar_Get("irc_logConsole", "0", CVAR_ARCHIVE, "Log all irc conversations to game console, too");
	irc_showIfNotInMenu = Cvar_Get("irc_showIfNotInMenu", "0", CVAR_ARCHIVE, "Show chat messages on top of the menu stack if we are not in the irc menu");
	irc_send_buffer = Cvar_Get("irc_send_buffer", "", 0, NULL);
	irc_nick = Cvar_Get("cl_name", "", 0, NULL);

	irc_messageBucketSize = Cvar_Get("irc_messageBucketSize", "100", CVAR_ARCHIVE, "max 100 messages in bucket");
	irc_messageBucketBurst = Cvar_Get("irc_messageBucketBurst", "5", CVAR_ARCHIVE, "max burst size is 5 messages");
	irc_characterBucketSize = Cvar_Get("irc_characterBucketSize", "2500", CVAR_ARCHIVE, "max 2,500 characters in bucket");
	irc_characterBucketBurst = Cvar_Get("irc_characterBucketBurst", "250", CVAR_ARCHIVE, "max burst size is 250 characters");
	irc_characterBucketRate = Cvar_Get("irc_characterBucketRate", "10", CVAR_ARCHIVE, "per second (100 characters in 10 seconds)");
}

void Irc_Shutdown (void)
{
	if (irc_connected)
		Irc_Logic_Disconnect("shutdown");

	Mem_DeletePool(cl_ircSysPool);
}
