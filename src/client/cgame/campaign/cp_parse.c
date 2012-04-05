/**
 * @file cp_parse.c
 * @brief Campaign parsing code
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_rank.h"
#include "cp_parse.h"
#include "../../cl_inventory.h" /* INV_GetEquipmentDefinitionByID */

/**
 * @return Alien mission category
 * @sa interestCategory_t
 */
static int CP_GetAlienMissionTypeByID (const char *type)
{
	if (Q_streq(type, "recon"))
		return INTERESTCATEGORY_RECON;
	else if (Q_streq(type, "terror"))
		return INTERESTCATEGORY_TERROR_ATTACK;
	else if (Q_streq(type, "baseattack"))
		return INTERESTCATEGORY_BASE_ATTACK;
	else if (Q_streq(type, "building"))
		return INTERESTCATEGORY_BUILDING;
	else if (Q_streq(type, "supply"))
		return INTERESTCATEGORY_SUPPLY;
	else if (Q_streq(type, "xvi"))
		return INTERESTCATEGORY_XVI;
	else if (Q_streq(type, "intercept"))
		return INTERESTCATEGORY_INTERCEPT;
	else if (Q_streq(type, "harvest"))
		return INTERESTCATEGORY_HARVEST;
	else if (Q_streq(type, "alienbase"))
		return INTERESTCATEGORY_ALIENBASE;
	else if (Q_streq(type, "rescue"))
		return INTERESTCATEGORY_RESCUE;
	else {
		Com_Printf("CP_GetAlienMissionTypeByID: unknown alien mission category '%s'\n", type);
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
 */
static void CP_ParseAlienTeam (const char *name, const char **text)
{
	const char *errhead = "CP_ParseAlienTeam: unexpected end of file (alienteam ";
	const char *token;
	int i;
	alienTeamCategory_t *alienCategory;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CP_ParseAlienTeam: alien team category \"%s\" without body ignored\n", name);
		return;
	}

	if (ccs.numAlienCategories >= ALIENCATEGORY_MAX) {
		Com_Printf("CP_ParseAlienTeam: maximum number of alien team category reached (%i)\n", ALIENCATEGORY_MAX);
		return;
	}

	/* search for category with same name */
	for (i = 0; i < ccs.numAlienCategories; i++)
		if (Q_streq(name, ccs.alienCategories[i].id))
			break;
	if (i < ccs.numAlienCategories) {
		Com_Printf("CP_ParseAlienTeam: alien category def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	alienCategory = &ccs.alienCategories[ccs.numAlienCategories++];
	Q_strncpyz(alienCategory->id, name, sizeof(alienCategory->id));

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (Q_streq(token, "equipment")) {
			linkedList_t **list = &alienCategory->equipment;
			token = Com_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("CP_ParseAlienTeam: alien team category \"%s\" has equipment with no opening brace\n", name);
				break;
			}
			do {
				token = Com_EParse(text, errhead, name);
				if (!*text || *token == '}')
					break;
				LIST_AddString(list, token);
			} while (*text);
		} else if (Q_streq(token, "category")) {
			token = Com_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("CP_ParseAlienTeam: alien team category \"%s\" has category with no opening brace\n", name);
				break;
			}
			do {
				token = Com_EParse(text, errhead, name);
				if (!*text || *token == '}')
					break;
				alienCategory->missionCategories[alienCategory->numMissionCategories] = CP_GetAlienMissionTypeByID(token);
				if (alienCategory->missionCategories[alienCategory->numMissionCategories] == INTERESTCATEGORY_NONE)
					Com_Printf("CP_ParseAlienTeam: alien team category \"%s\" is used with no mission category. It won't be used in game.\n", name);
				alienCategory->numMissionCategories++;
			} while (*text);
		} else if (Q_streq(token, "team")) {
			alienTeamGroup_t *group;

			token = Com_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("CP_ParseAlienTeam: alien team \"%s\" has team with no opening brace\n", name);
				break;
			}

			if (alienCategory->numAlienTeamGroups >= MAX_ALIEN_GROUP_PER_CATEGORY) {
				Com_Printf("CP_ParseAlienTeam: maximum number of alien team reached (%i) in category \"%s\"\n", MAX_ALIEN_GROUP_PER_CATEGORY, name);
				break;
			}

			group = &alienCategory->alienTeamGroups[alienCategory->numAlienTeamGroups];
			group->idx = alienCategory->numAlienTeamGroups;
			group->categoryIdx = alienCategory - ccs.alienCategories;
			alienCategory->numAlienTeamGroups++;

			do {
				token = Com_EParse(text, errhead, name);

				if (!Com_ParseBlockToken(name, text, group, alien_group_vals, cp_campaignPool, token)) {
					const teamDef_t *teamDef;
					if (!*text || *token == '}')
						break;

					/* This is an alien team */
					if (group->numAlienTeams >= MAX_TEAMS_PER_MISSION)
						Com_Error(ERR_DROP, "CL_ParseAlienTeam: MAX_TEAMS_PER_MISSION hit");
					teamDef = Com_GetTeamDefinitionByID(token);
					if (teamDef)
						group->alienTeams[group->numAlienTeams++] = teamDef;
				}
			} while (*text);
		} else {
			Com_Printf("CP_ParseAlienTeam: unknown token \"%s\" ignored (category %s)\n", token, name);
			continue;
		}
	} while (*text);

	if (LIST_IsEmpty(alienCategory->equipment))
		Sys_Error("alien category equipment list is empty");
}

