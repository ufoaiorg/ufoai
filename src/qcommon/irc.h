/**
 * @file irc.h
 * @brief IRC client header for UFO:AI
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

#ifndef IRC_NET_H
#define IRC_NET_H

#ifdef _WIN32
#	include <winsock.h>
	typedef SOCKET irc_socket_t;
#else
	typedef int irc_socket_t;
#endif

#define IRC_SEND_BUF_SIZE 512
#define IRC_RECV_BUF_SIZE 1024

#define IRC_DEFAULT_MESSAGE_BUCKET_SIZE		100		/* max 100 messages in bucket */
#define IRC_DEFAULT_MESSAGE_BUCKET_BURST	5		/* max burst size is 5 messages */
#define IRC_DEFAULT_MESSAGE_BUCKET_RATE		0.5		/* per second (5 messages in 10 seconds) */
#define IRC_DEFAULT_CHARACTER_BUCKET_SIZE	2500	/* max 2,500 characters in bucket */
#define IRC_DEFAULT_CHARACTER_BUCKET_BURST	250		/* max burst size is 250 characters */
#define IRC_DEFAULT_CHARACTER_BUCKET_RATE	10		/* per second (100 characters in 10 seconds) */

#define IRC_TRANSMIT_INTERVAL 10

#define IRC_API_VERSION 0

typedef struct irc_channel_s {
	char *name;
	char *topic;
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
	ERR_TOOMANYWATCH = 512,
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

qboolean Irc_Net_Connect(const char *host, unsigned short port);
qboolean Irc_Net_Disconnect(void);

qboolean Irc_Net_Send(const char *msg, size_t msg_len);
qboolean Irc_Net_Receive(char *buf, size_t buf_len, int *recvd);

void Irc_Logic_Connect(const char *server, unsigned short port);
void Irc_Logic_Disconnect(const char *reason);

#endif
