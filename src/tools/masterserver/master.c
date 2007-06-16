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
 * ufoai master server - based on gloommaster 0.0.3
 */

#define MASTER_SERVER "195.136.48.62" /* sponsored by NineX */
#define VERSION "0.0.5"

#define MS_PORT 27900

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <malloc.h>
# include <winsock.h>
# ifndef socklen_t
#  define socklen_t size_t
# endif
# define MS_CloseSocket closesocket
#else
# include <signal.h>
# include <errno.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <unistd.h>
# define SOCKET int
# define SOCKET_ERROR -1
# define TIMEVAL struct timeval
# define WSAGetLastError() errno
# define strnicmp strncasecmp
# define COMMANDLINE
# define MS_CloseSocket close
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

static server_t servers;

typedef struct msConfig_s {
	unsigned short port;
	const char* listenName;
	struct in_addr listenAddr;
	unsigned char daemonized;
} msConfig_t;

static msConfig_t config;

static struct sockaddr_in listenaddress;
static SOCKET listener;
static TIMEVAL delay;
#ifdef _WIN32
static WSADATA ws;
#define MS_vsnprintf _vsnprintf
#else
#define MS_vsnprintf vsnprintf
#endif
static fd_set set;
static char incoming[150000];
static int retval;
static FILE *logfile;
static int logileActive;

/**
 * @brief Output function
 */
static void MS_Printf (const char *msg, ...)
{
	va_list argptr;
	char text[1024];

	if (config.daemonized)
		return; /* don't print anything when master server is daemonized */

	va_start(argptr, msg);
	MS_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	/* ensure termination */
	text[sizeof(text) - 1] = '\0';
	if (logileActive) {
		logfile = fopen("masterserver.log", "wa");
		fprintf(logfile, "%s", text);
		fclose(logfile);
	}
	printf("%s", text);
}

#ifdef COMMANDLINE
/**
 * @brief Show all servers if the user typed 'list' to the command prompt
 */
static void MS_ListServers_f (void)
{
	server_t *server = &servers;
	int i = 0;

	while (server->next) {
		server = server->next;
		i++;

		if (server->validated)
			printf("V");
		else
			printf(" ");

		if (server->shutdown_issued)
			printf("S");
		else
			printf(" ");

		printf("%15s\n", inet_ntoa(server->ip.sin_addr));
	}

	printf("%i server(s) on the list\n", i);
}

/**
 * @brief Console input - currently only for linux
 */
static char* MS_RunInput (void)
{
	static char text[256];
	int len;
	fd_set inputset;
	struct timeval timeout;

	FD_ZERO(&inputset);
	FD_SET(0, &inputset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select(1, &inputset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &inputset))
		return NULL;

	len = read(0, text, sizeof(text));
	if (len == 0) { /* eof! */
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len-1] = 0;    /* rip off the /n and terminate */

	return text;
}
#endif /* COMMANDLINE */

#if defined COMMANDLINE || !defined _WIN32
/**
 * @brief Shutdown function
 * @sa signal_handler
 */
static void MS_ExitNicely (void)
{
	server_t *server = &servers;
	server_t *old = NULL;

	printf("[I] shutting down.\n");

	while (server->next) {
		if (old)
			free(old);
		server = server->next;
		old = server;
	}

	if (old)
		free(old);
}
#endif /* COMMANDLINE || ! _WIN32 */

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
	free(server);
}

/**
 * @brief
 * @param[in] normal if 1 it is a listenserver - otherwise a dedicated sends a heartbeat
 */
static void MS_AddServer (struct sockaddr_in *from, int normal)
{
	server_t *server = &servers;
	int preserved_heartbeats = 0;

	while (server->next) {
		server = server->next;
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
			/* already exists - could be a pending shutdown (ie killserver, change of map, etc) */
			if (server->shutdown_issued) {
				MS_Printf("[I] scheduled shutdown server %s sent another ping!\n", inet_ntoa(from->sin_addr));
				MS_DropServer(server);
				server = &servers;
				while (server->next)
					server = server->next;
				break;
			} else {
				MS_Printf("[W] dupe ping from %s!! ignored.\n", inet_ntoa(server->ip.sin_addr));
				return;
			}
		}
	}

	server->next = (server_t *)malloc(sizeof(server_t));

	server->next->prev = server;
	server = server->next;

	server->heartbeats = preserved_heartbeats;
	memcpy(&server->ip, from, sizeof(server->ip));
	server->last_heartbeat = time(0);
	server->next = NULL;
	server->port = from->sin_port;
	server->shutdown_issued = server->queued_pings = server->last_ping = server->validated = 0;

	MS_Printf("[I] server %s added to queue! (%d)\n", inet_ntoa(from->sin_addr), normal);
	/* write out to log file, I want to track this */
	if (normal) {
		struct sockaddr_in addr;
		memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
		sendto(listener, "\xFF\xFF\xFF\xFF""ack", 7, 0, (struct sockaddr*)&addr, sizeof(addr));
	}
}

/**
 * @brief
 */
