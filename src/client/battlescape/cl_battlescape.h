/**
 * @file
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

#pragma once

#include "../../common/grid.h"

typedef struct {
	char name[MAX_VAR];
} clientinfo_t;

/**
 * @brief This is the structure that should be used for data that is needed for tactical missions only.
 * @note the client_state_t structure is wiped completely at every server map change
 * @sa client_static_t
 */
typedef struct clientBattleScape_s {
	int time;					/**< this is the time value that the client
								 * is rendering at.  always <= cls.realtime */
	camera_t cam;

	le_t* teamList[MAX_ACTIVETEAM];
	int numTeamList;
	int numEnemiesSpotted;

	bool eventsBlocked;	/**< if the client interrupts the event execution, this is true */

	/** server state information */
	int pnum;			/**< player num you have on the server */
	int actTeam;		/**< the currently active team (in this round) */

	char configstrings[MAX_CONFIGSTRINGS][MAX_TOKEN_CHARS];

	/** locally derived information from server state */
	model_t* model_draw[MAX_MODELS];
	const struct cBspModel_s* model_clip[MAX_MODELS];

	bool radarInitialized;		/**< every radar image (for every level [1-8]) is loaded */

	clientinfo_t clientinfo[MAX_CLIENTS]; /**< client info of all connected clients */

	int mapMaxLevel;

	/** @todo make this private to the particle code */
	int numMapParticles;

	int numLMs;
	localModel_t LMs[MAX_LOCALMODELS];

	int numLEs;
	le_t LEs[MAX_EDICTS];

	const char* leInlineModelList[MAX_EDICTS + 1];

	bool spawned;		/**< soldiers already spawned? This is only true if we are already on battlescape but
							 * our team is not yet spawned */
	bool started;		/**< match already started? */

	mapData_t* mapData;

	pathing_t pathMap;		/**< This is where the data for TUS used to move and actor locations go */

	mapTiles_t* mapTiles;

	linkedList_t* chrList;	/**< the list of characters that are used as team in the currently running tactical mission */
} clientBattleScape_t;

extern clientBattleScape_t cl;

le_t* CL_BattlescapeSearchAtGridPos(const pos3_t pos, bool includingStunned, const le_t* actor);
bool CL_OnBattlescape(void);
bool CL_BattlescapeRunning(void);
int CL_GetHitProbability(const le_t* actor);
bool CL_OutsideMap(const vec3_t impact, const float delta);
int CL_CountVisibleEnemies(void);
char* CL_GetConfigString(int index);
int CL_GetConfigStringInteger(int index);
char* CL_SetConfigString(int index, dbuffer* msg);
#ifdef DEBUG
void Grid_DumpWholeClientMap_f(void);
void Grid_DumpClientRoutes_f(void);
#endif
