/**
 * @file cp_nation.c
 * @brief Nation code
 * @note Functions with NAT_*
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../cl_shared.h"
#include "../ui/ui_main.h"
#include "../ui/node/ui_node_linechart.h"
#include "../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_ufo.h"
#include "save/save_nation.h"

nation_t *NAT_GetNationByIDX (const int index)
{
	assert(index >= 0);
	assert(index < ccs.numNations);

	return &ccs.nations[index];
}

/**
 * @brief Return a nation-pointer by the nations id (nation_t->id) text.
 * @param[in] nationID nation id as defined in (nation_t->id)
 * @return nation_t pointer or NULL if nothing found (=error).
 */
nation_t *NAT_GetNationByID (const char *nationID)
{
	int i;

	if (!nationID) {
		Com_Printf("NAT_GetNationByID: NULL nationID\n");
		return NULL;
	}
	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = &ccs.nations[i];
		if (!strcmp(nation->id, nationID))
			return nation;
	}

	Com_Printf("NAT_GetNationByID: Could not find nation '%s'\n", nationID);

	/* No matching nation found - ERROR */
	return NULL;
}


/**
 * @brief Lower happiness of nations depending on alien activity.
 * @note Daily called
 * @sa CP_BuildBaseGovernmentLeave
 */
void NAT_UpdateHappinessForAllNations (void)
{
	const linkedList_t *list = ccs.missions;

	for (;list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		nation_t *nation = MAP_GetNation(mission->pos);
		/* Difficulty modifier range is [0, 0.02f] */

		/* Some non-water location have no nation */
		if (nation) {
			float happinessFactor;
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

			NAT_SetHappiness(nation, nation->stats[0].happiness + happinessFactor);
			Com_DPrintf(DEBUG_CLIENT, "Happiness of nation %s decreased: %.02f\n", nation->name, nation->stats[0].happiness);
		}
	}
}

/**
 * @brief Get the actual funding of a nation
 * @param[in] nation Pointer to the nation
 * @param[in] month idx of the month -- 0 for current month (sa nation_t)
 * @return actual funding of a nation
 * @sa CL_NationsMaxFunding
 */
int NAT_GetFunding (const nation_t* const nation, int month)
{
	return nation->maxFunding * nation->stats[month].happiness;
}

/**
 * @brief Translates the nation happiness float value to a string
 * @param[in] nation
 * @return Translated happiness string
 * @note happiness is between 0 and 1.0
 */
const char* NAT_GetHappinessString (const nation_t* nation)
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
	else if (nation->stats[0].happiness < 0.95)
		return _("Happy");
	else
		return _("Exuberant");
}

/**
 * @brief Updates the nation happiness
 * @param[in] nation The nation to update the happiness for
 * @param[in] happiness The new happiness value to set for the given nation
 */
void NAT_SetHappiness (nation_t *nation, const float happiness)
{
	const char *oldString = NAT_GetHappinessString(nation);
	const char *newString;
	const float oldHappiness = nation->stats[0].happiness;
	const float middleHappiness = (ccs.curCampaign->minhappiness + 1.0) / 2;
	notify_t notifyType = NT_NUM_NOTIFYTYPE;

	nation->stats[0].happiness = happiness;
	if (nation->stats[0].happiness < 0.0f)
		nation->stats[0].happiness = 0.0f;
	else if (nation->stats[0].happiness > 1.0f)
		nation->stats[0].happiness = 1.0f;

	newString = NAT_GetHappinessString(nation);

	if (oldString != newString) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Nation %s changed happiness from %s to %s"), _(nation->name), oldString, newString);
		notifyType = NT_HAPPINESS_CHANGED;
	} else if (oldHappiness > middleHappiness && happiness < middleHappiness) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Nation %s changed happiness to %s"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_PLEASED;
	} else if (happiness < ccs.curCampaign->minhappiness && oldHappiness > ccs.curCampaign->minhappiness) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), /** @todo need to more specific message */
			_("Happiness of nation %s is %s and less than minimal happiness allowed to the campaign"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_MIN;
	} else {
		return;
	}

	MSO_CheckAddNewMessage(notifyType, _("Nation changed happiness"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
}

/**
 * @brief Nation saving callback
 * @param[out] p XML Node structure, where we write the information to
 */
qboolean NAT_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n = mxml_AddNode(p, SAVE_NATION_NATIONS);

	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = NAT_GetNationByIDX(i);
		mxml_node_t *s;
		int j;

		if (!nation)
			continue;

		s = mxml_AddNode(n, SAVE_NATION_NATION);
		mxml_AddString(s, SAVE_NATION_ID, nation->id);
		for (j = 0; j < MONTHS_PER_YEAR; j++) {
			mxml_node_t *ss;

			if (!nation->stats[j].inuse)
				continue;

			ss = mxml_AddNode(s, SAVE_NATION_MONTH);
			mxml_AddInt(ss, SAVE_NATION_MONTH_IDX, j);
			mxml_AddFloat(ss, SAVE_NATION_HAPPINESS, ccs.nations[i].stats[j].happiness);
			mxml_AddInt(ss, SAVE_NATION_XVI, ccs.nations[i].stats[j].xviInfection);
		}
	}
	return qtrue;
}

