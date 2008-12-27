/**
 * @file client.h
 * @brief Primary header for client.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "cl_renderer.h"
#include "cl_video.h"
#include "../game/inv_shared.h"
#include "cl_sound.h"
#include "cl_screen.h"
#include "cl_input.h"
#include "cl_keys.h"
#include "cl_cinematic.h"
#include "cl_console.h"
#include "campaign/cl_market.h"
#include "campaign/cl_event.h"
#include "cl_save.h"
#include "cl_http.h"
#include "renderer/r_entity.h"

/*============================================================================= */

#define MAX_TEAMLIST	8

/* Map debugging constants */
#define MAPDEBUG_PATHING	(1<<0) /* Turns on pathing tracing. */
#define MAPDEBUG_TEXT		(1<<1) /* Creates arrows pointing at floors and ceilings at mouse cursor */
#define MAPDEBUG_CELLS		(1<<2) /* Creates arrows pointing at floors and ceilings at mouse cursor */
#define MAPDEBUG_WALLS		(1<<3) /* Creates arrows pointing at obstructions in the 8 primary directions */

/* Macros for faster access to the inventory-container. */
#define RIGHT(e) ((e)->i.c[csi.idRight])
#define LEFT(e)  ((e)->i.c[csi.idLeft])
#define FLOOR(e) ((e)->i.c[csi.idFloor])
#define HEADGEAR(e) ((e)->i.c[csi.idHeadgear])
#define EXTENSION(e) ((e)->i.c[csi.idExtension])

typedef struct {
	char name[MAX_QPATH];
	char cinfo[MAX_QPATH];
} clientinfo_t;

/**
 * @brief the client_state_t structure is wiped completely at every server map change
 * @sa client_static_t
 */
typedef struct client_state_s {
	int time;					/**< this is the time value that the client
								 * is rendering at.  always <= cls.realtime */
	int eventTime;				/**< similar to time, but not counting if blockEvents is set */

	camera_t cam;

	cmodes_t cmode;		/**< current selected action for the selected actor */
	int cfiremode;
	cmodes_t oldcmode;
	int oldstate;

	int msgTime;	/**< @sa MN_DisplayNotice @sa SCR_DisplayHudMessage */
	char msgText[256];	/**< @sa MN_DisplayNotice @sa SCR_DisplayHudMessage */

	struct le_s *teamList[MAX_TEAMLIST];
	int numTeamList;
	int numAliensSpotted;

	/** server state information */
	int servercount;	/**< server identification for prespawns */
	int pnum;			/**< player num you have on the server */
	int actTeam;		/**< the currently active team (in this round) */

	char configstrings[MAX_CONFIGSTRINGS][MAX_TOKEN_CHARS];

	/** locally derived information from server state */
	struct model_s *model_draw[MAX_MODELS];
	struct cBspModel_s *model_clip[MAX_MODELS];

	qboolean skipRadarNodes;	/**< maybe the current map doesn't have a radar image */
	qboolean radarInited;		/**< every radar image (for every level [1-8]) is loaded */

	clientinfo_t clientinfo[MAX_CLIENTS]; /**< client info of all connected clients */

	int map_maxlevel;
	int map_maxlevel_base;
} client_state_t;

extern client_state_t cl;

typedef enum {
	ca_uninitialized,
	ca_disconnected,			/**< not talking to a server */
	ca_sequence,				/**< rendering a sequence */
	ca_connecting,				/**< sending request packets to the server */
	ca_connected,				/**< netchan_t established, waiting for svc_serverdata */
	ca_active					/**< game views should be displayed */
} connstate_t;

typedef enum {
	key_game,
	key_input,
	key_console,
	key_message
} keydest_t;


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

#define MAX_SERVERLIST 128

/**
 * @brief Not cleared on a map change (static data)
 * @note Only some data is set to new values on a map change
 * @note the @c client_static_t structure is persistant through an arbitrary
 * number of server connections
 * @sa client_state_t
 */
typedef struct client_static_s {
	connstate_t state;
	keydest_t key_dest;
	keydest_t key_dest_old;
	qboolean deactivateKeyBindings;

	int realtime;				/**< always increasing, no clamping, etc */
	float frametime;			/**< seconds since last frame */
	float framerate;

	/** showing loading plaque between levels if time gets > 30 seconds ahead, break it */
	float disable_screen;

	/* connection information */
	char servername[MAX_OSPATH];	/**< name of server from original connect */
	char serverport[16];			/**< port the server is running at */
	int connectTime;				/**< for connection retransmits */
	int waitingForStart;			/**< waiting for EV_START or timeout */
	int serverListLength;
	int serverListPos;
	serverList_t serverList[MAX_SERVERLIST];
	serverList_t *selectedServer;

	struct datagram_socket *netDatagramSocket;
	struct net_stream *netStream;

	int challenge;				/**< from the server to use for connecting */

	/** needs to be here, because server can be shutdown, before we see the ending screen */
	int team;			/**< the team you are in @sa TEAM_CIVILIAN, TEAM_ALIEN */
	struct aircraft_s *missionaircraft;	/**< aircraft pointer for mission handling */

	float loadingPercent;
	char loadingMessages[96];

	int playingCinematic;	/**< Set to cinematic playing flags or 0 when not playing */

	int currentSelectedMap;	/**< current selected multiplayer or skirmish map */

	char downloadName[MAX_OSPATH];
	size_t downloadPosition;
	int downloadPercent;

	dlqueue_t downloadQueue;	/**< queue of paths we need */
	dlhandle_t HTTPHandles[4];	/**< actual download handles */
	char downloadServer[512];	/**< base url prefix to download from */
	char downloadReferer[32];	/**< libcurl requires a static string :( */
	CURL *curl;

	/* these models must only be loaded once */
	struct model_s *model_weapons[MAX_OBJDEFS];

	/* this pool is reloaded on every sound system restart */
	sfx_t *sound_pool[MAX_SOUNDIDS];
} client_static_t;