/**
 * @brief This function parses a list of items that should be set to researched = true after campaign start
 */
static void CP_ParseResearchedCampaignItems (const campaign_t *campaign, const char *name, const char **text)
{
	const char *errhead = "CP_ParseResearchedCampaignItems: unexpected end of file (equipment ";
	const char *token;
	int i;

	/* Don't parse if it is not definition for current type of campaign. */
	if (!Q_streq(campaign->researched, name))
		return;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CP_ParseResearchedCampaignItems: equipment def \"%s\" without body ignored (%s)\n",
				name, token);
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "..campaign research list '%s'\n", name);
	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < ccs.numTechnologies; i++) {
			technology_t *tech = RS_GetTechByIDX(i);
			assert(tech);
			if (Q_streq(token, tech->id)) {
				tech->mailSent = MAILSENT_FINISHED;
				tech->markResearched.markOnly[tech->markResearched.numDefinitions] = qtrue;
				tech->markResearched.campaign[tech->markResearched.numDefinitions] = Mem_PoolStrDup(name, cp_campaignPool, 0);
				tech->markResearched.numDefinitions++;
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", tech->id);
				break;
			}
		}

		if (i == ccs.numTechnologies)
			Com_Printf("CP_ParseResearchedCampaignItems: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/**
 * @brief This function parses a list of items that should be set to researchable = true after campaign start
 * @param[in] campaign The campaign data structure
 * @param[in] name Name of the techlist
 * @param[in,out] text Script to parse
 * @param[in] researchable Mark them researchable or not researchable
 * @sa CP_ParseScriptFirst
 */
static void CP_ParseResearchableCampaignStates (const campaign_t *campaign, const char *name, const char **text, qboolean researchable)
{
	const char *errhead = "CP_ParseResearchableCampaignStates: unexpected end of file (equipment ";
	const char *token;
	int i;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CP_ParseResearchableCampaignStates: equipment def \"%s\" without body ignored\n", name);
		return;
	}

	if (!Q_streq(campaign->researched, name)) {
		Com_DPrintf(DEBUG_CLIENT, "..don't use '%s' as researchable list\n", name);
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "..campaign researchable list '%s'\n", name);
	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < ccs.numTechnologies; i++) {
			technology_t *tech = RS_GetTechByIDX(i);
			if (Q_streq(token, tech->id)) {
				if (researchable) {
					tech->mailSent = MAILSENT_PROPOSAL;
					RS_MarkOneResearchable(tech);
				} else {
					/** @todo Mark unresearchable */
				}
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", tech->id);
				break;
			}
		}

		if (i == ccs.numTechnologies)
			Com_Printf("CP_ParseResearchableCampaignStates: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/* =========================================================== */

static const value_t salary_vals[] = {
	{"soldier_base", V_INT, offsetof(salary_t, base[EMPL_SOLDIER]), MEMBER_SIZEOF(salary_t, base[EMPL_SOLDIER])},
	{"soldier_rankbonus", V_INT, offsetof(salary_t, rankBonus[EMPL_SOLDIER]), MEMBER_SIZEOF(salary_t, rankBonus[EMPL_SOLDIER])},
	{"worker_base", V_INT, offsetof(salary_t, base[EMPL_WORKER]), MEMBER_SIZEOF(salary_t, base[EMPL_WORKER])},
	{"worker_rankbonus", V_INT, offsetof(salary_t, rankBonus[EMPL_WORKER]), MEMBER_SIZEOF(salary_t, rankBonus[EMPL_WORKER])},
	{"scientist_base", V_INT, offsetof(salary_t, base[EMPL_SCIENTIST]), MEMBER_SIZEOF(salary_t, base[EMPL_SCIENTIST])},
	{"scientist_rankbonus", V_INT, offsetof(salary_t, rankBonus[EMPL_SCIENTIST]), MEMBER_SIZEOF(salary_t, rankBonus[EMPL_SCIENTIST])},
	{"pilot_base", V_INT, offsetof(salary_t, base[EMPL_PILOT]), MEMBER_SIZEOF(salary_t, base[EMPL_PILOT])},
	{"pilot_rankbonus", V_INT, offsetof(salary_t, rankBonus[EMPL_PILOT]), MEMBER_SIZEOF(salary_t, rankBonus[EMPL_PILOT])},
	{"aircraft_factor", V_INT, offsetof(salary_t, aircraftFactor), MEMBER_SIZEOF(salary_t, aircraftFactor)},
	{"aircraft_divisor", V_INT, offsetof(salary_t, aircraftDivisor), MEMBER_SIZEOF(salary_t, aircraftDivisor)},
	{"base_upkeep", V_INT, offsetof(salary_t, baseUpkeep), MEMBER_SIZEOF(salary_t, baseUpkeep)},
	{"admin_initial", V_INT, offsetof(salary_t, adminInitial), MEMBER_SIZEOF(salary_t, adminInitial)},
	{"admin_soldier", V_INT, offsetof(salary_t, admin[EMPL_SOLDIER]), MEMBER_SIZEOF(salary_t, admin[EMPL_SOLDIER])},
	{"admin_worker", V_INT, offsetof(salary_t, admin[EMPL_WORKER]), MEMBER_SIZEOF(salary_t, admin[EMPL_WORKER])},
	{"admin_scientist", V_INT, offsetof(salary_t, admin[EMPL_SCIENTIST]), MEMBER_SIZEOF(salary_t, admin[EMPL_SCIENTIST])},
	{"admin_pilot", V_INT, offsetof(salary_t, admin[EMPL_PILOT]), MEMBER_SIZEOF(salary_t, admin[EMPL_PILOT])},
	{"debt_interest", V_FLOAT, offsetof(salary_t, debtInterest), MEMBER_SIZEOF(salary_t, debtInterest)},
	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the salaries from campaign definition
 * @param[in] name Name or ID of the found character skill and ability definition
 * @param[in] text The text of the nation node
 * @param[out] s Pointer to the campaign salaries data structure to parse into
 * @note Example:
 * <code>salary {
 *  soldier_base 3000
 * }</code>
 */
static void CP_ParseSalary (const char *name, const char **text, salary_t *s)
{
	Com_ParseBlock(name, text, s, salary_vals, cp_campaignPool);
}

/* =========================================================== */

static const value_t campaign_vals[] = {
	{"team", V_TEAM, offsetof(campaign_t, team), MEMBER_SIZEOF(campaign_t, team)},
	{"soldiers", V_INT, offsetof(campaign_t, soldiers), MEMBER_SIZEOF(campaign_t, soldiers)},
	{"workers", V_INT, offsetof(campaign_t, workers), MEMBER_SIZEOF(campaign_t, workers)},
	{"xvirate", V_INT, offsetof(campaign_t, maxAllowedXVIRateUntilLost), MEMBER_SIZEOF(campaign_t, maxAllowedXVIRateUntilLost)},
	{"maxdebts", V_INT, offsetof(campaign_t, negativeCreditsUntilLost), MEMBER_SIZEOF(campaign_t, negativeCreditsUntilLost)},
	{"minhappiness", V_FLOAT, offsetof(campaign_t, minhappiness), MEMBER_SIZEOF(campaign_t, minhappiness)},
	{"scientists", V_INT, offsetof(campaign_t, scientists), MEMBER_SIZEOF(campaign_t, scientists)},
	{"pilots", V_INT, offsetof(campaign_t, pilots), MEMBER_SIZEOF(campaign_t, pilots)},
	{"ugvs", V_INT, offsetof(campaign_t, ugvs), MEMBER_SIZEOF(campaign_t, ugvs)},
	{"equipment", V_STRING, offsetof(campaign_t, equipment), 0},
	{"soldierequipment", V_STRING, offsetof(campaign_t, soldierEquipment), 0},
	{"market", V_STRING, offsetof(campaign_t, market), 0},
	{"asymptotic_market", V_STRING, offsetof(campaign_t, asymptoticMarket), 0},
	{"researched", V_STRING, offsetof(campaign_t, researched), 0},
	{"difficulty", V_INT, offsetof(campaign_t, difficulty), MEMBER_SIZEOF(campaign_t, difficulty)},
	{"map", V_STRING, offsetof(campaign_t, map), 0},
	{"credits", V_INT, offsetof(campaign_t, credits), MEMBER_SIZEOF(campaign_t, credits)},
	{"visible", V_BOOL, offsetof(campaign_t, visible), MEMBER_SIZEOF(campaign_t, visible)},
	{"text", V_TRANSLATION_STRING, offsetof(campaign_t, text), 0}, /* just a gettext placeholder */
	{"name", V_TRANSLATION_STRING, offsetof(campaign_t, name), 0},
	{"date", V_DATE, offsetof(campaign_t, date), 0},
	{"basecost", V_INT, offsetof(campaign_t, basecost), MEMBER_SIZEOF(campaign_t, basecost)},
	{"firstbase", V_STRING, offsetof(campaign_t, firstBaseTemplate), 0},
	{NULL, 0, 0, 0}
};

/**
 * @sa CL_ParseClientData
 */
static void CP_ParseCampaign (const char *name, const char **text)
{
	const char *errhead = "CP_ParseCampaign: unexpected end of file (campaign ";
	campaign_t *cp;
	const char *token;
	int i;
	salary_t *s;

	/* search for campaigns with same name */
	for (i = 0; i < ccs.numCampaigns; i++)
		if (Q_streq(name, ccs.campaigns[i].id))
			break;

	if (i < ccs.numCampaigns) {
		Com_Printf("CP_ParseCampaign: campaign def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (ccs.numCampaigns >= MAX_CAMPAIGNS) {
		Com_Printf("CP_ParseCampaign: Max campaigns reached (%i)\n", MAX_CAMPAIGNS);
		return;
	}

	/* initialize the campaign */
	cp = &ccs.campaigns[ccs.numCampaigns++];
	OBJZERO(*cp);

	cp->idx = ccs.numCampaigns - 1;
	Q_strncpyz(cp->id, name, sizeof(cp->id));

	/* some default values */
	cp->team = TEAM_PHALANX;
	Q_strncpyz(cp->researched, "researched_human", sizeof(cp->researched));

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CP_ParseCampaign: campaign def \"%s\" without body ignored\n", name);
		ccs.numCampaigns--;
		return;
	}

	/* some default values */
	s = &cp->salaries;
	s->base[EMPL_SOLDIER] = 3000;
	s->rankBonus[EMPL_SOLDIER] = 500;
	s->base[EMPL_WORKER] = 3000;
	s->rankBonus[EMPL_WORKER] = 500;
	s->base[EMPL_SCIENTIST] = 3000;
	s->rankBonus[EMPL_SCIENTIST] = 500;
	s->base[EMPL_PILOT] = 3000;
	s->rankBonus[EMPL_PILOT] = 500;
	s->aircraftFactor = 1;
	s->aircraftDivisor = 25;
	s->baseUpkeep = 20000;
	s->adminInitial = 1000;
	s->admin[EMPL_SOLDIER] = 75;
	s->admin[EMPL_WORKER] = 75;
	s->admin[EMPL_SCIENTIST] = 75;
	s->admin[EMPL_PILOT] = 75;
	s->debtInterest = 0.005;

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		if (Com_ParseBlockToken(name, text, cp, campaign_vals, NULL, token)) {
			continue;
		} else if (Q_streq(token, "salary")) {
			CP_ParseSalary(token, text, s);
		} else if (Q_streq(token, "events")) {
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return;
			cp->events = CP_GetEventsByID(token);
		} else {
			Com_Printf("CP_ParseCampaign: unknown token \"%s\" ignored (campaign %s)\n", token, name);
			Com_EParse(text, errhead, name);
		}
	} while (*text);

	if (cp->difficulty < -4)
		cp->difficulty = -4;
	else if (cp->difficulty > 4)
		cp->difficulty = 4;
}

