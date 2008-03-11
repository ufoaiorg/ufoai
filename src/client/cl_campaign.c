/**
 * @file cl_campaign.c
 * @brief Single player campaign control.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"

/* public vars */
static mission_t missions[MAX_MISSIONS];	/**< Missions parsed in missions.ufo AND missions created during game (crash site, ...)
											 * (sa ccs.mission for active missions drawn on geoscape) */
static int numMissions;				/**< Number of mission parsed in missions.ufo PLUS nb of mission created during game (crash site, ...)
									 * (sa ccs.numMissions for number of active missions drawn on geoscape) */
actMis_t *selMis;				/**< Currently selected mission on geoscape */

static campaign_t campaigns[MAX_CAMPAIGNS];	/**< Document me. */
static int numCampaigns = 0;			/**< Document me. */

static stageSet_t stageSets[MAX_STAGESETS];	/**< Document me. */
static stage_t stages[MAX_STAGES];		/**< Document me. */
static int numStageSets = 0;			/**< Document me. */
static int numStages = 0;			/**< Document me. */

campaign_t *curCampaign;			/**< Current running campaign */
ccs_t ccs;					/**< Document me. */
base_t *baseCurrent;				/**< Pointer to current base. */

static technology_t *rs_alien_xvi;

static salary_t salaries[MAX_CAMPAIGNS];

/* extern in client.h */
stats_t stats;					/**< Document me. */

/*
============================================================================
Boolean expression parser
============================================================================
*/

enum {
	BEPERR_NONE,
	BEPERR_KLAMMER,
	BEPERR_NOEND,
	BEPERR_NOTFOUND
} BEPerror;

static char varName[MAX_VAR];			/**< Document me. */

static qboolean(*varFunc)(char *var);
static qboolean CheckAND(char **s);

static void SkipWhiteSpaces (char **s)
{
	while (**s == ' ')
		(*s)++;
}

static void NextChar (char **s)
{
	(*s)++;
	/* skip white-spaces too */
	SkipWhiteSpaces(s);
}

static char *GetSwitchName (char **s)
{
	int pos = 0;

	while (**s > 32 && **s != '^' && **s != '|' && **s != '&' && **s != '!' && **s != '(' && **s != ')') {
		varName[pos++] = **s;
		(*s)++;
	}
	varName[pos] = 0;

	return varName;
}

static qboolean CheckOR (char **s)
{
	qboolean result = qfalse;
	int goon = 0;

	SkipWhiteSpaces(s);
	do {
		if (goon == 2)
			result ^= CheckAND(s);
		else
			result |= CheckAND(s);

		if (**s == '|') {
			goon = 1;
			NextChar(s);
		} else if (**s == '^') {
			goon = 2;
			NextChar(s);
		} else {
			goon = 0;
		}
	} while (goon && !BEPerror);

	return result;
}

static qboolean CheckAND (char **s)
{
	qboolean result = qtrue;
	qboolean negate = qfalse;
	qboolean goon = qfalse;
	int value;

	do {
		while (**s == '!') {
			negate ^= qtrue;
			NextChar(s);
		}
		if (**s == '(') {
			NextChar(s);
			result &= CheckOR(s) ^ negate;
			if (**s != ')')
				BEPerror = BEPERR_KLAMMER;
			NextChar(s);
		} else {
			/* get the variable state */
			value = varFunc(GetSwitchName(s));
			if (value == -1)
				BEPerror = BEPERR_NOTFOUND;
			else
				result &= value ^ negate;
			SkipWhiteSpaces(s);
		}

		if (**s == '&') {
			goon = qtrue;
			NextChar(s);
		} else {
			goon = qfalse;
		}
		negate = qfalse;
	} while (goon && !BEPerror);

	return result;
}

/**
 * @brief Boolean expression parser
 *
 * @param[in] expr
 * @param[in] varFuncParam Function pointer
 * @return qboolean
 * @sa CheckOR
 * @sa CheckAND
 */
static qboolean CheckBEP (char *expr, qboolean(*varFuncParam) (char *var))
{
	qboolean result;
	char *str;

	BEPerror = BEPERR_NONE;
	varFunc = varFuncParam;
	str = expr;
	result = CheckOR(&str);

	/* check for no end error */
	if (*str && !BEPerror)
		BEPerror = BEPERR_NOEND;

	switch (BEPerror) {
	case BEPERR_NONE:
		/* do nothing */
		return result;
	case BEPERR_KLAMMER:
		Com_Printf("')' expected in BEP (%s).\n", expr);
		return qtrue;
	case BEPERR_NOEND:
		Com_Printf("Unexpected end of condition in BEP (%s).\n", expr);
		return result;
	case BEPERR_NOTFOUND:
		Com_Printf("Variable '%s' not found in BEP (%s).\n", varName, expr);
		return qfalse;
	default:
		/* shouldn't happen */
		Com_Printf("Unknown CheckBEP error in BEP (%s).\n", expr);
		return qtrue;
	}
}


/* =========================================================== */

/**
 * @brief Check wheter given date and time is later than current date.
 * @param[in] now Current date.
 * @param[in] compare Date to compare.
 * @return True if current date is later than given one.
 */
static qboolean Date_LaterThan (date_t now, date_t compare)
{
	if (now.day > compare.day)
		return qtrue;
	if (now.day < compare.day)
		return qfalse;
	if (now.sec > compare.sec)
		return qtrue;
	return qfalse;
}


/**
 * @brief Add two dates and return the result.
 * @param[in] a First date.
 * @param[in] b Second date.
 * @return The result of adding date_ b to date_t a.
 */
static date_t Date_Add (date_t a, date_t b)
{
	a.sec += b.sec;
	a.day += (a.sec / (3600 * 24)) + b.day;
	a.sec %= 3600 * 24;
	return a;
}

#if (0)
static date_t Date_Random (date_t frame)
{
	frame.sec = (frame.day * 3600 * 24 + frame.sec) * frand();
	frame.day = frame.sec / (3600 * 24);
	frame.sec = frame.sec % (3600 * 24);
	return frame;
}
#endif

static date_t Date_Random_Begin (date_t frame)
{
	int sec = frame.day * 3600 * 24 + frame.sec;

	/* first 1/3 of the frame */
	frame.sec = sec * frand() / 3;
	frame.day = frame.sec / (3600 * 24);
	frame.sec = frame.sec % (3600 * 24);
	return frame;
}


static date_t Date_Random_Middle (date_t frame)
{
	int sec = frame.day * 3600 * 24 + frame.sec;

	/* middle 1/3 of the frame */
	frame.sec = sec / 3 + sec * frand() / 3;
	frame.day = frame.sec / (3600 * 24);
	frame.sec = frame.sec % (3600 * 24);
	return frame;
}

/**
 * @brief Check conditions for new base and build it.
 * @param[in] pos Position on the geoscape.
 * @return True if the base has been build.
 * @sa B_BuildBase
 */
qboolean CL_NewBase (base_t* base, vec2_t pos)
{
	byte *colorTerrain;

	assert(base);

	if (base->founded) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: base already founded: %i\n", base->idx);
		return qfalse;
	} else if (gd.numBases == MAX_BASES) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: max base limit hit\n");
		return qfalse;
	}

	colorTerrain = MAP_GetColor(pos, MAPTYPE_TERRAIN);

	if (MapIsWater(colorTerrain)) {
		/* This should already have been catched in MAP_MapClick (cl_menu.c), but just in case. */
		MN_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_INFO, NULL);
		return qfalse;
	} else {
		base->mapZone = MAP_GetTerrainType(colorTerrain);
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: zoneType: '%s'\n", base->mapZone);
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for base terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build base */
	Vector2Copy(pos, base->pos);

	gd.numBases++;

	/* set up the base with buildings that have the autobuild flag set */
	B_SetUpBase(base);

	return qtrue;
}

/**
 * @brief The active stage in the current campaign
 * @sa CL_StageSetDone
 */
static const stage_t *activeStage = NULL;

/**
 * @brief Checks wheter a stage set exceeded the quota
 * @param[in] name Stage set name from script file
 * @return qboolean
 */
static qboolean CL_StageSetDone (char *name)
{
	setState_t *set;
	int i;

	assert(activeStage);

	for (i = 0, set = &ccs.set[activeStage->first]; i < activeStage->num; i++, set++)
		if (!Q_strncmp(name, set->def->name, MAX_VAR)) {
			if (set->def->number && set->num >= set->def->number)
				return qtrue;
			if (set->def->quota && set->done >= set->def->quota)
				return qtrue;
		}
	return qfalse;
}


/**
 * @sa CL_CampaignExecute
 */
static void CL_CampaignActivateStageSets (const stage_t *stage)
{
	setState_t *set;
	int i;

	activeStage = stage;
#ifdef PARANOID
	if (stage->first + stage->num >= MAX_STAGESETS)
		Sys_Error("CL_CampaignActivateStageSets '%s' (first: %i, num: %i)\n", stage->name, stage->first, stage->num);
#endif
	for (i = 0, set = &ccs.set[stage->first]; i < stage->num; i++, set++)
		if (!set->active && !set->num) {
			Com_DPrintf(DEBUG_CLIENT, "CL_CampaignActivateStageSets: i = %i, stage->first = %i, stage->num = %i, stage->name = %s\n", i, stage->first, stage->num, stage->name);
			assert(set->stage);
			assert(set->def);

			/* check needed sets */
			if (set->def->needed[0] && !CheckBEP(set->def->needed, CL_StageSetDone))
				continue;

			Com_DPrintf(DEBUG_CLIENT, "Activated: set->def->name = %s.\n", set->def->name);

			/* activate it */
			set->active = qtrue;
			set->start = Date_Add(ccs.date, set->def->delay);
			set->event = Date_Add(set->start, Date_Random_Begin(set->def->frame));
			if (*(set->def->sequence))
				Cbuf_AddText(va("seq_start %s;\n", set->def->sequence));
			/* XVI spreading has started */
			if (set->def->activateXVI) {
				ccs.XVISpreadActivated = qtrue;
				/* Mark prequesite of "rs_alien_xvi" as met. */
				RS_ResearchFinish(RS_GetTechByID("rs_alien_xvi_event"));
			}
			/* humans start to attacking player */
			if (set->def->humanAttack) {
				ccs.humansAttackActivated = qtrue;
				/* Mark prequesite of "rs_enemy_on_earth" as met. */
				RS_ResearchFinish(RS_GetTechByID("rs_enemy_on_earth_event"));
			}
		}
}


/**
 * @sa CL_CampaignActivateStageSets
 * @sa CL_CampaignExecute
 */
static stageState_t *CL_CampaignActivateStage (const char *name, qboolean setsToo)
{
	stage_t *stage;
	stageState_t *state;
	int i, j;

	for (i = 0, stage = stages; i < numStages; i++, stage++) {
		if (!Q_strncmp(stage->name, name, MAX_VAR)) {
			Com_Printf("Activate stage %s\n", stage->name);
			/* add it to the list */
			state = &ccs.stage[i];
			state->active = qtrue;
			state->def = stage;
			state->start = ccs.date;

			/* add stage sets */
			for (j = stage->first; j < stage->first + stage->num; j++) {
				memset(&ccs.set[j], 0, sizeof(setState_t));
				ccs.set[j].stage = stage;
				ccs.set[j].def = &stageSets[j];
			}
			/* activate stage sets */
			if (setsToo)
				CL_CampaignActivateStageSets(stage);

			return state;
		}
	}

	Com_Printf("CL_CampaignActivateStage: stage '%s' not found.\n", name);
	return NULL;
}


/**
 * @sa CL_CampaignExecute
 */
static void CL_CampaignEndStage (const char *name)
{
	stageState_t *state;
	int i;
	for (i = 0, state = ccs.stage; i < numStages; i++, state++) {
		if (state->def->name != NULL) {
			if (!Q_strncmp(state->def->name, name, MAX_VAR)) {
				state->active = qfalse;
				return;
			}
		}
	}

	Com_Printf("CL_CampaignEndStage: stage '%s' not found.\n", name);
}


/**
 * @sa CL_CampaignActivateStage
 * @sa CL_CampaignEndStage
 * @sa CL_CampaignActivateStageSets
 */
static void CL_CampaignExecute (setState_t * set)
{
	/* handle stages, execute commands */
	if (*set->def->nextstage)
		CL_CampaignActivateStage(set->def->nextstage, qtrue);

	if (*set->def->endstage)
		CL_CampaignEndStage(set->def->endstage);

	if (*set->def->cmds) {
		if (set->def->cmds[strlen(set->def->cmds)-1] != '\n'
		 && set->def->cmds[strlen(set->def->cmds)-1] != ';')
			Com_Printf("CL_CampaignExecute: missing command seperator for commands: '%s'\n", set->def->cmds);
		Cbuf_AddText(set->def->cmds);
	}

	/* activate new sets in old stage */
	CL_CampaignActivateStageSets(set->stage);
}

/**
 * @brief Returns the alien XVI tech if the tech was already researched
 */
technology_t *CP_IsXVIResearched (void)
{
	return RS_IsResearched_ptr(rs_alien_xvi) ? rs_alien_xvi : NULL;
}

#define DETAILSWIDTH 14
/**
 * @brief Console command to list all available missions
 */
static void CP_MissionList_f (void)
{
	int i, j;
	qboolean details = qfalse;
	char tmp[DETAILSWIDTH+1];

	if (Cmd_Argc() > 1)
		details = qtrue;
	else
		Com_Printf("Use details as parameter to get a more detailed list\n");

	/* detail header */
	if (details) {
		Com_Printf("| %-14s | %-14s | %-14s | %-14s | #  | %-14s | %-14s | #  |\n|", "id", "map", "param", "alienteam", "alienequip", "civteam");
		for (i = 0; i < 4; i++)
			Com_Printf("----------------|");
		Com_Printf("----|----------------|----------------|----|");
		Com_Printf("\n");
	}

	for (i = 0; i < numMissions; i++) {
		if (details) {
			Q_strncpyz(tmp, missions[i].name, sizeof(tmp));
			Com_Printf("| %-*s | ", DETAILSWIDTH, tmp);
			Q_strncpyz(tmp, missions[i].mapDef->map, sizeof(tmp));
			Com_Printf("%-*s | ", DETAILSWIDTH, tmp);
			if (missions[i].mapDef->param)
				Q_strncpyz(tmp, missions[i].mapDef->param, sizeof(tmp));
			else
				tmp[0] = '\0';
			Com_Printf("%-*s | ", DETAILSWIDTH, tmp);
			for (j = 0; j < missions[i].numAlienTeams; j++)
				Q_strncpyz(tmp, missions[i].alienTeams[j]->id, sizeof(tmp));
			Com_Printf("%-*s | ", DETAILSWIDTH, tmp);
			Com_Printf("%02i | ", missions[i].aliens);
			Q_strncpyz(tmp, missions[i].alienEquipment, sizeof(tmp));
			Com_Printf("%-*s | ", DETAILSWIDTH, tmp);
			Q_strncpyz(tmp, missions[i].civTeam, sizeof(tmp));
			Com_Printf("%-*s | ", DETAILSWIDTH, tmp);
			Com_Printf("%02i |", missions[i].civilians);
			Com_Printf("\n");
		} else
			Com_Printf("%s\n", missions[i].name);
	}
}

/**
 * @sa CP_SpawnBaseAttackMission
 */
qboolean CP_SpawnCrashSiteMission (aircraft_t* aircraft)
{
	mission_t* ms;
	actMis_t* mis;
	const char *zoneType;
	char missionName[MAX_VAR];
	const nation_t *nation;
	byte *color;

	assert(aircraft);

	color = MAP_GetColor(aircraft->pos, MAPTYPE_TERRAIN);
	zoneType = MAP_GetTerrainType(color);
	/* spawn new mission */
	/* some random data like alien race, alien count (also depends on ufo-size) */
	/* @todo: We should have a ufo crash theme for random map assembly */
	/* @todo: call the map assembly theme with the right parameter, e.g.: +ufocrash [climazone] */
	Com_sprintf(missionName, sizeof(missionName), "ufocrash%.0f:%.0f", aircraft->pos[0], aircraft->pos[1]);
	ms = CL_AddMission(missionName);
	if (!ms) {
		MN_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost."), qfalse, MSG_CRASHSITE, NULL);
		return qfalse;
	}
	ms->mapDef = Com_GetMapDefinitionByID("ufocrash");
	if (!ms->mapDef) {
		CP_RemoveLastMission();
		MN_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost."), qfalse, MSG_CRASHSITE, NULL);
		return qfalse;
	}

	ms->missionType = MIS_CRASHSITE;
	/* the size if somewhat random, because not all map tiles would have
		* alien spawn points */
	ms->aliens = aircraft->maxTeamSize;
	/* 1-4 civilians */
	ms->civilians = (frand() * 10);
	ms->civilians %= 4;
	ms->civilians += 1;
	ms->zoneType = zoneType; /* store to terrain type for texture replacement */

	/* realPos is set below */
	Vector2Set(ms->pos, aircraft->pos[0], aircraft->pos[1]);

	nation = MAP_GetNation(ms->pos);
	Com_sprintf(ms->type, sizeof(ms->type), _("UFO crash site"));
	if (nation) {
		Com_sprintf(ms->location, sizeof(ms->location), _(nation->name));
		Q_strncpyz(ms->civTeam, nation->id, sizeof(ms->civTeam));
	} else {
		Com_sprintf(ms->location, sizeof(ms->location), _("No nation"));
		Q_strncpyz(ms->civTeam, "europa", sizeof(ms->civTeam));
	}
	ms->missionText = "Crashed Alien Ship. Secure the area.";

	/* FIXME */
	ms->alienTeams[0] = Com_GetTeamDefinitionByID("ortnok");
	if (ms->alienTeams[0])
		ms->numAlienTeams++;
	/* FIXME */
	Com_sprintf(ms->alienEquipment, sizeof(ms->alienEquipment), "stage%i_%s", 1, "soldiers");

	/* use ufocrash.ump as random tile assembly */
	ms->mapDef->param = Mem_PoolStrDup(UFO_TypeToShortName(aircraft->ufotype), cl_localPool, CL_TAG_NONE);
	ms->mapDef->loadingscreen = Mem_PoolStrDup("crashsite", cl_localPool, CL_TAG_NONE);
	Com_sprintf(ms->onwin, sizeof(ms->onwin), "cp_ufocrashed %i;", aircraft->ufotype);
	mis = CL_CampaignAddGroundMission(ms);
	if (mis) {
		Vector2Set(mis->realPos, ms->pos[0], ms->pos[1]);
		MN_AddNewMessage(_("Interception"), _("UFO interception successful -- New mission available."), qfalse, MSG_CRASHSITE, NULL);
	} else {
		/* no active stage - to decrement the mission counter */
		CP_RemoveLastMission();
		MN_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost."), qfalse, MSG_CRASHSITE, NULL);
		return qfalse;
	}
	return qtrue;
}

/**
 * @sa B_BaseAttack
 * @sa CP_Load
 * @sa CP_SpawnCrashSiteMission
 */
