/**
 * @file cl_campaign.c
 * @brief Single player campaign control.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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
mission_t missions[MAX_MISSIONS];
int numMissions;
actMis_t *selMis;

nation_t nations[MAX_NATIONS];
int numNations = 0;

campaign_t campaigns[MAX_CAMPAIGNS];
int numCampaigns = 0;

stageSet_t stageSets[MAX_STAGESETS];
stage_t stages[MAX_STAGES];
int numStageSets = 0;
int numStages = 0;

campaign_t *curCampaign;
ccs_t ccs;
base_t *baseCurrent;

byte *maskPic;
static int maskWidth, maskHeight;

/* extern in client.h */
stats_t stats;

extern cmdList_t game_commands[];
extern qboolean CL_SendAircraftToMission(aircraft_t* aircraft, actMis_t* mission);
extern void CL_AircraftsNotifyMissionRemoved(const actMis_t* mission);
static void CL_GameExit(void);
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

char varName[MAX_VAR];

qboolean(*varFunc) (char *var);

qboolean CheckOR(char **s);
qboolean CheckAND(char **s);

static void SkipWhiteSpaces(char **s)
{
	while (**s == ' ')
		(*s)++;
}

static void NextChar(char **s)
{
	(*s)++;
	/* skip white-spaces too */
	SkipWhiteSpaces(s);
}

static char *GetSwitchName(char **s)
{
	int pos = 0;

	while (**s > 32 && **s != '^' && **s != '|' && **s != '&' && **s != '!' && **s != '(' && **s != ')') {
		varName[pos++] = **s;
		(*s)++;
	}
	varName[pos] = 0;

	return varName;
}

qboolean CheckOR(char **s)
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

qboolean CheckAND(char **s)
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
  * @brief
  *
  * @param[in] expr
  * @param[in] varFuncParam Function pointer
  * @return qboolean
  */
qboolean CheckBEP(char *expr, qboolean(*varFuncParam) (char *var))
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
  * @brief
  */
extern float CP_GetDistance(const vec2_t pos1, const vec2_t pos2)
{
	int a, b, c;

	a = abs(pos1[0] - pos2[0]);
	b = abs(pos1[1] - pos2[1]);
	c = (a * a) + (b * b);
	return sqrt(c);
}


/**
  * @brief
  */
qboolean CL_MapIsNight(vec2_t pos)
{
	float p, q, a, root, x;

	p = (float) ccs.date.sec / (3600 * 24);
	q = (ccs.date.day + p) * 2 * M_PI / 365.25 - M_PI;
	p = (0.5 + pos[0] / 360 - p) * 2 * M_PI - q;
	a = sin(pos[1] * M_PI / 180);
	root = sqrt(1 - a * a);
	x = sin(p) * root * sin(q) - (a * SIN_ALPHA + cos(p) * root * COS_ALPHA) * cos(q);
	return (x > 0);
}


/**
  * @brief
  */
qboolean Date_LaterThan(date_t now, date_t compare)
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
  * @brief
  */
date_t Date_Add(date_t a, date_t b)
{
	a.sec += b.sec;
	a.day += (a.sec / (3600 * 24)) + b.day;
	a.sec %= 3600 * 24;
	return a;
}


/**
  * @brief
  */
date_t Date_Random(date_t frame)
{
	frame.sec = (frame.day * 3600 * 24 + frame.sec) * frand();
	frame.day = frame.sec / (3600 * 24);
	frame.sec = frame.sec % (3600 * 24);
	return frame;
}


/* =========================================================== */


/**
  * @brief
  */
qboolean CL_MapMaskFind(byte * color, vec2_t polar)
{
	byte *c;
	int res, i, num;

	/* check color */
	if (!color[0] && !color[1] && !color[2])
		return qfalse;

	/* find possible positions */
	res = maskWidth * maskHeight;
	num = 0;
	for (i = 0, c = maskPic; i < res; i++, c += 4)
		if (c[0] == color[0] && c[1] == color[1] && c[2] == color[2])
			num++;

	/* nothing found? */
	if (!num)
		return qfalse;

	/* get position */
	num *= frand();
	for (i = 0, c = maskPic; i < num; c += 4)
		if (c[0] == color[0] && c[1] == color[1] && c[2] == color[2])
			i++;

	/* transform to polar coords */
	res = (c - maskPic) / 4;
	polar[0] = 180 - 360 * ((float) (res % maskWidth) + 0.5) / maskWidth;
	polar[1] = 90 - 180 * ((float) (res / maskWidth) + 0.5) / maskHeight;
	Com_DPrintf("Set new coords for mission to %.0f:%.0f\n", polar[0], polar[1]);
	return qtrue;
}

/**
  * @brief
  */
byte *CL_GetmapColor(vec2_t pos)
{
	int x, y;

	/* get coordinates */
	x = (180 - pos[0]) / 360 * maskWidth;
	y = (90 - pos[1]) / 180 * maskHeight;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	return maskPic + 4 * (x + y * maskWidth);
}

/**
  * @brief
  */
qboolean CL_NewBase(vec2_t pos)
{
	byte *color;

	color = CL_GetmapColor(pos);

	if (MapIsDesert(color)) {
		Com_DPrintf("Desertbase\n");
		baseCurrent->mapChar = 'd';
	} else if (MapIsArctic(color)) {
		Com_DPrintf("Articbase\n");
		baseCurrent->mapChar = 'a';
	} else if (MapIsWater(color)) {
		/* This should already have been catched in MN_MapClick (cl_menu.c), but just in case. */
		MN_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_STANDARD, NULL);
		return qfalse;
	} else {
		Com_DPrintf("Graslandbase\n");
		baseCurrent->mapChar = 'g';
	}

	Com_DPrintf("Colorvalues for base: R:%i G:%i B:%i\n", color[0], color[1], color[2]);

	/* build base */
	baseCurrent->pos[0] = pos[0];
	baseCurrent->pos[1] = pos[1];

	gd.numBases++;

	/* set up the base with buildings that have the autobuild flag set */
	B_SetUpBase();

	/* set up the aircraft */
	CL_NewAircraft(baseCurrent, "craft_dropship");

	return qtrue;
}


/* =========================================================== */


/**
  * @brief Checks wheter a stage set exceeded the quota
  *
  * @return qboolean
  */
static stage_t *testStage;

static qboolean CL_StageSetDone(char *name)
{
	setState_t *set;
	int i;

	for (i = 0, set = &ccs.set[testStage->first]; i < testStage->num; i++, set++)
		if (!Q_strncmp(name, set->def->name, MAX_VAR)) {
			if (set->done >= set->def->quota)
				return qtrue;
			else
				return qfalse;
		}

	/* didn't find set */
	return qfalse;
}


/**
  * @brief
  */
static void CL_CampaignActivateStageSets(stage_t * stage)
{
	setState_t *set;
	int i;

	testStage = stage;
	for (i = 0, set = &ccs.set[stage->first]; i < stage->num; i++, set++)
		if (!set->active && !set->done && !set->num) {
			/* check needed sets */
			if (set->def->needed[0] && !CheckBEP(set->def->needed, CL_StageSetDone))
				continue;

			/* activate it */
			set->active = qtrue;
			set->start = Date_Add(ccs.date, set->def->delay);
			set->event = Date_Add(set->start, Date_Random(set->def->frame));
		}
}


/**
  * @brief
  */
static stageState_t *CL_CampaignActivateStage(char *name, qboolean sequence)
{
	stage_t *stage;
	stageState_t *state;
	int i, j;

	for (i = 0, stage = stages; i < numStages; i++, stage++) {
		if (!Q_strncmp(stage->name, name, MAX_VAR)) {
			/* add it to the list */
			state = &ccs.stage[i];
			state->active = qtrue;
			state->def = stage;
			state->start = ccs.date;

			/* add stage sets */
			for (j = stage->first; j < stage->first + stage->num; j++) {
				memset(&ccs.set[j], 0, sizeof(setState_t));
				ccs.set[j].stage = &stage[j];
				ccs.set[j].def = &stageSets[j];
				if (*stageSets[j].sequence && sequence)
					Cbuf_ExecuteText(EXEC_APPEND, va("seq_start %s;\n", stageSets[j].sequence));
			}

			/* activate stage sets */
			CL_CampaignActivateStageSets(stage);

			Com_DPrintf("Activate stage %s\n", stage->name);

			return state;
		}
	}

	Com_Printf("CL_CampaignActivateStage: stage '%s' not found.\n", name);
	return NULL;
}


/**
  * @brief
  */
static void CL_CampaignEndStage(char *name)
{
	stageState_t *state;
	int i;

	for (i = 0, state = ccs.stage; i < numStages; i++, state++)
		if (!Q_strncmp(state->def->name, name, MAX_VAR)) {
			state->active = qfalse;
			return;
		}

	Com_Printf("CL_CampaignEndStage: stage '%s' not found.\n", name);
}


/**
  * @brief
  * @sa CL_CampaignRemoveMission
  */