/**
 * @brief Parses one "components" entry in a .ufo file and writes it into the next free entry in xxxxxxxx (components_t).
 * @param[in] name The unique id of a components_t array entry.
 * @param[in] text the whole following text after the "components" definition.
 * @sa CP_ParseScriptFirst
 */
static void CP_ParseComponents (const char *name, const char **text)
{
	components_t *comp;
	const char *errhead = "CP_ParseComponents: unexpected end of file.";
	const char *token;

	/* get body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("CP_ParseComponents: \"%s\" components def without body ignored.\n", name);
		return;
	}
	if (ccs.numComponents >= MAX_ASSEMBLIES) {
		Com_Printf("CP_ParseComponents: too many technology entries. limit is %i.\n", MAX_ASSEMBLIES);
		return;
	}

	/* New components-entry (next free entry in global comp-list) */
	comp = &ccs.components[ccs.numComponents];
	ccs.numComponents++;

	OBJZERO(*comp);

	/* set standard values */
	Q_strncpyz(comp->assemblyId, name, sizeof(comp->assemblyId));
	comp->assemblyItem = INVSH_GetItemByIDSilent(comp->assemblyId);
	if (comp->assemblyItem)
		Com_DPrintf(DEBUG_CLIENT, "CP_ParseComponents: linked item: %s with components: %s\n", name, comp->assemblyId);

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		if (Q_streq(token, "item")) {
			/* Defines what items need to be collected for this item to be researchable. */
			if (comp->numItemtypes < MAX_COMP) {
				/* Parse item name */
				token = Com_Parse(text);

				comp->items[comp->numItemtypes] = INVSH_GetItemByID(token);	/* item id -> item pointer */

				/* Parse number of items. */
				token = Com_Parse(text);
				comp->itemAmount[comp->numItemtypes] = atoi(token);
				token = Com_Parse(text);
				/* If itemcount needs to be scaled */
				if (token[0] == '%')
					comp->itemAmount2[comp->numItemtypes] = COMP_ITEMCOUNT_SCALED;
				else
					comp->itemAmount2[comp->numItemtypes] = atoi(token);

				/** @todo Set item links to NONE if needed */
				/* comp->item_idx[comp->numItemtypes] = xxx */

				comp->numItemtypes++;
			} else {
				Com_Printf("CP_ParseComponents: \"%s\" Too many 'items' defined. Limit is %i - ignored.\n", name, MAX_COMP);
			}
		} else if (Q_streq(token, "time")) {
			/* Defines how long disassembly lasts. */
			token = Com_Parse(text);
			comp->time = atoi(token);
		} else {
			Com_Printf("CP_ParseComponents: Error in \"%s\" - unknown token: \"%s\".\n", name, token);
		}
	} while (*text);
}