qboolean CP_SpawnBaseAttackMission (base_t* base, mission_t* ms, setState_t *cause)
{
	actMis_t *mis;

	assert(ms);
	assert(base);

	/* realPos must still be set */
	Vector2Set(ms->pos, base->pos[0], base->pos[1]);

	/* set the mission type to base attack and store the base in data pointer */
	/* this is useful if the mission expires and we want to know which base it was */
	ms->missionType = MIS_BASEATTACK;
	ms->data = (void*)base;

	Com_sprintf(ms->location, sizeof(ms->location), base->name);
	Q_strncpyz(ms->civTeam, "human_scientist", sizeof(ms->civTeam));
	Q_strncpyz(ms->type, _("Base attack"), sizeof(ms->type));
	ms->missionText = "Base is under attack.";
	/* FIXME */
	ms->alienTeams[0] = Com_GetTeamDefinitionByID("ortnok");
	if (ms->alienTeams[0])
		ms->numAlienTeams++;
	/* FIXME */
	Com_sprintf(ms->alienEquipment, sizeof(ms->alienEquipment), "stage%i_%s", 1, "soldiers");
	ms->aliens = 8;
	ms->civilians = 8;

	ms->zoneType = base->mapZone;

	ms->mapDef = Com_GetMapDefinitionByID("baseattack");
	if (!ms->mapDef) {
		CP_RemoveLastMission();
		Sys_Error("Could not find mapdef baseattack");
		return qfalse;
	}

	if (cause)
		return qtrue;

	mis = CL_CampaignAddGroundMission(ms);
	if (mis) {
		Vector2Set(mis->realPos, ms->pos[0], ms->pos[1]);
	} else {
		/* no active stage - to decrement the mission counter */
		CP_RemoveLastMission();
		baseCurrent->baseStatus = BASE_WORKING;
		Com_DPrintf(DEBUG_CLIENT, "CP_SpawnBaseAttackMission: Could not set base %s under attack - remove the mission data again\n", base->name);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Add a new ground mission to the ccs.mission array
 * @param[in] mis The mission to add to geoscape
 * @note This mission must already be defined completly
 * @note If the mapdef is missing, this function will return NULL, too
 * @sa CL_AddMission
 * @sa CL_CampaignAddMission
 * @return NULL if failed - actMis_t pointer (to ccs.mission array) if successful
 */
actMis_t* CL_CampaignAddGroundMission (mission_t* mission)
{
	stageState_t *stage = NULL;
	setState_t *set = NULL;
	actMis_t *mis = NULL;
	int i, j, activeCnt = 0;

	/* add mission */
	if (ccs.numMissions >= MAX_ACTMISSIONS) {
		Com_DPrintf(DEBUG_CLIENT, "CL_CampaignAddGroundMission: Too many active missions!\n");
		return NULL;
	}

	if (!mission->mapDef) {
		Com_Printf("CL_CampaignAddGroundMission: No mapdef pointer for mission: '%s'\n", mission->name);
		return NULL;
	}

	mis = &ccs.mission[ccs.numMissions++];
	memset(mis, 0, sizeof(actMis_t));

	/* set relevant info */
	mis->def = mission;

	/* check campaign events */
	for (i = 0, stage = ccs.stage; i < numStages; i++, stage++)
		if (stage->active) {
			for (j = 0, set = &ccs.set[stage->def->first]; j < stage->def->num; j++, set++)
				/* don't increase numMissions here - missions that are added
				 * with this function don't belong to the campaign definition
				 * but are dynamic missions - so chosing them as a random popup
				 * geoscape mission wouldn't be good */
				if (set->active && set->def->numMissions) {
					mis->cause = set;
					return mis;
				}
			activeCnt++;
		}
	Com_Printf("Could not add ground mission '%s' (active stages: %i)\n", mission->name, activeCnt);
	ccs.numMissions--;
	return NULL;
}

#define DIST_MIN_BASE_MISSION 4
/**
 * @note missions that have the keepAfterFail boolean set should not be removed
 * after the mission failed - but only when the mission was successful
 * @sa CL_CampaignRemoveMission
 * @sa CP_CheckEvents
 * @sa CL_AddMission
 * @sa CL_CampaignAddGroundMission
 */
static void CL_CampaignAddMission (setState_t * set)
{
	actMis_t *mis;

	mission_t *misTemp;
	int i;
	float f;

	/* add mission */
	if (ccs.numMissions >= MAX_ACTMISSIONS) {
		Com_DPrintf(DEBUG_CLIENT, "CL_CampaignAddMission: Too many active missions!\n");
		return;
	}

	do {
		misTemp = &missions[set->def->missions[rand() % set->def->numMissions]];

		if ((set->def->numMissions < 2
			 || Q_strncmp(misTemp->name, gd.oldMis1, MAX_VAR))
			&& (set->def->numMissions < 3
				|| Q_strncmp(misTemp->name, gd.oldMis2, MAX_VAR))
			&& (set->def->numMissions < 4
				|| Q_strncmp(misTemp->name, gd.oldMis3, MAX_VAR)))
			break;
	} while (1);

	/* maybe the mission is already on geoscape --- e.g. one-mission sets */
	if (misTemp->onGeoscape) {
		Com_DPrintf(DEBUG_CLIENT, "CL_CampaignAddMission: Mission is already on geoscape\n");
		return;
	} else {
		misTemp->onGeoscape = qtrue;
	}
	mis = &ccs.mission[ccs.numMissions++];
	memset(mis, 0, sizeof(actMis_t));

	/* set relevant info */
	mis->def = misTemp;
	mis->cause = set;
	Q_strncpyz(gd.oldMis3, gd.oldMis2, sizeof(gd.oldMis3));
	Q_strncpyz(gd.oldMis2, gd.oldMis1, sizeof(gd.oldMis2));
	Q_strncpyz(gd.oldMis1, misTemp->name, sizeof(gd.oldMis1));

	/* execute mission commands */
	if (*mis->def->cmds)
		Cmd_ExecuteString(mis->def->cmds);

	if (set->def->expire.day)
		mis->expire = Date_Add(ccs.date, Date_Random_Middle(set->def->expire));

	/* A mission must not be very near a base */
	for (i = 0; i < gd.numBases; i++) {
		if (MAP_GetDistance(mis->def->pos, gd.bases[i].pos) < DIST_MIN_BASE_MISSION) {
			f = frand();
			mis->def->pos[0] = gd.bases[i].pos[0] + (gd.bases[i].pos[0] < 0 ? f * DIST_MIN_BASE_MISSION : -f * DIST_MIN_BASE_MISSION);
			f = sin(acos(f));
			mis->def->pos[1] = gd.bases[i].pos[1] + (gd.bases[i].pos[1] < 0 ? f * DIST_MIN_BASE_MISSION : -f * DIST_MIN_BASE_MISSION);
			break;
		}
	}
	/* get default position first, then try to find a corresponding mask color */
	Vector2Set(mis->realPos, mis->def->pos[0], mis->def->pos[1]);
	MAP_MaskFind(mis->def->mask, mis->realPos);

	/* Add message to message-system. */
	Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Alien activity has been reported: '%s'"), mis->def->location);
	MN_AddNewMessage(_("Alien activity"), messageBuffer, qfalse, MSG_TERRORSITE, NULL);
	Com_DPrintf(DEBUG_CLIENT, "Alien activity at long: %.0f lat: %0.f\n", mis->realPos[0], mis->realPos[1]);

	/* prepare next event (if any) */
	set->num++;
	if (set->def->number && set->num >= set->def->number)
		set->active = qfalse;
	else
		set->event = Date_Add(ccs.date, Date_Random_Middle(set->def->frame));

	if (set->def->ufos > 0) {
		Cbuf_AddText("addufo;");
		set->def->ufos--;
	}

	/* stop time */
	CL_GameTimeStop();
}

/**
 * @brief Removes a mission from geoscape
 * @note This function is called in several different situations:
 * a mission was won
 * a mission doesn't have the keepAfterFail set and was lost
 * a mission that expired (prevent this via noExpire)
 * @sa CL_CampaignAddMission
 * @note Missions that are removed should already have the onGeoscape set to true
 * and thus won't be respawned on geoscape
 */
static void CL_CampaignRemoveMission (actMis_t * mis)
{
	int i, num;
	base_t *base;

	num = mis - ccs.mission;
	if (num >= ccs.numMissions) {
		Com_Printf("CL_CampaignRemoveMission: Can't remove mission.\n");
		return;
	}

	/* Clear base-attack status if required */
	if (mis->def->missionType == MIS_BASEATTACK) {
		base = (base_t*)mis->def->data;
		B_BaseResetStatus(base);
	}

	/* Notifications */
	MAP_NotifyMissionRemoved(mis);
	AIR_AircraftsNotifyMissionRemoved(mis);
	CL_PopupNotifyMissionRemoved(mis);

	ccs.numMissions--;
	Com_DPrintf(DEBUG_CLIENT, "%i missions left\n", ccs.numMissions);
	for (i = num; i < ccs.numMissions; i++)
		ccs.mission[i] = ccs.mission[i + 1];
}

/**
 * @brief Builds the aircraft list for textfield with id
 */
static void CL_AircraftList_f (void)
{
	char *s;
	int i, j;
	aircraft_t *aircraft;
	static char aircraftListText[1024];

	memset(aircraftListText, 0, sizeof(aircraftListText));
	for (j = 0; j < gd.numBases; j++) {
		if (!gd.bases[j].founded)
			continue;

		for (i = 0; i < gd.bases[j].numAircraftInBase; i++) {
			aircraft = &gd.bases[j].aircraft[i];
			s = va("%s (%i/%i)\t%s\t%s\n", _(aircraft->name), aircraft->teamSize,
				aircraft->maxTeamSize, AIR_AircraftStatusToName(aircraft), gd.bases[j].name);
			Q_strcat(aircraftListText, s, sizeof(aircraftListText));
		}
	}

	menuText[TEXT_AIRCRAFT_LIST] = aircraftListText;
}

/**
 * @brief Backs up each nation's relationship values.
 * @note Right after the copy the stats for the current month are the same as the ones from the (end of the) previous month.
 * They will change while the curent month is running of course :)
 * @todo otehr stuff to back up?
 */
static void CL_BackupMonthlyData (void)
{
	int i, nat;

	/**
	 * Back up nation relationship .
	 * "inuse" is copied as well so we do not need to set it anywhere.
	 */
	for (nat = 0; nat < gd.numNations; nat++) {
		nation_t *nation = &gd.nations[nat];

		for (i = MONTHS_PER_YEAR-1; i > 0; i--) {	/* Reverse copy to not overwrite with wrong data */
			memcpy(&nation->stats[i], &nation->stats[i-1], sizeof(nationInfo_t));
		}
	}
}

/**
 * @brief Function to handle the campaign end
 */
static void CP_EndCampaign (qboolean won)
{
	CL_GameExit();
	assert(!curCampaign);

	if (won)
		Cvar_Set("mn_afterdrop", "endgame");
	else
		Cvar_Set("mn_afterdrop", "lostgame");
	Com_Drop();
}

/**
 * @brief Return the average XVI rate
 * @note XVI = eXtraterrestial Viral Infection
 * @return value between 0 and 100 (and not between 0.00 and 1.00)
 */
static int CP_GetAverageXVIRate (void)
{
	float XVIRate = 0;
	int i;
	nation_t* nation;

	assert(gd.numNations);

	/* check for XVI infection rate */
	for (i = 0, nation = gd.nations; i < gd.numNations; i++, nation++) {
		XVIRate += nation->stats[0].xvi_infection;
	}
	XVIRate /= gd.numNations;
	XVIRate *= 100;
	return (int) XVIRate;
}

/**
 * @brief Checks whether the player has lost the campaign
 * @note
 */
static void CP_CheckLostCondition (qboolean lost, const mission_t* mission, int civiliansKilled)
{
	qboolean endCampaign = qfalse;
 	/* fraction of nation that can be below min happiness before the game is lost */
	const float nationBelowLimitPercentage = 0.5f;

	assert(curCampaign);

	if (!endCampaign && ccs.credits < -curCampaign->negativeCreditsUntilLost) {
		menuText[TEXT_STANDARD] = _("You've gone too far into debt.");
		endCampaign = qtrue;
	}

	if (!endCampaign) {
		if (CP_GetAverageXVIRate() > curCampaign->maxAllowedXVIRateUntilLost) {
			menuText[TEXT_STANDARD] = _("You have failed in your charter to protect Earth."
				" Our home and our people have fallen to the alien infection. Only a handful"
				" of people on Earth remain human, and the remaining few no longer have a"
				" chance to stem the tide. Your command is no more; PHALANX is no longer"
				" able to operate as a functioning unit. Nothing stands between the aliens"
				" and total victory.");
			endCampaign = qtrue;
		} else {
			/* check for nation happiness */
			int j, nationBelowLimit = 0;
			for (j = 0; j < gd.numNations; j++) {
				nation_t *nation = &gd.nations[j];
				if (nation->stats[0].happiness < curCampaign->minhappiness) {
					nationBelowLimit++;
				}
			}
			if (nationBelowLimit >= nationBelowLimitPercentage * gd.numNations) {
				/* lost the game */
				menuText[TEXT_STANDARD] = _("Under your command, PHALANX operations have"
					" consistently failed to protect nations."
					" The UN, highly unsatisfied with your performance, has decided to remove"
					" you from command and subsequently disbands the PHALANX project as an"
					" effective task force. No further attempts at global cooperation are made."
					" Earth's nations each try to stand alone against the aliens, and eventually"
					" fall one by one.");
				endCampaign = qtrue;
			}
		}
	}

	if (endCampaign) {
		CP_EndCampaign(qfalse);
	}
}

/* Initial fraction of the population in the country where a mission has been lost / won */
#define XVI_LOST_START_PERCENTAGE	0.20f
#define XVI_WON_START_PERCENTAGE	0.05f

/**
 * @brief Updates each nation's happiness and mission win/loss stats.
 * Should be called at the completion or expiration of every mission.
 * The nation where the mission took place will be most affected,
 * surrounding nations will be less affected.
 */
static void CL_HandleNationData (qboolean lost, int civiliansSurvived, int civiliansKilled, int aliensSurvived, int aliensKilled, actMis_t * mis)
{
	int i, is_on_Earth = 0;
	int civilianSum = civiliansKilled + civiliansSurvived;
	float civilianRatio = civilianSum ? (float)civiliansSurvived / (float)civilianSum : 0.;
	int alienSum = aliensKilled + aliensSurvived;
	float alienRatio = alienSum ? (float)aliensKilled / (float)alienSum : 0.;
	float performance = civilianRatio * 0.5 + alienRatio * 0.5;
	float delta_happiness;
	float xvi_infection_factor;

	if (lost) {
		stats.missionsLost++;
		ccs.civiliansKilled += civiliansKilled;
	} else
		stats.missionsWon++;

	for (i = 0; i < gd.numNations; i++) {
		nation_t *nation = &gd.nations[i];
		float alienHostile = 1.0f - nation->stats[0].alienFriendly;
		delta_happiness = 0.0f;
		xvi_infection_factor = 1.0f;

		if (lost) {
			if (!Q_strcmp(nation->id, mis->def->nation)) {
				/* Strong negative reaction (happiness_factor must be < 1) */
				delta_happiness = - (1.0f - performance) * nation->stats[0].alienFriendly * nation->stats[0].happiness;
				is_on_Earth++;
				if (ccs.XVISpreadActivated) {
					/* if there wasn't any XVI infection in this country (or small infection), start it */
					if (nation->stats[0].xvi_infection < XVI_LOST_START_PERCENTAGE)
						nation->stats[0].xvi_infection = XVI_LOST_START_PERCENTAGE;
					else
						/* increase infection by 50% */
						xvi_infection_factor = 1.50f;
				}
			} else {
				/* Minor negative reaction (10 times lower than if the failed mission were in the nation) */
				delta_happiness = - 0.1f * (1.0f - performance) * nation->stats[0].alienFriendly * nation->stats[0].happiness;
				/* increase infection by 10% for country already infected */
				xvi_infection_factor = 1.10f;
			}
		} else {
			if (!Q_strcmp(nation->id, mis->def->nation)) {
				/* Strong positive reaction (happiness_factor must be > 1) */
				delta_happiness = performance * alienHostile * (1.0f - nation->stats[0].happiness);
				is_on_Earth++;
				if (ccs.XVISpreadActivated) {
					/* if there wasn't any XVI infection in this country, start it */
					if (nation->stats[0].xvi_infection < XVI_WON_START_PERCENTAGE)
						nation->stats[0].xvi_infection = XVI_WON_START_PERCENTAGE;
					else
						/* increase infection by 10% */
						xvi_infection_factor = 1.10f;
				}
			} else {
				/* Minor positive reaction (10 times lower than if the mission were in the nation) */
				delta_happiness = 0.1f * performance * alienHostile * (1.0f - nation->stats[0].happiness);
				/* No spreading of XVI infection in other nations */
			}
				/* the happiness you can gain depends on the difficulty of the campaign */
				delta_happiness *= (0.2f + pow(4.0f - difficulty->integer, 2) / 32.0f);
		}

		/* update happiness */
		nation->stats[0].happiness += delta_happiness;
		/* Nation happiness is between 0 and 1 */
		if (nation->stats[0].happiness > 1.0f)
			nation->stats[0].happiness = 1.0f;
		else if (nation->stats[0].happiness < 0.0f)
			nation->stats[0].happiness = 0.0f;

		/* update xvi_infection value (note that there will be an effect only
		 * on nations where at least one mission already took place (for others, nation->stats[0].xvi_infection = 0) */
		if (ccs.XVISpreadActivated) {
			/* @todo: Send mails about critical rates */
			nation->stats[0].xvi_infection *= xvi_infection_factor;
			if (nation->stats[0].xvi_infection > 1.0f)
				nation->stats[0].xvi_infection = 1.0f;
		}
	}
	if (!is_on_Earth)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Warning, mission '%s' located in an unknown country '%s'.\n", mis->def->name, mis->def->nation);
	else if (is_on_Earth > 1)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Error, mission '%s' located in many countries '%s'.\n", mis->def->name, mis->def->nation);
}

/**
 * @brief Deletes employees from a base along with all base equipment.
 * Called when invading forces overrun a base after a base-attack mission
 * @param[in] base base_t base to be ransacked
 */
void CL_BaseRansacked (base_t *base)
{
	int item, ac;

	if (!base)
		return;

	/* Delete all employees from the base & the global list. */
	E_DeleteAllEmployees(base);

	/* Destroy all items in storage */
	for (item = 0; item < csi.numODs; item++)
		/* reset storage and capacity */
		B_UpdateStorageAndCapacity(base, item, 0, qtrue, qfalse);

	/* Remove all aircraft from the base. */
	for (ac = base->numAircraftInBase - 1; ac >= 0; ac--)
		AIR_DeleteAircraft(&base->aircraft[ac]);

	/* @todo: Maybe reset research in progress. ... needs playbalance
	 * need another value in technology_t to remember researched
	 * time from other bases?
	 * @todo: Destroy (or better: just damage) some random buildings. */

	Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Your base: %s has been ransacked! All employees killed and all equipment destroyed."), base->name);
	MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
	CL_GameTimeStop();
}


/**
 * @sa CL_CampaignAddMission
 * @sa CL_CampaignExecute
 * @sa UFO_CampaignCheckEvents
 * @sa CL_CampaignRemoveMission
 * @sa CL_HandleNationData
 */
static void CP_CheckEvents (void)
{
	stageState_t *stage = NULL;
	setState_t *set = NULL;
	actMis_t *mis = NULL;
	base_t *base = NULL;

	int i, j;

	/* check campaign events */
	for (i = 0, stage = ccs.stage; i < numStages; i++, stage++)
		if (stage->active)
			for (j = 0, set = &ccs.set[stage->def->first]; j < stage->def->num; j++, set++)
				if (set->active) {
					if (set->event.day && Date_LaterThan(ccs.date, set->event)) {
						if (set->def->numMissions) {
							CL_CampaignAddMission(set);
							if (gd.mapAction == MA_NONE) {
								gd.mapAction = MA_INTERCEPT;
								CL_AircraftList_f();
							}
						} else
							CL_CampaignExecute(set);
					}
				}

	/* Check UFOs events. */
	UFO_CampaignCheckEvents();

	/* Let missions expire. */
	for (i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++)
		if (!mis->def->noExpire && mis->expire.day && Date_LaterThan(ccs.date, mis->expire)) {
			/* Mission is expired. Calculating penalties for the various mission types. */
			switch (mis->def->missionType) {
			case MIS_BASEATTACK:
				/* Base attack mission never attended to, so
				 * invaders had plenty of time to ransack it */
				base = (base_t*)mis->def->data;
				CL_HandleNationData(qtrue, 0, mis->def->civilians, mis->def->aliens, 0, mis);
				CL_BaseRansacked(base);
				break;
			case MIS_TERRORATTACK:
				/* Normal ground mission. */
				CL_HandleNationData(qtrue, 0, mis->def->civilians, mis->def->aliens, 0, mis);
				if (!mis->def->storyRelated || mis->def->played) {
					Q_strncpyz(messageBuffer, va(ngettext("The mission %s expired and %i civilian died.", "The mission %s expired and %i civilians died.", mis->def->civilians), mis->def->name, mis->def->civilians), MAX_MESSAGE_TEXT);
					MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
				}
				break;
			default:
				Sys_Error("Unknown missionType for '%s'\n", mis->def->name);
				break;
			}

			CP_CheckLostCondition(qtrue, mis->def, mis->def->civilians);

			/* don't remove missions that were not played but were spawned
			 * (e.g. they expired) and have the storyRelated flag set */
			if (mis->def->storyRelated && !mis->def->played) {
				date_t date = {7, 0};
				Q_strncpyz(messageBuffer, va(ngettext("The aliens had enough time to kill %i civilian at %s.", "The aliens had enough time to kill %i civilians at %s.", mis->def->civilians), mis->def->civilians, mis->def->location), MAX_MESSAGE_TEXT);
				MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);

				/* nation's happiness decreases */
				for (j = 0; j < gd.numNations; j++) {
					nation_t *nation = &gd.nations[j];
					if (!Q_strcmp(nation->id, mis->def->nation)) {
						nation->stats[0].happiness *= .85f;
						break;
					}
				}

				/* FIXME use set->def->expire */
				mis->expire = Date_Add(ccs.date, Date_Random_Middle(date));
				CL_GameTimeStop();
				continue;
			}

			/* Remove mission from the map. */
			if (mis->cause->def->number && mis->cause->num >= mis->cause->def->number)
				CL_CampaignExecute(mis->cause);
			CL_CampaignRemoveMission(mis);
		}
}

static const int monthLength[MONTHS_PER_YEAR] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief Converts a number of days into the number of the current month and the current day in this month.
 * @note The seconds from "date" are ignored here.
 * @note The function always starts calculation from Jan. and also catches new years.
 * @param[in] date Contains the number of days to be converted.
 * @param[out] month The month.
 * @param[out] day The day in the month above.
 */
void CL_DateConvert (const date_t * date, int *day, int *month)
{
	int i, d;

	/* Get the day in the year. Other years are ignored. */
	d = date->day % DAYS_PER_YEAR;

	/* Subtract days until no full month is left. */
	for (i = 0; d >= monthLength[i]; i++)
		d -= monthLength[i];

	/* Prepare return values. */
	*day = d + 1;
	*month = i;
}

/**
 * @brief Returns the monatname to the given month index
 * @param[in] month The month index - starts at 0 - ends at 11
 * @return month name as char*
 */
const char *CL_DateGetMonthName (int month)
{
	switch (month) {
	case 0:
		return _("Jan");
	case 1:
		return _("Feb");
	case 2:
		return _("Mar");
	case 3:
		return _("Apr");
	case 4:
		return _("May");
	case 5:
		return _("Jun");
	case 6:
		return _("Jul");
	case 7:
		return _("Aug");
	case 8:
		return _("Sep");
	case 9:
		return _("Oct");
	case 10:
		return _("Nov");
	case 11:
		return _("Dec");
	default:
		return "Error";
	}
}

/**
 * @brief Translates the nation happiness float value to a string
 * @param[in] nation
 * @return Translated happiness string
 */
static const char* CL_GetNationHappinessString (const nation_t* nation)
{
	if (nation->stats[0].happiness < 0.015)
		return _("Giving up");
	else if (nation->stats[0].happiness < 0.025)
		return _("Furious");
	else if (nation->stats[0].happiness < 0.04)
		return _("Angry");
	else if (nation->stats[0].happiness < 0.06)
		return _("Mad");
	else if (nation->stats[0].happiness < 0.10)
		return _("Upset");
	else if (nation->stats[0].happiness < 0.20)
		return _("Tolerant");
	else if (nation->stats[0].happiness < 0.30)
		return _("Neutral");
	else if (nation->stats[0].happiness < 0.50)
		return _("Content");
	else if (nation->stats[0].happiness < 0.70)
		return _("Pleased");
	else if (nation->stats[0].happiness < 1.0)
		return _("Happy");
	else
		return _("Exuberant");
}

/**
 * @brief Get the actual funding of a nation
 * @param[in] nation Pointer to the nation
 * @param[in] month idx of the month -- 0 for current month (sa nation_t)
 * @return actual funding of a nation
 * @sa CL_NationsMaxFunding
 */
static int CL_GetNationFunding (const nation_t* const nation, int month)
{
	return nation->maxFunding * nation->stats[month].happiness;
}

/**
 * @brief Update the nation data from all parsed nation each month
 * @note give us nation support by:
 * * credits
 * * new soldiers
 * * new scientists
 * @note Called from CL_CampaignRun
 * @sa CL_CampaignRun
 * @sa B_CreateEmployee
 */
static void CL_HandleBudget (void)
{
	int i, j;
	char message[1024];
	int funding;
	int cost;
	nation_t *nation;
	int initial_credits = ccs.credits;
	int new_scientists, new_medics, new_soldiers, new_workers;

	for (i = 0; i < gd.numNations; i++) {
		nation = &gd.nations[i];
		funding = CL_GetNationFunding(nation, 0);
		CL_UpdateCredits(ccs.credits + funding);

		new_scientists = new_medics = new_soldiers = new_workers = 0;

		for (j = 0; 0.25 + j < (float) nation->maxScientists * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
			/* Create a scientist, but don't auto-hire her. */
			E_CreateEmployee(EMPL_SCIENTIST);
			new_scientists++;
		}

		for (j = 0; 0.25 + j * 3 < (float) nation->maxScientists * nation->stats[0].happiness; j++) {
			/* Create a medic. */
			E_CreateEmployee(EMPL_MEDIC);
			new_medics++;
		}

		for (j = 0; 0.25 + j < (float) nation->maxSoldiers * nation->stats[0].happiness * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
			/* Create a soldier. */
			E_CreateEmployee(EMPL_SOLDIER);
			new_soldiers++;
		}

		for (j = 0; 0.25 + j * 2 < (float) nation->maxSoldiers * nation->stats[0].happiness; j++) {
			/* Create a worker. */
			E_CreateEmployee(EMPL_WORKER);
			new_workers++;
		}

		Com_sprintf(message, sizeof(message), _("Gained %i %s, %i %s, %i %s, %i %s, and %i %s from nation %s (%s)"),
					funding, ngettext("credit", "credits", funding),
					new_scientists, ngettext("scientist", "scientists", new_scientists),
					new_medics, ngettext("medic", "medics", new_medics),
					new_soldiers, ngettext("soldier", "soldiers", new_soldiers),
					new_workers, ngettext("worker", "workers", new_workers),
					_(nation->name), CL_GetNationHappinessString(nation));
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		if (gd.employees[EMPL_SOLDIER][i].hired)
			cost += SALARY_SOLDIER_BASE + gd.employees[EMPL_SOLDIER][i].chr.rank * SALARY_SOLDIER_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to soldiers"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_WORKER]; i++) {
		if (gd.employees[EMPL_WORKER][i].hired)
			cost += SALARY_WORKER_BASE + gd.employees[EMPL_WORKER][i].chr.rank * SALARY_WORKER_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to workers"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_SCIENTIST]; i++) {
		if (gd.employees[EMPL_SCIENTIST][i].hired)
			cost += SALARY_SCIENTIST_BASE + gd.employees[EMPL_SCIENTIST][i].chr.rank * SALARY_SCIENTIST_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to scientists"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_MEDIC]; i++) {
		if (gd.employees[EMPL_MEDIC][i].hired)
			cost += SALARY_MEDIC_BASE + gd.employees[EMPL_MEDIC][i].chr.rank * SALARY_MEDIC_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to medics"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		if (gd.employees[EMPL_ROBOT][i].hired)
			cost += SALARY_ROBOT_BASE + gd.employees[EMPL_ROBOT][i].chr.rank * SALARY_ROBOT_RANKBONUS;
	}

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for robots"), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	cost = 0;
	for (i = 0; i < gd.numBases; i++) {
		for (j = 0; j < gd.bases[i].numAircraftInBase; j++) {
			cost += gd.bases[i].aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for aircraft"), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	for (i = 0; i < gd.numBases; i++) {
		cost = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			cost += gd.buildings[i][j].varCosts;
		}

		Com_sprintf(message, sizeof(message), _("Paid %i credits for upkeep of base %s"), cost, gd.bases[i].name);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
		CL_UpdateCredits(ccs.credits - cost);
	}

	cost = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_MEDIC] * SALARY_ADMIN_MEDIC + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
	Com_sprintf(message, sizeof(message), _("Paid %i credits for administrative overhead."), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	if (initial_credits < 0) {
		float interest = initial_credits * SALARY_DEBT_INTEREST;

		cost = (int)ceil(interest);
		Com_sprintf(message, sizeof(message), _("Paid %i credits in interest on your debt."), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}
	CL_GameTimeStop();
}

/**
 * @brief sets market prices at start of the game
 * @sa CL_GameInit
 * @sa BS_Load (Market load function)
 * @param[in] load It this an attempt to init the market for a savegame?
 */
static void CL_CampaignInitMarket (qboolean load)
{
	int i;
	equipDef_t *ed;

	assert(curCampaign);

	/* find the relevant market */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(curCampaign->market, ed->name, MAX_VAR)) {
			curCampaign->marketDef = ed;
			break;
		}

	if (!curCampaign->marketDef)
		Sys_Error("CL_CampaignInitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
			curCampaign->id, curCampaign->market);

	/* the savegame loading process will get the following values from savefile */
	if (load)
		return;

	for (i = 0; i < csi.numODs; i++) {
		if (ccs.eMarket.ask[i] == 0) {
			ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i]*BID_FACTOR);
		}

		if (!ed->num[i])
			continue;
		if (!RS_ItemIsResearched(csi.ods[i].id))
			Com_Printf("CL_CampaignInitMarket: Could not add item %s to the market - not marked as researched\n", csi.ods[i].id);
		else
			/* the other relevant values were already set in CL_CampaignInitMarket */
			ccs.eMarket.num[i] = ed->num[i];
	}
}