#define DIST_MIN_BASE_MISSION 4
static void CL_CampaignAddMission(setState_t * set)
{
	actMis_t *mis;
	/* maybe the mission is already on geoscape */
	mission_t *misTemp;
	int i;
	float f;

	/* add mission */
	if (ccs.numMissions >= MAX_ACTMISSIONS) {
		Com_Printf("Too many active missions!\n");
		return;
	}

	misTemp = &missions[set->def->missions[(int) (set->def->numMissions * frand())]];
	if (misTemp->onGeoscape) {
		Com_DPrintf("Mission is already on geoscape\n");
		return;
	} else {
		misTemp->onGeoscape = qtrue;
	}
	mis = &ccs.mission[ccs.numMissions++];
	memset(mis, 0, sizeof(actMis_t));

	/* set relevant info */
	mis->def = misTemp;
	mis->cause = set;

	/* execute mission commands */
	if (*mis->def->cmds)
		Cbuf_ExecuteText(EXEC_NOW, mis->def->cmds);

	if (set->def->expire.day)
		mis->expire = Date_Add(ccs.date, set->def->expire);

	if (!Q_strncmp(mis->def->name, "baseattack", 10)) {
		baseCurrent = &gd.bases[rand() % gd.numBases];
		/* the first founded base is more likely to be attacked */
		if (!baseCurrent->founded) {
			for (i=0; i<MAX_BASES;i++) {
				if (gd.bases[i].founded) {
					baseCurrent = &gd.bases[i];
					break;
				}
			}
			 /* at this point there should be at least one base */
			if (i==MAX_BASES || !baseCurrent)
				Sys_Error("No bases found\n");
		}

		mis->realPos[0] = baseCurrent->pos[0];
		mis->realPos[1] = baseCurrent->pos[1];
		/* Add message to message-system. */
		Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Your base %s is under attack."), baseCurrent->name );
		MN_AddNewMessage(_("Base Attack"), messageBuffer, qfalse, MSG_BASEATTACK, NULL);

		Cbuf_ExecuteText(EXEC_NOW, va("base_attack %i", baseCurrent->idx));
	} else {
		/* A mission must not be very near a base */
		for(i=0 ; i < gd.numBases ; i++) {
#if (0)
			/* To check multi selection */
			{
#else
			if (CP_GetDistance(mis->def->pos, gd.bases[i].pos) < DIST_MIN_BASE_MISSION) {
#endif
				f = frand();
				mis->def->pos[0] = gd.bases[i].pos[0] + (gd.bases[i].pos[0] < 0	? f * DIST_MIN_BASE_MISSION	: -f * DIST_MIN_BASE_MISSION);
				f = sin(acos(f));
				mis->def->pos[1] = gd.bases[i].pos[1] +	(gd.bases[i].pos[1] < 0 ? f* DIST_MIN_BASE_MISSION	: -f * DIST_MIN_BASE_MISSION);
				break;
			}
		}
		/* get default position first, then try to find a corresponding mask color */
		mis->realPos[0] = mis->def->pos[0];
		mis->realPos[1] = mis->def->pos[1];
		CL_MapMaskFind(mis->def->mask, mis->realPos);

		/* Add message to message-system. */
		Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Alien activity has been reported: '%s'"), mis->def->location );
		MN_AddNewMessage(_("Alien activity"), messageBuffer, qfalse, MSG_TERRORSITE, NULL);
		Com_DPrintf("Alien activity at long: %.0f lat: %0.f\n", mis->realPos[0], mis->realPos[1]);
	}

	/* prepare next event (if any) */
	set->num++;
	if (set->def->number && set->num >= set->def->number)
		set->active = qfalse;
	else
		set->event = Date_Add(ccs.date, Date_Random(set->def->frame));

	/* stop time */
	CL_GameTimeStop();
}

/**
  * @brief
  * @sa CL_CampaignAddMission
  */
static void CL_CampaignRemoveMission(actMis_t * mis)
{
	int i, num;

	num = mis - ccs.mission;
	if (num >= ccs.numMissions) {
		Com_Printf("CL_CampaignRemoveMission: Can't remove mission.\n");
		return;
	}

	ccs.numMissions--;

	Com_DPrintf("%i missions left\n", ccs.numMissions);

	/* allow respawn on geoscape */
	mis->def->onGeoscape = qfalse;

	for (i = num; i < ccs.numMissions; i++)
		ccs.mission[i] = ccs.mission[i + 1];

	/* Notifications */
	MAP_NotifyMissionRemoved(mis);
	CL_AircraftsNotifyMissionRemoved(mis);
	CL_PopupNotifyMIssionRemoved(mis);
}

/**
  * @brief
  */
static void CL_CampaignExecute(setState_t * set)
{
	/* handle stages, execute commands */
	if (*set->def->nextstage)
		CL_CampaignActivateStage(set->def->nextstage, qtrue);

	if (*set->def->endstage)
		CL_CampaignEndStage(set->def->endstage);

	if (*set->def->cmds)
		Cbuf_AddText(set->def->cmds);

	/* activate new sets in old stage */
	CL_CampaignActivateStageSets(set->stage);
}

/**
  * @brief Builds the aircraft list for textfield with id
  */
static void CL_BuildingAircraftList_f(void)
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
			s = va("%s (%i/%i)\t%s\t%s\n", aircraft->name, *aircraft->teamSize, aircraft->size, CL_AircraftStatusToName(aircraft), gd.bases[j].name);
			Q_strcat(aircraftListText, s, sizeof(aircraftListText));
		}
	}

	menuText[TEXT_AIRCRAFT_LIST] = aircraftListText;
}

/**
  * @brief
  *
  * if expires is true a mission expires without any reaction
  * this will cost money and decrease nation support for this area
  * TODO: Use mis->pos to determine the position on the geoscape and get the nation
  * TODO: Use colors for nations
  */
static void CL_HandleNationData(qboolean expires, actMis_t * mis)
{

}


/**
  * @brief
  */
void CL_CampaignCheckEvents(void)
{
	stageState_t *stage;
	setState_t *set;
	actMis_t *mis;
	int i, j;

	/* check campaign events */
	for (i = 0, stage = ccs.stage; i < numStages; i++, stage++)
		if (stage->active)
			for (j = 0, set = &ccs.set[stage->def->first]; j < stage->def->num; j++, set++)
				if (set->active && set->event.day && Date_LaterThan(ccs.date, set->event)) {
					if (set->def->numMissions) {
						CL_CampaignAddMission(set);
						if (gd.mapAction == MA_NONE) {
							gd.mapAction = MA_INTERCEPT;
							CL_BuildingAircraftList_f();
						}
					} else
						CL_CampaignExecute(set);
				}

	/* check UFOs events */
	UFO_CampaignCheckEvents();

	/* let missions expire */
	for (i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++)
		if (mis->expire.day && Date_LaterThan(ccs.date, mis->expire)) {
			/* ok, waiting and not doing a mission will costs money */
			int lose = mis->def->cr_win + mis->def->civilians * mis->def->cr_civilian;

			CL_UpdateCredits(ccs.credits - lose);
			CL_HandleNationData(qtrue, mis);
			Q_strncpyz(messageBuffer, va(_("The mission expired and %i civilians died. You've lost %i credits."), mis->def->civilians, lose), MAX_MESSAGE_TEXT);
			MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
			/* Remove mission from the map. */
			CL_CampaignRemoveMission(mis);
		}
}

char *monthNames[12] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

int monthLength[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
  * @brief
  */
void CL_DateConvert(date_t * date, int *day, int *month)
{
	int i, d;

	/* get day */
	d = date->day % 365;
	for (i = 0; d >= monthLength[i]; i++)
		d -= monthLength[i];

	/* prepare return values */
	*day = d + 1;
	*month = i;
}

/**
  * @brief
  */
char *CL_DateGetMonthName(int month)
{
	return _(monthNames[month]);
}

/**
 * @brief
 *
 * Update the nation data from all parsed nation each month
 * give us nation support by:
 * * credits
 * * new soldiers
 * * new scientists
 * Called from CL_CampaignRun
 * @sa CL_CampaignRun
 * @sa B_CreateEmployee
 */
#define NATION_PROBABILITY 0.3
static void CL_UpdateNationData(void)
{
	int i, j;
	char message[1024];
	nation_t *nation;

	for (i = 0; i < numNations; i++) {
		/* maybe we don't get fund of this nation */
		if ( frand() <= NATION_PROBABILITY )
			continue;
		nation = &nations[i];
		Com_sprintf(message, sizeof(message), _("Gained %i credits from nation %s"), nation->funding, _(nation->name));
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
		CL_UpdateCredits(ccs.credits + nation->funding);
		/* maybe we don't get scientists of this nation */
		if ( frand() <= NATION_PROBABILITY )
			continue;
		for (j=0; j<nation->scientists; j++)
			B_CreateEmployee(EMPL_SCIENTIST);
		/* TODO: soldiers */
	}
}

/**
  * @brief Called every frame when we are in geoscape view
  *
  * Called for node types MN_MAP and MN_3DMAP
  *
  * @sa MN_DrawMenus
  * @sa CL_UpdateNationData
  * @sa B_UpdateBaseData
  * @sa CL_CampaignRunAircraft
  * @sa CL_CampaignCheckEvents
  */
void CL_CampaignRun(void)
{
	/* advance time */
	ccs.timer += cls.frametime * gd.gameTimeScale;
	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int dt, day, month;

		dt = floor(ccs.timer);
		ccs.date.sec += dt;
		ccs.timer -= dt;
		while (ccs.date.sec > 3600 * 24) {
			ccs.date.sec -= 3600 * 24;
			ccs.date.day++;
			/* every day */
			B_UpdateBaseData();
			PR_ProductionRun();
		}

		/* check for campaign events */
		CL_CampaignRunAircraft(dt);
		UFO_CampaignRunUfos(dt);
		CL_CampaignCheckEvents();

		/* set time cvars */
		CL_DateConvert(&ccs.date, &day, &month);
		/* every first day of a month */
		if (day == 1 && gd.fund != qfalse) {
			CL_UpdateNationData();
			gd.fund = qfalse;
		} else if (day > 1)
			gd.fund = qtrue;
		Cvar_Set("mn_mapdate", va("%i %s %i", ccs.date.day / 365, CL_DateGetMonthName(month), day));	/* CL_DateGetMonthName is already "gettexted" */
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("%02i:%i%i"), ccs.date.sec / 3600, (ccs.date.sec % 3600) / 60 / 10,
					(ccs.date.sec % 3600) / 60 % 10);
		Cvar_Set("mn_maptime", messageBuffer);
	}
}


/* =========================================================== */

typedef struct gameLapse_s {
	char name[16];
	int scale;
} gameLapse_t;

