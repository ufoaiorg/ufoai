/**
 * @file scp_missions.c
 * @brief Singleplayer static campaign mission code
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

#include "scp_missions.h"
#include "../campaign/cp_time.h"
#include "../campaign/cp_missions.h"
#include "scp_shared.h"
#include "../../../common/binaryexpressionparser.h"

static qboolean SCP_StageSetDone (const char *name)
{
	setState_t *set;
	int i;

	for (i = 0, set = &scd->set[scd->testStage->first]; i < scd->testStage->num; i++, set++)
		if (Q_streq(name, set->def->name))
			return set->done >= set->def->quota;

	/* didn't find set */
	return qfalse;
}

static void SCP_CampaignActivateStageSets (stage_t *stage)
{
	setState_t *set;
	int i;

	scd->testStage = stage;
	for (i = 0, set = &scd->set[stage->first]; i < stage->num; i++, set++) {
		if (!set->active && !set->done && !set->num) {
			const date_t zero = {0, 0};
			int i;
			/* check needed sets */
			if (set->def->needed[0] != '\0' && !BEP_Evaluate(set->def->needed, SCP_StageSetDone))
				continue;

			Com_Printf("activate stage set '%s'\n", set->def->name);
			/* activate it */
			set->active = qtrue;
			set->start = Date_Add(ccs.date, set->def->delay);
			set->event = Date_Add(set->start, Date_Random(zero, set->def->frame));
			for (i = 0; i < set->def->ufos; i++) {
				const float r = frand();
				interestCategory_t category = INTERESTCATEGORY_TERROR_ATTACK;
				if (r > 0.9f)
					category = INTERESTCATEGORY_BASE_ATTACK;
				else if (r > 0.6f)
					category = INTERESTCATEGORY_INTERCEPT;
				CP_CreateNewMission(category, qtrue);
			}
		}
	}
}

static stageState_t *SCP_CampaignActivateStage (const char *name)
{
	stage_t *stage;
	stageState_t *state;
	int i, j;

	Com_Printf("activate stage '%s'\n", name);

	for (i = 0, stage = scd->stages; i < scd->numStages; i++, stage++) {
		if (Q_streq(stage->name, name)) {
			/* add it to the list */
			state = &scd->stage[i];
			state->active = qtrue;
			state->def = stage;
			state->start = ccs.date;

			/* add stage sets */
			for (j = stage->first; j < stage->first + stage->num; j++) {
				setState_t *set = &scd->set[j];
				OBJZERO(*set);
				set->stage = stage;
				set->def = &scd->stageSets[j];
			}

			/* activate stage sets */
			SCP_CampaignActivateStageSets(stage);
			return state;
		}
	}

	Com_Printf("SCP_CampaignActivateStage: stage '%s' not found.\n", name);
	return NULL;
}

static void SCP_CampaignEndStage (const char *name)
{
	stageState_t *state;
	int i;

	for (i = 0, state = scd->stage; i < scd->numStages; i++, state++)
		if (Q_streq(state->def->name, name)) {
			state->active = qfalse;
			return;
		}

	Com_Printf("SCP_CampaignEndStage: stage '%s' not found.\n", name);
}

static void SCP_CampaignExecute (setState_t *set)
{
	/* handle stages, execute commands */
	if (set->def->nextstage[0] != '\0')
		SCP_CampaignActivateStage(set->def->nextstage);

	if (set->def->endstage[0] != '\0')
		SCP_CampaignEndStage(set->def->endstage);

	if (set->def->cmds[0] != '\0')
		Cmd_ExecuteString(set->def->cmds);

	/* activate new sets in old stage */
	SCP_CampaignActivateStageSets(set->stage);
}


static staticMission_t* SCP_GetMission_r (setState_t *set, int defIndex, int run)
{
	const int misIndex = set->def->missions[defIndex];
	staticMission_t *mis = &scd->missions[misIndex];
	if (mis->count && run < set->def->numMissions) {
		Com_Printf("mission: %s already used: %i\n", mis->id, run);
		defIndex++;
		defIndex %= set->def->numMissions;
		return SCP_GetMission_r(set, defIndex, ++run);
	}
	return mis;
}