/**
 * @brief Returns components definition for an item.
 * @param[in] item Item to search the components for.
 * @return Pointer to @c components_t definition.
 */
components_t *CP_GetComponentsByItem (const objDef_t *item)
{
	int i;

	for (i = 0; i < ccs.numComponents; i++) {
		components_t *comp = &ccs.components[i];
		if (comp->assemblyItem == item) {
			Com_DPrintf(DEBUG_CLIENT, "CP_GetComponentsByItem: found components id: %s\n", comp->assemblyId);
			return comp;
		}
	}
	Com_Error(ERR_DROP, "CP_GetComponentsByItem: could not find components id for: %s", item->id);
}

/**
 * @brief Returns components definition by ID.
 * @param[in] id assemblyId of the component definition.
 * @return Pointer to @c components_t definition.
 */
components_t *CP_GetComponentsByID (const char *id)
{
	int i;

	for (i = 0; i < ccs.numComponents; i++) {
		components_t *comp = &ccs.components[i];
		if (Q_streq(comp->assemblyId, id)) {
			return comp;
		}
	}
	Com_Error(ERR_DROP, "CP_GetComponentsByID: could not find components id for: %s", id);
}

/**
 * @brief Parsing campaign data
 *
 * first stage parses all the main data into their struct
 * see CP_ParseScriptSecond for more details about parsing stages
 * @sa CP_ParseCampaignData
 * @sa CP_ParseScriptSecond
 */