#define NUM_TIMELAPSE 6

static gameLapse_t lapse[NUM_TIMELAPSE] = {
	{"5 sec", 5},
	{"5 mins", 5 * 60},
	{"1 hour", 60 * 60},
	{"12 hour", 12 * 3600},
	{"1 day", 24 * 3600},
	{"5 days", 5 * 24 * 3600}
};

int gameLapse;
/**
  * @brief Stop game time speed
  */
void CL_GameTimeStop(void)
{
	/* don't allow time scale in tactical mode */
	if (!Com_ServerState()) {
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
void CL_GameTimeSlow(void)
{
	/* don't allow time scale in tactical mode */
	if (!Com_ServerState()) {
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
void CL_GameTimeFast(void)
{
	/* don't allow time scale in tactical mode */
	if (!Com_ServerState()) {
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

/**
  * @brief Sets credits and update mn_credits cvar
  *
  * Checks whether credits are bigger than MAX_CREDITS
  */
#define MAX_CREDITS 10000000
void CL_UpdateCredits(int credits)
{
	/* credits */
	if ( credits > MAX_CREDITS )
		credits = MAX_CREDITS;
	ccs.credits = credits;
	Cvar_Set("mn_credits", va("%i c", ccs.credits));
}

/* =========================================================== */

#define MAX_STATS_BUFFER 1024
/**
  * @brief
  *
  * Shows the current stats from stats_t stats
  */
void CL_Stats_Update(void)
{
	static char statsBuffer[MAX_STATS_BUFFER];

	/* delete buffer */
	*statsBuffer = '\0';

	/* TODO: implement me */
	Com_sprintf(statsBuffer, MAX_STATS_BUFFER, _("Missions:\nwon:\t%i\tlost:\t%i\nBases:\nbuild:\t%i\tattacked:\t%i\n"),
				stats.missionsWon, stats.missionsLost, stats.basesBuild, stats.basesAttacked);
	menuText[TEXT_STANDARD] = statsBuffer;
}

/**
  * @brief
  * @sa CL_GameLoad
  * @sa CL_SaveEquipment
  */
void CL_LoadEquipment ( sizebuf_t *buf, base_t* base )
{
	item_t item;
	int container, x, y;
	int i, j;
	character_t *chr = base->wholeTeam;

	assert(base);

	/* link the inventory in after load */
	for (i = 0; i < base->numWholeTeam; i++)
		base->wholeTeam[i].inv = &base->teamInv[i];

	/* inventory */
	for (i = 0; i < base->numWholeTeam; chr++, i++) {
		for (j = 0; j < MAX_CONTAINERS; j++)
			chr->inv->c[j] = NULL;
		item.t = MSG_ReadByte(buf);
		while (item.t != NONE) {
			/* read info */
			MSG_ReadFormat(buf, "bbbbb", &item.a, &item.m, &container, &x, &y);

			/* check info and add item if ok */
			Com_AddToInventory(chr->inv, item, container, x, y);

			/* get next item */
			item.t = MSG_ReadByte(buf);
		}
	}
}

/**
  * @brief Stores the equipment for a game
  * @sa CL_LoadEquipment
  */
void CL_SaveEquipment ( sizebuf_t *buf, character_t *team, const int num )
{
	invList_t *ic;
	int i, j;
	character_t *chr = team;

	for (i = 0; i < num; chr++, i++) {
		/* equipment */
		for (j = 0; j < csi.numIDs; j++)
			for (ic = chr->inv->c[j]; ic; ic = ic->next)
				CL_SendItem(buf, ic->item, j, ic->x, ic->y);

		/* terminate list */
		MSG_WriteByte(buf, NONE);
	}

}

/**
 * @brief Saved the complete message stack
 * @sa CL_GameSave
 * @sa MN_AddNewMessage
 */
void CL_MessageSave(sizebuf_t* sb, message_t* message)
{
	int idx = -1;
	if (!message)
		return;
	/* bottom up */
	CL_MessageSave(sb, message->next);

	if (message->pedia)
		idx = message->pedia->idx;

	Com_DPrintf("CL_MessageSave: Save '%s' - '%s'\n", message->title, message->text);
	MSG_WriteString(sb, message->title);
	MSG_WriteString(sb, message->text);
	MSG_WriteByte(sb, message->type);
	MSG_WriteLong(sb, idx);
	MSG_WriteLong(sb, message->d);
	MSG_WriteLong(sb, message->m);
	MSG_WriteLong(sb, message->y);
	MSG_WriteLong(sb, message->h);
	MSG_WriteLong(sb, message->min);
	MSG_WriteLong(sb, message->s);
}

/**
  * @brief
  * @sa CL_GameLoad
  */
void CL_GameSave(char *filename, char *comment)
{
	stageState_t *state;
	actMis_t *mis;
	base_t *base;
	sizebuf_t sb;
	char savegame[MAX_OSPATH];
	message_t *message;
	byte *buf;
	int res;
	int i, j;

	if (!curCampaign) {
		Com_Printf("No campaign active.\n");
		return;
	}

	Com_sprintf(savegame, MAX_OSPATH, "%s/save/%s.sav", FS_Gamedir(), filename );

	buf = (byte*)malloc(sizeof(byte)*MAX_GAMESAVESIZE);
	/* create data */
	SZ_Init(&sb, buf, MAX_GAMESAVESIZE);

	/* write prefix and version */
	MSG_WriteLong(&sb, SAVE_FILE_VERSION);

	MSG_WriteLong(&sb, MAX_GAMESAVESIZE);

	/* store comment */
	MSG_WriteString(&sb, comment);

	/* store campaign name */
	MSG_WriteString(&sb, curCampaign->id);

	/* store date */
	MSG_WriteLong(&sb, ccs.date.day);
	MSG_WriteLong(&sb, ccs.date.sec);

	/* store map view */
	MSG_WriteFloat(&sb, ccs.center[0]);
	MSG_WriteFloat(&sb, ccs.center[1]);
	MSG_WriteFloat(&sb, ccs.zoom);

	/* FIXME: We should remove the version stuff and include the sizeof(globalData_t) as info */
	/* when size of globalData_t in savegame differs from actual size of globalData_t we won't even load the game */
	Com_DPrintf("CL_GameSave: sizeof globalData_t: %i max.gamedatasize: %i\n", sizeof(globalData_t), MAX_GAMESAVESIZE);
	SZ_Write(&sb, &gd, sizeof(globalData_t));

	/* store message system items */
	for ( i = 0, message = messageStack; message; i++, message = message->next );
	Com_DPrintf("CL_GameSave: Saving %i messages from message stack\n", i);
	MSG_WriteByte(&sb, i);
	CL_MessageSave(&sb, messageStack);

	/* store credits */
	MSG_WriteLong(&sb, ccs.credits);

	/* store equipment */
	for (i = 0; i < MAX_OBJDEFS; i++) {
		MSG_WriteLong(&sb, ccs.eCampaign.num[i]);
		MSG_WriteByte(&sb, ccs.eCampaign.num_loose[i]);
	}

	/* store market */
	for (i = 0; i < MAX_OBJDEFS; i++)
		MSG_WriteLong(&sb, ccs.eMarket.num[i]);

	/* store campaign data */
	for (i = 0, state = ccs.stage; i < numStages; i++, state++)
		if (state->active) {
			/* write head */
			setState_t *set;

			MSG_WriteString(&sb, state->def->name);
			MSG_WriteLong(&sb, state->start.day);
			MSG_WriteLong(&sb, state->start.sec);
			MSG_WriteByte(&sb, state->def->num);

			/* write all sets */
			for (j = 0, set = &ccs.set[state->def->first]; j < state->def->num; j++, set++) {
				MSG_WriteString(&sb, set->def->name);
				MSG_WriteByte(&sb, set->active);
				MSG_WriteShort(&sb, set->num);
				MSG_WriteShort(&sb, set->done);
				MSG_WriteLong(&sb, set->start.day);
				MSG_WriteLong(&sb, set->start.sec);
				MSG_WriteLong(&sb, set->event.day);
				MSG_WriteLong(&sb, set->event.sec);
			}
		}
	/* terminate list */
	MSG_WriteByte(&sb, 0);

	/* store active missions */
	MSG_WriteByte(&sb, ccs.numMissions);
	for (i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++) {
		MSG_WriteString(&sb, mis->def->name);
		MSG_WriteString(&sb, mis->cause->def->name);
		MSG_WriteFloat(&sb, mis->realPos[0]);
		MSG_WriteFloat(&sb, mis->realPos[1]);
		MSG_WriteLong(&sb, mis->expire.day);
		MSG_WriteLong(&sb, mis->expire.sec);
	}

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++)
		CL_SaveEquipment( &sb, base->wholeTeam, base->numWholeTeam );

	/* save all the stats */
	SZ_Write(&sb, &stats, sizeof(stats));

	/* write data */
	res = FS_WriteFile(buf, sb.cursize, savegame);
	free(buf);

	if (res == sb.cursize) {
		Cvar_Set("mn_lastsave", filename);
		Com_Printf("Campaign '%s' saved.\n", filename);
	}
}


/**
  * @brief
  */
static void CL_GameSaveCmd(void)
{
	char comment[MAX_COMMENTLENGTH];
	char *arg;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: game_save <filename> <comment>\n");
		return;
	}

	if (!curCampaign) {
		Com_Printf("No running game - no saving...\n");
		return;
	}

	/* get comment */
	if (Cmd_Argc() > 2) {
		arg = Cmd_Argv(2);
		if (arg[0] == '*')
			Q_strncpyz(comment, Cvar_VariableString(arg + 1), MAX_COMMENTLENGTH);
		else
			Q_strncpyz(comment, arg, MAX_COMMENTLENGTH);
	} else
		comment[0] = 0;

	/* save the game */
	CL_GameSave(Cmd_Argv(1), comment);
}

/**
 * @brief Will fix the pointers in gd after loading
 */
void CL_UpdatePointersInGlobalData(void)
{
	int i, j, p;
	base_t* base;
	aircraft_t *aircraft;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		/* fix aircraft homepage and teamsize pointers */
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			aircraft->teamSize = &base->numOnTeam[aircraft->idxInBase];
			aircraft->homebase = &gd.bases[aircraft->idxBase];
		}

		/* now fix the curTeam pointers */
		for (i = 0, p = 0; i < base->numWholeTeam; i++)
			if (base->teamMask[base->aircraftCurrent] & (1 << i)) {
				/* maybe we already have soldiers in this base */
				base->curTeam[p] = &base->wholeTeam[i];
				p++;
			}
		/* rest will be null */
		for (; p < MAX_ACTIVETEAM; p++)
			base->curTeam[p] = NULL;
	}
}

/**
  * @brief Loads a savegame from file
  *
  * @param filename Savegame to load (relative to writepath/save)
  *
  * @sa CL_GameLoadCmd
  * @sa CL_GameSave
  * @sa CL_MessageSave
  * @sa CL_ReadSinglePlayerData
  * @sa CL_UpdatePointersInGlobalData
  */
int CL_GameLoad(char *filename)
{
	actMis_t *mis;
	stageState_t *state;
	setState_t *set;
	setState_t dummy;
	sizebuf_t sb;
	byte *buf;
	base_t *base;
	char *name, *title, *text;
	FILE *f;
	int version, dataSize;
	int i, j, num;
	char val[32];
	message_t *mess;

	/* open file */
	f = fopen(va("%s/save/%s.sav", FS_Gamedir(), filename), "rb");
	if (!f) {
		Com_Printf("Couldn't open file '%s'.\n", filename);
		return 1;
	}

	buf = (byte*)malloc(sizeof(byte)*MAX_GAMESAVESIZE);

	/* read data */
	SZ_Init(&sb, buf, MAX_GAMESAVESIZE);
	sb.cursize = fread(buf, 1, MAX_GAMESAVESIZE, f);
	fclose(f);

	version = MSG_ReadLong(&sb);
	Com_Printf("Savefile version %d detected\n", version);
	dataSize = MSG_ReadLong(&sb);

	if (dataSize != MAX_GAMESAVESIZE) {
		Com_Printf("File '%s' is incompatible to current version\n", filename);
		free(buf);
		return 1;
	}

	/* check current version */
	if (version > SAVE_FILE_VERSION) {
		Com_Printf("File '%s' is a more recent version (%d) than is supported.\n", filename, version);
		free(buf);
		return 1;
	} else if (version < SAVE_FILE_VERSION) {
		Com_Printf("Savefileformat has changed ('%s' is version %d) - you may experience problems.\n", filename, version);
	}

	/* exit running game */
	if (curCampaign)
		CL_GameExit();

	memset(&gd,0,sizeof(gd));
	CL_ReadSinglePlayerData();

	/* read comment */
	MSG_ReadString(&sb);

	/* read campaign name */
	name = MSG_ReadString(&sb);

	for (i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++)
		if (!Q_strncmp(name, curCampaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("CL_GameLoad: Campaign \"%s\" doesn't exist.\n", name);
		curCampaign = NULL;
		free(buf);
		return 1;
	}

	Com_sprintf(val, sizeof(val), "%i", curCampaign->difficulty);
	Cvar_ForceSet("difficulty", val);

	re.LoadTGA(va("pics/menu/%s_mask.tga", curCampaign->map), &maskPic, &maskWidth, &maskHeight);
	if (!maskPic)
		Sys_Error("Couldn't load map mask %s_mask.tga in pics/menu\n", curCampaign->map);

	/* reset */
	MAP_ResetAction();

	memset(&ccs, 0, sizeof(ccs_t));

	/* read date */
	ccs.date.day = MSG_ReadLong(&sb);
	ccs.date.sec = MSG_ReadLong(&sb);

	/* read map view */
	ccs.center[0] = MSG_ReadFloat(&sb);
	ccs.center[1] = MSG_ReadFloat(&sb);
	ccs.zoom = MSG_ReadFloat(&sb);

	/* Recently it was loaded from disk. Attention, bad pointers!!! */
	memcpy(&gd, sb.data + sb.readcount, sizeof(globalData_t));
	sb.readcount += sizeof(globalData_t);

	CL_UpdatePointersInGlobalData();

	/* we start not in base view */
	baseCurrent = NULL;

	/* how many message items */
	i = MSG_ReadByte(&sb);
	Com_DPrintf("%i messages on messagestack");
	for (;i--;) {
		title = MSG_ReadString(&sb);
		text = MSG_ReadString(&sb);
		Com_DPrintf("CL_GameLoad: Load message '%s' - '%s'\n", title, text);
		mess = MN_AddNewMessage(title, text, qfalse, MSG_ReadByte(&sb), RS_GetTechByIDX(MSG_ReadLong(&sb)));
		mess->d = MSG_ReadLong(&sb);
		mess->m = MSG_ReadLong(&sb);
		mess->y = MSG_ReadLong(&sb);
		mess->h = MSG_ReadLong(&sb);
		mess->min = MSG_ReadLong(&sb);
		mess->s = MSG_ReadLong(&sb);
	}

	/* read credits */
	CL_UpdateCredits(MSG_ReadLong(&sb));

	/* read equipment */
	for (i = 0; i < MAX_OBJDEFS; i++) {
		if (version == 0) {
			ccs.eCampaign.num[i] = MSG_ReadByte(&sb);
			ccs.eCampaign.num_loose[i] = 0;
		} else if (version >= 1) {
			ccs.eCampaign.num[i] = MSG_ReadLong(&sb);
			ccs.eCampaign.num_loose[i] = MSG_ReadByte(&sb);
		}
	}

	/* read market */
	for (i = 0; i < MAX_OBJDEFS; i++) {
		if (version == 0)
			ccs.eMarket.num[i] = MSG_ReadByte(&sb);
		else if (version >= 1)
			ccs.eMarket.num[i] = MSG_ReadLong(&sb);
	}

	/* read campaign data */
	name = MSG_ReadString(&sb);
	while (*name) {
		state = CL_CampaignActivateStage(name, qfalse);
		if (!state) {
			Com_Printf("Unable to load campaign '%s', unknown stage '%s'\n", filename, name);
			curCampaign = NULL;
			Cbuf_AddText("mn_pop\n");
			free(buf);
			return 1;
		}

		state->start.day = MSG_ReadLong(&sb);
		state->start.sec = MSG_ReadLong(&sb);
		num = MSG_ReadByte(&sb);
		for (i = 0; i < num; i++) {
			name = MSG_ReadString(&sb);
			for (j = 0, set = &ccs.set[state->def->first]; j < state->def->num; j++, set++)
				if (!Q_strncmp(name, set->def->name, MAX_VAR))
					break;
			/* write on dummy set, if it's unknown */
			if (j >= state->def->num) {
				Com_Printf("Warning: Set '%s' not found\n", name);
				set = &dummy;
			}

			set->active = MSG_ReadByte(&sb);
			set->num = MSG_ReadShort(&sb);
			set->done = MSG_ReadShort(&sb);
			set->start.day = MSG_ReadLong(&sb);
			set->start.sec = MSG_ReadLong(&sb);
			set->event.day = MSG_ReadLong(&sb);
			set->event.sec = MSG_ReadLong(&sb);
		}

		/* read next stage name */
		name = MSG_ReadString(&sb);
	}

	/* store active missions */
	ccs.numMissions = MSG_ReadByte(&sb);
	for (i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++) {
		/* get mission definition */
		name = MSG_ReadString(&sb);
		for (j = 0; j < numMissions; j++)
			if (!Q_strncmp(name, missions[j].name, MAX_VAR)) {
				mis->def = &missions[j];
				break;
			}
		if (j >= numMissions)
			Com_Printf("Warning: Mission '%s' not found\n", name);

		/* get mission definition */
		name = MSG_ReadString(&sb);
		for (j = 0; j < numStageSets; j++)
			if (!Q_strncmp(name, stageSets[j].name, MAX_VAR)) {
				mis->cause = &ccs.set[j];
				break;
			}
		if (j >= numStageSets)
			Com_Printf("Warning: Stage set '%s' not found\n", name);

		/* read position and time */
		mis->realPos[0] = MSG_ReadFloat(&sb);
		mis->realPos[1] = MSG_ReadFloat(&sb);
		mis->expire.day = MSG_ReadLong(&sb);
		mis->expire.sec = MSG_ReadLong(&sb);

		/* ignore incomplete info */
		if (!mis->def || !mis->cause) {
			memset(mis, 0, sizeof(actMis_t));
			mis--;
			i--;
			ccs.numMissions--;
		}
	}

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++)
		CL_LoadEquipment( &sb, base );

	/* load the stats */
	memcpy(&stats, sb.data + sb.readcount, sizeof(stats_t));
	sb.readcount += sizeof(stats_t);

	Com_Printf("Campaign '%s' loaded.\n", filename);

	CL_GameInit();

	Cvar_Set("mn_main", "singleplayer");
	Cvar_Set("mn_active", "map");
	Cbuf_AddText("disconnect\n");
	ccs.singleplayer = qtrue;

	MN_PopMenu(qtrue);
	MN_PushMenu("map");
	free(buf);

	return 0;
}


/**
  * @brief Console command to load a savegame
  *
  * @sa CL_GameLoad
  */
static void CL_GameLoadCmd(void)
{
	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: game_load <filename>\n");
		return;
	}

	Com_DPrintf("load file '%s'\n", Cmd_Argv(1));

	/* load and go to map */
	CL_GameLoad(Cmd_Argv(1));
}


/**
  * @brief Console commands to read the comments from savegames
  *
  * The comment is the part of the savegame that you type in at saving
  * for reidentifying the savegame
  *
  * @sa CL_GameLoadCmd
  * @sa CL_GameLoad
  */
static void CL_GameCommentsCmd(void)
{
	char comment[MAX_VAR];
	FILE *f;
	int i;

	if (Cmd_Argc() == 2) {
		/* checks whether we plan to save without a running game */
		if (!Q_strncmp(Cmd_Argv(1), "save", 4) && !curCampaign) {
			Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
			return;
		}
	}

	for (i = 0; i < 8; i++) {
		/* open file */
		f = fopen(va("%s/save/slot%i.sav", FS_Gamedir(), i), "rb");
		if (!f) {
			Cvar_Set(va("mn_slot%i", i), "");
			continue;
		}

		/* skip the version number */
		fread(comment, sizeof(int), 1, f);
		/* skip the globalData_t size */
		fread(comment, sizeof(int), 1, f);
		/* read the comment */
		fread(comment, 1, MAX_VAR, f);
		Cvar_Set(va("mn_slot%i", i), comment);
		fclose(f);
	}
}


/**
  * @brief Loads the last saved game
  *
  * @note At saving the archive cvar mn_lastsave was set to the latest savegame
  * @sa CL_GameLoad
  */
static void CL_GameContinue(void)
{
	if (cls.state == ca_active) {
		MN_PopMenu(qfalse);
		return;
	}

	if (!curCampaign) {
		/* try to load the current campaign */
		CL_GameLoad(mn_lastsave->string);
		if (!curCampaign)
			return;
	} else {
		MN_PopMenu(qfalse);
	}
}


/**
  * @brief Starts a selected mission
  *
  * Checks whether a dropship is near the landing zone and whether it has a team on board
  */
static void CL_GameGo(void)
{
	mission_t *mis;
	char expanded[MAX_QPATH];
	char timeChar;
	aircraft_t* aircraft;

	if (!curCampaign || gd.interceptAircraft < 0) {
		Com_DPrintf("curCampaign: %p, selMis: %p, interceptAircraft: %i\n", curCampaign, selMis, gd.interceptAircraft);
		return;
	}

	aircraft = CL_AircraftGetFromIdx(gd.interceptAircraft);
	/* update mission-status (active?) for the selected aircraft */
	/* Check what this was supposed to do ? */
	/* CL_CheckAircraft(aircraft); */

	if (!selMis)
		selMis = aircraft->mission;

	if (!selMis) {
		Com_DPrintf("No selMis\n");
		return;
	}

	mis = selMis->def;
	baseCurrent = aircraft->homebase;
	assert(baseCurrent && mis && aircraft);

	if (!ccs.singleplayer && B_GetNumOnTeam() == 0) {
	/* multiplayer */
		MN_Popup(_("Note"), _("Assemble or load a team"));
		return;
	} else if (ccs.singleplayer && (!mis->active || !*(aircraft->teamSize))) {
		/* dropship not near landing zone */
		Com_DPrintf("Dropship not near landingzone: mis->active: %i\n", mis->active);
		return;
	}

	/* start the map */
	Cvar_SetValue("ai_numaliens", (float) mis->aliens);
	Cvar_SetValue("ai_numcivilians", (float) mis->civilians);
	Cvar_Set("ai_alien", mis->alienTeam);
	Cvar_Set("ai_civilian", mis->civTeam);
	Cvar_Set("ai_equipment", mis->alienEquipment);
	Cvar_Set("ai_armor", mis->alienArmor);
	Cvar_Set("music", mis->music);
	Cvar_Set("equip", curCampaign->equipment);
	/* TODO: Map assembling to get the current used dropship in the map is not fully implemented */
	/* but can be done via the map assembling part of the random map assembly */
	Cvar_Set("map_dropship", aircraft->id);

	/* check inventory */
	ccs.eMission = ccs.eCampaign;
	CL_CheckInventory(&ccs.eMission);

	/* prepare */
	baseCurrent->deathMask = 0;
	MN_PopMenu(qtrue);
	Cvar_Set("mn_main", "singleplayermission");

	/* get appropriate map */
	if (CL_MapIsNight(mis->pos))
		timeChar = 'n';
	else
		timeChar = 'd';

	/* base attack */
	/* maps starts with a dot */
	if (mis->map[0] == '.') {
		if (B_GetCount() > 0 && baseCurrent && baseCurrent->baseStatus == BASE_UNDER_ATTACK) {
			Cbuf_AddText(va("base_assemble %i", baseCurrent->idx));
			return;
		} else
			return;
	}

	if (mis->map[0] == '+')
		Com_sprintf(expanded, sizeof(expanded), "maps/%s%c.ump", mis->map + 1, timeChar);
	else
		Com_sprintf(expanded, sizeof(expanded), "maps/%s%c.bsp", mis->map, timeChar);

	if (FS_LoadFile(expanded, NULL) != -1)
		Cbuf_AddText(va("map %s%c %s\n", mis->map, timeChar, mis->param));
	else
		Cbuf_AddText(va("map %s %s\n", mis->map, mis->param));


}

/**
  * @brief Executes console commands after a mission
  *
  * @param m Pointer to mission_t
  * @param won Int value that is one when you've won the game, and zero when the game was lost
  * Can execute console commands (triggers) on win and lose
  * This can be used for story dependent missions
  */
static void CP_ExecuteMissionTrigger(mission_t * m, int won)
{
	if (won && *m->onwin)
		Cbuf_ExecuteText(EXEC_NOW, m->onwin);
	else if (!won && *m->onlose)
		Cbuf_ExecuteText(EXEC_NOW, m->onlose);
}

/**
  * @brief Checks whether you have to play this mission
  *
  * You can mark a mission as story related.
  * If a mission is story related the cvar game_autogo is set to 0
  * If this cvar is 1 - the mission dialog will have a auto mission button
  * @sa CL_GameAutoGo
  */
static void CL_GameAutoCheck(void)
{
	if (!curCampaign || !selMis || gd.interceptAircraft < 0) {
		Com_DPrintf("No update after automission\n");
		return;
	}

	switch (selMis->def->storyRelated) {
	case qtrue:
		Com_DPrintf("story related - auto mission is disabled\n");
		Cvar_SetValue("game_autogo", 0.0f);
		break;
	default:
		Com_DPrintf("auto mission is enabled\n");
		Cvar_SetValue("game_autogo", 1.0f);
		break;
	}
}

/**
  * @brief
  *
  * TODO: Remove recruits when a mission was lost
  * @sa CL_GameAutoCheck
  * @sa CL_GameGo
  */
void CL_GameAutoGo(void)
{
	mission_t *mis;
	int won, i;
	aircraft_t* aircraft;

	if (!curCampaign || !selMis || gd.interceptAircraft < 0) {
		Com_DPrintf("No update after automission\n");
		return;
	}

	aircraft = CL_AircraftGetFromIdx(gd.interceptAircraft);

	/* start the map */
	mis = selMis->def;
	baseCurrent = aircraft->homebase;
	assert(baseCurrent && mis && aircraft);

	if (!mis->active) {
		MN_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
		return;
	} else if (mis->storyRelated) {
		Com_DPrintf("You have to play this mission, because it's story related\n");
		return;
	}

	MN_PopMenu(qfalse);

	/* FIXME: This needs work */
	won = mis->aliens * (int) difficulty->value > *(aircraft->teamSize) ? 0 : 1;

	Com_DPrintf("Aliens: %i (count as %i) - Soldiers: %i\n", mis->aliens, mis->aliens * (int) difficulty->value, *(aircraft->teamSize));

	/* give reward */
	if (won)
		CL_UpdateCredits(ccs.credits + mis->cr_win + (mis->cr_alien * mis->aliens));
	else
		CL_UpdateCredits(ccs.credits + mis->cr_win - (mis->cr_civilian * mis->civilians));

	/* add recruits */
	if (won && mis->recruits)
		for (i = 0; i < mis->recruits; i++)
			CL_GenerateCharacter(curCampaign->team, baseCurrent, ET_ACTOR);

	/* campaign effects */
	selMis->cause->done++;
	if (selMis->cause->done >= selMis->cause->def->quota)
		CL_CampaignExecute(selMis->cause);

	/* onwin and onlose triggers */
	CP_ExecuteMissionTrigger(selMis->def, won);

	CL_CampaignRemoveMission(selMis);

	if (won)
		MN_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
	else
		MN_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);

	MAP_ResetAction();
	CL_AircraftReturnToBase_f();
}