static staticMission_t* SCP_GetMission (setState_t *set)
{
	const int defIndex = (int) (set->def->numMissions * frand());
	return SCP_GetMission_r(set, defIndex, 0);
}

static void SCP_CampaignAddMission (setState_t *set)
{
	actMis_t *mis;

	/* add mission */
	if (scd->numActiveMissions >= MAX_ACTMISSIONS) {
		return;
	}

	mis = &scd->activeMissions[scd->numActiveMissions];
	OBJZERO(*mis);

	/* set relevant info */
	mis->def = SCP_GetMission(set);
	if (mis->def == NULL) {
		return;
	}
	mis->cause = set;

	if (set->def->expire.day)
		mis->expire = Date_Add(ccs.date, set->def->expire);

	/* prepare next event (if any) */
	set->num++;
	if (set->def->number && set->num >= set->def->number) {
		set->active = qfalse;
	} else {
		const date_t minTime = {0, 0};
		set->event = Date_Add(ccs.date, Date_Random(minTime, set->def->frame));
	}

	mission_t * mission = CP_CreateNewMission(INTERESTCATEGORY_TERROR_ATTACK, qtrue);
	mission->mapDef = Com_GetMapDefinitionByID(mis->def->id);
	if (!mission->mapDef) {
		Com_Printf("SCP_CampaignAddMission: Could not get the mapdef '%s'\n", mis->def->id);
		CP_MissionRemove(mission);
		return;
	}
	Vector2Copy(mis->def->pos, mission->pos);
	mission->posAssigned = qtrue;
	CP_TerrorMissionStart(mission);
	mission->finalDate = mis->expire;
	mis->mission = mission;

	Com_Printf("spawned map '%s'\n", mis->def->id);

	scd->numActiveMissions++;
}

static void SCP_CampaignRemoveMission (actMis_t *mis)
{
	int i;
	const int num = mis - scd->activeMissions;
	if (num >= scd->numActiveMissions) {
		Com_Printf("SCP_CampaignRemoveMission: Can't remove activeMissions.\n");
		return;
	}

	scd->numActiveMissions--;
	for (i = num; i < scd->numActiveMissions; i++)
		scd->activeMissions[i] = scd->activeMissions[i + 1];
}

void SCP_SpawnNewMissions (void)
{
	stageState_t *stage;
	actMis_t *mis;
	int i, j;

	/* check campaign events */
	for (i = 0, stage = scd->stage; i < scd->numStages; i++, stage++) {
		setState_t *set;
		if (stage->active) {
			for (j = 0, set = &scd->set[stage->def->first]; j < stage->def->num; j++, set++) {
				if (set->active && set->event.day && Date_LaterThan(&ccs.date, &set->event)) {
					if (set->def->numMissions) {
						SCP_CampaignAddMission(set);
					} else {
						SCP_CampaignExecute(set);
					}
				}
			}
		}
	}

	/* let missions expire */
	for (i = 0, mis = scd->activeMissions; i < scd->numActiveMissions; i++, mis++) {
		if (mis->expire.day && Date_LaterThan(&ccs.date, &mis->expire)) {
			SCP_CampaignRemoveMission(mis);
		}
	}
}

void SCP_CampaignActivateFirstStage (void)
{
	SCP_CampaignActivateStage("intro");
}

void SCP_CampaignProgress (const missionResults_t *results)
{
	actMis_t *mission;
	int i;

	for (i = 0; i < scd->numActiveMissions; i++) {
		mission = &scd->activeMissions[i];
		if (mission->mission == results->mission) {
			break;
		}
	}

	if (i == scd->numActiveMissions) {
		Com_Printf("SCP_CampaignProgress: Could not find an active mission\n");
		return;
	}

	/* campaign effects */
	if (results->won)
		mission->cause->done++;
	mission->def->count++;
	Com_Printf("finished map '%s'\n", mission->def->id);
	if (mission->cause->done >= mission->cause->def->quota)
		SCP_CampaignExecute(mission->cause);
	else
		Com_Printf("%i missions left to do\n", mission->cause->def->quota - mission->cause->done);

	/* remove activeMissions from list */
	SCP_CampaignRemoveMission(mission);
}
