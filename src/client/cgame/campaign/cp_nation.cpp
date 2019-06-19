/**
 * @file
 * @brief Nation code
 * @note Functions with NAT_*
 */

/*
Copyright (C) 2002-2019 UFO: Alien Invasion.

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
#include "../../../shared/shared.h"
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_ufo.h"
#include "cp_time.h"
#include "save/save_nation.h"
#include "../../ui/ui_main.h" /* ui_node_linechart.h */
#include "../../ui/node/ui_node_linechart.h" /* lineStrip_t */
#include "cp_missions.h"

/* nation happiness constants */
#define HAPPINESS_ALIEN_MISSION_LOSS		-0.02
#define HAPPINESS_MAX_MISSION_IMPACT		0.07

nation_t* NAT_GetNationByIDX (const int index)
{
	assert(index >= 0);
	assert(index < ccs.numNations);

	return &ccs.nations[index];
}

/**
 * @brief Return a nation-pointer by the nations id (nation_t->id) text.
 * @param[in] nationID nation id as defined in (nation_t->id)
 * @return nation_t pointer or nullptr if nothing found (=error).
 */
nation_t* NAT_GetNationByID (const char* nationID)
{
	if (!nationID) {
		cgi->Com_Printf("NAT_GetNationByID: nullptr nationID\n");
		return nullptr;
	}
	for (int i = 0; i < ccs.numNations; i++) {
		nation_t* nation = NAT_GetNationByIDX(i);
		if (Q_streq(nation->id, nationID))
			return nation;
	}

	cgi->Com_Printf("NAT_GetNationByID: Could not find nation '%s'\n", nationID);

	/* No matching nation found - ERROR */
	return nullptr;
}


/**
 * @brief Lower happiness of nations depending on alien activity.
 * @note Daily called
 * @sa CP_BuildBaseGovernmentLeave
 */
void NAT_UpdateHappinessForAllNations (const float minhappiness)
{
	MIS_Foreach(mission) {
		nation_t* nation = GEO_GetNation(mission->pos);
		/* Difficulty modifier range is [0, 0.02f] */

		/* Some non-water location have no nation */
		if (!nation)
			continue;

		float happinessFactor;
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		switch (mission->stage) {
		case STAGE_TERROR_MISSION:
		case STAGE_SUBVERT_GOV:
		case STAGE_RECON_GROUND:
		case STAGE_SPREAD_XVI:
		case STAGE_HARVEST:
			happinessFactor = HAPPINESS_ALIEN_MISSION_LOSS;
			break;
		default:
			/* mission is not active on earth or does not have any influence
			 * on the nation happiness, skip this mission */
			continue;
		}

		NAT_SetHappiness(minhappiness, nation, stats->happiness + happinessFactor);
		cgi->Com_DPrintf(DEBUG_CLIENT, "Happiness of nation %s decreased: %.02f\n", nation->name, stats->happiness);
	}
}

/**
 * @brief Get the actual funding of a nation
 * @param[in] nation Pointer to the nation
 * @param[in] month idx of the month -- 0 for current month
 * @return actual funding of a nation
 */
int NAT_GetFunding (const nation_t* const nation, int month)
{
	assert(month >= 0);
	assert(month < MONTHS_PER_YEAR);
	return nation->maxFunding * nation->stats[month].happiness;
}

/**
 * @brief Get the current month nation stats
 * @param[in] nation Pointer to the nation
 * @return The current month nation stats
 */
const nationInfo_t* NAT_GetCurrentMonthInfo (const nation_t* const nation)
{
	return &nation->stats[0];
}

/**
 * @brief Translates the nation happiness float value to a string
 * @param[in] happiness value
 * @return Translated happiness string
 * @note happiness is between 0 and 1.0
*/
static const char* NAT_GetHappinessString (const float happiness)
{
	if (happiness < 0.015)
		return _("Giving up");
	else if (happiness < 0.025)
		return _("Furious");
	else if (happiness < 0.04)
		return _("Angry");
	else if (happiness < 0.06)
		return _("Mad");
	else if (happiness < 0.10)
		return _("Upset");
	else if (happiness < 0.20)
		return _("Tolerant");
	else if (happiness < 0.30)
		return _("Neutral");
	else if (happiness < 0.50)
		return _("Content");
	else if (happiness < 0.70)
		return _("Pleased");
	else if (happiness < 0.95)
		return _("Happy");
	else
		return _("Exuberant");

}