static void CP_ParseScriptFirst (const char *type, const char *name, const char **text)
{
	/* check for client interpretable scripts */
	if (Q_streq(type, "up_chapters"))
		UP_ParseChapters(name, text);
	else if (Q_streq(type, "building"))
		B_ParseBuildings(name, text, qfalse);
	else if (Q_streq(type, "installation"))
		INS_ParseInstallations(name, text);
	else if (Q_streq(type, "tech"))
		RS_ParseTechnologies(name, text);
	else if (Q_streq(type, "nation"))
		CL_ParseNations(name, text);
	else if (Q_streq(type, "city"))
		CL_ParseCities(name, text);
	else if (Q_streq(type, "rank"))
		CL_ParseRanks(name, text);
	else if (Q_streq(type, "aircraft"))
		AIR_ParseAircraft(name, text, qfalse);
	else if (Q_streq(type, "mail"))
		CL_ParseEventMails(name, text);
	else if (Q_streq(type, "events"))
		CL_ParseCampaignEvents(name, text);
	else if (Q_streq(type, "components"))
		CP_ParseComponents(name, text);
	else if (Q_streq(type, "alienteam"))
		CP_ParseAlienTeam(name, text);
	else if (Q_streq(type, "msgoptions"))
		MSO_ParseSettings(name, text);
	else if (Q_streq(type, "msgcategory"))
		MSO_ParseCategories(name, text);
}