static void MS_QueueShutdown (struct sockaddr_in *from, server_t *myserver)
{
	server_t *server = &servers;

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
		memcpy(&addr.sin_addr, &myserver->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

		/* hack, server will drop in next minute IF it doesn't respond to our ping */
		myserver->shutdown_issued = 1;

		MS_Printf("[I] %s shutdown queued\n", inet_ntoa(myserver->ip.sin_addr));
		sendto(listener, "\xFF\xFF\xFF\xFF""ping", 8, 0, (struct sockaddr *)&addr, sizeof(addr));
		return;
	} else {
		MS_Printf("[W] shutdown issued from unregistered server %s!\n", inet_ntoa(from->sin_addr));
	}
}

/**
 * @brief
 */
static void MS_RunFrame (void)
{
	server_t *server = &servers;
	unsigned int curtime = time(0);

	while (server->next) {
		server = server->next;
		if (curtime - server->last_heartbeat > 30) {
			server_t *old = server;

			server = old->prev;

			if (old->shutdown_issued || old->queued_pings > 6) {
				MS_Printf("[I] %s shut down.\n", inet_ntoa(old->ip.sin_addr));
				MS_DropServer(old);
				continue;
			}

			server = old;
			if (curtime - server->last_ping >= 10) {
				struct sockaddr_in addr;
				memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
				addr.sin_family = AF_INET;
				addr.sin_port = server->port;
				memset (&addr.sin_zero, 0, sizeof(addr.sin_zero));
				server->queued_pings++;
				server->last_ping = curtime;
				MS_Printf("[I] ping %s\n", inet_ntoa(server->ip.sin_addr));
				sendto(listener, "\xFF\xFF\xFF\xFF""ping", 8, 0, (struct sockaddr *)&addr, sizeof(addr));
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
	int buflen;
	char buff[0xFFFF];
	server_t *server = &servers;

	buflen = 0;
	memset(buff, 0, sizeof(buff));

	memcpy(buff, "\xFF\xFF\xFF\xFF""servers ", 12);
	buflen += 12;

	while (server->next) {
		server = server->next;
		/* at least one heartbeat */
		if (server->validated) {
			memcpy(buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;
			/*port = ntohs(server->port); */
			memcpy(buff + buflen, &server->port, 2);
			buflen += 2;
		}
	}

	MS_Printf("[I] query response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa(from->sin_addr), ntohs(from->sin_port));

	if ((sendto(listener, buff, buflen, 0, (struct sockaddr *)from, sizeof(*from))) == SOCKET_ERROR) {
		MS_Printf("[E] socket error on send! code %d.\n", WSAGetLastError());
	}

	MS_Printf("[I] sent server list to client %s\n", inet_ntoa(from->sin_addr));
}

/**
 * @brief
 */
static void MS_Ack (struct sockaddr_in *from)
{
	server_t *server = &servers;

	/* iterate through known servers */
	while (server->next) {
		server = server->next;
		/* a match! */
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
			printf("[I] ack from %s (%d).\n", inet_ntoa(server->ip.sin_addr), server->queued_pings);
			server->last_heartbeat = time(0);
			server->queued_pings = 0;
			server->heartbeats++;
			server->validated = 1;
			return;
		}
	}
}

/**
 * @brief
 */
static void MS_HeartBeat (struct sockaddr_in *from, char *data)
{
	server_t *server = &servers;

	/* iterate through known servers */
	while (server->next) {
		server = server->next;
		/* a match! */
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port) {
			struct sockaddr_in addr;

			memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
			addr.sin_family = AF_INET;
			addr.sin_port = server->port;
			memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

			server->validated = 1;
			server->last_heartbeat = time(0);
			MS_Printf("[I] heartbeat from %s.\n", inet_ntoa (server->ip.sin_addr));
			sendto(listener, "\xFF\xFF\xFF\xFF""ack", 7, 0, (struct sockaddr*)&addr, sizeof(addr));
			return;
		}
	}

	MS_AddServer(from, 0);
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

	/* MS_Printf ("[I] %s:%d : query (%d bytes) '%s'\n", inet_ntoa(from->sin_addr), htons(from->sin_port), dglen, data); */

	if (strnicmp(cmd, "ping", 4) == 0) {
		MS_AddServer(from, 1);
	} else if (strnicmp(cmd, "heartbeat", 9) == 0 || strnicmp(cmd, "print", 5) == 0) {
		MS_HeartBeat(from, line);
	} else if (strncmp(cmd, "ack", 3) == 0) {
		MS_Ack(from);
	} else if (strnicmp(cmd, "shutdown", 8) == 0) {
		MS_QueueShutdown(from, NULL);
	} else if (strnicmp(cmd, "getservers", 10) == 0) {
		MS_SendServerListToClient(from);
	} else {
		printf("[W] Unknown command from %s!\n... cmd: '%s'", inet_ntoa(from->sin_addr), cmd);
	}
}

#ifndef _WIN32
/**
 * @brief Signal handler function that calls the shutdown function
 * @sa MS_ExitNicely
 */
static void signal_handler (int sig)
{
	printf("[W] Received signal %d, exiting...\n", sig);
	MS_ExitNicely();
	exit(0);
}
#endif

/**
 * @brief Command line parameter help
 */
static void MS_PrintHelp (void)
{
	printf(
		"    -h                  : show this screen\n"
		"    -p <port number>    : show this screen\n"
		"    -l <address>        : only listen to this address\n"
#ifndef _WIN32
		"    -d                  : daemonize the masterserver\n"
#endif
);
}

/**
 * @brief Parse command line parameters
 */
static void MS_ParseCommandLine (int argc, const char **argv)
{
	int i = 1;

	/* clear the config data */
	memset(&config, 0, sizeof(config));

	/* default values */
	config.port = MS_PORT;

	while (i < argc) {
		if (argv[i][0] != '-') {
			printf("[E] Invalid commandline options - skip the rest\n");
			return;
		}
		switch (argv[i][1]) {
		case 'l':
			i++;
			if (i >= argc || *argv[i] == '\0')
				printf("[W] Invalid address parameter\n");
			else
				config.listenName = argv[i];
			break;
		case 'h':
			MS_PrintHelp();
			exit(0);
#ifndef _WIN32
		case 'd':
			if (daemon(0, 0)) {
				printf("[W] Daemonization failed\n");
			} else
				config.daemonized = 1;
			break;
#endif
		case 'p':
			i++;
			config.port = atoi(argv[i]);
			if (!config.port || config.port < 1024) {
				printf("[W] Invalid port - using default %i\n", MS_PORT);
				config.port = MS_PORT;
			}
			break;
		default:
			printf("[E] Unknown commandline option %s\n", &argv[i][1]);
		}
		i++;
	}
}

/**
 * @brief
 */
int main (int argc, const char **argv)
{
	int len;
	socklen_t fromlen;
	struct sockaddr_in from;
#ifndef _WIN32
	const char *input;
#endif
	printf("    ufomaster %s\n", VERSION);

	/* default is no logging */
	logileActive = 0;

#ifdef _WIN32
	WSAStartup((WORD)MAKEWORD (1,1), &ws);
#else
	if (geteuid() == 0) {
		printf("[E] You should not run this as root - quitting now\n");
		return 0;
	}
#endif

	/* now parse the commandline and set the config values */
	MS_ParseCommandLine(argc, argv);

	listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	memset(&listenaddress, 0, sizeof(listenaddress));
	listenaddress.sin_family = AF_INET;
	if (config.listenName != NULL) {
		listenaddress.sin_addr.s_addr = inet_addr(config.listenName);
		if (listenaddress.sin_addr.s_addr == -1) {
			printf("[W] Could not use address %s - listen to any\n", config.listenName);
			listenaddress.sin_addr.s_addr = INADDR_ANY;
		} else
			printf("[I] Only listen at %s\n", config.listenName);
	} else {
		listenaddress.sin_addr.s_addr = INADDR_ANY;
	}
	listenaddress.sin_port = htons(config.port);

	if ((bind(listener, (struct sockaddr *)&listenaddress, sizeof(listenaddress))) != 0) {
		printf("[E] Couldn't bind to port %i UDP (something is probably using it)\n", config.port);
		MS_CloseSocket(listener);
		return 1;
	}

	delay.tv_sec = 1;
	delay.tv_usec = 0;

	/* listen(listener, SOMAXCONN); */

	memset(&servers, 0, sizeof(servers));

	printf("[I] listening on port %i (UDP)\n", config.port);
	printf("[I] type 'help' to get a list of available commands\n");

#ifndef _WIN32
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
#endif

	while (1) {
		FD_ZERO(&set);
		FD_SET(listener, &set);
		delay.tv_sec = 1;
		delay.tv_usec = 0;

		retval = select(listener + 1, &set, NULL, NULL, &delay);
		if (retval > 0) {
			if (FD_ISSET(listener, &set)) {
				fromlen = sizeof(from);
				len = recvfrom(listener, incoming, sizeof(incoming), 0, (struct sockaddr *)&from, &fromlen);
				if (len > 0) {
					if (len > 4) {
						/* parse this packet */
						MS_ParseResponse(&from, incoming, len);
					} else {
						MS_Printf("[W] runt packet from %s:%d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
					}

					/* reset for next packet */
					memset(incoming, 0, sizeof(incoming));
				} else {
					MS_Printf("[E] socket error during select from %s:%d (%d)\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), WSAGetLastError());
				}
			}
		}

		/* destroy old servers, etc */
		MS_RunFrame();
#ifdef COMMANDLINE
		input = MS_RunInput();
		if (input) {
			if (!strcmp(input, "quit")) {
				MS_ExitNicely();
				exit(0);
			} else if (!strcmp(input, "list"))
				MS_ListServers_f();
			else if (!strcmp(input, "log on"))
				logileActive = 1;
			else if (!strcmp(input, "log off"))
				logileActive = 0;
			else if (!strcmp(input, "help")) {
				printf("[H] quit         : shutdown masterserver\n");
				printf("[H] log <on|off> : activate/deactivate logging\n");
				printf("[H] help         : show this help\n");
				printf("[H] list         : list of all known servers\n");
			}
		}
#endif
	}
}