/**
 * @brief Translates the current nation happiness float value to a string
 * @param[in] nation
 * @return Translated happiness string
 * @note happiness is between 0 and 1.0
 */
const char* NAT_GetCurrentHappinessString (const nation_t* nation)
{
	const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
	return NAT_GetHappinessString(stats->happiness);
}

/**
 * @brief Updates the nation happiness
 * @param[in] minhappiness Minimum value of mean happiness before the game is lost
 * @param[in] nation The nation to update the happiness for
 * @param[in] happiness The new happiness value to set for the given nation
 */
void NAT_SetHappiness (const float minhappiness, nation_t* nation, const float happiness)
{
	const char* oldString = NAT_GetCurrentHappinessString(nation);
	const char* newString;
	nationInfo_t* stats = &nation->stats[0];
	const float oldHappiness = stats->happiness;
	const float middleHappiness = (minhappiness + 1.0) / 2;
	notify_t notifyType = NT_NUM_NOTIFYTYPE;

	stats->happiness = happiness;
	if (stats->happiness < 0.0f)
		stats->happiness = 0.0f;
	else if (stats->happiness > 1.0f)
		stats->happiness = 1.0f;

	newString = NAT_GetCurrentHappinessString(nation);

	if (oldString != newString) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Nation %s changed happiness from %s to %s"), _(nation->name), oldString, newString);
		notifyType = NT_HAPPINESS_CHANGED;
	} else if (oldHappiness > middleHappiness && happiness < middleHappiness) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Nation %s changed happiness to %s"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_PLEASED;
	} else if (happiness < minhappiness && oldHappiness > minhappiness) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), /** @todo need to more specific message */
			_("Happiness of nation %s is %s and less than minimal happiness allowed to the campaign"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_MIN;
	} else {
		return;
	}

	MSO_CheckAddNewMessage(notifyType, _("Nation changed happiness"), cp_messageBuffer);
}

/**
 * @brief Nation saving callback
 * @param[out] p XML Node structure, where we write the information to
 */
bool NAT_SaveXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_NATION_NATIONS);

	for (int i = 0; i < ccs.numNations; i++) {
		nation_t* nation = NAT_GetNationByIDX(i);

		if (!nation)
			continue;

		xmlNode_t* s = cgi->XML_AddNode(n, SAVE_NATION_NATION);
		cgi->XML_AddString(s, SAVE_NATION_ID, nation->id);
		for (int j = 0; j < MONTHS_PER_YEAR; j++) {
			const nationInfo_t* stats = &nation->stats[j];

			if (!stats->inuse)
				continue;

			xmlNode_t* ss = cgi->XML_AddNode(s, SAVE_NATION_MONTH);
			cgi->XML_AddInt(ss, SAVE_NATION_MONTH_IDX, j);
			cgi->XML_AddFloat(ss, SAVE_NATION_HAPPINESS, stats->happiness);
			cgi->XML_AddInt(ss, SAVE_NATION_XVI, stats->xviInfection);
		}
	}
	return true;
}

/**
 * @brief Updates each nation's happiness.
 * Should be called at the completion or expiration of every mission.
 * The nation where the mission took place will be most affected,
 * surrounding nations will be less affected.
 * @param[in] minHappiness Minimum value of mean happiness before the game is lost
 * @param[in] mis Mission to evaluate
 * @param[in] affectedNation Nation that's happiness is changing
 * @param[in] results Mission result structure
 * @param[in] won if PHALANX won
 * @todo Scoring should eventually be expanded to include such elements as
 * infected humans and mission objectives other than xenocide.
 */