/**
 * @brief simulates one hour of supply and demand on the market (adds items and sets prices)
 * @sa CL_CampaignRun
 * @sa CL_GameNew_f
 */
static void CL_CampaignRunMarket (void)
{
	int i, market_action;
	double research_factor, price_factor, curr_supp_diff;
	/* supply and demand */
	const double mrs1 = 0.1, mpr1 = 400, mrg1 = 0.002, mrg2 = 0.03;

	assert(curCampaign->marketDef);

	/* @todo: Find out why there is a 2 days discrepancy in reasearched_date*/
	for (i = 0; i < csi.numODs; i++) {
		if (RS_ItemIsResearched(csi.ods[i].id)) {
			/* supply balance */
			technology_t *tech = RS_GetTechByProvided(csi.ods[i].id);
			int reasearched_date;
			if (!tech)
				Sys_Error("No tech that provides '%s'\n", csi.ods[i].id);
			reasearched_date = tech->researchedDateDay + tech->researchedDateMonth * 30 +  tech->researchedDateYear * DAYS_PER_YEAR - 2;
			if (reasearched_date <= curCampaign->date.sec / 86400 + curCampaign->date.day)
				reasearched_date -= 100;
			research_factor = mrs1 * sqrt(ccs.date.day - reasearched_date);
			price_factor = mpr1 / sqrt(csi.ods[i].price+1);
			curr_supp_diff = floor(research_factor*price_factor - ccs.eMarket.num[i]);
			if (curr_supp_diff != 0)
				ccs.eMarket.cumm_supp_diff[i] += curr_supp_diff;
			else
				ccs.eMarket.cumm_supp_diff[i] *= 0.9;
			market_action = floor(mrg1 * ccs.eMarket.cumm_supp_diff[i] + mrg2 * curr_supp_diff);
			ccs.eMarket.num[i] += market_action;
			if (ccs.eMarket.num[i] < 0)
				ccs.eMarket.num[i] = 0;

			/* set item price based on supply imbalance */
			if (research_factor*price_factor >= 1)
				ccs.eMarket.ask[i] = floor(csi.ods[i].price * (1 - (1 - BID_FACTOR) / 2 * (1 / (1 + exp(curr_supp_diff / (research_factor * price_factor))) * 2 - 1)) );
			else
				ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i] * BID_FACTOR);
		}
	}
}

/**
 * @brief Called every frame when we are in geoscape view
 * @note Called for node types MN_MAP and MN_3DMAP
 * @sa MN_DrawMenus
 * @sa CL_HandleBudget
 * @sa B_UpdateBaseData
 * @sa CL_CampaignRunAircraft
 * @sa CP_CheckEvents
 */
void CL_CampaignRun (void)
{
	/* advance time */
	ccs.timer += cls.frametime * gd.gameTimeScale;
	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int dt, day, month, currenthour;

		dt = (int)floor(ccs.timer);
		currenthour = (int)floor(ccs.date.sec / 3600);
		ccs.date.sec += dt;
		ccs.timer -= dt;

		/* compute hourly events  */
		/* (this may run multiple times if the time stepping is > 1 hour at a time) */
		while (currenthour < (int)floor(ccs.date.sec / 3600)) {
			currenthour++;
			CL_CheckResearchStatus();
			PR_ProductionRun();
			CL_CampaignRunMarket();
			UFO_Recovery();
			AII_UpdateInstallationDelay();
			TR_TransferCheck();
		}

		/* daily events */
		while (ccs.date.sec > 3600 * 24) {
			ccs.date.sec -= 3600 * 24;
			ccs.date.day++;
			/* every day */
			B_UpdateBaseData();
			HOS_HospitalRun();
			BDEF_ReloadBattery();
		}

		/* check for campaign events */
		CL_CampaignRunAircraft(dt);
		UFO_CampaignRunUfos(dt);
		AIRFIGHT_CampaignRunBaseDefense(dt);
		CP_CheckEvents();
		CP_CheckLostCondition(qtrue, NULL, 0);
		AIRFIGHT_CampaignRunProjectiles(dt);

		/* set time cvars */
		CL_DateConvert(&ccs.date, &day, &month);
		/* every first day of a month */
		if (day == 1 && gd.fund != qfalse && gd.numBases) {
			CL_BackupMonthlyData();	/* Back up monthly data. */
			CL_HandleBudget();
			gd.fund = qfalse;
		} else if (day > 1)
			gd.fund = qtrue;

		UP_GetUnreadMails();
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("%i %s %02i"), ccs.date.day / DAYS_PER_YEAR, CL_DateGetMonthName(month), day);
		Cvar_Set("mn_mapdate", messageBuffer);
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("%02i:%02i"), ccs.date.sec / 3600, ((ccs.date.sec % 3600) / 60));
		Cvar_Set("mn_maptime", messageBuffer);
	}
}


/* =========================================================== */

typedef struct gameLapse_s {
	const char name[16];
	int scale;
} gameLapse_t;

#define NUM_TIMELAPSE 6

/** @brief The possible geoscape time intervalls */
static const gameLapse_t lapse[NUM_TIMELAPSE] = {
	{"5 sec", 5},
	{"5 mins", 5 * 60},
	{"1 hour", 60 * 60},
	{"12 hour", 12 * 3600},
	{"1 day", 24 * 3600},
	{"5 days", 5 * 24 * 3600}
};

static int gameLapse;

/**
 * @brief Stop game time speed
 */
void CL_GameTimeStop (void)
{
	/* don't allow time scale in tactical mode */
	if (!CL_OnBattlescape()) {
		gameLapse = 0;
		Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
		gd.gameTimeScale = lapse[gameLapse].scale;
	}
}


/**
 * @brief Decrease game time speed
 *
 * Decrease game time speed - only works when there is already a base available
 */
void CL_GameTimeSlow (void)
{
	/* don't allow time scale in tactical mode */
	if (!CL_OnBattlescape()) {
		/*first we have to set up a home base */
		if (!gd.numBases)
			CL_GameTimeStop();
		else {
			if (gameLapse > 0)
				gameLapse--;
			Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
			gd.gameTimeScale = lapse[gameLapse].scale;
		}
	}
}

/**
 * @brief Increase game time speed
 *
 * Increase game time speed - only works when there is already a base available
 */
void CL_GameTimeFast (void)
{
	/* don't allow time scale in tactical mode */
	if (!CL_OnBattlescape()) {
		/*first we have to set up a home base */
		if (!gd.numBases)
			CL_GameTimeStop();
		else {
			if (gameLapse < NUM_TIMELAPSE - 1)
				gameLapse++;
			Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
			gd.gameTimeScale = lapse[gameLapse].scale;
		}
	}
}

#define MAX_CREDITS 10000000
/**
 * @brief Sets credits and update mn_credits cvar
 *
 * Checks whether credits are bigger than MAX_CREDITS
 */
void CL_UpdateCredits (int credits)
{
	/* credits */
	if (credits > MAX_CREDITS)
		credits = MAX_CREDITS;
	ccs.credits = credits;
	Cvar_Set("mn_credits", va(_("%i c"), ccs.credits));
}

