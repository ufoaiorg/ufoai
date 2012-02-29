/**
 * @file scp_shared.h
 * @brief Singleplayer static campaign shared header
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

#ifndef SCP_SHARED_H
#define SCP_SHARED_H

#define MAX_REQMISSIONS	4
#define MAX_ACTMISSIONS	16
#define MAX_SETMISSIONS	16
#define MAX_STAGESETS	256
#define MAX_STAGES		64
#define MAX_STATIC_MISSIONS 256

typedef struct stageSet_s
{
	char name[MAX_VAR];
	char needed[MAX_VAR];
	char nextstage[MAX_VAR];
	char endstage[MAX_VAR];
	char cmds[MAX_VAR];		/**< script commands to execute when stageset gets activated */
	int number;
	int quota;				/**< missions that must be done before the
							 * next stage of this set is going to get activated */
	int ufos;				/**< ufos that are spawned if this stageset is activated */
	int missions[MAX_SETMISSIONS];
	date_t delay;
	date_t frame;
	date_t expire;			/**< date when this mission will expire and will be removed from geoscape */
	byte numMissions;		/**< number of missions in this set */
} stageSet_t;

typedef struct stage_s
{
	char name[MAX_VAR];	/**< stage name */
	int first;			/**< index of the first entry in the stageset array */
	int num;			/**< the amount of stagesets that are part of this stage */
} stage_t;

typedef struct setState_s
{
	const stageSet_t *def;
	stage_t *stage;
	date_t start;		/**< date when the set was activated */
	date_t event;
	int num;
	int done;			/**< how many mission out of the mission pool are already done */
	byte active;		/**< is this set active? */
} setState_t;

typedef struct stageState_s
{
	stage_t *def;
	date_t start;
	byte active;
} stageState_t;

typedef struct staticMission_s
{
	char id[MAX_VAR];		/**< script id and mapdef id */
	vec2_t pos;				/**< the polor coordinates the mission should get placed at */
	int count;
} staticMission_t;

typedef struct actMis_s
{
	staticMission_t *def;
	setState_t *cause;
	date_t expire;
	mission_t *mission;
} actMis_t;

typedef struct staticCampaignData_s {
	int numStageSets;
	stageSet_t stageSets[MAX_STAGESETS];

	int numStages;
	stage_t stages[MAX_STAGES];

	stageState_t stage[MAX_STAGES];
	setState_t set[MAX_STAGESETS];

	int numActiveMissions;
	actMis_t activeMissions[MAX_ACTMISSIONS];

	int numMissions;
	staticMission_t missions[MAX_STATIC_MISSIONS];

	stage_t *testStage;
	qboolean initialized;
} staticCampaignData_t;

extern staticCampaignData_t* scd;

#endif
