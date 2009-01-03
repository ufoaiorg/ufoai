/**
 * @file cp_parse.c
 * @brief Campaign parsing code
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

#include "../client.h"
#include "../cl_global.h"
#include "cp_parse.h"

/**
 * @return Alien mission category
 * @sa interestCategory_t
 */
static int CL_GetAlienMissionTypeByID (const char *type)
{
	if (!Q_strncmp(type, "recon", MAX_VAR))
		return INTERESTCATEGORY_RECON;
	else if (!Q_strncmp(type, "terror", MAX_VAR))
		return INTERESTCATEGORY_TERROR_ATTACK;
	else if (!Q_strncmp(type, "baseattack", MAX_VAR))
		return INTERESTCATEGORY_BASE_ATTACK;
	else if (!Q_strncmp(type, "building", MAX_VAR))
		return INTERESTCATEGORY_BUILDING;
	else if (!Q_strncmp(type, "supply", MAX_VAR))
		return INTERESTCATEGORY_SUPPLY;
	else if (!Q_strncmp(type, "xvi", MAX_VAR))
		return INTERESTCATEGORY_XVI;
	else if (!Q_strncmp(type, "intercept", MAX_VAR))
		return INTERESTCATEGORY_INTERCEPT;
	else if (!Q_strncmp(type, "harvest", MAX_VAR))
		return INTERESTCATEGORY_HARVEST;
	else if (!Q_strncmp(type, "alienbase", MAX_VAR))
		return INTERESTCATEGORY_ALIENBASE;
	else {
		Com_Printf("CL_GetAlienMissionTypeByID: unknown alien mission category '%s'\n", type);
		return INTERESTCATEGORY_NONE;
	}
}

static const value_t alien_group_vals[] = {
	{"mininterest", V_INT, offsetof(alienTeamGroup_t, minInterest), 0},
	{"maxinterest", V_INT, offsetof(alienTeamGroup_t, maxInterest), 0},
	{NULL, 0, 0, 0}
};

/**
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseAlienTeam (const char *name, const char **text)
{
	const char *errhead = "CL_ParseAlienTeam: unexpected end of file (alienteam ";
	const char *token;
	const value_t *vp;
	int i;
	alienTeamCategory_t *alienCategory;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseAlienTeam: alien team category \"%s\" without body ignored\n", name);
		return;
	}

	if (gd.numAlienCategories >= ALIENCATEGORY_MAX) {
		Com_Printf("CL_ParseAlienTeam: maximum number of alien team category reached (%i)\n", ALIENCATEGORY_MAX);
		return;
	}

	/* search for category with same name */
	for (i = 0; i < gd.numAlienCategories; i++)
		if (!Q_strncmp(name, gd.alienCategories[i].id, sizeof(gd.alienCategories[i].id)))
			break;
	if (i < gd.numAlienCategories) {
		Com_Printf("CL_ParseAlienTeam: alien category def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	alienCategory = &gd.alienCategories[gd.numAlienCategories++];
	Q_strncpyz(alienCategory->id, name, sizeof(alienCategory->id));

	do {
		linkedList_t **list;
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Q_strcmp(token, "equipment")) {
			list = &alienCategory->equipment;
			token = COM_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("CL_ParseAlienTeam: alien team category \"%s\" has equipment with no opening brace\n", name);
				break;
			}
			do {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token == '}')
					break;
				LIST_AddString(list, token);
			} while (*text);
		} else if (!Q_strcmp(token, "category")) {
			token = COM_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("CL_ParseAlienTeam: alien team category \"%s\" has category with no opening brace\n", name);
				break;
			}
			do {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token == '}')
					break;
				alienCategory->missionCategories[alienCategory->numMissionCategories] = CL_GetAlienMissionTypeByID(token);
				if (alienCategory->missionCategories[alienCategory->numMissionCategories] == INTERESTCATEGORY_NONE)
					Com_Printf("CL_ParseAlienTeam: alien team category \"%s\" is used with no mission category. It won't be used in game.\n", name);
				alienCategory->numMissionCategories++;
			} while (*text);
		} else if (!Q_strcmp(token, "team")) {
			alienTeamGroup_t *group;

			token = COM_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("CL_ParseAlienTeam: alien team \"%s\" has team with no opening brace\n", name);
				break;
			}

			if (alienCategory->numAlienTeamGroups >= MAX_ALIEN_GROUP_PER_CATEGORY) {
				Com_Printf("CL_ParseAlienTeam: maximum number of alien team reached (%i) in category \"%s\"\n", MAX_ALIEN_GROUP_PER_CATEGORY, name);
				break;
			}

			group = &alienCategory->alienTeamGroups[alienCategory->numAlienTeamGroups];
			group->idx = alienCategory->numAlienTeamGroups;
			group->categoryIdx = alienCategory - gd.alienCategories;
			alienCategory->numAlienTeamGroups++;

			do {
				token = COM_EParse(text, errhead, name);

				/* check for some standard values */
				for (vp = alien_group_vals; vp->string; vp++)
					if (!Q_strcmp(token, vp->string)) {
						/* found a definition */
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;

						Com_EParseValue(group, token, vp->type, vp->ofs, vp->size);
						break;
					}

				if (!vp->string) {
					teamDef_t *teamDef;
					if (!*text || *token == '}')
						break;

					/* This is an alien team */
					if (group->numAlienTeams >= MAX_TEAMS_PER_MISSION)
						Sys_Error("CL_ParseAlienTeam: MAX_TEAMS_PER_MISSION hit");
					teamDef = Com_GetTeamDefinitionByID(token);
					if (teamDef)
						group->alienTeams[group->numAlienTeams++] = teamDef;
				}
			} while (*text);
		} else {
			Com_Printf("CL_ParseAlienTeam: unknown token \"%s\" ignored (category %s)\n", token, name);
			continue;
		}
	} while (*text);
}

