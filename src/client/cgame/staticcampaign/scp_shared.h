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

typedef struct stageSet_s
{
	char name[MAX_VAR];
	char needed[MAX_VAR];
	char nextstage[MAX_VAR];
	char endstage[MAX_VAR];
	char cmds[MAX_VAR];
	date_t delay;
	date_t frame;
	date_t expire;
	int number;
	int quota;
	byte numMissions;
	int missions[MAX_SETMISSIONS];
} stageSet_t;

typedef struct stage_s
{
	char name[MAX_VAR];
	int first, num;
} stage_t;

typedef struct setState_s
{
	stageSet_t *def;
	stage_t *stage;
	byte active;
	date_t start;
	date_t event;
	int num;
	int done;
} setState_t;

typedef struct stageState_s
{
	stage_t *def;
	byte active;
	date_t start;
} stageState_t;

typedef struct staticMission_s
{
	char mapDef[MAX_VAR];
	vec2_t pos;
} staticMission_t;

typedef struct actMis_s
{
	staticMission_t *def;
	setState_t *cause;
	date_t expire;
	vec2_t realPos;
} actMis_t;

typedef struct staticCampaignData_s {
	int numStageSets;
	stageSet_t stageSets[MAX_STAGESETS];
	int numStages;
	stage_t stages[MAX_STAGES];
	stageState_t _stage[MAX_STAGES];
	setState_t _set[MAX_STAGESETS];
	actMis_t mission[MAX_ACTMISSIONS];
	staticMission_t missions[256];
	int numMissions;
	stage_t *testStage;
} staticCampaignData_t;

extern staticCampaignData_t* scd;

#endif