/**
  * @brief
  */
void CL_GameAbort(void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

/* =========================================================== */

/**
  * @brief Collect aliens from battlefield (e.g. for autopsy)
  *
  * @param[in] mission
  *
  * loop through all entities and put the ones that are stunned
  * as living aliens into our labs
  * TODO: put them into the labs
  */
void CL_CollectAliens(mission_t * mission)
{
	int i, j;
	le_t *le = NULL;
	teamDesc_t *td = NULL;
	technology_t *tech = NULL;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;

		if ((le->type == ET_ACTOR || le->type == ET_UGV) && le->team == TEAM_ALIEN) {
			if (le->state & STATE_STUN) {
				/* a stunned actor */
				for (j = 0, td = teamDesc; j < numTeamDesc; j++) {
					if (!Q_strncmp(td->id, selMis->def->alienTeam, MAX_VAR)) {
						/* get interrogation tech */
						tech = RS_GetTechByID(td->interrogation);
						if (tech)
							tech->statusCollected++;
						/* get interrogation_commander tech */
						tech = RS_GetTechByID(td->interrogation_com);
						if (tech)
							tech->statusCollected++;
						/* TODO: get xenobiology tech */
						tech = RS_GetTechByID(td->xenobiology);
						if (tech)
							tech->statusCollected++;
					}
				}
			} else if (le->HP <= 0) {
				/* a death actor */
				for (j = 0, td = teamDesc; j < numTeamDesc; j++) {
					if (!Q_strncmp(td->id, selMis->def->alienTeam, MAX_VAR)) {
						/* get autopsy tech */
						tech = RS_GetTechByID(td->autopsy);
						if (tech)
							tech->statusCollected++;
					}
				}
			}
		}
	}
}