void CP_HandleNationData (float minHappiness, mission_t* mis, const nation_t* affectedNation, const missionResults_t* results, bool won)
{
	const float civilianSum = (float) (results->civiliansSurvived + results->civiliansKilled + results->civiliansKilledFriendlyFire);
	const float alienSum = (float) (results->aliensSurvived + results->aliensKilled + results->aliensStunned);
	float performance;
	float performanceAlien;
	float performanceCivilian;
	float deltaHappiness = 0.0f;
	float happinessDivisor = 5.0f;
	float victoryBonusPerAlien = 0.1f;

	if (mis->mapDef->victoryBonusPerAlien) {
		victoryBonusPerAlien = mis->mapDef->victoryBonusPerAlien;
	}

	/** @todo HACK: This should be handled properly, i.e. civilians should only factor into the scoring
	 * if the mission objective is actually to save civilians. */
	if (civilianSum <= 1) {
		performanceCivilian = 0.0f;
	} else {
		performanceCivilian = (2 * civilianSum - results->civiliansKilled - 2
			* results->civiliansKilledFriendlyFire) * 3 / (2 * civilianSum) - 2;
	}

	/* Calculate how well the mission went. */
	/** @todo The score for aliens should be dependent on the mission objective.
	 * In a mission that has a special objective, the amount of killed aliens should
	 * only serve to increase the score, not reduce the penalty. */
	if (won) {
		performanceAlien = (results->aliensKilled + results->aliensStunned) * victoryBonusPerAlien;
	} else {
		performanceAlien = results->aliensKilled + results->aliensStunned - alienSum;
	}
	performance = performanceCivilian + performanceAlien;

	/* Calculate the actual happiness delta. The bigger the mission, the more potential influence. */
	deltaHappiness = 0.004 * civilianSum + 0.004 * alienSum;

	/* There is a maximum base happiness delta. */
	if (deltaHappiness > HAPPINESS_MAX_MISSION_IMPACT)
		deltaHappiness = HAPPINESS_MAX_MISSION_IMPACT;

	for (int i = 0; i < ccs.numNations; i++) {
		nation_t* nation = NAT_GetNationByIDX(i);
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		float happinessFactor;

		/* update happiness. */
		if (nation == affectedNation)
			happinessFactor = deltaHappiness;
		else
			happinessFactor = deltaHappiness / happinessDivisor;

		NAT_SetHappiness(minHappiness, nation, stats->happiness + performance * happinessFactor);
	}
}

/**
 * @brief Nation loading xml callback
 * @param[in] p XML Node structure, where we get the information from
 */
bool NAT_LoadXML (xmlNode_t* p)
{
	xmlNode_t* n;
	xmlNode_t* s;

	n = cgi->XML_GetNode(p, SAVE_NATION_NATIONS);
	if (!n)
		return false;

	/* nations loop */
	for (s = cgi->XML_GetNode(n, SAVE_NATION_NATION); s; s = cgi->XML_GetNextNode(s, n, SAVE_NATION_NATION)) {
		xmlNode_t* ss;
		nation_t* nation = NAT_GetNationByID(cgi->XML_GetString(s, SAVE_NATION_ID));

		if (!nation)
			return false;

		/* month loop */
		for (ss = cgi->XML_GetNode(s, SAVE_NATION_MONTH); ss; ss = cgi->XML_GetNextNode(ss, s, SAVE_NATION_MONTH)) {
			int monthIDX = cgi->XML_GetInt(ss, SAVE_NATION_MONTH_IDX, MONTHS_PER_YEAR);
			nationInfo_t* stats = &nation->stats[monthIDX];

			if (monthIDX < 0 || monthIDX >= MONTHS_PER_YEAR)
				return false;

			stats->inuse = true;
			stats->happiness = cgi->XML_GetFloat(ss, SAVE_NATION_HAPPINESS, 0.0);
			stats->xviInfection = cgi->XML_GetInt(ss, SAVE_NATION_XVI, 0);
		}
	}
	return true;
}

/*==========================================
Parsing
==========================================*/

