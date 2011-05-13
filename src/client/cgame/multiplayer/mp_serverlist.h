/**
 * @file mp_serverlist.c
 * @brief Serverlist management headers for multiplayer
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef MP_SERVERLIST_H
#define MP_SERVERLIST_H

#define MAX_BOOKMARKS 16

typedef struct serverList_s {
	char *node;						/**< node ip address */
	char *service;					/**< node port */
	qboolean pinged;				/**< already pinged */
	char sv_hostname[MAX_OSPATH];	/**< the server hostname */
	char mapname[16];				/**< currently running map */
	char version[8];				/**< the game version */
	char gametype[8];				/**< the game type */
	qboolean sv_dedicated;			/**< dedicated server */
	int sv_maxclients;				/**< max. client amount allowed */
	int clients;					/**< already connected clients */
	int serverListPos;				/**< position in the server list array */
} serverList_t;

extern serverList_t *selectedServer;

struct cgame_import_s;

void CL_PingServers_f(void);
void CL_PrintServerList_f(void);

void CL_ParseTeamInfoMessage(struct dbuffer *msg);
void CL_ParseServerInfoMessage(struct dbuffer *msg, const char *hostname);

void MP_ServerListInit(const struct cgame_import_s *import);
void MP_ServerListShutdown(void);

#endif