/**
  * @brief Do the real collection of the items
  *
  * @param[in] weapon Which weapon
  * @param[in] left_hand Determines whether the container is the left hand container
  * @param[in] market Add it to market or mission equipment
  *
  * Called from CL_CollectItems.
  * Put every item to the market inventory list
  */
void CL_CollectItemAmmo(invList_t * weapon, int left_hand, qboolean market)
{
	technology_t *tech = NULL;

	/* twohanded weapons and container is left hand container */
	/* item.t ?? */
	if (weapon->item.t == NONE || (left_hand && csi.ods[weapon->item.t].twohanded))
		return;
	if (market)
		ccs.eMarket.num[weapon->item.t]++;
	else
		ccs.eMission.num[weapon->item.t]++;

	tech = csi.ods[weapon->item.t].tech;
	if ( !tech )
		Sys_Error("No tech for %s\n", csi.ods[weapon->item.t].name);
	tech->statusCollected++;

	if (!csi.ods[weapon->item.t].reload || weapon->item.m == NONE)
		return;
	if (market) {
		ccs.eMarket.num_loose[weapon->item.m] += weapon->item.a;
		if (ccs.eMarket.num_loose[weapon->item.m] >= csi.ods[weapon->item.t].ammo) {
			ccs.eMarket.num_loose[weapon->item.m] -= csi.ods[weapon->item.t].ammo;
			ccs.eMarket.num[weapon->item.m]++;
		}
	} else {
		ccs.eMission.num_loose[weapon->item.m] += weapon->item.a;
		if (ccs.eMission.num_loose[weapon->item.m] >= csi.ods[weapon->item.t].ammo) {
			ccs.eMission.num_loose[weapon->item.m] -= csi.ods[weapon->item.t].ammo;
			ccs.eMission.num[weapon->item.m]++;
		}
	}
}