/**
 * @brief Parsing only for singleplayer
 *
 * parsed if we are no dedicated server
 * second stage links all the parsed data from first stage
 * example: we need a techpointer in a building - in the second stage the buildings and the
 * techs are already parsed - so now we can link them
 * @sa CP_ParseCampaignData
 * @sa Com_ParseScripts
 * @sa CL_ParseScriptFirst
 */
static void CP_ParseScriptSecond (const char *type, const char *name, const char **text)
{
	/* check for client interpretable scripts */
	if (Q_streq(type, "building"))
		B_ParseBuildings(name, text, qtrue);
	else if (Q_streq(type, "aircraft"))
		AIR_ParseAircraft(name, text, qtrue);
	else if (Q_streq(type, "basetemplate"))
		B_ParseBaseTemplate(name, text);
	else if (Q_streq(type, "campaign"))
		CP_ParseCampaign(name, text);
}

/**
 * @brief Parses the campaign specific data - this data can only be parsed once the campaign started
 */
static void CP_ParseScriptCampaignRelated (const campaign_t *campaign, const char *type, const char *name, const char **text)
{
	if (Q_streq(type, "researched"))
		CP_ParseResearchedCampaignItems(campaign, name, text);
	else if (Q_streq(type, "researchable"))
		CP_ParseResearchableCampaignStates(campaign, name, text, qtrue);
	else if (Q_streq(type, "notresearchable"))
		CP_ParseResearchableCampaignStates(campaign, name, text, qfalse);
}

/**
 * @brief Make sure values of items after parsing are proper.
 */