static void CP_NationStatsClick_f (void)
{
	int num;

	if (!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* Which entry in the list? */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= gd.numNations)
		return;

	MN_PushMenu("nations");
	Cbuf_AddText(va("nation_select %i;", num));
}

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 */
static void CL_StatsUpdate_f (void)
{
	char *pos;
	static char statsBuffer[MAX_STATS_BUFFER];
	int hired[MAX_EMPL];
	int i = 0, j, costs = 0, sum = 0;

	/* delete buffer */
	memset(statsBuffer, 0, sizeof(statsBuffer));
	memset(hired, 0, sizeof(hired));

	pos = statsBuffer;

	/* missions */
	menuText[TEXT_STATS_1] = pos;
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), stats.missionsWon, stats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	menuText[TEXT_STATS_2] = pos;
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
		stats.basesBuild, gd.numBases, stats.basesAttacked),

	/* nations */
	pos += (strlen(pos) + 1);
	menuText[TEXT_STATS_3] = pos;
	for (i = 0; i < gd.numNations; i++) {
		Q_strcat(pos, va(_("%s\t%s\n"), _(gd.nations[i].name), CL_GetNationHappinessString(&gd.nations[i])), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs */
	for (i = 0; i < gd.numEmployees[EMPL_SCIENTIST]; i++) {
		if (gd.employees[EMPL_SCIENTIST][i].hired) {
			costs += SALARY_SCIENTIST_BASE + gd.employees[EMPL_SCIENTIST][i].chr.rank * SALARY_SCIENTIST_RANKBONUS;
			hired[EMPL_SCIENTIST]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		if (gd.employees[EMPL_SOLDIER][i].hired) {
			costs += SALARY_SOLDIER_BASE + gd.employees[EMPL_SOLDIER][i].chr.rank * SALARY_SOLDIER_RANKBONUS;
			hired[EMPL_SOLDIER]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_WORKER]; i++) {
		if (gd.employees[EMPL_WORKER][i].hired) {
			costs += SALARY_WORKER_BASE + gd.employees[EMPL_WORKER][i].chr.rank * SALARY_WORKER_RANKBONUS;
			hired[EMPL_WORKER]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_MEDIC]; i++) {
		if (gd.employees[EMPL_MEDIC][i].hired) {
			costs += SALARY_MEDIC_BASE + gd.employees[EMPL_MEDIC][i].chr.rank * SALARY_MEDIC_RANKBONUS;
			hired[EMPL_MEDIC]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		if (gd.employees[EMPL_ROBOT][i].hired) {
			costs += SALARY_ROBOT_BASE + gd.employees[EMPL_ROBOT][i].chr.rank * SALARY_ROBOT_RANKBONUS;
			hired[EMPL_ROBOT]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	menuText[TEXT_STATS_4] = pos;
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	menuText[TEXT_STATS_5] = pos;
	Q_strcat(pos, va(_("Employees:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	costs = 0;
	for (i = 0; i < gd.numBases; i++) {
		for (j = 0; j < gd.bases[i].numAircraftInBase; j++) {
			costs += gd.bases[i].aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}
	Q_strcat(pos, va(_("Aircraft:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	for (i = 0; i < gd.numBases; i++) {
		costs = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			costs += gd.buildings[i][j].varCosts;
		}
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), gd.bases[i].name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_MEDIC] * SALARY_ADMIN_MEDIC + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
	Q_strcat(pos, va(_("Administrative costs:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	if (ccs.credits < 0) {
		float interest = ccs.credits * SALARY_DEBT_INTEREST;

		costs = (int)ceil(interest);
		Q_strcat(pos, va(_("Debt:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}
	Q_strcat(pos, va(_("\n\t-------\nSum:\t%i c\n"), sum), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* campaign */
	pos += (strlen(pos) + 1);
	menuText[TEXT_GENERIC] = pos;
	Q_strcat(pos, va(_("Max. allowed debts: %ic\n"), curCampaign->negativeCreditsUntilLost),
		(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* only show the xvi spread data when it's available */
	if (ccs.XVISpreadActivated) {
		Q_strcat(pos, va(_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			curCampaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate()),
			(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}
}

static screenPoint_t fundingPts[MAX_NATIONS][MONTHS_PER_YEAR]; /* Space for month-lines with 12 points for each nation. */
static int usedFundPtslist = 0;
static screenPoint_t colorLinePts[MAX_NATIONS][2]; /* Space for 1 line (2 points) for each nation. */
static int usedColPtslists = 0;

static screenPoint_t coordAxesPts[3];	/* Space for 2 lines (3 points) */

static const vec4_t graphColors[MAX_NATIONS] = {
	{1.0, 0.5, 0.5, 1.0},
	{0.5, 1.0, 0.5, 1.0},
	{0.5, 0.5, 1.0, 1.0},
	{1.0, 0.0, 0.0, 1.0},
	{0.0, 1.0, 0.0, 1.0},
	{0.0, 0.0, 1.0, 1.0},
	{1.0, 1.0, 0.0, 1.0},
	{0.0, 1.0, 1.0, 1.0}
};
static const vec4_t graphColorSelected = {1, 1, 1, 1};

/**
 * @brief Search the maximum (current) funding from all the nations (in all logged months).
 * @note nation->maxFunding is _not_ the real funding value.
 * @return The maximum funding value.
 * @todo Extend to other values?
 * @sa CL_GetNationFunding
 */
static int CL_NationsMaxFunding (void)
{
	nation_t *nation = NULL;
	int m, n;
	int funding;
	int max = 0;

	for (n = 0; n < gd.numNations; n++) {
		nation = &gd.nations[n];
		for (m = 0; m < MONTHS_PER_YEAR; m++) {
			if (nation->stats[m].inuse) {
				/** nation->stats[m].happiness = sqrt((float)m / 12.0);  @todo  DEBUG */
				funding = CL_GetNationFunding(nation, m);
				if (max < funding)
					max = funding;
			} else {
				/* Abort this months-loops */
				break;
			}
		}
	}
	return max;
}

/**
 * @brief Draws a graph for the funding values over time.
 * @param[in] nation The nation to draw the graph for.
 * @param[in] node A pointer to a "linestrip" node that we want to draw in.
 * @param[in] maxFunding The upper limit of the graph - all values willb e scaled to this one.
 * @todo Somehow the display of the months isnt really correct right now (straight line :-/)
 */
static void CL_NationDrawStats (nation_t *nation, menuNode_t *node, int maxFunding, int color)
{
	int width, height, x, y, dx;
	int m;

	int minFunding = 0;
	int funding;
	int ptsNumber = 0;

	if (!nation || !node)
		return;

	width	= (int)node->size[0];
	height	= (int)node->size[1];
	x = node->pos[0];
	y = node->pos[1];
	dx = (int)(width / MONTHS_PER_YEAR);

	if (minFunding == maxFunding) {
		Com_Printf("CL_NationDrawStats: Given maxFunding value is the same as minFunding (min=%i, max=%i) - abort.\n", minFunding, maxFunding);
		return;
	}

	/* Generate pointlist. */
	/** @todo Sort this in reverse? -> Having current month on the right side? */
	for (m = 0; m < MONTHS_PER_YEAR; m++) {
		if (nation->stats[m].inuse) {
			funding = CL_GetNationFunding(nation, m);
			fundingPts[usedFundPtslist][m].x = x + (m * dx);
			fundingPts[usedFundPtslist][m].y = y - height * (funding - minFunding) / (maxFunding - minFunding);
			ptsNumber++;
		} else {
			break;
		}
	}

	/* Guarantee displayable data even for only one month */
	if (ptsNumber == 1) {
		/* Set the second point haft the distance to the next month to the right - small horiz line. */
		fundingPts[usedFundPtslist][1].x = fundingPts[usedFundPtslist][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
		fundingPts[usedFundPtslist][1].y = fundingPts[usedFundPtslist][0].y;
		ptsNumber++;
	}

	/* Break if we reached the max strip number. */
	if (node->linestrips.numStrips >= MAX_LINESTRIPS-1)
		return;

	/* Link graph to node */
	node->linestrips.pointList[node->linestrips.numStrips] = (int*)fundingPts[usedFundPtslist];
	node->linestrips.numPoints[node->linestrips.numStrips] = ptsNumber;
	if (color < 0) {
		Vector4Copy(graphColorSelected, node->linestrips.color[node->linestrips.numStrips]);
	} else {
		Vector4Copy(graphColors[color], node->linestrips.color[node->linestrips.numStrips]);
	}
	node->linestrips.numStrips++;

	usedFundPtslist++;
}

static int selectedNation = 0;
/**
 * @brief Shows the current nation list + statistics.
 * @note See menu_stats.ufo
 */
static void CL_NationStatsUpdate_f (void)
{
	int i;
	int funding, maxFunding;
	menuNode_t *colorNode = NULL;
	menuNode_t *graphNode = NULL;
	const vec4_t colorAxes = {1, 1, 1, 0.5};
	int dy = 10;

	usedColPtslists = 0;

	colorNode = MN_GetNodeFromCurrentMenu("nation_graph_colors");
	if (colorNode) {
		dy = (int)(colorNode->size[1] / MAX_NATIONS);
		colorNode->linestrips.numStrips = 0;
	}

	for (i = 0; i < gd.numNations; i++) {
		funding = CL_GetNationFunding(&(gd.nations[i]), 0);

		if (selectedNation == i) {
			Cbuf_AddText(va("nation_marksel%i;",i));
		} else {
			Cbuf_AddText(va("nation_markdesel%i;",i));
		}
		Cvar_Set(va("mn_nat_name%i",i), _(gd.nations[i].name));
		Cvar_Set(va("mn_nat_fund%i",i), va("%i", funding));

		if (colorNode) {
			colorLinePts[usedColPtslists][0].x = colorNode->pos[0];
			colorLinePts[usedColPtslists][0].y = colorNode->pos[1] - (int)colorNode->size[1] + dy * i;
			colorLinePts[usedColPtslists][1].x = colorNode->pos[0] + (int)colorNode->size[0];
			colorLinePts[usedColPtslists][1].y = colorLinePts[usedColPtslists][0].y;

			colorNode->linestrips.pointList[colorNode->linestrips.numStrips] = (int*)colorLinePts[usedColPtslists];
			colorNode->linestrips.numPoints[colorNode->linestrips.numStrips] = 2;

			if (i == selectedNation) {
				Vector4Copy(graphColorSelected, colorNode->linestrips.color[colorNode->linestrips.numStrips]);
			} else {
				Vector4Copy(graphColors[i], colorNode->linestrips.color[colorNode->linestrips.numStrips]);
			}

			usedColPtslists++;
			colorNode->linestrips.numStrips++;
		}
	}

	/* Hide unused nation-entries. */
	for (i = gd.numNations; i < MAX_NATIONS; i++) {
		Cbuf_AddText(va("nation_hide%i;",i));
	}

	/** @todo Display summary of nation info */

	/* Display graph of nations-values so far. */
	graphNode = MN_GetNodeFromCurrentMenu("nation_graph_funding");
	if (graphNode) {
		usedFundPtslist = 0;
		graphNode->linestrips.numStrips = 0;

		/* Generate axes & link to node. */
		/** @todo Maybe create a margin toward the axes? */
		coordAxesPts[0].x = graphNode->pos[0];	/* x */
		coordAxesPts[0].y = graphNode->pos[1] - (int)graphNode->size[1];	/* y - height */
		coordAxesPts[1].x = graphNode->pos[0];	/* x */
		coordAxesPts[1].y = graphNode->pos[1];	/* y */
		coordAxesPts[2].x = graphNode->pos[0] + (int)graphNode->size[0];	/* x + width */
		coordAxesPts[2].y = graphNode->pos[1];	/* y */
		graphNode->linestrips.pointList[graphNode->linestrips.numStrips] = (int*)coordAxesPts;
		graphNode->linestrips.numPoints[graphNode->linestrips.numStrips] = 3;
		Vector4Copy(colorAxes, graphNode->linestrips.color[graphNode->linestrips.numStrips]);
		graphNode->linestrips.numStrips++;

		maxFunding = CL_NationsMaxFunding();
		for (i = 0; i < gd.numNations; i++) {
			if (i == selectedNation) {
				CL_NationDrawStats(&gd.nations[i], graphNode, maxFunding, -1);
			} else {
				CL_NationDrawStats(&gd.nations[i], graphNode, maxFunding, i);
			}
		}
	}
}

/**
 * @brief Select nation and display all relevant information for it.
 */
static void CL_NationSelect_f (void)
{
	int nat;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nat_idx>\n", Cmd_Argv(0));
		return;
	}

	nat = atoi(Cmd_Argv(1));
	if (nat < 0 || nat >=  gd.numNations) {
		Com_Printf("Invalid nation index: %is\n",nat);
		return;
	}

	selectedNation = nat;
	CL_NationStatsUpdate_f();
}

/**
 * @brief Load callback for campaign data
 * @sa CP_Save
 * @sa SAV_GameSave
 * @sa CP_SpawnBaseAttackMission
 * @sa CP_SpawnCrashSiteMission
 */
qboolean CP_Load (sizebuf_t *sb, void *data)
{
	actMis_t *mis;
	stageState_t *state;
	setState_t *set;
	setState_t dummy;
	const char *name, *selectedMission;
	int i, j, num;
	int misType;
	char val[32];
	base_t *base;

	/* read campaign name */
	name = MSG_ReadString(sb);

	for (i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++)
		if (!Q_strncmp(name, curCampaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("......campaign \"%s\" doesn't exist.\n", name);
		curCampaign = NULL;
		return qfalse;
	}

	CL_GameInit(qtrue);

	Com_sprintf(val, sizeof(val), "%i", curCampaign->difficulty);
	Cvar_ForceSet("difficulty", val);

	/* init the map images and reset the map actions */
	MAP_Init();

	memset(&ccs, 0, sizeof(ccs_t));
	ccs.singleplayer = qtrue;

	ccs.angles[YAW] = GLOBE_ROTATE;

	gd.fund = MSG_ReadByte(sb);
	gd.nextUCN = MSG_ReadShort(sb);

	/* read date */
	ccs.date.day = MSG_ReadLong(sb);
	ccs.date.sec = MSG_ReadLong(sb);

	/* read map view */
	ccs.center[0] = MSG_ReadFloat(sb);
	ccs.center[1] = MSG_ReadFloat(sb);
	ccs.zoom = MSG_ReadFloat(sb);

	Q_strncpyz(gd.oldMis1, MSG_ReadString(sb), sizeof(gd.oldMis1));
	Q_strncpyz(gd.oldMis2, MSG_ReadString(sb), sizeof(gd.oldMis2));
	Q_strncpyz(gd.oldMis3, MSG_ReadString(sb), sizeof(gd.oldMis3));

	/* read credits */
	CL_UpdateCredits(MSG_ReadLong(sb));

	/* read other campaign data */
	ccs.civiliansKilled = MSG_ReadShort(sb);
	ccs.aliensKilled = MSG_ReadShort(sb);
	ccs.XVISpreadActivated = MSG_ReadByte(sb);
	ccs.humansAttackActivated = MSG_ReadByte(sb);

	/* read campaign data */
	name = MSG_ReadString(sb);
	while (*name) {
		state = CL_CampaignActivateStage(name, qfalse);
		if (!state) {
			Com_Printf("......unable to load campaign, unknown stage '%s'\n", name);
			curCampaign = NULL;
			Cbuf_AddText("mn_pop\n");
			return qfalse;
		}

		state->start.day = MSG_ReadLong(sb);
		state->start.sec = MSG_ReadLong(sb);
		num = MSG_ReadByte(sb);
		if (num != state->def->num)
			Com_Printf("......warning: Different number of sets: savegame: %i, scripts: %i\n", num, state->def->num);
		for (i = 0; i < num; i++) {
			name = MSG_ReadString(sb);
			for (j = 0, set = &ccs.set[state->def->first]; j < state->def->num; j++, set++)
				if (!Q_strncmp(name, set->def->name, MAX_VAR))
					break;
			/* write on dummy set, if it's unknown */
			if (j >= state->def->num) {
				Com_Printf("......warning: Set '%s' not found (%i/%i)\n", name, num, state->def->num);
				set = &dummy;
			}

			if (!set->def->numMissions)
				Com_Printf("......warning: Set with no missions (%s)\n", set->def->name);

			set->active = MSG_ReadByte(sb);
			set->num = MSG_ReadShort(sb);
			set->done = MSG_ReadShort(sb);
			set->start.day = MSG_ReadLong(sb);
			set->start.sec = MSG_ReadLong(sb);
			set->event.day = MSG_ReadLong(sb);
			set->event.sec = MSG_ReadLong(sb);
		}

		/* read next stage name */
		name = MSG_ReadString(sb);
	}

	/* reset team */
	Cvar_Set("team", curCampaign->team);

	/* store active missions */
	num = MSG_ReadByte(sb);
	ccs.numMissions = num;
	for (i = 0; i < num; i++) {
		/* needed because we might add new missions inside
		 * this loop (base attack and crashsites) */
		mis = &ccs.mission[i + ccs.numMissions - num];
		mis->def = NULL;
		mis->cause = NULL;

		/* get mission cause */
		name = MSG_ReadString(sb);

		for (j = 0; j < numStageSets; j++)
			if (!Q_strncmp(name, stageSets[j].name, MAX_VAR)) {
				mis->cause = &ccs.set[j];
				break;
			}
		if (j >= numStageSets)
			Com_Printf("......warning: Stage set '%s' not found\n", name);

		/* get mission definition */
		name = MSG_ReadString(sb);

		misType = MSG_ReadByte(sb);

		/* ignore incomplete info */
		if (!mis->cause) {
			Com_Printf("......warning: Incomplete mission info for mission type %i (name: %s)\n", misType, name);
			return qfalse;
		}

		for (j = 0; j < numMissions; j++)
			if (!Q_strncmp(name, missions[j].name, MAX_VAR)) {
				mis->def = &missions[j];
				break;
			}
		if (j >= numMissions) {
			switch (misType) {
			case MIS_BASEATTACK:
				assert(!mis->def);
				{
					/* Load IDX of base under attack */
					int baseidx = MSG_ReadByte(sb);
					base = &gd.bases[baseidx];
					Vector2Copy(base->pos, mis->realPos);
					if (base->baseStatus != BASE_UNDER_ATTACK)
						Com_Printf("......warning: base %i (%s) is supposedly under attack but base status doesn't match!\n", j, base->name);
					mis->def = CL_AddMission(name);
					if (!mis->def) {
						Com_Printf("......warning: could not load base attack mission '%s'!\n", name);
						return qfalse;
					}
					if (!CP_SpawnBaseAttackMission(base, mis->def, mis->cause)) {
						Com_Printf("......warning: could not spawn base attack mission on geoscape '%s'!\n", name);
						return qfalse;
					}
				}
				break;
			case MIS_CRASHSITE:
				assert(!mis->def);
				{
					const nation_t *nation = NULL;
					/* create a new mission - this mission is not in the campaign definition */
					mis->def = CL_AddMission(name);
					if (!mis->def) {
						CP_RemoveLastMission();
						Com_Printf("......warning: could not load crashsite mission '%s'!\n", name);
						return qfalse;
					}
					mis->def->mapDef = Com_GetMapDefinitionByID("ufocrash");
					if (!mis->def->mapDef) {
						CP_RemoveLastMission();
						Com_Printf("......warning: could not load crashsite mission '%s'!\n", name);
						return qfalse;
					}
					mis->realPos[0] = MSG_ReadFloat(sb);
					mis->realPos[1] = MSG_ReadFloat(sb);
					mis->def->aliens = MSG_ReadByte(sb);
					mis->def->civilians = MSG_ReadByte(sb);
					mis->def->mapDef->loadingscreen = Mem_PoolStrDup(MSG_ReadString(sb), cl_localPool, CL_TAG_NONE);
					Q_strncpyz(mis->def->onwin, MSG_ReadString(sb), sizeof(mis->def->onwin));
					mis->def->mapDef->param = Mem_PoolStrDup(MSG_ReadString(sb), cl_localPool, CL_TAG_NONE);
					/** @sa CP_SpawnCrashSiteMission */
					nation = MAP_GetNation(mis->realPos);
					Com_sprintf(mis->def->type, sizeof(mis->def->type), _("UFO crash site"));
					if (nation) {
						Com_sprintf(mis->def->location, sizeof(mis->def->location), _(nation->name));
						Q_strncpyz(mis->def->civTeam, nation->id, sizeof(mis->def->civTeam));
					} else {
						Com_sprintf(mis->def->location, sizeof(mis->def->location), _("No nation"));
						Q_strncpyz(mis->def->civTeam, "europa", sizeof(mis->def->civTeam));
					}
					mis->def->missionText = "Crashed Alien Ship. Secure the area.";
				}
				break;
			case MIS_TERRORATTACK:
				Com_Printf("......warning: Mission '%s' not found\n", name);
				break;
			default:
				Com_Printf("......warning: Unknown mission type %i for mission %s\n", misType, name);
				return qfalse;
			}
		} else {
			assert(mis);
			assert(misType == MIS_TERRORATTACK);
			mis->realPos[0] = MSG_ReadFloat(sb);
			mis->realPos[1] = MSG_ReadFloat(sb);
		}

		/* ignore incomplete info */
		if (!mis->def) {
			Com_Printf("......warning: Incomplete mission info for mission type %i (name: %s)\n", misType, name);
			return qfalse;
		}

		/* get mission type and location */
		mis->def->missionType = misType;
		mis->def->storyRelated = MSG_ReadByte(sb);
		mis->def->played = MSG_ReadByte(sb);
		mis->def->noExpire = MSG_ReadByte(sb);
		Q_strncpyz(mis->def->location, MSG_ReadString(sb), sizeof(mis->def->location));

		/* read time */
		mis->expire.day = MSG_ReadLong(sb);
		mis->expire.sec = MSG_ReadLong(sb);

		mis->def->onGeoscape = qtrue;
		if (MSG_ReadByte(sb) != 0) {
			Com_Printf("......warning: Sanity check for mission type %i (name: %s) failed\n", misType, mis->def->name);
			return qfalse;
		}
	}

	/* stores the select mission on geoscape */
	selectedMission = MSG_ReadString(sb);
	selMis = NULL;
	if (*selectedMission) {
		for (i = 0; i < ccs.numMissions; i++) {
			if (!Q_strcmp(ccs.mission[i].def->name, selectedMission)) {
				selMis = &ccs.mission[i];
				if (!selMis->def) {
					Com_Printf("......warning: incomplete mission data (%s)\n", name);
					return qfalse;
				}
			}
		}
	}

	/* and now fix the mission pointers for e.g. ufocrash sites
	 * this is needed because the base load function which loads the aircraft
	 * doesn't know anything (at that stage) about the new missions that were
	 * add in this load function */
	for (base = gd.bases, i = 0; i < gd.numBases; i++, base++) {
		for (j = 0; j < base->numAircraftInBase; j++) {
			if (base->aircraft[j].status == AIR_MISSION) {
				assert(base->aircraft[j].missionID);
				for (num = 0; num < ccs.numMissions; num++) {
					if (!Q_strcmp(ccs.mission[num].def->name, base->aircraft[j].missionID)) {
						base->aircraft[j].mission = &ccs.mission[num];
						Mem_Free(base->aircraft[j].missionID);
						break;
					}
				}
				/* not found */
				if (num == ccs.numMissions) {
					Com_Printf("Could not link mission '%s' in aircraft\n", base->aircraft[j].missionID);
					Mem_Free(base->aircraft[j].missionID);
					return qfalse;
				}
			}
		}
	}

	return qtrue;
}

/**
 * @brief Save callback for campaign data
 * @sa CP_Load
 * @sa SAV_GameSave
 */
qboolean CP_Save (sizebuf_t *sb, void *data)
{
	stageState_t *state;
	actMis_t *mis;
	int i, j;
	base_t *base;

	/* store campaign name */
	MSG_WriteString(sb, curCampaign->id);

	MSG_WriteByte(sb, gd.fund);
	MSG_WriteShort(sb, gd.nextUCN);

	/* store date */
	MSG_WriteLong(sb, ccs.date.day);
	MSG_WriteLong(sb, ccs.date.sec);

	/* store map view */
	MSG_WriteFloat(sb, ccs.center[0]);
	MSG_WriteFloat(sb, ccs.center[1]);
	MSG_WriteFloat(sb, ccs.zoom);

	MSG_WriteString(sb, gd.oldMis1);
	MSG_WriteString(sb, gd.oldMis2);
	MSG_WriteString(sb, gd.oldMis3);

	/* store credits */
	MSG_WriteLong(sb, ccs.credits);

	/* store other campaign data */
	MSG_WriteShort(sb, ccs.civiliansKilled);
	MSG_WriteShort(sb, ccs.aliensKilled);
	MSG_WriteByte(sb, ccs.XVISpreadActivated);
	MSG_WriteByte(sb, ccs.humansAttackActivated);

	/* store campaign data */
	for (i = 0, state = ccs.stage; i < numStages; i++, state++)
		if (state->active) {
			/* write head */
			setState_t *set;

			MSG_WriteString(sb, state->def->name);
			MSG_WriteLong(sb, state->start.day);
			MSG_WriteLong(sb, state->start.sec);
			MSG_WriteByte(sb, state->def->num);

			/* write all sets */
			for (j = 0, set = &ccs.set[state->def->first]; j < state->def->num; j++, set++) {
				MSG_WriteString(sb, set->def->name);
				MSG_WriteByte(sb, set->active);
				MSG_WriteShort(sb, set->num);
				MSG_WriteShort(sb, set->done);
				MSG_WriteLong(sb, set->start.day);
				MSG_WriteLong(sb, set->start.sec);
				MSG_WriteLong(sb, set->event.day);
				MSG_WriteLong(sb, set->event.sec);
			}
		}
	/* terminate list */
	MSG_WriteByte(sb, 0);

	/* store active missions */
	MSG_WriteByte(sb, ccs.numMissions);
	for (i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++) {
		MSG_WriteString(sb, mis->cause->def->name);

		MSG_WriteString(sb, mis->def->name);
		MSG_WriteByte(sb, mis->def->missionType);
		switch (mis->def->missionType) {
		case MIS_BASEATTACK:
			/* save IDX of base under attack if required */
			base = (base_t*)mis->def->data;
			assert(base);
			MSG_WriteByte(sb, base->idx);
			break;
		case MIS_TERRORATTACK:
			MSG_WriteFloat(sb, mis->realPos[0]);
			MSG_WriteFloat(sb, mis->realPos[1]);
			break;
		case MIS_CRASHSITE:
			MSG_WriteFloat(sb, mis->realPos[0]);
			MSG_WriteFloat(sb, mis->realPos[1]);
			MSG_WriteByte(sb, mis->def->aliens);
			MSG_WriteByte(sb, mis->def->civilians);
			MSG_WriteString(sb, mis->def->mapDef->loadingscreen);
			MSG_WriteString(sb, mis->def->onwin);
			MSG_WriteString(sb, mis->def->mapDef->param);
			break;
		default:
			Com_Printf("CP_Save: Error - unknown mission type: %i\n", mis->def->missionType);
			return qfalse;
		}
		MSG_WriteByte(sb, mis->def->storyRelated);
		MSG_WriteByte(sb, mis->def->played);
		MSG_WriteByte(sb, mis->def->noExpire);
		MSG_WriteString(sb, mis->def->location);

		MSG_WriteLong(sb, mis->expire.day);
		MSG_WriteLong(sb, mis->expire.sec);
		MSG_WriteByte(sb, 0);
	}

	/* stores the select mission on geoscape */
	if (selMis && selMis->def)
		MSG_WriteString(sb, selMis->def->name);
	else
		MSG_WriteString(sb, "");

	return qtrue;
}


qboolean STATS_Save (sizebuf_t* sb, void* data)
{
	MSG_WriteShort(sb, stats.missionsWon);
	MSG_WriteShort(sb, stats.missionsLost);
	MSG_WriteShort(sb, stats.basesBuild);
	MSG_WriteShort(sb, stats.basesAttacked);
	MSG_WriteShort(sb, stats.interceptions);
	MSG_WriteShort(sb, stats.soldiersLost);
	MSG_WriteShort(sb, stats.soldiersNew);
	MSG_WriteShort(sb, stats.killedAliens);
	MSG_WriteShort(sb, stats.rescuedCivilians);
	MSG_WriteShort(sb, stats.researchedTechnologies);
	MSG_WriteShort(sb, stats.moneyInterceptions);
	MSG_WriteShort(sb, stats.moneyBases);
	MSG_WriteShort(sb, stats.moneyResearch);
	MSG_WriteShort(sb, stats.moneyWeapons);
	return qtrue;
}

qboolean STATS_Load (sizebuf_t* sb, void* data)
{
	stats.missionsWon = MSG_ReadShort(sb);
	stats.missionsLost = MSG_ReadShort(sb);
	stats.basesBuild = MSG_ReadShort(sb);
	stats.basesAttacked = MSG_ReadShort(sb);
	stats.interceptions = MSG_ReadShort(sb);
	stats.soldiersLost = MSG_ReadShort(sb);
	stats.soldiersNew = MSG_ReadShort(sb);
	stats.killedAliens = MSG_ReadShort(sb);
	stats.rescuedCivilians = MSG_ReadShort(sb);
	stats.researchedTechnologies = MSG_ReadShort(sb);
	stats.moneyInterceptions = MSG_ReadShort(sb);
	stats.moneyBases = MSG_ReadShort(sb);
	stats.moneyResearch = MSG_ReadShort(sb);
	stats.moneyWeapons = MSG_ReadShort(sb);
	return qtrue;
}

/**
 * @brief Nation saving callback
 */
qboolean NA_Save (sizebuf_t* sb, void* data)
{
	int i, j;
	for (i = 0; i < presaveArray[PRE_NATION]; i++) {
		for (j = 0; j < MONTHS_PER_YEAR; j++) {
			MSG_WriteByte(sb, gd.nations[i].stats[j].inuse);
			MSG_WriteFloat(sb, gd.nations[i].stats[j].happiness);
			MSG_WriteFloat(sb, gd.nations[i].stats[j].xvi_infection);
			MSG_WriteFloat(sb, gd.nations[i].stats[j].alienFriendly);
		}
	}
	return qtrue;
}

/**
 * @brief Nation loading callback
 */
qboolean NA_Load (sizebuf_t* sb, void* data)
{
	int i, j;

	for (i = 0; i < presaveArray[PRE_NATION]; i++) {
		for (j = 0; j < MONTHS_PER_YEAR; j++) {
			gd.nations[i].stats[j].inuse = MSG_ReadByte(sb);
			gd.nations[i].stats[j].happiness = MSG_ReadFloat(sb);
			gd.nations[i].stats[j].xvi_infection = MSG_ReadFloat(sb);
			gd.nations[i].stats[j].alienFriendly = MSG_ReadFloat(sb);
		}
	}
	return qtrue;
}

/**
 * @brief Set some needed cvars from mission definition
 * @param[in] mission mission definition pointer with the needed data to set the cvars to
 * @sa CL_StartMission_f
 * @sa CL_GameGo
 */
static void CP_SetMissionVars (mission_t* mission)
{
	int i;

	assert(mission);
	assert(mission->mapDef);

	Com_DPrintf(DEBUG_CLIENT, "CP_SetMissionVars:\n");

	/* start the map */
	Cvar_SetValue("ai_numaliens", (float) mission->aliens);
	Cvar_SetValue("ai_numcivilians", (float) mission->civilians);
	Cvar_Set("ai_civilian", mission->civTeam);
	Cvar_Set("ai_equipment", mission->alienEquipment);
	if (mission->mapDef->music) {
		Cvar_Set("snd_music", mission->mapDef->music);
	} else {
		Com_DPrintf(DEBUG_CLIENT, "..mission '%s' doesn't have a music track assigned\n", mission->name);
		Cbuf_AddText("music_randomtrack;");
	}

	/* now store the alien teams in the shared csi struct to let the game dll
	 * have access to this data, too */
	for (i = 0; i < MAX_TEAMS_PER_MISSION; i++)
		csi.alienTeams[i] = mission->alienTeams[i];
	csi.numAlienTeams = mission->numAlienTeams;

	Com_DPrintf(DEBUG_CLIENT, "..numAliens: %i\n..numCivilians: %i\n..alienTeams: '%i'\n..civTeam: '%s'\n..alienEquip: '%s'\n..music: '%s'\n",
		mission->aliens,
		mission->civilians,
		mission->numAlienTeams,
		mission->civTeam,
		mission->alienEquipment,
		mission->mapDef->music);

	mission->played = qtrue;
}

/**
 * @brief Select the mission type and start the map from mission definition
 * @param[in] mission Mission definition to start the map from
 * @sa CL_GameGo
 * @sa CL_StartMission_f
 */
static void CP_StartMissionMap (mission_t* mission)
{
	char expanded[MAX_QPATH];
	char timeChar;
	base_t *bAttack;

	/* prepare */
	MN_PopMenu(qtrue);
	Cvar_Set("mn_main_afterdrop", "singleplayermission");

	/* get appropriate map */
	if (MAP_IsNight(mission->pos))
		timeChar = 'n';
	else
		timeChar = 'd';

	assert(mission->mapDef->map);

	/** @note set the mapZone - this allows us to replace the ground texture
	 * with the suitable terrain texture - just use tex_terrain/dummy for the
	 * brushes you want the terrain textures on
	 * @sa Mod_LoadTexinfo */
	refdef.mapZone = mission->zoneType;

	switch (mission->mapDef->map[0]) {
	case '+':
		Com_sprintf(expanded, sizeof(expanded), "maps/%s%c.ump", mission->mapDef->map + 1, timeChar);
		break;
	/* base attack maps starts with a dot */
	case '.':
		bAttack = (base_t*)mission->data;
		if (!bAttack) {
			/* assemble a random base and set the base status to BASE_UNDER_ATTACK */
			Cbuf_AddText("base_assemble_rand 1;");
			return;
		} else if (bAttack->baseStatus != BASE_UNDER_ATTACK) {
			Com_Printf("Base is not under attack\n");
			return;
		}
		/* check whether there are founded bases */
		if (B_GetFoundedBaseCount() > 0)
			Cbuf_AddText(va("base_assemble %i 1;", bAttack->idx));
		/* quick save is called when base is really assembled
		 * @sa B_AssembleMap_f */
		return;
	default:
		Com_sprintf(expanded, sizeof(expanded), "maps/%s%c.bsp", mission->mapDef->map, timeChar);
		break;
	}

	SAV_QuickSave();

	if (FS_LoadFile(expanded, NULL) != -1)
		Cbuf_AddText(va("map %s%c %s\n", mission->mapDef->map, timeChar, mission->mapDef->param ? mission->mapDef->param : ""));
	else
		Cbuf_AddText(va("map %s %s\n", mission->mapDef->map, mission->mapDef->param ? mission->mapDef->param : ""));

	/* let the (local) server know which map we are running right now */
	csi.currentMD = mission->mapDef;
}

/**
 * @brief Starts a selected mission
 *
 * @note Checks whether a dropship is near the landing zone and whether it has a team on board
 * @sa CP_SetMissionVars
 * @sa CL_StartMission_f
 */
static void CL_GameGo (void)
{
	mission_t *mis;
	aircraft_t *aircraft;
	base_t *base = NULL;

	if (!curCampaign) {
		Com_DPrintf(DEBUG_CLIENT, "curCampaign: %p, selMis: %p, interceptAircraft: %i\n", (void*)curCampaign, (void*)selMis, gd.interceptAircraft);
		return;
	}

	aircraft = cls.missionaircraft;
	base = CP_GetMissionBase();

	if (!selMis)
		selMis = aircraft->mission;

	if (!selMis) {
		Com_DPrintf(DEBUG_CLIENT, "No selMis\n");
		return;
	}

	mis = selMis->def;
	map_maxlevel_base = 0;
	assert(mis && aircraft);

	/* Before we start, we should clear the missionresults array. */
	memset(&missionresults, 0, sizeof(missionresults));

	/* Various sanity checks. */
	if (ccs.singleplayer) {
		if (!mis->active) {
			Com_DPrintf(DEBUG_CLIENT, "CL_GameGo: Dropship not near landing zone: mis->active: %i\n", mis->active);
			return;
		}
		if (aircraft->teamSize <= 0) {
			Com_DPrintf(DEBUG_CLIENT, "CL_GameGo: No team in dropship. teamSize=%i\n", aircraft->teamSize);
			return;
		}
	} else {
		if (B_GetNumOnTeam(aircraft) == 0) {
			/** Multiplayer; but we never reach this far!
			 * @todo Why is that? GameGo can be called via console.*/
			MN_Popup(_("Note"), _("Assemble or load a team"));
			return;
		}
	}

	CP_SetMissionVars(mis);
	/* Set the states of mission Cvars to proper values. */
	Cvar_SetValue("mission_homebase", base->idx);
	Cvar_SetValue("mission_uforecovered", 0);

	/* manage inventory */
	ccs.eMission = base->storage; /* copied, including arrays inside! */
	CL_CleanTempInventory(base);
	CL_ReloadAndRemoveCarried(aircraft, &ccs.eMission);
	/* remove inventory of any old temporary LEs */
	LE_Cleanup();

	CP_StartMissionMap(mis);
}

/**
 * @brief For things like craft_ufo_scout that are no real items this function will
 * increase the collected counter by one
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddItemAsCollected (void)
{
	int i, baseID;
	objDef_t *item = NULL;
	const char* id = NULL;

	/* baseid is appened in mission trigger function */
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <item>\n", Cmd_Argv(0));
		return;
	}

	id = Cmd_Argv(1);
	baseID = atoi(Cmd_Argv(2));

	/* i = item index */
	for (i = 0; i < csi.numODs; i++) {
		item = &csi.ods[i];
		if (!Q_strncmp(id, item->id, MAX_VAR)) {
			gd.bases[baseID].storage.num[i]++;
			Com_DPrintf(DEBUG_CLIENT, "add item: '%s'\n", item->id);
			assert(item->tech);
			RS_MarkCollected((technology_t*)(item->tech));
		}
	}
}

/**
 * @brief Changes nation happiness by given multiplier.
 * @note There must be argument passed to this function being converted to float.
 */
static void CP_ChangeNationHappiness_f (void)
{
	float multiplier = 0;
	nation_t *nation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <multiplier>\n", Cmd_Argv(0));
		return;
	}
	multiplier = atof(Cmd_Argv(1));

	/* we can use an assert here - because this script function will only
	 * be available as trigger command - selMis must be set at that stage */
	assert(selMis);
	nation = MAP_GetNation(selMis->def->pos);
	assert(nation);

	nation->stats[0].happiness = nation->stats[0].happiness * multiplier;

	/* Nation happiness cannot be greater than 1 */
	if (nation->stats[0].happiness > 1.0f)
		nation->stats[0].happiness = 1.0f;
}

/**
 * @brief Adds new message generated from mission triggerText variable.
 */
static void CP_AddTriggerMessage_f (void)
{
	/* we can use an assert here - because this script function will only
	 * be available as trigger command - selMis must be set at that stage */
	assert(selMis);

	if (selMis->def->triggerText)
		MN_AddNewMessage(_("Notice"), selMis->def->triggerText, qfalse, MSG_STANDARD, NULL);
	else
		Com_Printf("CP_AddTriggerMessage_f()... function call without mission triggerText (%s)\n", selMis->def->name);
}

/** @brief mission trigger functions */
static const cmdList_t cp_commands[] = {
	{"cp_add_item", CP_AddItemAsCollected, "Add an item as collected"},
	{"cp_triggermessage", CP_AddTriggerMessage_f, "Function to add new message with mission triggerText."},
	{"cp_changehappiness", CP_ChangeNationHappiness_f, "Function to raise or lower nation hapiness."},

	{NULL, NULL, NULL}
};

/**
 * @brief Add/Remove temporary mission trigger functions
 * @note These function can be defined via onwin/onlose parameters in missions.ufo
 * @param[in] add If true, add the trigger functions, otherwise delete them
 */
static void CP_MissionTriggerFunctions (qboolean add)
{
	const cmdList_t *commands;

	for (commands = cp_commands; commands->name; commands++)
		if (add)
			Cmd_AddCommand(commands->name, commands->function, commands->description);
		else
			Cmd_RemoveCommand(commands->name);
}

/**
 * @brief Executes console commands after a mission
 *
 * @param m Pointer to mission_t
 * @param won Int value that is one when you've won the game, and zero when the game was lost
 * Can execute console commands (triggers) on win and lose
 * This can be used for story dependent missions
 */
void CP_ExecuteMissionTrigger (mission_t * m, qboolean won)
{
	/* we add them only here - and remove them afterwards to prevent cheating */
	CP_MissionTriggerFunctions(qtrue);
	Com_DPrintf(DEBUG_CLIENT, "Execute mission triggers\n");
	if (won && *m->onwin) {
		Com_DPrintf(DEBUG_CLIENT, "...won - executing '%s'\n", m->onwin);
		Cbuf_AddText(va("%s\n", m->onwin));
	} else if (!won && *m->onlose) {
		Com_DPrintf(DEBUG_CLIENT, "...lost - executing '%s'\n", m->onlose);
		Cbuf_AddText(va("%s\n", m->onlose));
	}
	Cbuf_Execute();
	CP_MissionTriggerFunctions(qfalse);
}

/**
 * @brief Checks whether you have to play this mission
 *
 * You can mark a mission as story related.
 * If a mission is story related the cvar game_autogo is set to 0
 * If this cvar is 1 - the mission dialog will have a auto mission button
 * @sa CL_GameAutoGo_f
 */
static void CL_GameAutoCheck_f (void)
{
	if (!curCampaign || !selMis || gd.interceptAircraft < 0 || gd.interceptAircraft >= gd.numAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "No update after automission\n");
		return;
	}

	switch (selMis->def->storyRelated) {
	case qtrue:
		Com_DPrintf(DEBUG_CLIENT, "story related - auto mission is disabled\n");
		Cvar_Set("game_autogo", "0");
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "auto mission is enabled\n");
		Cvar_Set("game_autogo", "1");
		break;
	}
}

/**
 * @brief Handles the auto mission for none storyrelated missions or missions that
 * failed to assembly
 * @sa CL_GameAutoGo_f
 * @sa CL_Drop
 * @sa AL_CollectingAliens
 */
void CL_GameAutoGo (actMis_t *mission)
{
	qboolean won;
	aircraft_t *aircraft = NULL;
	mission_t *mis;
	int i, amount = 0;
	int civiliansKilled = 0; /* @todo: fill this for the case you won the game */
	aliensTmp_t *cargo = NULL;

	assert(mission);
	mis = mission->def;
	assert(mis);

	if (mis->missionType != MIS_BASEATTACK) {
		if (gd.interceptAircraft < 0 || gd.interceptAircraft >= gd.numAircraft) {
			Com_DPrintf(DEBUG_CLIENT, "No update after automission\n");
			return;
		}

		aircraft = AIR_AircraftGetFromIdx(gd.interceptAircraft);
		assert(aircraft);
		baseCurrent = aircraft->homebase;
		assert(baseCurrent);
		baseCurrent->aircraftCurrent = aircraft->idxInBase; /* Might not be needed, but it's used later on in AIR_AircraftReturnToBase_f */

		if (!mis->active) {
			MN_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mis->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("game_autogo", "0");
			return;
		}
		/* FIXME: This needs work */
		won = mis->aliens * difficulty->integer > aircraft->teamSize ? qfalse : qtrue;
		Com_DPrintf(DEBUG_CLIENT, "Aliens: %i (count as %i) - Soldiers: %i\n", mis->aliens, mis->aliens * difficulty->integer, aircraft->teamSize);
	} else {
		baseCurrent = (base_t*)mis->data;
		assert(baseCurrent);
		/* FIXME: This needs work */
		won = qtrue;
	}

	MN_PopMenu(qfalse);

	/* update nation opinions */
	if (won) {
		CL_HandleNationData(!won, mis->civilians, 0, 0, mis->aliens, selMis);
		CP_CheckLostCondition(!won, mis, civiliansKilled);
	} else {
		CL_HandleNationData(!won, 0, mis->civilians, mis->aliens, 0, selMis);
		CP_CheckLostCondition(!won, mis, mis->civilians);
	}

	if (mis->missionType != MIS_BASEATTACK) {
		assert(aircraft);
		/* campaign effects */
		selMis->cause->done++;
		if ((selMis->cause->def->quota && selMis->cause->done >= selMis->cause->def->quota)
			|| (selMis->cause->def->number
				&& selMis->cause->num >= selMis->cause->def->number)) {
			selMis->cause->active = qfalse;
			CL_CampaignExecute(selMis->cause);
		}

		/* collect all aliens as dead ones */
		cargo = aircraft->aliencargo;

		/* Make sure dropship aliencargo is empty. */
		memset(aircraft->aliencargo, 0, sizeof(aircraft->aliencargo));

		/* @todo: Check whether there are already aliens
		* @sa AL_CollectingAliens */
		/*if (aircraft->alientypes) {
		}*/

		aircraft->alientypes = mis->numAlienTeams;
		for (i = 0; i < aircraft->alientypes; i++) {
			Q_strncpyz(cargo[i].alientype, mis->alienTeams[i]->name, sizeof(cargo[i].alientype));
			/* FIXME: This could lead to more aliens in their sum */
			cargo[i].amount_dead = rand() % mis->aliens;
			amount += cargo[i].amount_dead;
		}
		if (amount)
			MN_AddNewMessage(_("Notice"), va(_("Collected %i dead alien bodies"), amount), qfalse, MSG_STANDARD, NULL);

		if (aircraft->alientypes && !(aircraft->homebase->hasBuilding[B_ALIEN_CONTAINMENT])) {
			/* We have captured/killed aliens, but the homebase of this aircraft does not have alien containment. Popup aircraft transer dialog. */
			TR_TransferAircraftMenu(aircraft);
		}

		AIR_AircraftReturnToBase(aircraft);
	}

	/* onwin and onlose triggers */
	CP_ExecuteMissionTrigger(selMis->def, won);

	if (won || !selMis->def->keepAfterFail)
		CL_CampaignRemoveMission(selMis);

	if (won)
		MN_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
	else
		MN_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);

	MAP_ResetAction();
}

/**
 * @sa CL_GameAutoCheck_f
 * @sa CL_GameGo
 */
static void CL_GameAutoGo_f (void)
{
	if (!curCampaign || !selMis) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoGo_f: No update after automission\n");
		return;
	}

	if (!cls.missionaircraft) {
		Com_Printf("CL_GameAutoGo_f: No aircraft at target pos\n");
		return;
	}

	/* start the map */
	CL_GameAutoGo(selMis);
}

/**
 * @brief Let the aliens win the match
 */
static void CL_GameAbort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

/**
 * @brief Update employeer stats after mission.
 * @param[in] won Determines whether we won the mission or not.
 * @note Soldier promotion is being done here.
 * @note Soldier skill upgrade is being done here.
 * @sa CL_GameResults_f
 *
 * FIXME: See @todo and FIXME included
 */
static void CL_UpdateCharacterStats (int won)
{
	character_t *chr = NULL;
	rank_t *rank = NULL;
	aircraft_t *aircraft;
	int i, j, idx = 0;

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: numTeamList: %i\n", cl.numTeamList);

	/* aircraft = &baseCurrent->aircraft[gd.interceptAircraft]; remove this @todo: check if baseCurrent has the currect aircraftCurrent.  */
	aircraft = AIR_AircraftGetFromIdx(gd.interceptAircraft);

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: baseCurrent: %s\n", baseCurrent->name);

	/** @todo What about UGVs/Tanks? */
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++)
		if (CL_SoldierInAircraft(i, EMPL_SOLDIER, gd.interceptAircraft) ) {
			Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: searching for soldier: %i\n", i);
			chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, -(idx+1));
			assert(chr);
			/* count every hired soldier in aircraft */
			idx++;
			chr->assigned_missions++;

			CL_UpdateCharacterSkills(chr);

			/* @todo: use chrScore_t to determine negative influence on soldier here,
			   like killing too many civilians and teammates can lead to unhire and disband
			   such soldier, or maybe rank degradation. */

			/* Check if the soldier meets the requirements for a higher rank
			   and do a promotion. */
			/* @todo: use param[in] in some way. */
			if (gd.numRanks >= 2) {
				for (j = gd.numRanks - 1; j > chr->rank; j--) {
					rank = &gd.ranks[j];
					/* FIXME: (Zenerka 20080301) extend ranks and change calculations here. */
					if (rank->type == EMPL_SOLDIER && (chr->skills[ABILITY_MIND] >= rank->mind)
						&& (chr->kills[KILLED_ALIENS] >= rank->killed_enemies)
						&& ((chr->kills[KILLED_CIVILIANS] + chr->kills[KILLED_TEAM]) <= rank->killed_others)) {
						chr->rank = j;
						if (chr->HP > 0)
							Com_sprintf(messageBuffer, sizeof(messageBuffer), _("%s has been promoted to %s.\n"), chr->name, rank->name);
						else
							Com_sprintf(messageBuffer, sizeof(messageBuffer), _("%s has been awarded the posthumous rank of %s\\for inspirational gallantry in the face of overwhelming odds.\n"), chr->name, rank->name);
						MN_AddNewMessage(_("Soldier promoted"), messageBuffer, qfalse, MSG_PROMOTION, NULL);
						break;
					}
				}
			}
		/* also count the employees that are only hired but not in the aircraft */
		} else if (E_GetHiredEmployee(baseCurrent, EMPL_SOLDIER, i))
			idx++;
	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: Done\n");
}

#ifdef DEBUG
/**
 * @brief Debug function to add one item of every type to base storage and mark them collected.
 * @note Command to call this: debug_additems
 */
static void CL_DebugAllItems_f (void)
{
	int i;
	base_t *base;
	technology_t *tech;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = gd.bases + i;
	assert(base);

	for (i = 0; i < csi.numODs; i++) {
		if (!csi.ods[i].weapon && !csi.ods[i].numWeapons)
			continue;
		B_UpdateStorageAndCapacity(base, i, 1, qfalse, qtrue);
		if (base->storage.num[i] > 0) {
			tech = csi.ods[i].tech;
			if (!tech)
				Sys_Error("CL_DebugAllItems_f: No tech for %s / %s\n", csi.ods[i].id, csi.ods[i].name);
			RS_MarkCollected(tech);
		}
	}
}

/**
 * @brief Debug function to show items in base storage.
 * @note Command to call this: debug_showitems
 */
static void CL_DebugShowItems_f (void)
{
	int i;
	base_t *base;
	technology_t *tech;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = gd.bases + i;
	assert(base);

	for (i = 0; i < csi.numODs; i++) {
		tech = csi.ods[i].tech;
		if (!tech)
			Sys_Error("CL_DebugAllItems_f: No tech for %s / %s\n", csi.ods[i].id, csi.ods[i].name);
		Com_Printf("%i. %s: (%s) %i\n", i, csi.ods[i].name, csi.ods[i].id, base->storage.num[i]);
	}
}

/**
 * @brief Debug function to set the credits to max
 */
static void CL_DebugFullCredits_f (void)
{
	CL_UpdateCredits(MAX_CREDITS);
}

/**
 * @brief Debug function to add 5 new unhired employees of each type
 */
static void CL_DebugNewEmployees_f (void)
{
	int j;

	for (j = 0; j < 5; j++)
		/* Create a scientist */
		E_CreateEmployee(EMPL_SCIENTIST);

	for (j = 0; j < 5; j++)
		/* Create a medic. */
		E_CreateEmployee(EMPL_MEDIC);

	for (j = 0; j < 5; j++)
		/* Create a soldier. */
		E_CreateEmployee(EMPL_SOLDIER);

	for (j = 0; j < 5; j++)
		/* Create a worker. */
		E_CreateEmployee(EMPL_WORKER);
}

/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CL_DebugChangeCharacterStats_f (void)
{
	int i, j;
	character_t *chr;

	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, i);
		if (chr) {
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				chr->kills[j]++;
		}
	}
	CL_UpdateCharacterStats(1);
}
#endif

/**
 * @sa CL_ParseResults
 * @sa CL_ParseCharacterData
 * @sa CL_GameAbort_f
 */
static void CL_GameResults_f (void)
{
	int won;
	int civilians_killed;
	int aliens_killed;
	int i;
	base_t *base;
	employee_t* employee;
	int numberofsoldiers = 0; /* DEBUG */
	character_t *chr = NULL;

	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f\n");

	/* multiplayer? */
	if (!curCampaign || !selMis || !baseCurrent)
		return;

	/* check for replay */
	if (Cvar_VariableInteger("game_tryagain")) {
		/* don't collect things and update stats --- we retry the mission */
		CL_GameGo();
		return;
	}
	/* check for win */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <won>\n", Cmd_Argv(0));
		return;
	}
	won = atoi(Cmd_Argv(1));

	if (selMis->def->missionType == MIS_BASEATTACK) {
		baseCurrent = (base_t*)selMis->def->data;
	} else {
		assert(gd.interceptAircraft >= 0);
		baseCurrent = AIR_AircraftGetFromIdx(gd.interceptAircraft)->homebase;
		baseCurrent->aircraftCurrent = AIR_AircraftGetFromIdx(gd.interceptAircraft)->idxInBase;
	}

	/* add the looted goods to base storage and market */
	baseCurrent->storage = ccs.eMission; /* copied, including the arrays! */

	civilians_killed = ccs.civiliansKilled;
	aliens_killed = ccs.aliensKilled;
	Com_DPrintf(DEBUG_CLIENT, "Won: %d   Civilians: %d/%d   Aliens: %d/%d\n",
		won, selMis->def->civilians - civilians_killed, civilians_killed,
		selMis->def->aliens - aliens_killed, aliens_killed);
	CL_HandleNationData(!won, selMis->def->civilians - civilians_killed, civilians_killed, selMis->def->aliens - aliens_killed, aliens_killed, selMis);
	CP_CheckLostCondition(!won, selMis->def, civilians_killed);

	/* update the character stats */
	CL_ParseCharacterData(NULL, qtrue);

	/* update stats */
	CL_UpdateCharacterStats(won);

	/* Backward loop because gd.numEmployees[EMPL_SOLDIER] is decremented by E_DeleteEmployee */
	for (i = gd.numEmployees[EMPL_SOLDIER] - 1; i >= 0; i--) {
		/* if employee is marked as dead */
		if (CL_SoldierInAircraft(i, EMPL_SOLDIER, gd.interceptAircraft))	/* DEBUG? */
			numberofsoldiers++;

		Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - try to get player %i \n", i);
		employee = E_GetHiredEmployee(baseCurrent, EMPL_SOLDIER, i);

		if (employee != NULL) {
			chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, i);
			assert(chr);
			Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - idx %d hp %d\n",chr->ucn, chr->HP);
			if (chr->HP <= 0) { /* @todo: <= -50, etc. */
				/* Delete the employee. */
				/* sideeffect: gd.numEmployees[EMPL_SOLDIER] and teamNum[] are decremented by one here. */
				Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f: Delete this dead employee: %i (%s)\n", i, gd.employees[EMPL_SOLDIER][i].chr.name);
				E_DeleteEmployee(employee, EMPL_SOLDIER);
			} /* if dead */
		} /* if employee != NULL */
	} /* for */
	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - num %i\n", numberofsoldiers); /* DEBUG */

	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - done removing dead players\n");

	/* no transfer or campaign effects for base attack missions */
	if (selMis->def->missionType != MIS_BASEATTACK) {
		/* Check for alien containment in aircraft homebase. */
		if (baseCurrent->aircraft[baseCurrent->aircraftCurrent].alientypes && !baseCurrent->hasBuilding[B_ALIEN_CONTAINMENT]) {
			/* We have captured/killed aliens, but the homebase of this aircraft does not have alien containment. Popup aircraft transer dialog. */
			TR_TransferAircraftMenu(&(baseCurrent->aircraft[baseCurrent->aircraftCurrent]));
		} else {
			/* The aircraft can be savely sent to its homebase without losing aliens */

			/* @todo: Is this really needed? At the beginning of CL_GameResults_f we already have this status (if I read this correctly). */
			baseCurrent->aircraft[baseCurrent->aircraftCurrent].homebase = baseCurrent;
		}
		AIR_AircraftReturnToBase_f();

		/* campaign effects */
		selMis->cause->done++;
		if ((selMis->cause->def->quota
			&& selMis->cause->done >= selMis->cause->def->quota)
			|| (selMis->cause->def->number
				&& selMis->cause->num >= selMis->cause->def->number)) {
			selMis->cause->active = qfalse;
			CL_CampaignExecute(selMis->cause);
		}
	}

	/* handle base attack mission */
	if (selMis->def->missionType == MIS_BASEATTACK) {
		base = (base_t*)selMis->def->data;
		assert(base);
		if (won) {
			Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Defense of base: %s successful!"), base->name);
			MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
			base->baseStatus = BASE_WORKING;
			/* @todo: @sa AIRFIGHT_ProjectileHitsBase notes */
		} else
			CL_BaseRansacked(base);
	}

	/* remove mission from list */
	if (won || !selMis->def->keepAfterFail)
		CL_CampaignRemoveMission(selMis);
}

/* =========================================================== */

/** @brief valid mission descriptors */
static const value_t mission_vals[] = {
	{"location", V_TRANSLATION_STRING, offsetof(mission_t, location), 0},
	{"type", V_TRANSLATION_STRING, offsetof(mission_t, type), 0},
	{"text", V_TRANSLATION_MANUAL_STRING, offsetof(mission_t, missionText), 0},
	{"triggertext", V_TRANSLATION_MANUAL_STRING, offsetof(mission_t, triggerText), 0},
	{"alttext", V_TRANSLATION_MANUAL_STRING, offsetof(mission_t, missionTextAlternate), 0},
	{"nation", V_STRING, offsetof(mission_t, nation), 0},
	{"pos", V_POS, offsetof(mission_t, pos), MEMBER_SIZEOF(mission_t, pos)},
	{"mask", V_RGBA, offsetof(mission_t, mask), MEMBER_SIZEOF(mission_t, mask)}, /* color values from map mask this mission needs */
	{"aliens", V_INT, offsetof(mission_t, aliens), MEMBER_SIZEOF(mission_t, aliens)},
	{"maxugv", V_INT, offsetof(mission_t, ugv), MEMBER_SIZEOF(mission_t, ugv)},
	{"commands", V_STRING, offsetof(mission_t, cmds), 0},	/* commands that are excuted when this mission gets active */
	{"onwin", V_STRING, offsetof(mission_t, onwin), 0},
	{"onlose", V_STRING, offsetof(mission_t, onlose), 0},
	{"alienequip", V_STRING, offsetof(mission_t, alienEquipment), 0},
	{"civilians", V_INT, offsetof(mission_t, civilians), MEMBER_SIZEOF(mission_t, civilians)},
	{"civteam", V_STRING, offsetof(mission_t, civTeam), 0},
	{"storyrelated", V_BOOL, offsetof(mission_t, storyRelated), MEMBER_SIZEOF(mission_t, storyRelated)},
	{"keepafterfail", V_BOOL, offsetof(mission_t, keepAfterFail), MEMBER_SIZEOF(mission_t, keepAfterFail)},
	{"noexpire", V_BOOL, offsetof(mission_t, noExpire), MEMBER_SIZEOF(mission_t, noExpire)},

	{NULL, 0, 0, 0}
};

/**
 * @brief If e.g. CL_CampaignAddGroundMission failed after creating the new mission
 * with CL_AddMission we have to decrease the mission pointer, too
 * @sa CL_AddMission
 * @sa CL_CampaignAddGroundMission
 */
void CP_RemoveLastMission (void)
{
	numMissions--;
}

/**
 * @brief Adds a mission to current stageSet
 * @note the returned mission_t pointer has to be filled - this function only fills the name
 * @param[in] name valid mission name
 * @return ms is the mission_t pointer of the mission to add
 * @sa CL_CampaignAddMission
 * @sa CL_CampaignAddGroundMission
 */
mission_t *CL_AddMission (const char *name)
{
	int i;
	mission_t *ms;

	/* just to let us know: search for missions with same name */
	for (i = 0; i < numMissions; i++)
		if (!Q_strncmp(name, missions[i].name, MAX_VAR))
			break;
	if (i < numMissions) {
		Com_DPrintf(DEBUG_CLIENT, "CL_AddMission: mission def \"%s\" with same name found\n", name);
		return &missions[i];
	}

	if (numMissions >= MAX_MISSIONS) {
		Com_Printf("CL_AddMission: Max missions reached\n");
		return NULL;
	}

	/* initialize the mission */
	ms = &missions[numMissions++];
	memset(ms, 0, sizeof(mission_t));
	Q_strncpyz(ms->name, name, sizeof(ms->name));
	Com_DPrintf(DEBUG_CLIENT, "CL_AddMission: mission name: '%s'\n", name);

	/* this might be null for baseattack and crashsite missions */
	ms->mapDef = Com_GetMapDefinitionByID(name);

	return ms;
}

/**
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseMission (const char *name, const char **text)
{
	const char *errhead = "CL_ParseMission: unexpected end of file (mission ";
	mission_t *ms;
	const value_t *vp;
	const char *token;

	ms = CL_AddMission(name);
	if (!ms->mapDef) {
		numMissions--;
		return;
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseMission: mission def \"%s\" without body ignored\n", name);
		numMissions--;
		return;
	}

	if (!ms) {
		Com_Printf("CL_ParseMission: could not add mission \"%s\"\n", name);
		numMissions--;
		FS_SkipBlock(text);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (vp = mission_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				default:
					Com_ParseValue(ms, token, vp->type, vp->ofs, vp->size);
					break;
				case V_TRANSLATION_MANUAL_STRING:
					if (*token == '_')
						token++;
				/* fall through */
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)ms + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				}
				break;
			}

		if (!vp->string) {
			/* alienteams might be seperated by whitespaces */
			if (!Q_strcmp(token, "alienteam")) {
				char teamID[MAX_VAR];
				const char *tokenTeam, *parseTeam;
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				/* prepare parsing the team list */
				Q_strncpyz(teamID, token, sizeof(teamID));
				parseTeam = teamID;
				do {
					tokenTeam = COM_Parse(&parseTeam);
					if (!parseTeam)
						break;
					if (ms->numAlienTeams >= MAX_TEAMS_PER_MISSION) {
						Com_Printf("CL_ParseMission: Max teams per mission exceeded (%i)\n", MAX_TEAMS_PER_MISSION);
						break;
					}
					ms->alienTeams[ms->numAlienTeams] = Com_GetTeamDefinitionByID(tokenTeam);
					if (ms->alienTeams[ms->numAlienTeams]) {
						ms->numAlienTeams++;
					} else
						Com_Printf("CL_ParseMission: Could not find team with id: '%s'\n", tokenTeam);
				} while (parseTeam);
				if (!ms->numAlienTeams)
					Com_Printf("CL_ParseMission: This mission will always use the default alien team\n"
						"please set the alienteam variable in the mission description of '%s'\n", ms->name);
			} else
				Com_Printf("CL_ParseMission: unknown token \"%s\" ignored (mission %s)\n", token, name);
		}

	} while (*text);

	if (abs(ms->pos[0]) > 180.0f)
		Com_Printf("CL_ParseMission: Longitude for mission '%s' is bigger than 180 EW (%.0f)\n", ms->name, ms->pos[0]);
	if (abs(ms->pos[1]) > 90.0f)
		Com_Printf("CL_ParseMission: Latitude for mission '%s' is bigger than 90 NS (%.0f)\n", ms->name, ms->pos[1]);
	if (*ms->onwin && ms->onwin[strlen(ms->onwin)-1] != ';')
		Com_Printf("CL_ParseMission: onwin trigger do not end on ; - mission '%s'\n", ms->name);
	if (*ms->onlose && ms->onlose[strlen(ms->onlose)-1] != ';')
		Com_Printf("CL_ParseMission: onlose trigger do not end on ; - mission '%s'\n", ms->name);

	if (!*ms->civTeam)
		Q_strncpyz(ms->civTeam, ms->nation, sizeof(ms->civTeam));
}

/* =========================================================== */

/**
 * @brief This function parses a list of items that should be set to researched = true after campaign start
 * @todo: Implement the use of this function
 */
void CL_ParseResearchedCampaignItems (const char *name, const char **text)
{
	const char *errhead = "CL_ParseResearchedCampaignItems: unexpected end of file (equipment ";
	const char *token;
	int i;
	campaign_t* campaign = NULL;

	campaign = CL_GetCampaign(Cvar_VariableString("campaign"));
	if (!campaign) {
		Com_Printf("CL_ParseResearchedCampaignItems: failed\n");
		return;
	}
	/* Don't parse if it is not definition for current type of campaign. */
	if ((Q_strncmp(campaign->researched, name, MAX_VAR)) != 0)
		return;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseResearchedCampaignItems: equipment def \"%s\" without body ignored (%s)\n", name, token);
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "..campaign research list '%s'\n", name);
	do {
		token = COM_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < gd.numTechnologies; i++)
			if (!Q_strncmp(token, gd.technologies[i].id, MAX_VAR)) {
				gd.technologies[i].mailSent = MAILSENT_FINISHED;
				gd.technologies[i].markResearched.markOnly[gd.technologies[i].markResearched.numDefinitions] = qtrue;
				gd.technologies[i].markResearched.campaign[gd.technologies[i].markResearched.numDefinitions] = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
				gd.technologies[i].markResearched.numDefinitions++;
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", gd.technologies[i].id);
				break;
			}

		if (i == gd.numTechnologies)
			Com_Printf("CL_ParseResearchedCampaignItems: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/**
 * @brief This function parses a list of items that should be set to researchable = true after campaign start
 * @param researchable Mark them researchable or not researchable
 * @sa CL_ParseScriptFirst
 */
void CL_ParseResearchableCampaignStates (const char *name, const char **text, qboolean researchable)
{
	const char *errhead = "CL_ParseResearchableCampaignStates: unexpected end of file (equipment ";
	const char *token;
	int i;
	campaign_t* campaign = NULL;

	campaign = CL_GetCampaign(Cvar_VariableString("campaign"));
	if (!campaign) {
		Com_Printf("CL_ParseResearchableCampaignStates: failed\n");
		return;
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseResearchableCampaignStates: equipment def \"%s\" without body ignored\n", name);
		return;
	}

	if (Q_strncmp(campaign->researched, name, MAX_VAR)) {
		Com_DPrintf(DEBUG_CLIENT, "..don't use '%s' as researchable list\n", name);
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "..campaign researchable list '%s'\n", name);
	do {
		token = COM_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < gd.numTechnologies; i++)
			if (!Q_strncmp(token, gd.technologies[i].id, MAX_VAR)) {
				if (researchable) {
					gd.technologies[i].mailSent = MAILSENT_PROPOSAL;
					RS_MarkOneResearchable(&gd.technologies[i]);
				} else
					Com_Printf("@todo: Mark unresearchable");
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", gd.technologies[i].id);
				break;
			}

		if (i == gd.numTechnologies)
			Com_Printf("CL_ParseResearchableCampaignStates: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/* =========================================================== */

static const value_t stageset_vals[] = {
	{"needed", V_STRING, offsetof(stageSet_t, needed), 0},
	{"delay", V_DATE, offsetof(stageSet_t, delay), 0},
	{"frame", V_DATE, offsetof(stageSet_t, frame), 0},
	{"expire", V_DATE, offsetof(stageSet_t, expire), 0},
	{"number", V_INT, offsetof(stageSet_t, number), MEMBER_SIZEOF(stageSet_t, number)},
	{"quota", V_INT, offsetof(stageSet_t, quota), MEMBER_SIZEOF(stageSet_t, quota)},
	{"activatexvi", V_INT, offsetof(stageSet_t, activateXVI), MEMBER_SIZEOF(stageSet_t, activateXVI)},
	{"humanattack", V_INT, offsetof(stageSet_t, humanAttack), MEMBER_SIZEOF(stageSet_t, humanAttack)},
	{"ufos", V_INT, offsetof(stageSet_t, ufos), MEMBER_SIZEOF(stageSet_t, ufos)},
	{"sequence", V_STRING, offsetof(stageSet_t, sequence), 0},
	{"cutscene", V_STRING, offsetof(stageSet_t, cutscene), 0},
	{"nextstage", V_STRING, offsetof(stageSet_t, nextstage), 0},
	{"endstage", V_STRING, offsetof(stageSet_t, endstage), 0},
	{"commands", V_STRING, offsetof(stageSet_t, cmds), 0},
	{NULL, 0, 0, 0}
};

/**
 * @sa CL_ParseScriptSecond
 * @sa CL_ParseStage
 */
static void CL_ParseStageSet (const char *name, const char **text)
{
	const char *errhead = "CL_ParseStageSet: unexpected end of file (stageset ";
	stageSet_t *sp;
	const value_t *vp;
	char missionstr[256];
	const char *token;
	const char *misp;
	int j;

	if (numStageSets >= MAX_STAGESETS) {
		Com_Printf("CL_ParseStageSet: Max stagesets reached\n");
		return;
	}

	/* initialize the stage */
	sp = &stageSets[numStageSets++];
	memset(sp, 0, sizeof(stageSet_t));
	Q_strncpyz(sp->name, name, sizeof(sp->name));

	/* get it's body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseStageSets: stageset def \"%s\" without body ignored\n", name);
		numStageSets--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = stageset_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(sp, token, vp->type, vp->ofs, vp->size);
				break;
			}
		if (vp->string)
			continue;

		/* get mission set */
		if (!Q_strncmp(token, "missions", 8)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			Q_strncpyz(missionstr, token, sizeof(missionstr));
			misp = missionstr;

			/* add mission options */
			if (sp->numMissions)
				Sys_Error("CL_ParseStageSet: Double mission tag in set '%s'\n", sp->name);

			sp->numMissions = 0;
			do {
				token = COM_Parse(&misp);
				if (!misp)
					break;

				for (j = 0; j < numMissions; j++)
					if (!Q_strncmp(token, missions[j].name, MAX_VAR)) {
						sp->missions[sp->numMissions++] = j;
						break;
					}

				if (j == numMissions)
					Com_Printf("Com_ParseStageSet: unknown mission \"%s\" ignored (stageset %s)\n", token, sp->name);
			} while (sp->numMissions < MAX_SETMISSIONS);
			continue;
		}
		Com_Printf("Com_ParseStageSet: unknown token \"%s\" ignored (stageset %s)\n", token, name);
	} while (*text);
	if (sp->numMissions && !sp->number && !sp->quota) {
		sp->quota = (int)(sp->numMissions / 2)+1;
		Com_Printf("Com_ParseStageSet: Set quota to %i for stage set '%s' with %i missions\n", sp->quota, sp->name, sp->numMissions);
	}
}


/**
 * @sa CL_ParseStageSet
 * @sa CL_ParseScriptSecond
 */
void CL_ParseStage (const char *name, const char **text)
{
	const char *errhead = "CL_ParseStage: unexpected end of file (stage ";
	stage_t *sp;
	const char *token;
	int i;

	/* search for campaigns with same name */
	for (i = 0; i < numStages; i++)
		if (!Q_strncmp(name, stages[i].name, MAX_VAR))
			break;

	if (i < numStages) {
		Com_Printf("Com_ParseStage: stage def \"%s\" with same name found, second ignored\n", name);
		return;
	} else if (numStages >= MAX_STAGES) {
		Com_Printf("CL_ParseStages: Max stages reached\n");
		return;
	}

	/* get it's body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseStages: stage def \"%s\" without body ignored\n", name);
		return;
	}

	/* initialize the stage */
	sp = &stages[numStages++];
	memset(sp, 0, sizeof(stage_t));
	Q_strncpyz(sp->name, name, sizeof(sp->name));
	sp->first = numStageSets;

	Com_DPrintf(DEBUG_CLIENT, "stage: %s\n", name);

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Q_strncmp(token, "set", 3)) {
			token = COM_EParse(text, errhead, name);
			CL_ParseStageSet(token, text);
		} else
			Com_Printf("Com_ParseStage: unknown token \"%s\" ignored (stage %s)\n", token, name);
	} while (*text);

	sp->num = numStageSets - sp->first;
}

/* =========================================================== */

static const value_t salary_vals[] = {
	{"soldier_base", V_INT, offsetof(salary_t, soldier_base), MEMBER_SIZEOF(salary_t, soldier_base)},
	{"soldier_rankbonus", V_INT, offsetof(salary_t, soldier_rankbonus), MEMBER_SIZEOF(salary_t, soldier_rankbonus)},
	{"worker_base", V_INT, offsetof(salary_t, worker_base), MEMBER_SIZEOF(salary_t, worker_base)},
	{"worker_rankbonus", V_INT, offsetof(salary_t, worker_rankbonus), MEMBER_SIZEOF(salary_t, worker_rankbonus)},
	{"scientist_base", V_INT, offsetof(salary_t, scientist_base), MEMBER_SIZEOF(salary_t, scientist_base)},
	{"scientist_rankbonus", V_INT, offsetof(salary_t, scientist_rankbonus), MEMBER_SIZEOF(salary_t, scientist_rankbonus)},
	{"medic_base", V_INT, offsetof(salary_t, medic_base), MEMBER_SIZEOF(salary_t, medic_base)},
	{"medic_rankbonus", V_INT, offsetof(salary_t, medic_rankbonus), MEMBER_SIZEOF(salary_t, medic_rankbonus)},
	{"robot_base", V_INT, offsetof(salary_t, robot_base), MEMBER_SIZEOF(salary_t, robot_base)},
	{"robot_rankbonus", V_INT, offsetof(salary_t, robot_rankbonus), MEMBER_SIZEOF(salary_t, robot_rankbonus)},
	{"aircraft_factor", V_INT, offsetof(salary_t, aircraft_factor), MEMBER_SIZEOF(salary_t, aircraft_factor)},
	{"aircraft_divisor", V_INT, offsetof(salary_t, aircraft_divisor), MEMBER_SIZEOF(salary_t, aircraft_divisor)},
	{"base_upkeep", V_INT, offsetof(salary_t, base_upkeep), MEMBER_SIZEOF(salary_t, base_upkeep)},
	{"admin_initial", V_INT, offsetof(salary_t, admin_initial), MEMBER_SIZEOF(salary_t, admin_initial)},
	{"admin_soldier", V_INT, offsetof(salary_t, admin_soldier), MEMBER_SIZEOF(salary_t, admin_soldier)},
	{"admin_worker", V_INT, offsetof(salary_t, admin_worker), MEMBER_SIZEOF(salary_t, admin_worker)},
	{"admin_scientist", V_INT, offsetof(salary_t, admin_scientist), MEMBER_SIZEOF(salary_t, admin_scientist)},
	{"admin_medic", V_INT, offsetof(salary_t, admin_medic), MEMBER_SIZEOF(salary_t, admin_medic)},
	{"admin_robot", V_INT, offsetof(salary_t, admin_robot), MEMBER_SIZEOF(salary_t, admin_robot)},
	{"debt_interest", V_FLOAT, offsetof(salary_t, debt_interest), MEMBER_SIZEOF(salary_t, debt_interest)},
	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the salaries from campaign definition
 * @param[in] name Name or ID of the found character skill and ability definition
 * @param[in] text The text of the nation node
 * @param[in] campaignID Current campaign id (idx)
 * @note Example:
 * <code>salary {
 *  soldier_base 3000
 * }</code>
 */
static void CL_ParseSalary (const char *name, const char **text, int campaignID)
{
	const char *errhead = "CL_ParseSalary: unexpected end of file ";
	salary_t *s;
	const value_t *vp;
	const char *token;

	/* initialize the campaign */
	s = &salaries[campaignID];

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseSalary: salary def without body ignored\n");
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = salary_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(s, token, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string) {
			Com_Printf("CL_ParseSalary: unknown token \"%s\" ignored (campaignID %i)\n", token, campaignID);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/* =========================================================== */

static const value_t campaign_vals[] = {
	{"team", V_STRING, offsetof(campaign_t, team), 0},
	{"soldiers", V_INT, offsetof(campaign_t, soldiers), MEMBER_SIZEOF(campaign_t, soldiers)},
	{"workers", V_INT, offsetof(campaign_t, workers), MEMBER_SIZEOF(campaign_t, workers)},
	{"medics", V_INT, offsetof(campaign_t, medics), MEMBER_SIZEOF(campaign_t, medics)},
	{"xvirate", V_INT, offsetof(campaign_t, maxAllowedXVIRateUntilLost), MEMBER_SIZEOF(campaign_t, maxAllowedXVIRateUntilLost)},
	{"maxdebts", V_INT, offsetof(campaign_t, negativeCreditsUntilLost), MEMBER_SIZEOF(campaign_t, negativeCreditsUntilLost)},
	{"minhappiness", V_FLOAT, offsetof(campaign_t, minhappiness), MEMBER_SIZEOF(campaign_t, minhappiness)},
	{"scientists", V_INT, offsetof(campaign_t, scientists), MEMBER_SIZEOF(campaign_t, scientists)},
	{"ugvs", V_INT, offsetof(campaign_t, ugvs), MEMBER_SIZEOF(campaign_t, ugvs)},
	{"equipment", V_STRING, offsetof(campaign_t, equipment), 0},
	{"market", V_STRING, offsetof(campaign_t, market), 0},
	{"researched", V_STRING, offsetof(campaign_t, researched), 0},
	{"difficulty", V_INT, offsetof(campaign_t, difficulty), MEMBER_SIZEOF(campaign_t, difficulty)},
	{"firststage", V_STRING, offsetof(campaign_t, firststage), 0},
	{"map", V_STRING, offsetof(campaign_t, map), 0},
	{"credits", V_INT, offsetof(campaign_t, credits), MEMBER_SIZEOF(campaign_t, credits)},
	{"visible", V_BOOL, offsetof(campaign_t, visible), MEMBER_SIZEOF(campaign_t, visible)},
	{"text", V_TRANSLATION_MANUAL_STRING, offsetof(campaign_t, text), 0}, /* just a gettext placeholder */
	{"name", V_TRANSLATION_STRING, offsetof(campaign_t, name), 0},
	{"date", V_DATE, offsetof(campaign_t, date), 0},
	{NULL, 0, 0, 0}
};

/**
 * @sa CL_ParseClientData
 */
void CL_ParseCampaign (const char *name, const char **text)
{
	const char *errhead = "CL_ParseCampaign: unexpected end of file (campaign ";
	campaign_t *cp;
	const value_t *vp;
	const char *token;
	int i;
	salary_t *s;

	/* search for campaigns with same name */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strncmp(name, campaigns[i].id, sizeof(campaigns[i].id)))
			break;

	if (i < numCampaigns) {
		Com_Printf("CL_ParseCampaign: campaign def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (numCampaigns >= MAX_CAMPAIGNS) {
		Com_Printf("CL_ParseCampaign: Max campaigns reached (%i)\n", MAX_CAMPAIGNS);
		return;
	}

	/* initialize the campaign */
	cp = &campaigns[numCampaigns++];
	memset(cp, 0, sizeof(campaign_t));

	cp->idx = numCampaigns-1;
	Q_strncpyz(cp->id, name, sizeof(cp->id));

	/* some default values */
	Q_strncpyz(cp->team, "human", sizeof(cp->team));
	Q_strncpyz(cp->researched, "researched_human", sizeof(cp->researched));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseCampaign: campaign def \"%s\" without body ignored\n", name);
		numCampaigns--;
		return;
	}

	/* some default values */
	s = &salaries[cp->idx];
	memset(s, 0, sizeof(salary_t));
	s->soldier_base = 3000;
	s->soldier_rankbonus = 500;
	s->worker_base = 3000;
	s->worker_rankbonus = 500;
	s->scientist_base = 3000;
	s->scientist_rankbonus = 500;
	s->medic_base = 3000;
	s->medic_rankbonus = 500;
	s->robot_base = 7500;
	s->robot_rankbonus = 1500;
	s->aircraft_factor = 1;
	s->aircraft_divisor = 25;
	s->base_upkeep = 20000;
	s->admin_initial = 1000;
	s->admin_soldier = 75;
	s->admin_worker = 75;
	s->admin_scientist = 75;
	s->admin_medic = 75;
	s->admin_robot = 150;
	s->debt_interest = 0.005;

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = campaign_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(cp, token, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!Q_strncmp(token, "salary", MAX_VAR)) {
			CL_ParseSalary(token, text, cp->idx);
		} else if (!vp->string) {
			Com_Printf("CL_ParseCampaign: unknown token \"%s\" ignored (campaign %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/* =========================================================== */

static const value_t nation_vals[] = {
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(nation_t, name), 0},
	{"pos", V_POS, offsetof(nation_t, pos), MEMBER_SIZEOF(nation_t, pos)},
	{"color", V_COLOR, offsetof(nation_t, color), MEMBER_SIZEOF(nation_t, color)},
	{"funding", V_INT, offsetof(nation_t, maxFunding), MEMBER_SIZEOF(nation_t, maxFunding)},
	{"happiness", V_FLOAT, offsetof(nation_t, stats[0].happiness), MEMBER_SIZEOF(nation_t, stats[0].happiness)},
	{"alien_friendly", V_FLOAT, offsetof(nation_t, stats[0].alienFriendly), MEMBER_SIZEOF(nation_t, stats[0].alienFriendly)},
	{"soldiers", V_INT, offsetof(nation_t, maxSoldiers), MEMBER_SIZEOF(nation_t, maxSoldiers)},
	{"scientists", V_INT, offsetof(nation_t, maxScientists), MEMBER_SIZEOF(nation_t, maxScientists)},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the nation data from script file
 * @param[in] name Name or ID of the found nation
 * @param[in] text The text of the nation node
 * @sa nation_vals
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseNations (const char *name, const char **text)
{
	const char *errhead = "CL_ParseNations: unexpected end of file (nation ";
	nation_t *nation;
	const value_t *vp;
	const char *token;

	if (gd.numNations >= MAX_NATIONS) {
		Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	nation = &gd.nations[gd.numNations++];
	memset(nation, 0, sizeof(nation_t));

	Com_DPrintf(DEBUG_CLIENT, "...found nation %s\n", name);
	nation->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	nation->stats[0].inuse = qtrue;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseNations: nation def \"%s\" without body ignored\n", name);
		gd.numNations--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/**
		 * <code>
		 * borders {
		 *	"13.5 27.4"
		 *	"-13.5 87.4"
		 *	"[...]"
		 * }
		 * </code>
		 */
		if (!Q_strcmp(token, "borders")) {
			/* found border definitions */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			if (*token != '{') {
				Com_Printf("CL_ParseNations: empty nation borders - skip it (%s)\n", name);
				continue;
			}
			do {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				if (*token != '}') {
					if (nation->numBorders >= MAX_NATION_BORDERS)
						Sys_Error("CL_ParseNations: too many nation borders for nation '%s'\n", name);
					Com_ParseValue(nation, token, V_POS, offsetof(nation_t, borders[nation->numBorders++]), sizeof(nation->borders[nation->numBorders]));
				}
			} while (*token != '}');
		} else {
			/* check for some standard values */
			for (vp = nation_vals; vp->string; vp++)
				if (!Q_strcmp(token, vp->string)) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					switch (vp->type) {
					case V_TRANSLATION_MANUAL_STRING:
						token++;
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)nation + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
						break;
					default:
						if (Com_ParseValue(nation, token, vp->type, vp->ofs, vp->size) == -1)
							Com_Printf("CL_ParseNations: Wrong size for value %s\n", vp->string);
						break;
					}
					break;
				}

			if (!vp->string) {
				Com_Printf("CL_ParseNations: unknown token \"%s\" ignored (nation %s)\n", token, name);
				COM_EParse(text, errhead, name);
			}
		}
	} while (*text);
}

/**
 * @brief Check whether we are in a tactical mission as server or as client
 * @note handles multiplayer and singleplayer
 *
 * @return true when we are not in battlefield
 * @todo: Check cvar mn_main for value
 */
qboolean CL_OnBattlescape (void)
{
	/* server_state is set to zero (ss_dead) on every battlefield shutdown */
	if (Com_ServerState())
		return qtrue; /* server */

	/* client */
	if (cls.state >= ca_connected)
		return qtrue;

	return qfalse;
}

/**
 * @brief Starts a given mission
 * @note Mainly for testing
 * @sa CP_SetMissionVars
 * @sa CL_GameGo
 */
static void CL_StartMission_f (void)
{
	int i;
	const char *missionID;
	mission_t* mission = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <missionID>\n", Cmd_Argv(0));
		return;
	}

	missionID = Cmd_Argv(1);

	for (i = 0; i < numMissions; i++) {
		mission = &missions[i];
		if (!Q_strncmp(missions[i].name, missionID, sizeof(missions[i].name))) {
			break;
		}
	}
	if (i == numMissions) {
		Com_Printf("Mission with id '%s' was not found\n", missionID);
		return;
	}

	CP_SetMissionVars(mission);
	CP_StartMissionMap(mission);
}

/**
 * @brief Scriptfunction to list all parsed nations with their current values
 */
static void CL_NationList_f (void)
{
	int i;
	for (i = 0; i < gd.numNations; i++) {
		Com_Printf("Nation ID: %s\n", gd.nations[i].id);
		Com_Printf("...max-funding %i c\n", gd.nations[i].maxFunding);
		Com_Printf("...alienFriendly %0.2f\n", gd.nations[i].stats[0].alienFriendly);
		Com_Printf("...happiness %0.2f\n", gd.nations[i].stats[0].happiness);
		Com_Printf("...max-soldiers %i\n", gd.nations[i].maxSoldiers);
		Com_Printf("...max-scientists %i\n", gd.nations[i].maxScientists);
		Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", gd.nations[i].color[0], gd.nations[i].color[1], gd.nations[i].color[2], gd.nations[i].color[3]);
		Com_Printf("...pos x:%.0f y:%.0f\n", gd.nations[i].pos[0], gd.nations[i].pos[1]);
	}
}

/* ===================================================================== */

/* these commands are only available in singleplayer */
static const cmdList_t game_commands[] = {
	{"mn_next_aircraft", AIM_NextAircraft_f, NULL},
	{"mn_prev_aircraft", AIM_PrevAircraft_f, NULL},
	{"aircraft_new", AIR_NewAircraft_f, NULL},
	{"mn_reset_air", AIM_ResetAircraftCvars_f, NULL},
	{"aircraft_return", AIR_AircraftReturnToBase_f, NULL},
	{"aircraft_list", CL_AircraftList_f, "Generate the aircraft (interception) list"},
	{"aircraft_start", AIM_AircraftStart_f, NULL},
	{"aircraft_select", AIR_AircraftSelect_f, NULL},
	{"airequip_updatemenu", AIM_AircraftEquipMenuUpdate_f, "Init function for the aircraft equip menu"},
	{"airequip_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"airequip_slot_select", AIM_AircraftEquipSlotSelect_f, NULL},
	{"airequip_zone_select", AIM_AircraftEquipZoneSelect_f, NULL},
	{"airequip_add_item", AIM_AircraftEquipAddItem_f, "Add item to slot"},
	{"airequip_del_item", AIM_AircraftEquipDeleteItem_f, "Remove item from slot"},
	{"add_battery", BDEF_AddBattery_f, "Add a new battery to base"},
	{"remove_battery", BDEF_RemoveBattery_f, "Remove a battery from base"},
	{"basedef_initmenu", BDEF_MenuInit_f, "Inits base defense menu"},
	{"basedef_updatemenu", BDEF_BaseDefenseMenuUpdate_f, "Inits base defense menu"},
	{"basedef_slot_list_click", BDEF_ListClick_f, "Inits base defense menu"},
	{"basedef_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"addeventmail", CL_EventAddMail_f, "Add a new mail (event trigger) - e.g. after a mission"},
	{"mission", CL_StartMission_f, NULL},
	{"stats_update", CL_StatsUpdate_f, NULL},
	{"nationlist", CL_NationList_f, "List all nations on the game console"},
	{"nation_stats_click", CP_NationStatsClick_f, NULL},
	{"nation_update", CL_NationStatsUpdate_f, "Shows the current nation list + statistics."},
	{"nation_select", CL_NationSelect_f, "Select nation and display all relevant information for it."},
	{"game_go", CL_GameGo, NULL},
	{"game_auto_check", CL_GameAutoCheck_f, "Checks whether this mission can be done automatically"},
	{"game_auto_go", CL_GameAutoGo_f, "Let the current selection mission be done automatically"},
	{"game_abort", CL_GameAbort_f, "Abort the game and let the aliens win"},
	{"game_results", CL_GameResults_f, "Parses and shows the game results"},
	{"game_timestop", CL_GameTimeStop, NULL},
	{"game_timeslow", CL_GameTimeSlow, NULL},
	{"game_timefast", CL_GameTimeFast, NULL},
	{"mn_mapaction_reset", MAP_ResetAction, NULL},
	{"map_center", MAP_CenterOnPoint_f, "Centers the geoscape view on items on the geoscape - and cycle through them"},
	{"map_zoom", MAP_Zoom_f, ""},
	{"map_scroll", MAP_Scroll_f, ""},
#ifdef DEBUG
	{"debug_aircraftlist", AIR_ListAircraft_f, "Debug function to list all aircraft in all bases"},
	{"debug_fullcredits", CL_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_newemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type"},
	{"debug_additems", CL_DebugAllItems_f, "Debug function to add one item of every type to base storage and mark related tech collected"},
	{"debug_showitems", CL_DebugShowItems_f, "Debug function to show all items in base storage"},
#endif
	{NULL, NULL, NULL}
};

/**
 * @sa CL_GameNew_f
 * @sa SAV_GameLoad
 */
void CL_GameExit (void)
{
	const cmdList_t *commands;

	if (Com_ServerState())
		SV_Shutdown("Game exit", qfalse);
	else
		CL_Disconnect();

	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_active", "");

	/* singleplayer commands are no longer available */
	if (ccs.singleplayer) {
		Com_DPrintf(DEBUG_CLIENT, "Remove game commands\n");
		for (commands = game_commands; commands->name; commands++) {
			Com_DPrintf(DEBUG_CLIENT, "...%s\n", commands->name);
			Cmd_RemoveCommand(commands->name);
		}
		CL_Disconnect();

		/* @todo: make sure all of gd is empty */
		gd.numBases=0;
		E_ResetEmployees();

		MN_PopMenu(qtrue);
		MN_PushMenu("main");
	} else {
		MN_PushMenu("main");
		Cbuf_Execute();
		MN_PushMenu("multiplayer");
		CL_Disconnect();
	}
	curCampaign = NULL;
}

/**
 * @brief Called at new game and load game
 * @param[in] load qtrue if we are loading game, qfalse otherwise
 * @sa SAV_GameLoad
 * @sa CL_GameNew_f
 */
void CL_GameInit (qboolean load)
{
	const cmdList_t *commands;

	assert(curCampaign);

	Com_DPrintf(DEBUG_CLIENT, "Init game commands\n");
	for (commands = game_commands; commands->name; commands++) {
		Com_DPrintf(DEBUG_CLIENT, "...%s\n", commands->name);
		Cmd_AddCommand(commands->name, commands->function, commands->description);
	}

	CL_GameTimeStop();

	Com_AddObjectLinks();	/* Add tech links + ammo<->weapon links to items.*/
	RS_InitTree(load);	/* Initialise all data in the research tree. */

	CL_CampaignInitMarket(load);

	/* Init popup and map/geoscape */
	CL_PopupInit();
	MAP_GameInit();

	rs_alien_xvi = RS_GetTechByID("rs_alien_xvi");
	if (!rs_alien_xvi)
		Sys_Error("CL_ResetCampaign: Could not find tech definition for rs_alien_xvi");

	/* now check the parsed values for errors that are not catched at parsing stage */
	if (!load)
		CL_ScriptSanityCheck();
}

/**
 * @brief Returns the campaign pointer from global campaign array
 * @param name Name of the campaign
 * @return campaign_t pointer to campaign with name or NULL if not found
 */
campaign_t* CL_GetCampaign (const char* name)
{
	campaign_t* campaign;
	int i;

	for (i = 0, campaign = campaigns; i < numCampaigns; i++, campaign++)
		if (!Q_strncmp(name, campaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("CL_GetCampaign: Campaign \"%s\" doesn't exist.\n", name);
		return NULL;
	}
	return campaign;
}

/**
 * @brief Starts a new single-player game
 * @sa CL_GameInit
 * @sa CL_CampaignInitMarket
 */
static void CL_GameNew_f (void)
{
	char val[8];

	Cvar_SetValue("sv_maxclients", 1.0f);

	/* exit running game */
	if (curCampaign)
		CL_GameExit();

	/* get campaign - they are already parsed here */
	curCampaign = CL_GetCampaign(Cvar_VariableString("campaign"));
	if (!curCampaign)
		return;

	/* reset, set time */
	selMis = NULL;

	memset(&ccs, 0, sizeof(ccs_t));
	CL_StartSingleplayer(qtrue);

	/* initialise view angle for 3D geoscape so that europe is seen */
	ccs.angles[YAW] = GLOBE_ROTATE;
	/* initialise date */
	ccs.date = curCampaign->date;

	memset(&gd, 0, sizeof(gd));
	memset(&stats, 0, sizeof(stats));
	CL_ReadSinglePlayerData();

	Cvar_Set("team", curCampaign->team);

	/* difficulty */
	Com_sprintf(val, sizeof(val), "%i", curCampaign->difficulty);
	Cvar_ForceSet("difficulty", val);

	if (difficulty->integer < -4)
		Cvar_ForceSet("difficulty", "-4");
	else if (difficulty->integer > 4)
		Cvar_ForceSet("difficulty", "4");

	MAP_Init();

	/* base setup */
	gd.numBases = 0;
	Cvar_Set("mn_base_count", "0");
	gd.numAircraft = 0;

	B_NewBases();
	PR_ProductionInit();

	/* ensure ccs.singleplayer is set to true */
	CL_StartSingleplayer(qtrue);

	/* get day */
	while (ccs.date.sec > 3600 * 24) {
		ccs.date.sec -= 3600 * 24;
		ccs.date.day++;
	}

	/* set map view */
	ccs.center[0] = ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	CL_UpdateCredits(curCampaign->credits);

	/* stage setup */
	CL_CampaignActivateStage(curCampaign->firststage, qtrue);

	MN_PopMenu(qtrue);
	Cvar_Set("mn_main", "singleplayerInGame");
	Cvar_Set("mn_active", "map");
	MN_PushMenu("map");

	/* create a base as first step */
	Cmd_ExecuteString("mn_select_base -1");

	CL_GameInit(qfalse);
	Cmd_ExecuteString("addeventmail prolog");

	CL_CampaignRunMarket();
}

#define MAXCAMPAIGNTEXT 4096
static char campaignText[MAXCAMPAIGNTEXT];
static char campaignDesc[MAXCAMPAIGNTEXT];
/**
 * @brief Fill the campaign list with available campaigns
 */
static void CP_GetCampaigns_f (void)
{
	int i;

	*campaignText = *campaignDesc = '\0';
	for (i = 0; i < numCampaigns; i++) {
		if (campaigns[i].visible)
			Q_strcat(campaignText, va("%s\n", campaigns[i].name), MAXCAMPAIGNTEXT);
	}
	/* default campaign */
	menuText[TEXT_STANDARD] = campaignDesc;

	/* select main as default */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strncmp("main", campaigns[i].id, MAX_VAR)) {
			Cmd_ExecuteString(va("campaignlist_click %i", i));
			return;
		}
	Cmd_ExecuteString("campaignlist_click 0");
}

/**
 * @brief Script function to select a campaign from campaign list
 */
static void CP_CampaignsClick_f (void)
{
	int num;
	const char *racetype;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/*which building? */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= numCampaigns)
		return;

	/* jump over all invisible campaigns */
	while (!campaigns[num].visible) {
		num++;
		if (num >= numCampaigns)
			return;
	}

	Cvar_Set("campaign", campaigns[num].id);
	if (!Q_strncmp(campaigns[num].team, "human", 5))
		racetype = _("Human");
	else
		racetype = _("Aliens");

	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("Race: %s\nRecruits: %i %s, %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"),
			racetype,
			campaigns[num].soldiers, ngettext("soldier", "soldiers", campaigns[num].soldiers),
			campaigns[num].scientists, ngettext("scientist", "scientists", campaigns[num].scientists),
			campaigns[num].workers, ngettext("worker", "workers", campaigns[num].workers),
			campaigns[num].medics, ngettext("medic", "medics", campaigns[num].medics),
			campaigns[num].credits, CL_ToDifficultyName(campaigns[num].difficulty),
			(int)(round(campaigns[num].minhappiness * 100.0f)), campaigns[num].negativeCreditsUntilLost,
			_(campaigns[num].text));
	menuText[TEXT_STANDARD] = campaignDesc;
}

/**
 * @brief Will clear most of the parsed singleplayer data
 * @sa INVSH_InitInventory
 * @sa CL_ReadSinglePlayerData
 */
void CL_ResetSinglePlayerData (void)
{
	int i;

	numStageSets = numStages = numMissions = 0;
	memset(missions, 0, sizeof(mission_t) * MAX_MISSIONS);
	memset(stageSets, 0, sizeof(stageSet_t) * MAX_STAGESETS);
	memset(stages, 0, sizeof(stage_t) * MAX_STAGES);
	memset(&invList, 0, sizeof(invList));
	messageStack = NULL;

	/* cleanup dynamic mails */
	CL_FreeDynamicEventMail();

	Mem_FreePool(cl_localPool);

	/* called to flood the hash list - because the parse tech function
	 * was maybe already called */
	RS_ResetHash();
	E_ResetEmployees();
	INVSH_InitInventory(invList);
	/* Count Alien team definitions. */
	for (i = 0; i < csi.numTeamDefs; i++) {
		if (csi.teamDef[i].alien)
			gd.numAliensTD++;
	}
}

#ifdef DEBUG
/**
 * @brief Show campaign stats in console
 * call this function via debug_campaignstats
 */
static void CP_CampaignStats_f (void)
{
	setState_t *set;
	int i;

	if (!curCampaign) {
		Com_Printf("No campaign active\n");
		return;
	}

	Com_Printf("Campaign id: %s\n", curCampaign->id);
	Com_Printf("..research list: %s\n", curCampaign->researched);
	Com_Printf("..equipment: %s\n", curCampaign->equipment);
	Com_Printf("..team: %s\n", curCampaign->team);

	Com_Printf("..active stage: %s\n", activeStage->name);
	for (i = 0, set = &ccs.set[activeStage->first]; i < activeStage->num; i++, set++) {
		Com_Printf("....name: %s\n", set->def->name);
		Com_Printf("......needed: %s\n", set->def->needed);
		Com_Printf("......quota: %i\n", set->def->quota);
		Com_Printf("......number: %i\n", set->def->number);
		Com_Printf("......done: %i\n", set->done);
	}
	Com_Printf("..salaries:\n");
	Com_Printf("...soldier_base: %i\n", SALARY_SOLDIER_BASE);
	Com_Printf("...soldier_rankbonus: %i\n", SALARY_SOLDIER_RANKBONUS);
	Com_Printf("...worker_base: %i\n", SALARY_WORKER_BASE);
	Com_Printf("...worker_rankbonus: %i\n", SALARY_WORKER_RANKBONUS);
	Com_Printf("...scientist_base: %i\n", SALARY_SCIENTIST_BASE);
	Com_Printf("...scientist_rankbonus: %i\n", SALARY_SCIENTIST_RANKBONUS);
	Com_Printf("...medic_base: %i\n", SALARY_MEDIC_BASE);
	Com_Printf("...medic_rankbonus: %i\n", SALARY_MEDIC_RANKBONUS);
	Com_Printf("...robot_base: %i\n", SALARY_ROBOT_BASE);
	Com_Printf("...robot_rankbonus: %i\n", SALARY_ROBOT_RANKBONUS);
	Com_Printf("...aircraft_factor: %i\n", SALARY_AIRCRAFT_FACTOR);
	Com_Printf("...aircraft_divisor: %i\n", SALARY_AIRCRAFT_DIVISOR);
	Com_Printf("...base_upkeep: %i\n", SALARY_BASE_UPKEEP);
	Com_Printf("...admin_initial: %i\n", SALARY_ADMIN_INITIAL);
	Com_Printf("...admin_soldier: %i\n", SALARY_ADMIN_SOLDIER);
	Com_Printf("...admin_worker: %i\n", SALARY_ADMIN_WORKER);
	Com_Printf("...admin_scientist: %i\n", SALARY_ADMIN_SCIENTIST);
	Com_Printf("...admin_medic: %i\n", SALARY_ADMIN_MEDIC);
	Com_Printf("...admin_robot: %i\n", SALARY_ADMIN_ROBOT);
	Com_Printf("...debt_interest: %.5f\n", SALARY_DEBT_INTEREST);
}
#endif /* DEBUG */

/* ======================== */
/* Onwin mission functions. */
/*
1. CP_UFORecovered_f() is triggered by mission "onwin" cp_uforecovery value.
2. CP_UFORecoveredStore_f() prepares UFO recovery process.
3. CP_UfoRecoveryBaseSelectPopup_f() allows to select desired base.
4. CP_UFORecoveredStart_f() starts UFO recovery process.
5. CP_UfoRecoveryNationSelectPopup_f() allows to select desired nation.
6. CP_UFOSellStart_f() starts UFO sell process.
7. CP_UFORecoveredSell_f() allows to sell UFO to desired nation.
8. CP_UFORecoveredDestroy_f() destroys UFO.
9. CP_UFORecoveryDone() finishes the recovery process and updates buttons on menu won.
*/
/* ======================== */

/**
 * @brief Send an email to list all recovered item.
 * @sa CL_EventAddMail_f
 * @sa CL_NewEventMail
 */
void CP_UFOSendMail (const aircraft_t *ufocraft, const base_t *base)
{
	int i, j;
	components_t *comp = NULL;
	objDef_t *compod;
	eventMail_t *mail;
	char body[512] = "";

	assert(ufocraft);
	assert(base);

	if (missionresults.crashsite) {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_crashed_report", va("ufo_crashed_report%i", ccs.date.sec), NULL);
		if (!mail)
			Sys_Error("CP_UFOSendMail: ufo_crashed_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Sys_Error("CP_UFOSendMail: ufo_crashed_report has no mail body");

		/* Find components definition. */
		comp = INV_GetComponentsByItemIdx(INVSH_GetItemByID(ufocraft->id));
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			for (j = 0, compod = csi.ods; j < csi.numODs; j++, compod++) {
				if (!Q_strncmp(compod->id, comp->item_id[i], MAX_VAR))
					break;
			}
			assert(compod);
			if (comp->item_amount2[i] > 0)
				Q_strcat(body, va(_("  * %i x\t%s\n"), comp->item_amount2[i], compod->name), sizeof(body));
		}
	} else {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_recovery_report", va("ufo_recovery_report%i", ccs.date.sec), NULL);
		if (!mail)
			Sys_Error("CP_UFOSendMail: ufo_recovery_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Sys_Error("CP_UFOSendMail: ufo_recovery_report has no mail body");

		/* Find components definition. */
		comp = INV_GetComponentsByItemIdx(INVSH_GetItemByID(ufocraft->id));
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			for (j = 0, compod = csi.ods; j < csi.numODs; j++, compod++) {
				if (!Q_strncmp(compod->id, comp->item_id[i], MAX_VAR))
					break;
			}
			assert(compod);
			if (comp->item_amount[i] > 0)
				Q_strcat(body, va(_("  * %s\n"), compod->name), sizeof(body));
		}
	}
	assert(mail);

	/* don't free the old mail body here - it's the string of the source mail */
	mail->body = Mem_PoolStrDup(va(_(mail->body), UFO_TypeToName(missionresults.ufotype), base->name, body), cl_localPool, CL_TAG_NONE);

	/* update subject */
	/* Insert name of the mission in the template */
	mail->subject = Mem_PoolStrDup(va(_(mail->subject), base->name), cl_localPool, CL_TAG_NONE);

	/* Add the mail to unread mail */
	Cmd_ExecuteString(va("addeventmail %s", mail->id));
}

/**
 * @brief Function to trigger UFO Recovered event.
 * @note This function prepares related cvars for won menu.
 * @note Command to call this: cp_uforecovery.
 */
static void CP_UFORecovered_f (void)
{
	int i;
	ufoType_t UFOtype;
	base_t *base = NULL;
	aircraft_t *ufocraft = NULL;
	qboolean store = qfalse, ufofound = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		Com_Printf("CP_UFORecovered()... UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* At the beginning we enable all UFO recovery options. */
	Cmd_ExecuteString("menuwon_update_buttons");

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == UFOtype) {
			ufofound = qtrue;
			break;
		}
	}
	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFORecovered()... UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* Now we have to check whether we can store the UFO in any base. */
	for (i = 0; i < gd.numBases; i++) {
		if (!gd.bases[i].founded)
			continue;
		base = &gd.bases[i];
		if (UFO_ConditionsForStoring(base, ufocraft)) {
			store = qtrue;
			break;
		}
	}
	/* Put relevant info into missionresults array. */
	missionresults.recovery = qtrue;
	missionresults.crashsite = qfalse;
	missionresults.ufotype = ufocraft->ufotype;
	/* Do nothing without any base. */
	if (!base)
		return;
	/* Prepare related cvars. */
	Cvar_SetValue("mission_uforecovered", 1);	/* This is used in menus to enable UFO Recovery nodes. */
	Cvar_SetValue("mission_uforecoverydone", 0);	/* This is used in menus to block UFO Recovery nodes. */
	Cvar_SetValue("mission_ufotype", UFOtype);
	Cvar_SetValue("mission_recoverynation", -1);
	Cvar_SetValue("mission_recoverybase", -1);
	/* @todo block Sell button if no nation with requirements */
	if (!store) {
		/* Block store option if storing not possible. */
		Cmd_ExecuteString("disufostore");
		Cvar_SetValue("mission_noufohangar", 1);
	} else
		Cvar_SetValue("mission_noufohangar", 0);
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecovered_f()...: base: %s, UFO: %i\n", base->name, UFOtype);
}

/**
 * @brief Updates UFO recovery process and disables buttons.
 */
static void CP_UFORecoveryDone (void)
{
	/* Disable Try Again a mission. */
	Cvar_SetValue("mission_tryagain", 1);
	Cbuf_AddText("distryagain\n");
	/* Disable UFORecovery buttons. */
	Cbuf_AddText("disallrecovery\n");

	/* Set done cvar for function updating. */
	Cvar_SetValue("mission_uforecoverydone", 1);
}

/** @brief Array of base indexes where we can store UFO. */
static int UFObases[MAX_BASES];

/**
 * @brief Finds the destination base for UFO recovery.
 * @note The base selection is being done here.
 * @note Callback command: cp_uforecovery_baselist_click.
 */
static void CP_UfoRecoveryBaseSelectPopup_f (void)
{
	int num;
	base_t* base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= MAX_BASES || UFObases[num] == -1)
		return;

	base = &gd.bases[UFObases[num]];

	assert(base);

	/* Pop the menu and launch it again - now with updated value of selected base. */
	Cvar_SetValue("mission_recoverybase", base->idx);
	Com_DPrintf(DEBUG_CLIENT, "CP_UfoRecoveryBaseSelectPopup_f()... picked base: %s\n", base->name);
	MN_PopMenu(qfalse);
	Cmd_ExecuteString("cp_uforecoverystore");
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecoverystart.
 */
static void CP_UFORecoveredStart_f (void)
{
	base_t *base;
	int i;

	i = Cvar_VariableInteger("mission_recoverybase");

	if (i < 0 || i > MAX_BASES)
		return;

	base = &gd.bases[i];
	assert(base);
	Com_sprintf(messageBuffer, sizeof(messageBuffer),
		_("Recovered %s from the battlefield. UFO is being transported to base %s."),
		UFO_TypeToName(Cvar_VariableInteger("mission_ufotype")), base->name);
	MN_AddNewMessage(_("UFO Recovery"), messageBuffer, qfalse, MSG_STANDARD, NULL);
	UFO_PrepareRecovery(base);

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to store recovered UFO in desired base.
 * @note Command to call this: cp_uforecoverystore.
 * @sa UFO_ConditionsForStoring
 */
static void CP_UFORecoveredStore_f (void)
{
	int i, baseHasUFOHangarCount = 0, recoveryBase = -1;
	base_t *base = NULL;
	aircraft_t *ufocraft = NULL;
	static char recoveryBaseSelectPopup[512];
	qboolean ufofound = qfalse;

	/* Do nothing if recovery process is finished. */
	if (Cvar_VariableInteger("mission_uforecoverydone") == 1)
		return;

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == Cvar_VariableInteger("mission_ufotype")) {
			ufofound = qtrue;
			break;
		}
	}

	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFORecoveredStore_f... UFOType: %i does not have valid craft definition!\n", Cvar_VariableInteger("mission_ufotype"));
		return;
	}

	/* Clear UFObases. */
	memset(UFObases, -1, sizeof(UFObases));

	recoveryBaseSelectPopup[0] = '\0';
	/* Check how many bases can store this UFO. */
	for (i = 0; i < gd.numBases; i++) {
		if (!gd.bases[i].founded)
			continue;
		base = &gd.bases[i];
		if (UFO_ConditionsForStoring(base, ufocraft)) {
			Q_strcat(recoveryBaseSelectPopup, gd.bases[i].name, sizeof(recoveryBaseSelectPopup));
			Q_strcat(recoveryBaseSelectPopup, "\n", sizeof(recoveryBaseSelectPopup));
			UFObases[baseHasUFOHangarCount++] = i;
		}
	}

	/* Do nothing without any base. */
	if (!base)
		return;

	/* If only one base with UFO hangars, the recovery will be done in this base. */
	switch (baseHasUFOHangarCount) {
	case 0:
		/* No UFO base with proper conditions, do nothing. */
		return;
	case 1:
		/* there should only be one entry in UFObases - so use that one. */
		Cvar_SetValue("mission_recoverybase", UFObases[0]);
		CP_UFORecoveredStart_f();
		break;
	default:
		recoveryBase = Cvar_VariableInteger("mission_recoverybase");
		if (recoveryBase < 0) {
			/* default selection: make sure you select one base before leaving popup window */
			Cvar_SetValue("mission_recoverybase", UFObases[0]);
			recoveryBase = UFObases[0];
		}
		Q_strcat(recoveryBaseSelectPopup, _("\n\nSelected base:\t\t\t"), sizeof(recoveryBaseSelectPopup));
		if (recoveryBase >= 0 && recoveryBase < MAX_BASES) {
			for (i = 0; i < baseHasUFOHangarCount; i++) {
				if (UFObases[i] == recoveryBase) {
					Q_strcat(recoveryBaseSelectPopup, gd.bases[UFObases[i]].name, sizeof(recoveryBaseSelectPopup));
					break;
				}
			}
		}
		/* If more than one - popup with list to select base. */
		menuText[TEXT_LIST] = recoveryBaseSelectPopup;
		MN_PushMenu("popup_recoverybaselist");
		break;
	}
}