/**
 * @brief Nation loading xml callback
 * @param[in] p XML Node structure, where we get the information from
 */
qboolean NAT_LoadXML (mxml_node_t * p)
{
	mxml_node_t *n;
	mxml_node_t *s;

	n = mxml_GetNode(p, SAVE_NATION_NATIONS);
	if (!n)
		return qfalse;

	/* nations loop */
	for (s = mxml_GetNode(n, SAVE_NATION_NATION); s; s = mxml_GetNextNode(s, n, SAVE_NATION_NATION)) {
		mxml_node_t *ss;
		nation_t *nation = NAT_GetNationByID(mxml_GetString(s, SAVE_NATION_ID));

		if (!nation)
			return qfalse;

		/* month loop */
		for (ss = mxml_GetNode(s, SAVE_NATION_MONTH); ss; ss = mxml_GetNextNode(ss, s, SAVE_NATION_MONTH)) {
			int monthIDX = mxml_GetInt(ss, SAVE_NATION_MONTH_IDX, MONTHS_PER_YEAR);

			if (monthIDX < 0 || monthIDX >= MONTHS_PER_YEAR)
				return qfalse;

			nation->stats[monthIDX].inuse = qtrue;
			nation->stats[monthIDX].happiness = mxml_GetFloat(ss, SAVE_NATION_HAPPINESS, 0.0);
			nation->stats[monthIDX].xviInfection = mxml_GetInt(ss, SAVE_NATION_XVI, 0);
		}
	}
	return qtrue;
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

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the nation data from script file
 * @param[in] name Name or ID of the found nation
 * @param[in] text The text of the nation node
 * @sa nation_vals
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseNations (const char *name, const char **text)
{
	const char *errhead = "CL_ParseNations: unexpected end of file (nation ";
	nation_t *nation;
	const value_t *vp;
	const char *token;
	int i;

	if (ccs.numNations >= MAX_NATIONS) {
		Com_Printf("CL_ParseNations: nation number exceeding maximum number of nations: %i\n", MAX_NATIONS);
		return;
	}

	/* search for nations with same name */
	for (i = 0; i < ccs.numNations; i++)
		if (!strncmp(name, ccs.nations[i].id, sizeof(ccs.nations[i].id)))
			break;
	if (i < ccs.numNations) {
		Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	nation = &ccs.nations[ccs.numNations];
	memset(nation, 0, sizeof(*nation));
	nation->idx = ccs.numNations;
	ccs.numNations++;

	Com_DPrintf(DEBUG_CLIENT, "...found nation %s\n", name);
	nation->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	nation->stats[0].inuse = qtrue;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseNations: nation def \"%s\" without body ignored\n", name);
		ccs.numNations--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = nation_vals; vp->string; vp++)
			if (!strcmp(token, vp->string)) {
				/* found a definition */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)nation + (int)vp->ofs), cp_campaignPool, 0);
					break;
				default:
					if (Com_EParseValue(nation, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("CL_ParseNations: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseNations: unknown token \"%s\" ignored (nation %s)\n", token, name);
			Com_EParse(text, errhead, name);
		}
	} while (*text);
}