static const value_t nation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(nation_t, name), 0},
	{"pos", V_POS, offsetof(nation_t, pos), MEMBER_SIZEOF(nation_t, pos)},
	{"color", V_COLOR, offsetof(nation_t, color), MEMBER_SIZEOF(nation_t, color)},
	{"funding", V_INT, offsetof(nation_t, maxFunding), MEMBER_SIZEOF(nation_t, maxFunding)},
	{"happiness", V_FLOAT, offsetof(nation_t, stats[0].happiness), MEMBER_SIZEOF(nation_t, stats[0].happiness)},
	{"soldiers", V_INT, offsetof(nation_t, maxSoldiers), MEMBER_SIZEOF(nation_t, maxSoldiers)},
	{"scientists", V_INT, offsetof(nation_t, maxScientists), MEMBER_SIZEOF(nation_t, maxScientists)},
	{"workers", V_INT, offsetof(nation_t, maxWorkers), MEMBER_SIZEOF(nation_t, maxWorkers)},
	{"pilots", V_INT, offsetof(nation_t, maxPilots), MEMBER_SIZEOF(nation_t, maxPilots)},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parse the nation data from script file
 * @param[in] name Name or ID of the found nation
 * @param[in] text The text of the nation node
 * @sa nation_vals
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseNations (const char* name, const char** text)
{
	if (ccs.numNations >= MAX_NATIONS) {
		cgi->Com_Printf("CL_ParseNations: nation number exceeding maximum number of nations: %i\n", MAX_NATIONS);
		return;
	}

	/* search for nations with same name */
	int i;
	for (i = 0; i < ccs.numNations; i++) {
		const nation_t* n = NAT_GetNationByIDX(i);
		if (Q_streq(name, n->id))
			break;
	}
	if (i < ccs.numNations) {
		cgi->Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	nation_t* nation = &ccs.nations[ccs.numNations];
	OBJZERO(*nation);
	nation->idx = ccs.numNations;
	nation->stats[0].inuse = true;

	if (cgi->Com_ParseBlock(name, text, nation, nation_vals, cp_campaignPool)) {
		ccs.numNations++;

		cgi->Com_DPrintf(DEBUG_CLIENT, "...found nation %s\n", name);
		nation->id = cgi->PoolStrDup(name, cp_campaignPool, 0);
	}
}

/**
 * @brief Finds a city by it's scripted identifier
 * @param[in] cityId Scripted ID of the city
 */
city_t* CITY_GetById (const char* cityId)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		if (Q_streq(cityId, city->id))
			return city;
	}
	return nullptr;
}

/**
 * @brief Finds a city by it's geoscape coordinates
 * @param[in] pos Position of the city
 */
city_t* CITY_GetByPos (vec2_t pos)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		if (Vector2Equal(pos, city->pos))
			return city;
	}
	return nullptr;
}

static const value_t city_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(city_t, name), 0},
	{"pos", V_POS, offsetof(city_t, pos), MEMBER_SIZEOF(city_t, pos)},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parse the city data from script file
 * @param[in] name ID of the found nation
 * @param[in] text The text of the nation node
 */