/** @brief Array of prices proposed by nation. */
static int UFOprices[MAX_NATIONS];

/**
 * @brief Finds the nation to which recovered UFO will be sold.
 * @note The nation selection is being done here.
 * @note Callback command: cp_uforecovery_nationlist_click.
 */
static void CP_UfoRecoveryNationSelectPopup_f (void)
{
	int i, j = -1, num;
	nation_t *nation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nationid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, nation = gd.nations; i < gd.numNations; i++, nation++) {
		j++;
		if (j == num) {
			Cvar_SetValue("mission_recoverynation", i);
			break;
		}
	}
	assert(nation);

	/* Pop the menu and launch it again - now with updated value of selected nation. */
	MN_PopMenu(qfalse);
	Com_DPrintf(DEBUG_CLIENT, "CP_UfoRecoveryNationSelectPopup_f()... picked nation: %s\n", nation->name);
	Cmd_ExecuteString("cp_uforecoverysell");
}

/**
 * @brief Function to start UFO selling process.
 * @note Command to call this: cp_ufosellstart.
 */
static void CP_UFOSellStart_f (void)
{
	nation_t *nation;
	int i;

	nation = &gd.nations[Cvar_VariableInteger("mission_recoverynation")];
	assert(nation);
	Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."),
	UFO_TypeToName(Cvar_VariableInteger("mission_ufotype")), _(nation->name), UFOprices[Cvar_VariableInteger("mission_recoverynation")]);
	MN_AddNewMessage(_("UFO Recovery"), messageBuffer, qfalse, MSG_STANDARD, NULL);
	CL_UpdateCredits(ccs.credits + UFOprices[Cvar_VariableInteger("mission_recoverynation")]);

	/* update nation happiness */
	for (i = 0; i < gd.numNations; i++) {
		if (gd.nations + i == nation) {
			/* nation is happy because it got the UFO */
			gd.nations[i].stats[0].happiness += 0.3f * (1.0f - gd.nations[i].stats[0].happiness);
			/* Nation happiness cannot be greater than 1 */
			if (nation->stats[0].happiness > 1.0f)
				nation->stats[0].happiness = 1.0f;
		} else
			/* nation is unhappy because it wanted the UFO */
			gd.nations[i].stats[0].happiness *= .95f;
	}

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecoverysell.
 */
