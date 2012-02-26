/**
 * @file client.h
 * @brief Primary header for client.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/client.h
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef CLIENT_CLIENT_H
#define CLIENT_CLIENT_H

#include "cl_shared.h"
#include "cl_renderer.h"
#include "cl_video.h"
#include "cl_team.h"
#include "sound/s_main.h"
#include "input/cl_input.h"
#include "input/cl_keys.h"
#include "battlescape/cl_camera.h"
#include "battlescape/cl_localentity.h"
#include "battlescape/cl_battlescape.h"
#include "../game/inventory.h"
#include "renderer/r_model.h"
#include "../common/http.h"

struct cgame_export_s;

/**
 * @brief Not cleared on a map change (static data)
 * @note Only some data is set to new values on a map change
 * @note the @c client_static_t structure is persistant through an arbitrary
 * number of server connections
 * @sa client_state_t
 */
typedef struct client_static_s {
	connstate_t state;
	keydest_t keyDest;

	int realtime;				/**< always increasing, no clamping, etc */
	float frametime;			/**< seconds since last frame */
	float framerate;

	/** showing loading plaque between levels if time gets > 30 seconds ahead, break it */
	int disableScreen;

	const struct cgame_export_s *gametype;		/**< singleplayer or multiplayer */

	/* connection information */
	char servername[MAX_VAR];		/**< name of server from original connect */
	char serverport[16];			/**< port the server is running at */
	int connectTime;				/**< for connection retransmits */
	int reconnectTime;				/**< time until the reconnect to a server is triggered */
	int waitingForStart;			/**< waiting for EV_START or timeout */

	struct net_stream *netStream;

	/** needs to be here, because server can be shutdown, before we see the ending screen */
	int team;			/**< the team you are in @sa TEAM_CIVILIAN, TEAM_ALIEN */
	int teamSaveSlotIndex;	/**< currently loaded team slot, -1 if no team slot was loaded */

	int currentSelectedMap;	/**< current selected multiplayer or skirmish map */

	char downloadName[MAX_OSPATH];
	size_t downloadPosition;
	int downloadPercent;

	qboolean downloadMaps;
	dlqueue_t downloadQueue;	/**< queue of paths we need */
	dlhandle_t HTTPHandles[4];	/**< actual download handles */
	char downloadServer[512];	/**< base url prefix to download from */
	char downloadReferer[32];	/**< libcurl requires a static string :( */
	CURL *curl;

	/** these models must only be loaded once */
	struct model_s *modelPool[MAX_OBJDEFS];

	/* unique character id */
	int nextUniqueCharacterNumber;

	inventoryInterface_t i;

	actorSkin_t actorSkins[MAX_ACTORSKINNAME];
	unsigned int numActorSkins;

#ifndef HARD_LINKED_CGAME
	void *cgameLibrary;
#endif
} client_static_t;

extern client_static_t cls;

/*============================================================================= */

/* cvars */
extern cvar_t *cl_fps;
extern cvar_t *cl_selected;
extern cvar_t *cl_teamnum;

extern cvar_t *s_language;

/* cl_main.c */
int CL_GetClientState(void);
void CL_SetClientState(int state);
void CL_Disconnect(void);
void CL_Init(void);

#endif /* CLIENT_CLIENT_H */