/**
  * @brief Collect items from battlefield
  *
  * @param[in] won Determines whether we have won the match or not
  *
  * collects all items from battlefield (if we've won the match)
  * and put them back to inventory. Calls CL_CollectItemAmmo which
  * does the real collecting
  */
void CL_CollectItems(int won)
{
	int i;
	le_t *le;
	invList_t *item;
	int container;

	/* only call this when the match was won */
	if (!won)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		/* Winner collects everything on the floor, and everything carried */
		/* by surviving actors.  Loser only gets what their living team */
		/* members carry. */
		if (!le->inuse)
			continue;
		switch (le->type) {
		case ET_ITEM:
			for (item = FLOOR(le); item; item = item->next)
				CL_CollectItemAmmo(item, 0, qtrue);
			break;
		case ET_ACTOR:
		case ET_UGV:
			/* the items are already dropped to floor and are available */
			/* as ET_ITEM */
			/* TODO: Does a stunned actor lose his inventory, too? */
			if (le->state & STATE_DEAD)
				break;
			/* living actor */
			for (container = 0; container < csi.numIDs; container++)
				for (item = le->i.c[container]; item; item = item->next)
					CL_CollectItemAmmo(item, (container == csi.idLeft), qfalse);
			break;
		default:
			break;
		}
	}
/*	RS_MarkCollected();*/
	RS_MarkResearchable();
}

/**
  * @brief
  *
  * FIXME: See TODO and FIXME included
  */
void CL_UpdateCharacterStats(int won)
{
	character_t *chr = NULL;
	rank_t *rank = NULL;
	int i, j;

	Com_DPrintf("CL_UpdateCharacterStats: numTeamList: %i\n", cl.numTeamList);
	for (i=0; i<baseCurrent->numWholeTeam; i++)
		if (baseCurrent->teamMask[baseCurrent->aircraftCurrent] & (1 << i)) {
			chr = &baseCurrent->wholeTeam[i];
			assert(chr);
			chr->assigned_missions++;

			/* FIXME: */
			for (j = 0; j < SKILL_NUM_TYPES; j++)
				if (chr->skills[j] < MAX_SKILL)
					chr->skills[j]++;

			/* Check if the soldier meets the requirements for a higher rank -> Promotion */
			if (gd.numRanks >= 2) {
				for (j = gd.numRanks - 1; j > 0; j--) {
					rank = &gd.ranks[j];
					if ((chr->skills[ABILITY_MIND] >= rank->mind)
						&& (chr->kills[KILLED_ALIENS] >= rank->killed_enemies)
						&& ((chr->kills[KILLED_CIVILIANS] + chr->kills[KILLED_TEAM]) <= rank->killed_others)) {
						if (chr->rank != j) {
							chr->rank = j;
							Com_sprintf(messageBuffer, sizeof(messageBuffer), _("%s has been promoted to %s.\n"), chr->name, rank->name);
							MN_AddNewMessage(_("Soldier promoted"), messageBuffer, qfalse, MSG_PROMOTION, NULL);
						}
						break;
					}
				}
			}
		}
}

#ifdef DEBUG
/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CL_DebugChangeCharacterStats_f(void)
{
	int i, j;
	character_t* chr;
	for (i = 0; i < baseCurrent->numWholeTeam;i++) {
		chr = &baseCurrent->wholeTeam[i];
		for (j=0; j<KILLED_NUM_TYPES; j++)
			chr->kills[j]++;
	}
	CL_UpdateCharacterStats(1);
}
#endif

/**
  * @brief
  * @sa CL_ParseResults
  * @sa CL_ParseCharacterData
  */
static void CL_GameResultsCmd(void)
{
	int won;
	int i, j;
	int tempMask;

	/* multiplayer? */
	if (!curCampaign || !selMis || !baseCurrent )
		return;

	/* check for replay */
	if ((int) Cvar_VariableValue("game_tryagain")) {
		/* don't update the character stats because we retry the mission */
		CL_GameGo();
		return;
	}

	/* check for win */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: game_results <won>\n");
		return;
	}
	won = atoi(Cmd_Argv(1));

	/* update the character stats */
	CL_ParseCharacterData(NULL, qtrue);

	/* give reward, change equipment */
	CL_UpdateCredits(ccs.credits + ccs.reward);

	/* remove the dead (and their item preference) */
	for (i = 0; i < baseCurrent->numWholeTeam;) {
		if (baseCurrent->deathMask & (1 << i)) {
			Com_DPrintf("CL_GameResultsCmd - remove player %i - he died\n", i);
			baseCurrent->deathMask >>= 1;
			tempMask = baseCurrent->teamMask[baseCurrent->aircraftCurrent] >> 1;
			baseCurrent->teamMask[baseCurrent->aircraftCurrent] =
				(baseCurrent->teamMask[baseCurrent->aircraftCurrent] & ((1 << i) - 1)) | (tempMask & ~((1 << i) - 1));
			baseCurrent->numWholeTeam--;
			baseCurrent->numOnTeam[baseCurrent->aircraftCurrent]--;
			(baseCurrent->teamInv[i]).c[csi.idFloor] = NULL;
			Com_DestroyInventory(&baseCurrent->teamInv[i]);
			for (j = i; j < baseCurrent->numWholeTeam; j++) {
				baseCurrent->teamInv[j] = baseCurrent->teamInv[j + 1];
				baseCurrent->wholeTeam[j] = baseCurrent->wholeTeam[j + 1];
				baseCurrent->wholeTeam[j].inv = &baseCurrent->teamInv[j];
			}
			memset(&baseCurrent->teamInv[j], 0, sizeof(inventory_t));
		} else
			i++;
	}

	/* add recruits */
	if (won && selMis->def->recruits)
		for (i = 0; i < selMis->def->recruits; i++)
			CL_GenerateCharacter(curCampaign->team, baseCurrent, ET_ACTOR);

	/* onwin and onlose triggers */
	CP_ExecuteMissionTrigger(selMis->def, won);

	/* send the dropship back to base */
	CL_AircraftReturnToBase_f();

	/* campaign effects */
	selMis->cause->done++;
	if (selMis->cause->done >= selMis->cause->def->quota)
		CL_CampaignExecute(selMis->cause);

	/* remove mission from list */
	CL_CampaignRemoveMission(selMis);
}

/* =========================================================== */


value_t mission_vals[] = {
	{"location", V_TRANSLATION_STRING, offsetof(mission_t, location)}
	,
	{"type", V_TRANSLATION_STRING, offsetof(mission_t, type)}
	,
	{"text", V_TRANSLATION_STRING, 0} /* max length is 128 */
	,
	{"map", V_STRING, offsetof(mission_t, map)}
	,
	{"param", V_STRING, offsetof(mission_t, param)}
	,
	{"music", V_STRING, offsetof(mission_t, music)}
	,
	{"pos", V_POS, offsetof(mission_t, pos)}
	,
	{"mask", V_RGBA, offsetof(mission_t, mask)} /* color values from map mask this mission needs */
	,
	{"aliens", V_INT, offsetof(mission_t, aliens)}
	,
	{"maxugv", V_INT, offsetof(mission_t, ugv)}
	,
	{"commands", V_STRING, offsetof(mission_t, cmds)} /* commands that are excuted when this mission gets active */
	,
	{"onwin", V_STRING, offsetof(mission_t, onwin)}
	,
	{"onlose", V_STRING, offsetof(mission_t, onlose)}
	,
	{"alienteam", V_STRING, offsetof(mission_t, alienTeam)}
	,
	{"alienarmor", V_STRING, offsetof(mission_t, alienArmor)}
	,
	{"alienequip", V_STRING, offsetof(mission_t, alienEquipment)}
	,
	{"civilians", V_INT, offsetof(mission_t, civilians)}
	,
	{"civteam", V_STRING, offsetof(mission_t, civTeam)}
	,
	{"recruits", V_INT, offsetof(mission_t, recruits)}
	,
	{"storyrelated", V_BOOL, offsetof(mission_t, storyRelated)}
	,
	{"$win", V_INT, offsetof(mission_t, cr_win)}
	,
	{"$alien", V_INT, offsetof(mission_t, cr_alien)}
	,
	{"$civilian", V_INT, offsetof(mission_t, cr_civilian)}
	,
	{NULL, 0, 0}
	,
};