static void CP_UFORecoveredSell_f (void)
{
	int i, nations = 0;
	nation_t *nation = NULL;
	aircraft_t *ufocraft = NULL;
	static char recoveryNationSelectPopup[512];

	/* Do nothing if recovery process is finished. */
	if (Cvar_VariableInteger("mission_uforecoverydone") == 1)
		return;

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == Cvar_VariableInteger("mission_ufotype"))
			break;
	}

	if (Cvar_VariableInteger("mission_recoverynation") == -1)
		memset (UFOprices, 0, sizeof(UFOprices));
	recoveryNationSelectPopup[0] = '\0';
	for (i = 0; i < gd.numNations; i++) {
		nation = &gd.nations[i];
		/* @todo only nations with proper alien infiltration values */
		nations++;
		/* Calculate price offered by nation only if this is first popup opening. */
		if (Cvar_VariableInteger("mission_recoverynation") == -1)
			UFOprices[i] = ufocraft->price + (int)(frand() * 100000);
		Q_strcat(recoveryNationSelectPopup, _(gd.nations[i].name), sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, "\t\t\t", sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, va("%i", UFOprices[i]), sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, "\t\t", sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, CL_GetNationHappinessString(nation), sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, "\n", sizeof(recoveryNationSelectPopup));
	}
	Q_strcat(recoveryNationSelectPopup, _("\n\nSelected nation:\t\t\t"), sizeof(recoveryNationSelectPopup));
	if (Cvar_VariableInteger("mission_recoverynation") != -1) {
		for (i = 0; i < gd.numNations; i++) {
			if (i == Cvar_VariableInteger("mission_recoverynation")) {
				Q_strcat(recoveryNationSelectPopup, _(gd.nations[i].name), sizeof(recoveryNationSelectPopup));
				break;
			}
		}
	}

	/* Do nothing without at least one nation. */
	if (nations == 0)
		return;

	menuText[TEXT_LIST] = recoveryNationSelectPopup;
	MN_PushMenu("popup_recoverynationlist");
}