static qboolean CP_ItemsSanityCheck (void)
{
	int i;
	qboolean result = qtrue;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = INVSH_GetItemByIDX(i);

		/* Warn if item has no size set. */
		if (item->size <= 0 && B_ItemIsStoredInBaseStorage(item)) {
			result = qfalse;
			Com_Printf("CP_ItemsSanityCheck: Item %s has zero size set.\n", item->id);
		}

		/* Warn if no price is set. */
		if (item->price <= 0 && BS_IsOnMarket(item)) {
			result = qfalse;
			Com_Printf("CP_ItemsSanityCheck: Item %s has zero price set.\n", item->id);
		}

		if (item->price > 0 && !BS_IsOnMarket(item) && !PR_ItemIsProduceable(item)) {
			result = qfalse;
			Com_Printf("CP_ItemsSanityCheck: Item %s has a price set though it is neither available on the market and production.\n", item->id);
		}

		/* extension and headgear are mutual exclusive */
		if (item->extension && item->headgear) {
			result = qfalse;
			Com_Printf("CP_ItemsSanityCheck: Item %s has both extension and headgear set.\n",  item->id);
		}
	}

	return result;
}

/** @brief struct that holds the sanity check data */
typedef struct {
	qboolean (*check)(void);	/**< function pointer to check function */
	const char* name;			/**< name of the subsystem to check */
} sanity_functions_t;

/** @brief Data for sanity check of parsed script data */
static const sanity_functions_t sanity_functions[] = {
	{B_BuildingScriptSanityCheck, "buildings"},
	{RS_ScriptSanityCheck, "tech"},
	{AIR_ScriptSanityCheck, "aircraft"},
	{CP_ItemsSanityCheck, "items"},
	{NAT_ScriptSanityCheck, "nations"},

	{NULL, NULL}
};

/**
 * @brief Check the parsed script values for errors after parsing every script file
 * @sa CP_ParseCampaignData
 */
void CP_ScriptSanityCheck (void)
{
	qboolean status;
	const sanity_functions_t *s;

	Com_Printf("Sanity check for script data\n");
	s = sanity_functions;
	while (s->check) {
		status = s->check();
		Com_Printf("...%s %s\n", s->name, (status ? "ok" : "failed"));
		s++;
	}
}

/**
 * @brief Read the data for campaigns
 * @sa SAV_GameLoad
 * @sa CP_ResetCampaignData
 */
void CP_ParseCampaignData (void)
{
	const char *type, *name, *text;
	int i;
	campaign_t *campaign;

	/* pre-stage parsing */
	FS_BuildFileList("ufos/*.ufo");
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL)
		CP_ParseScriptFirst(type, name, &text);

	/* fill in IDXs for required research techs */
	RS_RequiredLinksAssign();

	/* stage two parsing */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	Com_DPrintf(DEBUG_CLIENT, "Second stage parsing started...\n");
	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL)
		CP_ParseScriptSecond(type, name, &text);

	for (i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t *teamDef = &csi.teamDef[i];
		if (!CHRSH_IsTeamDefAlien(teamDef))
			continue;

		ccs.teamDefTechs[teamDef->idx] = RS_GetTechByID(teamDef->tech);
		if (ccs.teamDefTechs[teamDef->idx] == NULL)
			Com_Error(ERR_DROP, "Could not find a tech for teamdef %s", teamDef->id);
	}

	for (i = 0, campaign = ccs.campaigns; i < ccs.numCampaigns; i++, campaign++) {
		/* find the relevant markets */
		campaign->marketDef = INV_GetEquipmentDefinitionByID(campaign->market);
		campaign->asymptoticMarketDef = INV_GetEquipmentDefinitionByID(campaign->asymptoticMarket);
	}

	Com_Printf("Campaign data loaded - size "UFO_SIZE_T" bytes\n", sizeof(ccs));
	Com_Printf("...techs: %i\n", ccs.numTechnologies);
	Com_Printf("...buildings: %i\n", ccs.numBuildingTemplates);
	Com_Printf("...ranks: %i\n", ccs.numRanks);
	Com_Printf("...nations: %i\n", ccs.numNations);
	Com_Printf("...cities: %i\n", ccs.numCities);
	Com_Printf("\n");
}

void CP_ReadCampaignData (const campaign_t *campaign)
{
	const char *type, *name, *text;

	/* stage two parsing */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	Com_DPrintf(DEBUG_CLIENT, "Second stage parsing started...\n");
	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL)
		CP_ParseScriptCampaignRelated(campaign, type, name, &text);
}