#define		MAX_MISSIONTEXTS	MAX_MISSIONS*128
static char missionTexts[MAX_MISSIONTEXTS];
char *mtp = missionTexts;

/**
 * @brief Adds a mission to current stageSet
 * @note the returned mission_t pointer has to be filled - this function only fills the name
 * @param[in] name valid mission name
 * @return ms is the mission_t pointer of the mission to add
 */
mission_t* CL_AddMission(char *name)
{
	int i;
	mission_t *ms;

	/* just to let us know: search for missions with same name */
	for (i = 0; i < numMissions; i++)
		if (!Q_strncmp(name, missions[i].name, MAX_VAR))
			break;
	if (i < numMissions)
		Com_DPrintf("CL_AddMission: mission def \"%s\" with same name found\n", name);

	if (numMissions >= MAX_MISSIONS) {
		Com_Printf("CL_AddMission: Max missions reached\n");
		return NULL;
	}
	/* initialize the mission */
	ms = &missions[numMissions++];
	memset(ms, 0, sizeof(mission_t));
	Q_strncpyz(ms->name, name, MAX_VAR);
	Com_DPrintf("CL_AddMission: mission name: '%s'\n", name);

	return ms;
}

/**
  * @brief
  */
void CL_ParseMission(char *name, char **text)
{
	char *errhead = "CL_ParseMission: unexptected end of file (mission ";
	mission_t *ms;
	value_t *vp;
	char *token;
	int i;

	/* search for missions with same name */
	for (i = 0; i < numMissions; i++)
		if (!Q_strncmp(name, missions[i].name, MAX_VAR))
			break;

	if (i < numMissions) {
		Com_Printf("CL_ParseMission: mission def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (numMissions >= MAX_MISSIONS) {
		Com_Printf("CL_ParseMission: Max missions reached\n");
		return;
	}

	/* initialize the mission */
	ms = &missions[numMissions++];
	memset(ms, 0, sizeof(mission_t));

	Q_strncpyz(ms->name, name, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseMission: mission def \"%s\" without body ignored\n", name);
		numMissions--;
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

				if (vp->ofs)
					Com_ParseValue(ms, token, vp->type, vp->ofs);
				else {
					if (*token == '_')
						token++;
					Q_strncpyz(mtp, _(token), 128);
					ms->text = mtp;
					do {
						mtp = strchr(mtp, '\\');
						if (mtp)
							*mtp = '\n';
					} while (mtp);
					mtp = ms->text + strlen(ms->text) + 1;
				}
				break;
			}

		if (!vp->string)
			Com_Printf("CL_ParseMission: unknown token \"%s\" ignored (mission %s)\n", token, name);

	} while (*text);
#ifdef DEBUG
	if ( abs(ms->pos[0]) > 180.0f )
		Com_Printf("Longitude for mission '%s' is bigger than 180 EW (%.0f)\n", ms->name, ms->pos[0]);
	if ( abs(ms->pos[1]) > 90.0f )
		Com_Printf("Latitude for mission '%s' is bigger than 90 NS (%.0f)\n", ms->name, ms->pos[1]);
#endif
}


/* =========================================================== */


value_t stageset_vals[] = {
	{"needed", V_STRING, offsetof(stageSet_t, needed)}
	,
	{"delay", V_DATE, offsetof(stageSet_t, delay)}
	,
	{"frame", V_DATE, offsetof(stageSet_t, frame)}
	,
	{"expire", V_DATE, offsetof(stageSet_t, expire)}
	,
	{"number", V_INT, offsetof(stageSet_t, number)}
	,
	{"quota", V_INT, offsetof(stageSet_t, quota)}
	,
	{"seq", V_STRING, offsetof(stageSet_t, sequence)}
	,
	{"nextstage", V_STRING, offsetof(stageSet_t, nextstage)}
	,
	{"endstage", V_STRING, offsetof(stageSet_t, endstage)}
	,
	{"commands", V_STRING, offsetof(stageSet_t, cmds)}
	,
	{NULL, 0, 0}
	,
};

/**
  * @brief
  */
void CL_ParseStageSet(char *name, char **text)
{
	char *errhead = "CL_ParseStageSet: unexptected end of file (stageset ";
	stageSet_t *sp;
	value_t *vp;
	char missionstr[256];
	char *token, *misp;
	int j;

	if (numStageSets >= MAX_STAGESETS) {
		Com_Printf("CL_ParseStageSet: Max stagesets reached\n");
		return;
	}

	/* initialize the stage */
	sp = &stageSets[numStageSets++];
	memset(sp, 0, sizeof(stageSet_t));
	Q_strncpyz(sp->name, name, MAX_VAR);

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

				Com_ParseValue(sp, token, vp->type, vp->ofs);
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
					Com_Printf("Com_ParseStageSet: unknown mission \"%s\" ignored (stageset %s)\n", token, name);
			}
			while (misp && sp->numMissions < MAX_SETMISSIONS);
			continue;
		}

		Com_Printf("Com_ParseStageSet: unknown token \"%s\" ignored (stageset %s)\n", token, name);
	} while (*text);
}


/**
  * @brief
  */
void CL_ParseStage(char *name, char **text)
{
	char *errhead = "CL_ParseStage: unexptected end of file (stage ";
	stage_t *sp;
	char *token;
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
	Q_strncpyz(sp->name, name, MAX_VAR);
	sp->first = numStageSets;

	Com_DPrintf("stage: %s\n", name);

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

value_t campaign_vals[] = {
	{"team", V_STRING, offsetof(campaign_t, team)}
	,
	{"soldiers", V_INT, offsetof(campaign_t, soldiers)}
	,
	{"ugvs", V_INT, offsetof(campaign_t, ugvs)}
	,
	{"equipment", V_STRING, offsetof(campaign_t, equipment)}
	,
	{"market", V_STRING, offsetof(campaign_t, market)}
	,
	{"difficulty", V_INT, offsetof(campaign_t, difficulty)}
	,
	{"firststage", V_STRING, offsetof(campaign_t, firststage)}
	,
	{"map", V_STRING, offsetof(campaign_t, map)}
	,
	{"credits", V_INT, offsetof(campaign_t, credits)}
	,
	{"visible", V_BOOL, offsetof(campaign_t, visible)}
	,
	{"text", V_TRANSLATION2_STRING, offsetof(campaign_t, text)}
	,							/* just a gettext placeholder */
	{"name", V_TRANSLATION_STRING, offsetof(campaign_t, name)}
	,
	{"date", V_DATE, offsetof(campaign_t, date)}
	,
	{NULL, 0, 0}
	,
};

/**
  * @brief
  */
void CL_ParseCampaign(char *id, char **text)
{
	char *errhead = "CL_ParseCampaign: unexptected end of file (campaign ";
	campaign_t *cp;
	value_t *vp;
	char *token;
	int i;

	/* search for campaigns with same name */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strncmp(id, campaigns[i].id, MAX_VAR))
			break;

	if (i < numCampaigns) {
		Com_Printf("CL_ParseCampaign: campaign def \"%s\" with same name found, second ignored\n", id);
		return;
	}

	if (numCampaigns >= MAX_CAMPAIGNS) {
		Com_Printf("CL_ParseCampaign: Max campaigns reached (%i)\n", MAX_CAMPAIGNS);
		return;
	}

	/* initialize the menu */
	cp = &campaigns[numCampaigns++];
	memset(cp, 0, sizeof(campaign_t));

	Q_strncpyz(cp->id, id, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseCampaign: campaign def \"%s\" without body ignored\n", id);
		numCampaigns--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, id);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = campaign_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;

				Com_ParseValue(cp, token, vp->type, vp->ofs);
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseCampaign: unknown token \"%s\" ignored (campaign %s)\n", token, id);
			COM_EParse(text, errhead, id);
		}
	} while (*text);
}

/* =========================================================== */

value_t nation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(nation_t, name)}
	,
	{"color", V_COLOR, offsetof(nation_t, color)}
	,
	{"funding", V_INT, offsetof(nation_t, funding)}
	,
	{"alien_friendly", V_FLOAT, offsetof(nation_t, alienFriendly)}
	,
	{"soldiers", V_INT, offsetof(nation_t, soldiers)}
	,
	{"scientists", V_INT, offsetof(nation_t, scientists)}
	,
	{"names", V_INT, offsetof(nation_t, names)}
	,

	{NULL, 0, 0}
	,
};

/**
  * @brief
  */