static const value_t city_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(city_t, name), 0},
	{"pos", V_POS, offsetof(city_t, pos), MEMBER_SIZEOF(city_t, pos)},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the city data from script file
 * @param[in] name ID of the found nation
 * @param[in] text The text of the nation node
 * @sa city_vals
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseCities (const char *name, const char **text)
{
	const char *errhead = "CL_ParseCities: unexpected end of file (nation ";
	city_t newCity;
	linkedList_t *cities;
	const value_t *vp;
	const char *token;

	/* search for cities with same name */
	for (cities = ccs.cities; cities != NULL; cities = cities->next) {
		const city_t *cty = (city_t*) cities->data;

		assert(cty);
		if (!strcmp(name, cty->id))
			break;
	}
	if (cities != NULL) {
		Com_Printf("CL_ParseCities: city def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	memset(&newCity, 0, sizeof(newCity));
	newCity.idx = ccs.numCities;
	ccs.numCities++;

	Com_DPrintf(DEBUG_CLIENT, "...found city %s\n", name);
	newCity.id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseCities: city def \"%s\" without body ignored\n", name);
		ccs.numCities--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = city_vals; vp->string; vp++)
			if (!strcmp(token, vp->string)) {
				/* found a definition */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)&newCity + (int)vp->ofs), cp_campaignPool, 0);
					break;
				default:
					if (Com_EParseValue(&newCity, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("CL_ParseCities: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseCities: unknown token \"%s\" ignored (nation %s)\n", token, name);
			Com_EParse(text, errhead, name);
		}
	} while (*text);

	/* Add city to the list */
	LIST_Add(&ccs.cities, (byte*) &newCity, sizeof(newCity));
}

/**
 * @brief Checks the parsed nations and cities for errors
 * @return false if there are errors - true otherwise
 */
qboolean NAT_ScriptSanityCheck (void)
{
	int i;
	int error = 0;
	linkedList_t *cities;

	/* Check if there is at least one map fitting city parameter for terror mission */
	for (cities = ccs.cities; cities != NULL; cities = cities->next) {
		int mapIdx;
		city_t *city = (city_t*) cities->data;
		const vec2_t pos = {city->pos[0], city->pos[1]};
		qboolean cityCanBeUsed = qfalse;
		qboolean parametersFit = qfalse;
		ufoType_t ufoTypes[UFO_MAX];
		int numTypes;

		if (!city->name) {
			error++;
			Com_Printf("...... city '%s' has no name\n", city->id);
		}

		numTypes = CP_TerrorMissionAvailableUFOs(NULL, ufoTypes);

		for (mapIdx = 0; mapIdx < cls.numMDs; mapIdx++) {
			const mapDef_t const *md = Com_GetMapDefByIDX(mapIdx);

			if (md->storyRelated)
				continue;

			if (MAP_PositionFitsTCPNTypes(pos, md->terrains, md->cultures, md->populations, NULL)) {
				/* this map fits city parameter, check if we have some terror mission UFOs available for this map */

				parametersFit = qtrue;

				/* no UFO on this map (LIST_ContainsString doesn't like empty string) */
				if (!md->ufos) {
					continue;
				}

				/* loop must be backward, as we remove items */
				for (i = numTypes - 1 ; i >= 0; i--) {
					if (LIST_ContainsString(md->ufos, Com_UFOTypeToShortName(ufoTypes[i]))) {
						REMOVE_ELEM(ufoTypes, i, numTypes);
					}
				}
			}
			if (numTypes == 0) {
				cityCanBeUsed = qtrue;
				break;
			}
		}

		if (!cityCanBeUsed) {
			error++;
			Com_Printf("...... city '%s' can't be used in game: it has no map fitting parameters\n", city->id);
			if (parametersFit) {
				Com_Printf("      (No map fitting");
				for (i = 0 ; i < numTypes; i++)
					Com_Printf(" %s", Com_UFOTypeToShortName(ufoTypes[i]));
				Com_Printf(")\n");
			}
			MAP_PrintParameterStringByPos(pos);
		}
	}

	return !error;
}

/*=====================================
Menu functions
=====================================*/


static void CP_NationStatsClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* Which entry in the list? */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= ccs.numNations)
		return;

	UI_PushWindow("nations", NULL);
	Cbuf_AddText(va("nation_select %i;", num));
}

/** Space for month-lines with 12 points for each nation. */
static screenPoint_t fundingPts[MAX_NATIONS][MONTHS_PER_YEAR];
static int usedFundPtslist = 0;
/** Space for 1 line (2 points) for each nation. */
static screenPoint_t colorLinePts[MAX_NATIONS][2];
static int usedColPtslists = 0;

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
 * @sa NAT_GetFunding
 */