/**
 * @brief Function to destroy recovered UFO.
 * @note Command to call this: cp_uforecoverydestroy.
 */
static void CP_UFORecoveredDestroy_f (void)
{
	/* Do nothing if recovery process is finished. */
	if (Cvar_VariableInteger("mission_uforecoverydone") == 1)
		return;

	Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Secured %s was destroyed."),
	UFO_TypeToName(Cvar_VariableInteger("mission_ufotype")));
	MN_AddNewMessage(_("UFO Recovery"), messageBuffer, qfalse, MSG_STANDARD, NULL);

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to process crashed UFO.
 * @note Command to call this: cp_ufocrashed.
 */
static void CP_UFOCrashed_f (void)
{
	int i, j;
	ufoType_t UFOtype;
	aircraft_t *aircraft = NULL, *ufocraft = NULL;
	qboolean ufofound = qfalse;
	components_t *comp = NULL;
	objDef_t *compod;
	itemsTmp_t *cargo;

	if (!baseCurrent || gd.interceptAircraft == -1)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		UFOtype = UFO_ShortNameToID(Cmd_Argv(1));
		if (UFOtype == UFO_MAX) {
			Com_Printf("CP_UFOCrashed_f()... UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == UFOtype) {
			ufofound = qtrue;
			break;
		}
	}

	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFOCrashed_f()... UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* Find dropship. */
	aircraft = AIR_AircraftGetFromIdx(gd.interceptAircraft);
	assert(aircraft);
	cargo = aircraft->itemcargo;

	/* Find components definition. */
	comp = INV_GetComponentsByItemIdx(INVSH_GetItemByID(ufocraft->id));
	assert(comp);

	/* Add components of crashed UFO to dropship. */
	for (i = 0; i < comp->numItemtypes; i++) {
		for (j = 0, compod = csi.ods; j < csi.numODs; j++, compod++) {
			if (!Q_strncmp(compod->id, comp->item_id[i], MAX_VAR))
				break;
		}
		assert(compod);
		Com_DPrintf(DEBUG_CLIENT, "CP_UFOCrashed_f()... Collected %i of %s\n", comp->item_amount2[i], comp->item_id[i]);
		/* Add items to cargo, increase itemtypes. */
		cargo[aircraft->itemtypes].idx = j;
		cargo[aircraft->itemtypes].amount = comp->item_amount2[i];
		aircraft->itemtypes++;
	}
	/* Put relevant info into missionresults array. */
	missionresults.recovery = qtrue;
	missionresults.crashsite = qtrue;
	missionresults.ufotype = ufocraft->ufotype;

	/* send mail */
	CP_UFOSendMail(ufocraft, aircraft->homebase);
}