/**
 * @brief This function parses a list of items that should be set to researched = true after campaign start
 * @todo Implement the use of this function
 */
void CL_ParseResearchedCampaignItems (const char *name, const char **text)
{
	const char *errhead = "CL_ParseResearchedCampaignItems: unexpected end of file (equipment ";
	const char *token;
	int i;
	campaign_t* campaign;
	technology_t *tech;

	campaign = CL_GetCampaign(cl_campaign->string);
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

		for (i = 0; i < gd.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
			assert(tech);
			if (!Q_strncmp(token, tech->id, MAX_VAR)) {
				tech->mailSent = MAILSENT_FINISHED;
				tech->markResearched.markOnly[tech->markResearched.numDefinitions] = qtrue;
				tech->markResearched.campaign[tech->markResearched.numDefinitions] = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
				tech->markResearched.numDefinitions++;
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", tech->id);
				break;
			}
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
	campaign_t* campaign;
	technology_t *tech;

	campaign = CL_GetCampaign(cl_campaign->string);
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

		for (i = 0; i < gd.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
			if (!Q_strncmp(token, tech->id, MAX_VAR)) {
				if (researchable) {
					tech->mailSent = MAILSENT_PROPOSAL;
					RS_MarkOneResearchable(tech);
				} else
					Com_Printf("@todo Mark unresearchable");
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", tech->id);
				break;
			}
		}

		if (i == gd.numTechnologies)
			Com_Printf("CL_ParseResearchableCampaignStates: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/* =========================================================== */

static const value_t salary_vals[] = {
	{"soldier_base", V_INT, offsetof(salary_t, soldier_base), MEMBER_SIZEOF(salary_t, soldier_base)},
	{"soldier_rankbonus", V_INT, offsetof(salary_t, soldier_rankbonus), MEMBER_SIZEOF(salary_t, soldier_rankbonus)},
	{"worker_base", V_INT, offsetof(salary_t, worker_base), MEMBER_SIZEOF(salary_t, worker_base)},
	{"worker_rankbonus", V_INT, offsetof(salary_t, worker_rankbonus), MEMBER_SIZEOF(salary_t, worker_rankbonus)},
	{"scientist_base", V_INT, offsetof(salary_t, scientist_base), MEMBER_SIZEOF(salary_t, scientist_base)},
	{"scientist_rankbonus", V_INT, offsetof(salary_t, scientist_rankbonus), MEMBER_SIZEOF(salary_t, scientist_rankbonus)},
	{"pilot_base", V_INT, offsetof(salary_t, pilot_base), MEMBER_SIZEOF(salary_t, pilot_base)},
	{"pilot_rankbonus", V_INT, offsetof(salary_t, pilot_rankbonus), MEMBER_SIZEOF(salary_t, pilot_rankbonus)},
	{"robot_base", V_INT, offsetof(salary_t, robot_base), MEMBER_SIZEOF(salary_t, robot_base)},
	{"robot_rankbonus", V_INT, offsetof(salary_t, robot_rankbonus), MEMBER_SIZEOF(salary_t, robot_rankbonus)},
	{"aircraft_factor", V_INT, offsetof(salary_t, aircraft_factor), MEMBER_SIZEOF(salary_t, aircraft_factor)},
	{"aircraft_divisor", V_INT, offsetof(salary_t, aircraft_divisor), MEMBER_SIZEOF(salary_t, aircraft_divisor)},
	{"base_upkeep", V_INT, offsetof(salary_t, base_upkeep), MEMBER_SIZEOF(salary_t, base_upkeep)},
	{"admin_initial", V_INT, offsetof(salary_t, admin_initial), MEMBER_SIZEOF(salary_t, admin_initial)},
	{"admin_soldier", V_INT, offsetof(salary_t, admin_soldier), MEMBER_SIZEOF(salary_t, admin_soldier)},
	{"admin_worker", V_INT, offsetof(salary_t, admin_worker), MEMBER_SIZEOF(salary_t, admin_worker)},
	{"admin_scientist", V_INT, offsetof(salary_t, admin_scientist), MEMBER_SIZEOF(salary_t, admin_scientist)},
	{"admin_pilot", V_INT, offsetof(salary_t, admin_pilot), MEMBER_SIZEOF(salary_t, admin_pilot)},
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

				Com_EParseValue(s, token, vp->type, vp->ofs, vp->size);
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
	{"xvirate", V_INT, offsetof(campaign_t, maxAllowedXVIRateUntilLost), MEMBER_SIZEOF(campaign_t, maxAllowedXVIRateUntilLost)},
	{"maxdebts", V_INT, offsetof(campaign_t, negativeCreditsUntilLost), MEMBER_SIZEOF(campaign_t, negativeCreditsUntilLost)},
	{"minhappiness", V_FLOAT, offsetof(campaign_t, minhappiness), MEMBER_SIZEOF(campaign_t, minhappiness)},
	{"scientists", V_INT, offsetof(campaign_t, scientists), MEMBER_SIZEOF(campaign_t, scientists)},
	{"ugvs", V_INT, offsetof(campaign_t, ugvs), MEMBER_SIZEOF(campaign_t, ugvs)},
	{"equipment", V_STRING, offsetof(campaign_t, equipment), 0},
	{"market", V_STRING, offsetof(campaign_t, market), 0},
	{"asymptotic_market", V_STRING, offsetof(campaign_t, asymptoticMarket), 0},
	{"researched", V_STRING, offsetof(campaign_t, researched), 0},
	{"difficulty", V_INT, offsetof(campaign_t, difficulty), MEMBER_SIZEOF(campaign_t, difficulty)},
	{"map", V_STRING, offsetof(campaign_t, map), 0},
	{"credits", V_INT, offsetof(campaign_t, credits), MEMBER_SIZEOF(campaign_t, credits)},
	{"visible", V_BOOL, offsetof(campaign_t, visible), MEMBER_SIZEOF(campaign_t, visible)},
	{"text", V_TRANSLATION_MANUAL_STRING, offsetof(campaign_t, text), 0}, /* just a gettext placeholder */
	{"name", V_TRANSLATION_STRING, offsetof(campaign_t, name), 0},
	{"date", V_DATE, offsetof(campaign_t, date), 0},
	{"basecost", V_INT, offsetof(campaign_t, basecost), MEMBER_SIZEOF(campaign_t, basecost)},
	{"firstbase", V_STRING, offsetof(campaign_t, firstBaseTemplate), 0},
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
	memset(cp, 0, sizeof(*cp));

	cp->idx = numCampaigns - 1;
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
	memset(s, 0, sizeof(*s));
	s->soldier_base = 3000;
	s->soldier_rankbonus = 500;
	s->worker_base = 3000;
	s->worker_rankbonus = 500;
	s->scientist_base = 3000;
	s->scientist_rankbonus = 500;
	s->pilot_base = 3000;
	s->pilot_rankbonus = 500;
	s->robot_base = 7500;
	s->robot_rankbonus = 1500;
	s->aircraft_factor = 1;
	s->aircraft_divisor = 25;
	s->base_upkeep = 20000;
	s->admin_initial = 1000;
	s->admin_soldier = 75;
	s->admin_worker = 75;
	s->admin_scientist = 75;
	s->admin_pilot = 75;
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

				Com_EParseValue(cp, token, vp->type, vp->ofs, vp->size);
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