static int CL_NationsMaxFunding (void)
{
	int m, n;
	int max = 0;

	for (n = 0; n < ccs.numNations; n++) {
		const nation_t *nation = &ccs.nations[n];
		for (m = 0; m < MONTHS_PER_YEAR; m++) {
			if (nation->stats[m].inuse) {
				const int funding = NAT_GetFunding(nation, m);
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

static int selectedNation = 0;

static lineStrip_t fundingLineStrip[MAX_NATIONS];

/**
 * @brief Draws a graph for the funding values over time.
 * @param[in] nation The nation to draw the graph for.
 * @param[in] node A pointer to a "linestrip" node that we want to draw in.
 * @param[out] funding Funding graph data in a lineStrip_t
 * @param[in] maxFunding The upper limit of the graph - all values will be scaled to this one.
 * @param[in] color If this is -1 draw the line for the current selected nation
 * @todo Somehow the display of the months isn't really correct right now (straight line :-/)
 */
static void CL_NationDrawStats (const nation_t *nation, uiNode_t *node, lineStrip_t *funding, int maxFunding, int color)
{
	int width, height, dx;
	int m;
	int minFunding = 0;
	int ptsNumber = 0;
	float dy;

	if (!nation || !node)
		return;

	/** @todo should be into the chart node code */
	width	= (int)node->size[0];
	height	= (int)node->size[1];
	dx = (int)(width / MONTHS_PER_YEAR);

	if (minFunding != maxFunding)
		dy = (float) height / (maxFunding - minFunding);
	else
		dy = 0;

	/* Generate pointlist. */
	/** @todo Sort this in reverse? -> Having current month on the right side? */
	for (m = 0; m < MONTHS_PER_YEAR; m++) {
		if (nation->stats[m].inuse) {
			const int fund = NAT_GetFunding(nation, m);
			fundingPts[usedFundPtslist][m].x = (m * dx);
			fundingPts[usedFundPtslist][m].y = height - dy * (fund - minFunding);
			ptsNumber++;
		} else {
			break;
		}
	}

	/* Guarantee displayable data even for only one month */
	if (ptsNumber == 1) {
		/* Set the second point half the distance to the next month to the right - small horizontal line. */
		fundingPts[usedFundPtslist][1].x = fundingPts[usedFundPtslist][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
		fundingPts[usedFundPtslist][1].y = fundingPts[usedFundPtslist][0].y;
		ptsNumber++;
	}

	/* Link graph to node */
	funding->pointList = (int*)fundingPts[usedFundPtslist];
	funding->numPoints = ptsNumber;
	if (color < 0) {
		Cvar_Set("mn_nat_symbol", va("nations/%s", ccs.nations[selectedNation].id));
		Vector4Copy(graphColorSelected, funding->color);
	} else {
		Vector4Copy(graphColors[color], funding->color);
	}

	usedFundPtslist++;
}

static lineStrip_t colorLineStrip[MAX_NATIONS];

/**
 * @brief Shows the current nation list + statistics.
 * @note See menu_stats.ufo
 */
static void CL_NationStatsUpdate_f (void)
{
	int i;
	uiNode_t *colorNode;
	uiNode_t *graphNode;
	int dy = 10;

	usedColPtslists = 0;

	colorNode = UI_GetNodeByPath("nations.nation_graph_colors");
	if (colorNode) {
		dy = (int)(colorNode->size[1] / MAX_NATIONS);
	}

	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = &ccs.nations[i];
		lineStrip_t *color = &colorLineStrip[i];
		const int funding = NAT_GetFunding(nation, 0);

		memset(color, 0, sizeof(*color));

		if (i > 0)
			colorLineStrip[i - 1].next = color;

		if (selectedNation == i) {
			UI_ExecuteConfunc("nation_marksel %i", i);
		} else {
			UI_ExecuteConfunc("nation_markdesel %i", i);
		}
		Cvar_Set(va("mn_nat_name%i",i), _(nation->name));
		Cvar_Set(va("mn_nat_fund%i",i), va("%i", funding));

		if (colorNode) {
			colorLinePts[usedColPtslists][0].x = 0;
			colorLinePts[usedColPtslists][0].y = (int)colorNode->size[1] - (int)colorNode->size[1] + dy * i;
			colorLinePts[usedColPtslists][1].x = (int)colorNode->size[0];
			colorLinePts[usedColPtslists][1].y = colorLinePts[usedColPtslists][0].y;

			color->pointList = (int*)colorLinePts[usedColPtslists];
			color->numPoints = 2;

			if (i == selectedNation) {
				Vector4Copy(graphColorSelected, color->color);
			} else {
				Vector4Copy(graphColors[i], color->color);
			}

			usedColPtslists++;
		}
	}

	UI_RegisterLineStrip(LINESTRIP_COLOR, &colorLineStrip[0]);

	/* Hide unused nation-entries. */
	for (i = ccs.numNations; i < MAX_NATIONS; i++) {
		UI_ExecuteConfunc("nation_hide %i", i);
	}

	/** @todo Display summary of nation info */

	/* Display graph of nations-values so far. */
	graphNode = UI_GetNodeByPath("nations.nation_graph_funding");
	if (graphNode) {
		const int maxFunding = CL_NationsMaxFunding();
		usedFundPtslist = 0;
		for (i = 0; i < ccs.numNations; i++) {
			nation_t *nation = &ccs.nations[i];
			lineStrip_t *funding = &fundingLineStrip[i];

			/* init the structure */
			memset(funding, 0, sizeof(funding));

			if (i > 0)
				fundingLineStrip[i - 1].next = funding;

			if (i == selectedNation) {
				CL_NationDrawStats(nation, graphNode, funding, maxFunding, -1);
			} else {
				CL_NationDrawStats(nation, graphNode, funding, maxFunding, i);
			}
		}

		UI_RegisterLineStrip(LINESTRIP_FUNDING, &fundingLineStrip[0]);
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
	if (nat < 0 || nat >= ccs.numNations) {
		Com_Printf("Invalid nation index: %is\n",nat);
		return;
	}

	selectedNation = nat;
	CL_NationStatsUpdate_f();
}

#ifdef DEBUG
/**
 * @brief Debug function to list all cities in game.
 * @note Called with debug_listcities.
 */
static void NAT_ListCities_f (void)
{
	linkedList_t *cities;

	for (cities = ccs.cities; cities != NULL; cities =cities->next) {
		const city_t const *city = (city_t*) cities->data;

		assert(city);
		Com_Printf("City '%s' -- position (%0.1f, %0.1f)\n", city->id, city->pos[0], city->pos[1]);
		MAP_PrintParameterStringByPos(city->pos);
	}
}

/**
 * @brief Scriptfunction to list all parsed nations with their current values
 * @note called with debug_listnations
 */
static void NAT_NationList_f (void)
{
	int i;
	for (i = 0; i < ccs.numNations; i++) {
		Com_Printf("Nation ID: %s\n", ccs.nations[i].id);
		Com_Printf("...max-funding %i c\n", ccs.nations[i].maxFunding);
		Com_Printf("...happiness %0.2f\n", ccs.nations[i].stats[0].happiness);
		Com_Printf("...xviInfection %i\n", ccs.nations[i].stats[0].xviInfection);
		Com_Printf("...max-soldiers %i\n", ccs.nations[i].maxSoldiers);
		Com_Printf("...max-scientists %i\n", ccs.nations[i].maxScientists);
		Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", ccs.nations[i].color[0], ccs.nations[i].color[1], ccs.nations[i].color[2], ccs.nations[i].color[3]);
		Com_Printf("...pos x:%.0f y:%.0f\n", ccs.nations[i].pos[0], ccs.nations[i].pos[1]);
	}
}
#endif

void NAT_InitStartup (void)
{
	Cmd_AddCommand("nation_stats_click", CP_NationStatsClick_f, NULL);
	Cmd_AddCommand("nation_update", CL_NationStatsUpdate_f, "Shows the current nation list + statistics.");
	Cmd_AddCommand("nation_select", CL_NationSelect_f, "Select nation and display all relevant information for it.");
#ifdef DEBUG
	Cmd_AddCommand("debug_listcities", NAT_ListCities_f, "Debug function to list all cities in game.");
	Cmd_AddCommand("debug_listnations", NAT_NationList_f, "List all nations on the game console");
#endif
}