void CITY_Parse (const char* name, const char** text)
{
	city_t newCity;

	/* search for cities with same name */
	if (CITY_GetById(name)) {
		cgi->Com_Printf("CITY_Parse: city def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	OBJZERO(newCity);
	newCity.idx = ccs.numCities;

	if (cgi->Com_ParseBlock(name, text, &newCity, city_vals, cp_campaignPool)) {
		ccs.numCities++;
		newCity.id = cgi->PoolStrDup(name, cp_campaignPool, 0);
		/* Add city to the list */
		LIST_Add(&ccs.cities, newCity);
	}
}

/**
 * @brief Checks the parsed nations and cities for errors
 * @return false if there are errors - true otherwise
 */
bool NAT_ScriptSanityCheck (void)
{
	int error = 0;

	/* Check if there is at least one map fitting city parameter for terror mission */
	LIST_Foreach(ccs.cities, city_t, city) {
		bool cityCanBeUsed = false;
		bool parametersFit = false;
		ufoType_t ufoTypes[UFO_MAX];
		int numTypes;
		const mapDef_t* md;

		if (!city->name) {
			error++;
			cgi->Com_Printf("...... city '%s' has no name\n", city->id);
		}

		if (MapIsWater(GEO_GetColor(city->pos, MAPTYPE_TERRAIN, nullptr))) {
			error++;
			cgi->Com_Printf("...... city '%s' has a position in the water\n", city->id);
		}

		numTypes = UFO_GetAvailableUFOsForMission(INTERESTCATEGORY_TERROR_ATTACK, ufoTypes, false);

		MapDef_ForeachSingleplayerCampaign(md) {
			if (md->storyRelated)
				continue;

			if (GEO_PositionFitsTCPNTypes(city->pos, md->terrains, md->cultures, md->populations, nullptr)) {
				/* this map fits city parameter, check if we have some terror mission UFOs available for this map */
				parametersFit = true;

				/* no UFO on this map (LIST_ContainsString doesn't like empty string) */
				if (!md->ufos) {
					continue;
				}

				/* loop must be backward, as we remove items */
				for (int i = numTypes - 1 ; i >= 0; i--) {
					if (cgi->LIST_ContainsString(md->ufos, cgi->Com_UFOTypeToShortName(ufoTypes[i]))) {
						REMOVE_ELEM(ufoTypes, i, numTypes);
					}
				}
			}
			if (numTypes == 0) {
				cityCanBeUsed = true;
				break;
			}
		}

		if (!cityCanBeUsed) {
			error++;
			cgi->Com_Printf("...... city '%s' can't be used in game: it has no map fitting parameters\n", city->id);
			if (parametersFit) {
				cgi->Com_Printf("      (No map fitting");
				for (int i = 0 ; i < numTypes; i++)
					cgi->Com_Printf(" %s", cgi->Com_UFOTypeToShortName(ufoTypes[i]));
				cgi->Com_Printf(")\n");
			}
			GEO_PrintParameterStringByPos(city->pos);
		}
	}

	return !error;
}

/*=====================================
Menu functions
=====================================*/

/**
 * @brief Console command for UI to gather nation statistics
 */
static void NAT_ListStats_f (void)
{
	const int argCount = cgi->Cmd_Argc();
	if (argCount < 2) {
		cgi->Com_Printf("Usage: %s <confunc> [nationID] [monthIDX]\n", cgi->Cmd_Argv(0));
		return;
	}
	char callback[MAX_VAR];
	Q_strncpyz(callback, cgi->Cmd_Argv(1), sizeof(callback));

	for (int nationIDX = 0; nationIDX < ccs.numNations; nationIDX++) {
		const nation_t *nation = NAT_GetNationByIDX(nationIDX);
		if (nation == nullptr)
			continue;

		if (argCount >= 3 && !Q_streq(nation->id, cgi->Cmd_Argv(1)))
			continue;

		for (int monthIDX = 0; monthIDX < MONTHS_PER_YEAR; monthIDX++) {
			if (!nation->stats[monthIDX].inuse)
				break;

			if (argCount >= 4 && monthIDX != -1 * atoi(cgi->Cmd_Argv(3)))
				continue;

			cgi->UI_ExecuteConfunc("%s %s \"%s\" %d %.4f \"%s\" %d \"%f, %f, %f, %f\"",
				callback,
				nation->id,
				_(nation->name),
				monthIDX,
				nation->stats[monthIDX].happiness,
				NAT_GetHappinessString(nation->stats[monthIDX].happiness),
				NAT_GetFunding(nation, monthIDX),
				nation->color[0],
				nation->color[1],
				nation->color[2],
				nation->color[3]
			);
		}
	}
}

/* Funding Chart */
static screenPoint_t fundingPts[MAX_NATIONS][MONTHS_PER_YEAR];
static lineStrip_t fundingLineStrip[MAX_NATIONS];
/* Happiness Chart */
static screenPoint_t happinessPts[MAX_NATIONS][MONTHS_PER_YEAR];
static lineStrip_t happinessLineStrip[MAX_NATIONS];
/* XVI Chart */
static screenPoint_t xviPts[MAX_NATIONS][MONTHS_PER_YEAR];
static lineStrip_t xviLineStrip[MAX_NATIONS];

/**
 * @brief Console command for UI to draw charts
 */
static void NAT_DrawCharts_f (void)
{
	const int argCount = cgi->Cmd_Argc();
	if (argCount < 3) {
		cgi->Com_Printf("Usage: %s <width> <height>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int width = atoi(cgi->Cmd_Argv(1));
	const int height = atoi(cgi->Cmd_Argv(2));
	if (width <= 0 || height <= 0)
		return;

	const int dx = (int)(width / MONTHS_PER_YEAR);

	/* Calculate chart bounds (maximums) */
	int maxFunding;
	float maxXVI;
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		if (nation == nullptr)
			continue;
		for (int monthIdx = 0; monthIdx < MONTHS_PER_YEAR; monthIdx++) {
			if (!nation->stats[monthIdx].inuse)
				break;

			if (i == 0 && monthIdx == 0) {
				maxFunding = NAT_GetFunding(nation, monthIdx);
				maxXVI = nation->stats[monthIdx].xviInfection;
			} else {
				if (maxFunding < NAT_GetFunding(nation, monthIdx))
					maxFunding = NAT_GetFunding(nation, monthIdx);
				if (maxXVI < nation->stats[monthIdx].xviInfection)
					maxXVI = nation->stats[monthIdx].xviInfection;
			}
		}
	}

	const float dyFunding = (0 != maxFunding) ? (float) height / maxFunding : 1;
	const float dyXvi = (0 != maxXVI) ? (float) height / maxXVI : 1;

	/* Fill the points */
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		if (nation == nullptr)
			continue;

		lineStrip_t* funding = &fundingLineStrip[i];
		lineStrip_t* happiness = &happinessLineStrip[i];
		lineStrip_t* xvi = &xviLineStrip[i];

		OBJZERO(*funding);
		OBJZERO(*happiness);
		OBJZERO(*xvi);

		Vector4Copy(nation->color, funding->color);
		Vector4Copy(nation->color, happiness->color);
		Vector4Copy(nation->color, xvi->color);

		funding->pointList = (int*)fundingPts[i];
		happiness->pointList = (int*)happinessPts[i];
		xvi->pointList = (int*)xviPts[i];

		if (i > 0) {
			fundingLineStrip[i - 1].next = funding;
			happinessLineStrip[i - 1].next = happiness;
			xviLineStrip[i - 1].next = xvi;
		}

		int monthIdx;
		for (monthIdx = 0; monthIdx < MONTHS_PER_YEAR; monthIdx++) {
			if (!nation->stats[monthIdx].inuse)
				break;

			const int funding = NAT_GetFunding(nation, monthIdx);
			fundingPts[i][monthIdx].x = (monthIdx * dx);
			fundingPts[i][monthIdx].y = height - dyFunding * funding;

			happinessPts[i][monthIdx].x = (monthIdx * dx);
			happinessPts[i][monthIdx].y = height - (height * nation->stats[monthIdx].happiness);

			const int xviInfection= nation->stats[monthIdx].xviInfection;
			xviPts[i][monthIdx].x = (monthIdx * dx);
			xviPts[i][monthIdx].y = height - dyXvi * xviInfection;
		}

		if (monthIdx == 0)
			continue;

		funding->numPoints = monthIdx;
		happiness->numPoints = monthIdx;
		xvi->numPoints = monthIdx;

		/* There only 1 data point, create a fake one to make the chart visible */
		if (monthIdx == 1 && !nation->stats[monthIdx].inuse) {
			fundingPts[i][1].x = fundingPts[i][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
			fundingPts[i][1].y = fundingPts[i][0].y;
			funding->numPoints++;

			happinessPts[i][1].x = happinessPts[i][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
			happinessPts[i][1].y = happinessPts[i][0].y;
			happiness->numPoints++;

			xviPts[i][1].x = xviPts[i][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
			xviPts[i][1].y = xviPts[i][0].y;
			xvi->numPoints++;
		}
	}

	cgi->UI_RegisterLineStrip(LINESTRIP_FUNDING, &fundingLineStrip[0]);
	cgi->UI_RegisterLineStrip(LINESTRIP_HAPPINESS, &happinessLineStrip[0]);
	cgi->UI_RegisterLineStrip(LINESTRIP_XVI, &xviLineStrip[0]);
}

#ifdef DEBUG
/**
 * @brief Debug function to list all cities in game.
 * @note Called with debug_listcities.
 */
static void NAT_ListCities_f (void)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		cgi->Com_Printf("City '%s' -- position (%0.1f, %0.1f)\n", city->id, city->pos[0], city->pos[1]);
		GEO_PrintParameterStringByPos(city->pos);
	}
}

/**
 * @brief Scriptfunction to list all parsed nations with their current values
 * @note called with debug_listnations
 */
static void NAT_NationList_f (void)
{
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = &ccs.nations[i];
		cgi->Com_Printf("Nation ID: %s\n", nation->id);
		cgi->Com_Printf("...max-funding %i c\n", nation->maxFunding);
		cgi->Com_Printf("...happiness %0.2f\n", nation->stats[0].happiness);
		cgi->Com_Printf("...xviInfection %i\n", nation->stats[0].xviInfection);
		cgi->Com_Printf("...max-soldiers %i\n", nation->maxSoldiers);
		cgi->Com_Printf("...max-scientists %i\n", nation->maxScientists);
		cgi->Com_Printf("...max-workers %i\n", nation->maxWorkers);
		cgi->Com_Printf("...max-pilots %i\n", nation->maxPilots);
		cgi->Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", nation->color[0], nation->color[1], nation->color[2], nation->color[3]);
		cgi->Com_Printf("...pos x:%.0f y:%.0f\n", nation->pos[0], nation->pos[1]);
	}
}
#endif

/**
 * @brief Update the nation data from all parsed nation each month
 * @note give us nation support by:
 * * credits
 * * new soldiers
 * * new scientists
 * * new pilots
 * @note Called from CP_CampaignRun
 * @sa CP_CampaignRun
 * @sa B_CreateEmployee
 * @todo This is very redundant with STATS_Update_f. Investigate and clean up.
 */
void NAT_HandleBudget (const campaign_t* campaign)
{
	char message[1024];
	int cost;
	int totalIncome = 0;
	int totalExpenditure = 0;
	int initialCredits = ccs.credits;
	const salary_t* salary = &campaign->salaries;

	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		const int funding = NAT_GetFunding(nation, 0);
		int newScientists = 0, newSoldiers = 0, newPilots = 0, newWorkers = 0;

		totalIncome += funding;

		for (int j = 0; 0.25 + j < (float) nation->maxScientists * stats->happiness * ccs.curCampaign->employeeRate; j++) {
			/* Create a scientist, but don't auto-hire her. */
			E_CreateEmployee(EMPL_SCIENTIST, nation, nullptr);
			newScientists++;
		}

		if (stats->happiness > 0) {
			for (int j = 0; 0.25 + j < (float) nation->maxSoldiers * stats->happiness * ccs.curCampaign->employeeRate; j++) {
				/* Create a soldier. */
				E_CreateEmployee(EMPL_SOLDIER, nation, nullptr);
				newSoldiers++;
			}
		}
		/* pilots */
		if (stats->happiness > 0) {
			for (int j = 0; 0.25 + j < (float) nation->maxPilots * stats->happiness * ccs.curCampaign->employeeRate; j++) {
				/* Create a pilot. */
				E_CreateEmployee(EMPL_PILOT, nation, nullptr);
				newPilots++;
			}
		}

		for (int j = 0; 0.25 + j * 2 < (float) nation->maxWorkers * stats->happiness * ccs.curCampaign->employeeRate; j++) {
			/* Create a worker. */
			E_CreateEmployee(EMPL_WORKER, nation, nullptr);
			newWorkers++;
		}

		Com_sprintf(message, sizeof(message), _("Gained %i %s, %i %s, %i %s, %i %s, and %i %s from nation %s (%s)"),
			funding, ngettext("credit", "credits", funding),
			newScientists, ngettext("scientist", "scientists", newScientists),
			newSoldiers, ngettext("soldier", "soldiers", newSoldiers),
			newPilots, ngettext("pilot", "pilots", newPilots),
			newWorkers, ngettext("worker", "workers", newWorkers),
			_(nation->name), NAT_GetCurrentHappinessString(nation));
		MS_AddNewMessage(_("Notice"), message);
	}

	for (int i = 0; i < MAX_EMPL; i++) {
		int count = 0;
		cost = 0;
		E_Foreach(i, employee) {
			if (!employee->isHired())
				continue;
			cost += employee->salary();
			count++;
		}
		totalExpenditure += cost;

		if (cost == 0)
			continue;

		Com_sprintf(message, sizeof(message), _("Paid %i credits to: %s"), cost, E_GetEmployeeString((employeeType_t)i, count));
		MS_AddNewMessage(_("Notice"), message);
	}

	cost = 0;
	AIR_Foreach(aircraft) {
		if (aircraft->status == AIR_CRASHED)
			continue;
		cost += aircraft->price * salary->aircraftFactor / salary->aircraftDivisor;
	}
	totalExpenditure += cost;

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for aircraft"), cost);
		MS_AddNewMessage(_("Notice"), message);
	}

	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		cost = CP_GetSalaryUpKeepBase(salary, base);
		totalExpenditure += cost;

		Com_sprintf(message, sizeof(message), _("Paid %i credits for upkeep of %s"), cost, base->name);
		MS_AddNewMessage(_("Notice"), message);
	}

	cost = CP_GetSalaryAdministrative(salary);
	Com_sprintf(message, sizeof(message), _("Paid %i credits for administrative overhead."), cost);
	totalExpenditure += cost;
	MS_AddNewMessage(_("Notice"), message);

	if (initialCredits < 0) {
		const float interest = initialCredits * campaign->salaries.debtInterest;

		cost = (int)ceil(interest);
		Com_sprintf(message, sizeof(message), _("Paid %i credits in interest on your debt."), cost);
		totalExpenditure += cost;
		MS_AddNewMessage(_("Notice"), message);
	}
	CP_UpdateCredits(ccs.credits - totalExpenditure + totalIncome);
	CP_GameTimeStop();
}