extern client_static_t cls;

extern struct memPool_s *cl_localPool;
extern struct memPool_s *cl_genericPool;
extern struct memPool_s *cl_ircSysPool;
extern struct memPool_s *cl_menuSysPool;
extern struct memPool_s *cl_soundSysPool;

/** @todo Make use of the tags */
typedef enum {
	CL_TAG_NONE,				/**< will be wiped on every new game */
	CL_TAG_PARSE_ONCE,			/**< will not be wiped on a new game (shaders, fonts) */
	CL_TAG_REPARSE_ON_NEW_GAME,	/**< reparse this stuff on a new game (techs, ...) */
	CL_TAG_MENU					/**< never delete it */
} clientMemoryTags_t;

/*============================================================================= */

/* i18n support via gettext */
#include <libintl.h>

/* the used textdomain for gettext */
#define TEXT_DOMAIN "ufoai"
#include <locale.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* cvars */
extern cvar_t *cl_http_downloads;
extern cvar_t *cl_http_filelists;
extern cvar_t *cl_http_max_connections;
extern cvar_t *cl_isometric;
extern cvar_t *cl_particleweather;
extern cvar_t *cl_leshowinvis;
extern cvar_t *cl_fps;
extern cvar_t *cl_logevents;
extern cvar_t *cl_centerview;
extern cvar_t *cl_worldlevel;
extern cvar_t *cl_selected;
extern cvar_t *cl_3dmap;
extern cvar_t *cl_numnames;
extern cvar_t *cl_start_employees;
extern cvar_t *cl_initial_equipment;
extern cvar_t *cl_start_buildings;
extern cvar_t *cl_team;
extern cvar_t *cl_teamnum;
extern cvar_t *cl_camzoommin;
extern cvar_t *cl_mapzoommax;
extern cvar_t *cl_mapzoommin;
extern cvar_t *cl_showCoords;
extern cvar_t *cl_autostand;

extern cvar_t *mn_active;
extern cvar_t *mn_afterdrop;
extern cvar_t *mn_main_afterdrop;
extern cvar_t *mn_main;
extern cvar_t *mn_sequence;
extern cvar_t *mn_hud;
extern cvar_t *mn_serverday;
extern cvar_t *mn_inputlength;

extern cvar_t *s_language;
extern cvar_t *difficulty;
extern cvar_t *confirm_actions;

extern cvar_t* cl_mapDebug;


/** limit the input for cvar editing (base name, save slots and so on) */
#define MAX_CVAR_EDITING_LENGTH 256 /* MAXCMDLINE */

void CL_LoadMedia(void);

/* cl_main.c */
void CL_SetClientState(int state);
void CL_Disconnect(void);
void CL_Init(void);
void CL_ReadSinglePlayerData(void);
void CL_RequestNextDownload(void);
void CL_StartSingleplayer(qboolean singleplayer);
void CL_ParseMedalsAndRanks(const char *name, const char **text, byte parserank);
void CL_ParseUGVs(const char *name, const char **text);
void CL_ScriptSanityCheck(void);
void CL_ScriptSanityCheckCampaign(void);

void CL_ClearState(void);

/* if you increase this, you also have to change the aircraft buy/sell menu scripts */
#define MAX_ACTIVETEAM	8

struct base_s;
struct installation_s;

typedef struct chr_list_s {
	character_t* chr[MAX_ACTIVETEAM];
	int num;	/* Number of entries */
} chrList_t;

/**
 * @brief List of currently displayed or equipeable characters.
 */
extern chrList_t chrDisplayList;
struct employee_s;

#include "cl_le.h"
#include "cl_menu.h"
#include "campaign/cl_radar.h"
#include "cl_research.h"
#include "campaign/cl_produce.h"
#include "campaign/cl_aliencont.h"
/* we need this in cl_aircraft.h */
#define MAX_EMPLOYEES 512
#include "cl_aircraft.h"
#include "campaign/cl_airfight.h"
/* needs the MAX_ACTIVETEAM definition from above. */
#include "cl_basemanagement.h"
#include "cl_employee.h"
#include "campaign/cl_transfer.h"
#include "campaign/cl_nation.h"
#include "campaign/cl_campaign.h"
#include "cl_inventory.h"
#include "cl_parse.h"
#include "campaign/cl_uforecovery.h"

#endif /* CLIENT_CLIENT_H */