void CL_ParseNations(char *name, char **text)
{
	char *errhead = "CL_ParseNations: unexptected end of file (aircraft ";
	nation_t *nation;
	value_t *vp;
	char *token;

	if (numNations >= MAX_NATIONS) {
		Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the menu */
	nation = &nations[numNations++];
	memset(nation, 0, sizeof(nation_t));

	Com_DPrintf("...found nation %s\n", name);
	Q_strncpyz(nation->id, name, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseNations: nation def \"%s\" without body ignored\n", name);
		numNations--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = nation_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(nation, token, vp->type, vp->ofs);
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseNations: unknown token \"%s\" ignored (nation %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}


/**
  * @brief
  *
  * returns true when we are not in battlefield
  * TODO: Check cvar mn_main for value
  */
qboolean CL_OnBattlescape(void)
{
	/* sv.state is set to zero on every battlefield shutdown */
	if (Com_ServerState() > 0)
		return qtrue;

	return qfalse;
}

/* ===================================================================== */

/* these commands are only available in singleplayer */
cmdList_t game_commands[] = {
	{"aircraft_start", CL_AircraftStart_f}
	,
	{"aircraftlist", CL_ListAircraft_f}
	,
	{"aircraft_select", CL_AircraftSelect}
	,
	{"aircraft_init", CL_AircraftInit}
	,
	{"airequip_init", CL_AircraftEquipmenuMenuInit_f}
	,
	{"airequip_weapons_click", CL_AircraftEquipmenuMenuWeaponsClick_f}
	,
	{"airequip_shields_click", CL_AircraftEquipmenuMenuShieldsClick_f}
	,
	{"mn_next_aircraft", MN_NextAircraft_f}
	,
	{"mn_prev_aircraft", MN_PrevAircraft_f}
	,
	{"aircraft_new", CL_NewAircraft_f}
	,
	{"aircraft_return", CL_AircraftReturnToBase_f}
	,
	{"aircraft_list", CL_BuildingAircraftList_f}
	,
	{"stats_update", CL_Stats_Update}
	,
	{"game_go", CL_GameGo}
	,
	{"game_auto_check", CL_GameAutoCheck}
	,
	{"game_auto_go", CL_GameAutoGo}
	,
	{"game_abort", CL_GameAbort}
	,
	{"game_results", CL_GameResultsCmd}
	,
	{"game_timestop", CL_GameTimeStop}
	,
	{"game_timeslow", CL_GameTimeSlow}
	,
	{"game_timefast", CL_GameTimeFast}
	,
	{"inc_sensor", B_SetSensor}
	,
	{"dec_sensor", B_SetSensor}
	,
	{"mn_mapaction_reset", MAP_ResetAction}
	,
	{NULL, NULL}
};

/**
  * @brief
  */
static void CL_GameExit(void)
{
	cmdList_t *commands;

	Cbuf_AddText("disconnect\n");
	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_active", "");
	ccs.singleplayer = qfalse;
	MN_ShutdownMessageSystem();
	CL_InitMessageSystem();
	/* singleplayer commands are no longer available */
	if (curCampaign)
		for (commands = game_commands; commands->name; commands++)
			Cmd_RemoveCommand(commands->name);
	curCampaign = NULL;
}

/**
  * @brief Called at new game and load game
  */
void CL_GameInit ( void )
{
	cmdList_t *commands;

	for (commands = game_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function);

	CL_GameTimeStop();

	/* init research tree */
	RS_AddObjectTechs();
	RS_InitTree();

	/* after inited the techtree */
	/* we can assign the weapons */
	/* and shields to aircrafts. */
	CL_AircraftInit();

	/* init employee list */
	B_InitEmployees();

	/* Init popup and map/geoscape */
	CL_PopupInit();
	MAP_GameInit();
}

/**
  * @brief
  */
static void CL_GameNew(void)
{
	equipDef_t *ed;
	char *name;
	int i;
	char val[32];

	Cvar_Set("mn_main", "singleplayer");
	Cvar_Set("mn_active", "map");
	Cvar_SetValue("maxclients", 1.0f);

	/* exit running game */
	if (curCampaign)
		CL_GameExit();
	ccs.singleplayer = qtrue;

	memset(&gd,0,sizeof(gd));
	CL_ReadSinglePlayerData();

	/* get campaign */
	name = Cvar_VariableString("campaign");
	for (i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++)
		if (!Q_strncmp(name, curCampaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("CL_GameNew: Campaign \"%s\" doesn't exist.\n", name);
		return;
	}

	/* difficulty */
	if ((int) difficulty->value < -3)
		Cvar_ForceSet("difficulty", "-3");
	else if ((int) difficulty->value > 3)
		Cvar_ForceSet("difficulty", "3");
	Com_sprintf(val, sizeof(val), "%i", curCampaign->difficulty);
	Cvar_ForceSet("difficulty", val);

	re.LoadTGA(va("pics/menu/%s_mask.tga", curCampaign->map), &maskPic, &maskWidth, &maskHeight);
	if (!maskPic)
		Sys_Error("Couldn't load map mask %s_mask.tga in pics/menu\n", curCampaign->map);

	/* base setup */
	gd.numBases = 0;
	B_NewBases();
	PR_ProductionInit();

	/* reset, set time */
	selMis = NULL;
	memset(&ccs, 0, sizeof(ccs_t));
	ccs.date = curCampaign->date;

	/* set map view */
	ccs.center[0] = 0.5;
	ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	CL_UpdateCredits(curCampaign->credits);

	/* equipment */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(curCampaign->equipment, ed->name, MAX_VAR))
			break;
	if (i != csi.numEDs)
		ccs.eCampaign = *ed;

	/* market */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(curCampaign->market, ed->name, MAX_VAR))
			break;
	if (i != csi.numEDs)
		ccs.eMarket = *ed;

	/* stage setup */
	CL_CampaignActivateStage(curCampaign->firststage, qtrue);

	MN_PopMenu(qtrue);
	MN_PushMenu("map");

	/* create a base as first step */
	Cbuf_ExecuteText(EXEC_NOW, "mn_select_base -1");

	CL_GameInit();
}

/**
  * @brief fill a list with available campaigns
  */
#define MAXCAMPAIGNTEXT 4096
static char campaignText[MAXCAMPAIGNTEXT];
static char campaignDesc[MAXCAMPAIGNTEXT];
static void CP_GetCampaigns_f(void)
{
	int i;

	*campaignText = *campaignDesc = '\0';
	for (i = 0; i < numCampaigns; i++) {
		if ( campaigns[i].visible )
			Q_strcat(campaignText, va("%s\n", campaigns[i].name), MAXCAMPAIGNTEXT);
	}
	/* default campaign */
	Cvar_Set("campaign", "main");
	menuText[TEXT_STANDARD] = campaignDesc;

	/* select main as default */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strncmp("main", campaigns[i].id, MAX_VAR)) {
			Com_sprintf(campaignDesc, MAXCAMPAIGNTEXT, _("Race: %s\nRecruits: %i\nCredits: %ic\nDifficulty: %s\n%s\n"), campaigns[i].team, campaigns[i].soldiers, campaigns[i].credits, CL_ToDifficultyName(campaigns[i].difficulty), _(campaigns[i].text));
			break;
		}

}

/**
  * @brief Script function to select a campaign from campaign list
  */
static void CP_CampaignsClick_f(void)
{
	int num;

	if (Cmd_Argc() < 2)
		return;

	/*which building? */
	num = atoi(Cmd_Argv(1));

	/* jump over all invisible campaigns */
	while ( !campaigns[num].visible ) {
		num++;
		if ( num >= numCampaigns )
			return;
	}

	if (num >= numCampaigns || num < 0)
		return;

	Cvar_Set("campaign", campaigns[num].id);
	/* FIXME: Translate the race to the name of a race */
	Com_sprintf(campaignDesc, MAXCAMPAIGNTEXT, _("Race: %s\nRecruits: %i\nCredits: %ic\nDifficulty: %s\n%s\n"), campaigns[num].team, campaigns[num].soldiers, campaigns[num].credits, CL_ToDifficultyName(campaigns[num].difficulty), _(campaigns[num].text));
	menuText[TEXT_STANDARD] = campaignDesc;
}

/**
  * @brief Will clear most of the parsed singleplayer data
  * @sa Com_InitInventory
  */
void CL_ResetSinglePlayerData ( void )
{
	numNations = numStageSets = numStages = numMissions = 0;
	memset(missions, 0, sizeof(mission_t)*MAX_MISSIONS);
	memset(nations, 0, sizeof(nation_t)*MAX_NATIONS);
	memset(stageSets, 0, sizeof(stageSet_t)*MAX_STAGESETS);
	memset(stages, 0, sizeof(stage_t)*MAX_STAGES);
	memset(&invList,0,sizeof(invList));
	Com_InitInventory(invList);
}

/**
  * @brief Show campaign stats in console
  *
  * call this function via campaign_stats
  */
static void CP_CampaignStats ( void )
{
	setState_t *set;
	int i;

	if ( !curCampaign ) {
		Com_Printf("No campaign active\n");
		return;
	}

	Com_Printf("Campaign id: %s\n", curCampaign->id);
	Com_Printf("..equipment: %s\n", curCampaign->equipment);
	Com_Printf("..team: %s\n", curCampaign->team);

	Com_Printf("..active stage: %s\n", testStage->name );
	for (i = 0, set = &ccs.set[testStage->first]; i < testStage->num; i++, set++) {
		Com_Printf("....name: %s\n", set->def->name);
		Com_Printf("......needed: %s\n", set->def->needed);
		Com_Printf("......quote: %i\n", set->def->quota);
		Com_Printf("......done: %i\n", set->done);
	}
}

/**
  * @brief
  */
void CL_ResetCampaign(void)
{
	/* reset some vars */
	curCampaign = NULL;
	baseCurrent = NULL;
	menuText[TEXT_CAMPAIGN_LIST] = campaignText;

	/* commands */
	Cmd_AddCommand("campaign_stats", CP_CampaignStats );
	Cmd_AddCommand("campaignlist_click", CP_CampaignsClick_f);
	Cmd_AddCommand("getcampaigns", CP_GetCampaigns_f);
	Cmd_AddCommand("game_new", CL_GameNew);
	Cmd_AddCommand("game_continue", CL_GameContinue);
	Cmd_AddCommand("game_exit", CL_GameExit);
	Cmd_AddCommand("game_save", CL_GameSaveCmd);
	Cmd_AddCommand("game_load", CL_GameLoadCmd);
	Cmd_AddCommand("game_comments", CL_GameCommentsCmd);
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CL_DebugChangeCharacterStats_f);
#endif
}