/**
 * @brief Backs up each nation's relationship values.
 * @note Right after the copy the stats for the current month are the same as the ones from the (end of the) previous month.
 * They will change while the curent month is running of course :)
 * @todo other stuff to back up?
 */
void NAT_BackupMonthlyData (void)
{
	/**
	 * Back up nation relationship .
	 * "inuse" is copied as well so we do not need to set it anywhere.
	 */
	for (int nat = 0; nat < ccs.numNations; nat++) {
		nation_t* nation = NAT_GetNationByIDX(nat);

		for (int i = MONTHS_PER_YEAR - 1; i > 0; i--) {	/* Reverse copy to not overwrite with wrong data */
			nation->stats[i] = nation->stats[i - 1];
		}
	}
}

static const cmdList_t nationCmds[] = {
	{"nation_getstats", NAT_ListStats_f, "Returns nation happiness and funding stats through a UI callback."},
	{"nation_drawcharts", NAT_DrawCharts_f, "Draws nation happiness and funding charts."},
#ifdef DEBUG
	{"debug_listcities", NAT_ListCities_f, "Debug function to list all cities in game."},
	{"debug_listnations", NAT_NationList_f, "List all nations on the game console"},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Init actions for nation-subsystem
 */
void NAT_InitStartup (void)
{
	cgi->Cmd_TableAddList(nationCmds);
}

/**
 * @brief Closing actions for nation-subsystem
 */
void NAT_Shutdown (void)
{
	cgi->LIST_Delete(&ccs.cities);

	cgi->Cmd_TableRemoveList(nationCmds);
}