/**
 * @brief Function to issue Try Again a Mission.
 * @note Command to call this: cp_tryagain.
 */
static void CP_TryAgain_f (void)
{
	/* Do nothing if user did other stuff. */
	if (Cvar_VariableInteger("mission_tryagain") == 1)
		return;
	Cbuf_AddText("set game_tryagain 1;mn_pop\n");
}

/**
 * @brief Returns "homebase" of the mission.
 * @note see struct client_static_s
 */
base_t *CP_GetMissionBase (void)
{
	assert(cls.missionaircraft && cls.missionaircraft->homebase);
	return cls.missionaircraft->homebase;
}

/**
 * @brief Determines a random position on Geoscape that fulfills certain criteria given via parameters
 * @param[in] pos The position that will be overwritten with the random point fulfilling the criterias
 * @param[in] terrainTypes A linkedList_t containing a list of strings determining the acceptable terrain types (e.g. "grass")
 * @param[in] cultureTypes A linkedList_t containing a list of strings determining the acceptable culture types (e.g. "western")
 * @param[in] populationTypes A linkedList_t containing a list of strings determining the acceptable population types (e.g. "suburban")
 * @param[in] nations A linkedList_t containing a list of strings determining the acceptable nations (e.g. "asia")
 * @return true if a location was found, otherwise false
 * @sa LIST_AddString
 * @sa LIST_Delete
 * @note When all parameters contain the string "Any", the algorithm assumes that it does not need to include "water" terrains when determining a random position
 * @note The function is nondeterministic when RASTER is set to a value > 1. The amount of possible alternatives is exactly defined by RASTER. I.e. if RASTER is set to 3, there are 3 different lists from which the random positions are chosen. The list is then chosen randomly.
 * @sa CP_GetRandomPosForAircraft
 */
qboolean CP_GetRandomPosOnGeoscape (vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations)
{
	float x, y;
	int num;
	int randomNum;
	float posX, posY;

	/* RASTER might reduce amount of tested locations to get a better performance */
	float maskWidth = 360.0 / RASTER;
	float maskHeight = 180.0 / RASTER;
	/* RASTER is minimizing the amount of locations, so an offset is introduced to enable access to all locations, depending on a random factor */
	float offset = rand() % RASTER;
	vec2_t posT;
	int hits = 0;

	/* check all locations for suitability in 2 iterations */
	/* prepare 1st iteration */

	/* ITERATION 1 */
	for (y = 0; y < maskHeight; y++) {
		for (x = 0; x < maskWidth; x++) {
			posX = x * 360.0 / maskWidth - 180.0 + offset;
			posY = y * 180.0 / maskHeight - 90.0 + offset;

			Vector2Set(posT, posX, posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				/* the location given in pos belongs to the terrain, culture, population types and nations
				 * that are acceptable, so count it */
				hits++;
			}
		}
	}

	/* if there have been no hits, the function failed to find a position */
	if (hits == 0)
		return qfalse;

	/* the 2nd iteration goes through the locations again, but does so only until a random point */
	/* prepare 2nd iteration */
	randomNum = num = rand() % hits;

	/* ITERATION 2 */
	for (y = 0; y < maskHeight; y++) {
		for (x = 0; x < maskWidth; x++) {
			posX = x * 360.0 / maskWidth - 180.0 + offset;
			posY = y * 180.0 / maskHeight - 90.0 + offset;

			Vector2Set(posT,posX,posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				num--;

				if (num < 1) {
					Vector2Set(pos, posX, posY);
					Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscape: New random coords for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
						pos[0], pos[1], randomNum, hits);
					return qtrue;
				}
			}
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscape: New random coords for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
		pos[0], pos[1], num, hits);

	return qtrue;
}

#ifdef DEBUG
/**
 * @brief Debug function to activate a given stage via console
 * @sa CP_CompleteActivateStage
 */
static void CP_CampaignActivateStage_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <stage-id>\n", Cmd_Argv(0));
		return;
	}
	CL_CampaignActivateStage(Cmd_Argv(1), qtrue);
}

/**
 * @brief Autocomplete function the debug_campaignactivatestage function
 * @sa Cmd_AddParamCompleteFunction
 * @sa CP_CampaignActivateStage_f
 */
static int CP_CompleteActivateStage (const char *partial, const char **match)
{
	int i, matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;

	len = strlen(partial);
	if (!len) {
		/* list them all if there was no parameter given */
		for (i = 0; i < numStages; i++) {
			Com_Printf("%s\n", stages[i].name);
		}
		return 0;
	}

	localMatch[matches] = NULL;

	/* search all matches and fill the localMatch array */
	for (i = 0; i < numStages; i++) {
		if (!Q_strncmp(partial, stages[i].name, len)) {
			Com_Printf("%s\n", stages[i].name);
			localMatch[matches++] = stages[i].name;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}
#endif

void CL_ResetCampaign (void)
{
	/* reset some vars */
	curCampaign = NULL;
	baseCurrent = NULL;
	menuText[TEXT_CAMPAIGN_LIST] = campaignText;

	/* commands */
	Cmd_AddCommand("campaignlist_click", CP_CampaignsClick_f, NULL);
	Cmd_AddCommand("getcampaigns", CP_GetCampaigns_f, "Fill the campaign list with available campaigns");
	Cmd_AddCommand("missionlist", CP_MissionList_f, "Shows all missions from the script files");
	Cmd_AddCommand("game_new", CL_GameNew_f, "Start the new campaign");
	Cmd_AddCommand("game_exit", CL_GameExit, NULL);
	Cmd_AddCommand("cp_tryagain", CP_TryAgain_f, "Try again a mission");
	Cmd_AddCommand("cp_uforecovery", CP_UFORecovered_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_baselist_click", CP_UfoRecoveryBaseSelectPopup_f, "Callback for recovery base list popup.");
	Cmd_AddCommand("cp_uforecoverystart", CP_UFORecoveredStart_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecoverystore", CP_UFORecoveredStore_f, "Function to store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", CP_UfoRecoveryNationSelectPopup_f, "Callback for recovery nation list popup.");
	Cmd_AddCommand("cp_ufosellstart", CP_UFOSellStart_f, "Function to start UFO selling processing.");
	Cmd_AddCommand("cp_uforecoverysell", CP_UFORecoveredSell_f, "Function to sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_uforecoverydestroy", CP_UFORecoveredDestroy_f, "Function to destroy recovered UFO.");
	Cmd_AddCommand("cp_ufocrashed", CP_UFOCrashed_f, "Function to process crashed UFO after a mission.");
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CL_DebugChangeCharacterStats_f, "Debug function to increase the kills and test the ranks");
	Cmd_AddCommand("debug_campaignstats", CP_CampaignStats_f, "Print campaign stats to game console");
	Cmd_AddCommand("debug_campaignactivatestage", CP_CampaignActivateStage_f, "Activates a specific stage");
	Cmd_AddParamCompleteFunction("debug_campaignactivatestage", CP_CompleteActivateStage);
#endif
}
