/**
 * @file cl_nation.c
 * @brief Nation code
 * @note Functions with NAT_*
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
#include "cl_map.h"
#include "cl_ufo.h"

/**
 * @brief Return a nation-pointer by the nations id (nation_t->id) text.
 * @param[in] nationID nation id as defined in (nation_t->id)
 * @return nation_t pointer or NULL if nothing found (=error).
 */
nation_t *NAT_GetNationByID (const char *nationID)
{
	int i;

	for (i = 0; i < gd.numNations; i++) {
		nation_t *nation = &gd.nations[i];
		if (!Q_strcmp(nation->id, nationID))
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
		const float difficultyModifier = (4.0f + curCampaign->difficulty) / 400;

		/* Some non-water location have no nation */
		if (nation) {
			float happinessFactor;
			switch (mission->stage) {
			case STAGE_TERROR_MISSION:
			case STAGE_SUBVERT_GOV:
			case STAGE_RECON_GROUND:
			case STAGE_SPREAD_XVI:
			case STAGE_HARVEST:
				happinessFactor = HAPPINESS_ALIEN_MISSION_LOSS - difficultyModifier;
				break;
			default:
				/* mission is not active on earth, skip this mission */
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
	const float middleHappiness = (curCampaign->minhappiness + 1.0) / 2;
	notify_t notifyType = NT_NUM_NOTIFYTYPE;

	nation->stats[0].happiness = happiness;
	if (nation->stats[0].happiness < 0.0f)
		nation->stats[0].happiness = 0.0f;
	else if (nation->stats[0].happiness > 1.0f)
		nation->stats[0].happiness = 1.0f;

	newString = NAT_GetHappinessString(nation);

	if (oldString != newString) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer),
			_("Nation %s changed happiness from %s to %s"), _(nation->name), oldString, newString);
		notifyType = NT_HAPPINESS_CHANGED;
	} else if (oldHappiness > middleHappiness && happiness < middleHappiness) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer),
			_("Nation %s changed happiness to %s"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_PLEASED;
	} else if (happiness < curCampaign->minhappiness && oldHappiness > curCampaign->minhappiness) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), /** @todo need to more specific message */
			_("Happiness of nation %s is %s and less than minimal happiness allowed to the campaign"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_MIN;
	} else {
		return;
	}

	MSO_CheckAddNewMessage(notifyType, _("Nation changed happiness"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
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
			MSG_WriteLong(sb, gd.nations[i].stats[j].xviInfection);
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
			gd.nations[i].stats[j].xviInfection = MSG_ReadLong(sb);
			gd.nations[i].stats[j].alienFriendly = MSG_ReadFloat(sb);
		}
	}
	return qtrue;
}

/*==========================================
Parsing
==========================================*/

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
	int i;

	if (gd.numNations >= MAX_NATIONS) {
		Com_Printf("CL_ParseNations: nation number exceeding maximum number of nations: %i\n", MAX_NATIONS);
		return;
	}

	/* search for nations with same name */
	for (i = 0; i < gd.numNations; i++)
		if (!Q_strncmp(name, gd.nations[i].id, sizeof(gd.nations[i].id)))
			break;
	if (i < gd.numNations) {
		Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	nation = &gd.nations[gd.numNations];
	memset(nation, 0, sizeof(*nation));
	nation->idx = gd.numNations;
	gd.numNations++;

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
					if (Com_EParseValue(nation, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("CL_ParseNations: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseNations: unknown token \"%s\" ignored (nation %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

static const value_t city_vals[] = {
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(city_t, name), 0},
	{"pos", V_POS, offsetof(city_t, pos), MEMBER_SIZEOF(city_t, pos)},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the city data from script file
 * @param[in] name ID of the found nation
 * @param[in] text The text of the nation node
 * @sa city_vals
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseCities (const char *name, const char **text)
{
	const char *errhead = "CL_ParseCities: unexpected end of file (nation ";
	city_t *city;
	const value_t *vp;
	const char *token;
	int i;

	if (gd.numCities >= MAX_CITIES) {
		Com_Printf("CL_ParseCities: city number exceeding maximum number of cities: %i\n", MAX_CITIES);
		return;
	}

	/* search for cities with same name */
	for (i = 0; i < gd.numCities; i++)
		if (!Q_strncmp(name, gd.cities[i].id, sizeof(gd.cities[i].id)))
			break;
	if (i < gd.numCities) {
		Com_Printf("CL_ParseCities: city def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	city = &gd.cities[gd.numCities];
	memset(city, 0, sizeof(*city));
	city->idx = gd.numCities;
	gd.numCities++;

	Com_DPrintf(DEBUG_CLIENT, "...found city %s\n", name);
	city->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseCities: city def \"%s\" without body ignored\n", name);
		gd.numCities--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = city_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_MANUAL_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)city + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				default:
					if (Com_EParseValue(city, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("CL_ParseCities: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseCities: unknown token \"%s\" ignored (nation %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}


/**
 * @brief Checks the parsed nations and cities for errors
 * @return false if there are errors - true otherwise
 */
qboolean NAT_ScriptSanityCheck (void)
{
	int idx, i;
	int error = 0;

	/* Check if there is at least one map fitting city parameter for terror mission */
	for (idx = 0; idx < gd.numCities; idx++) {
		int mapIdx;
		city_t *city = &gd.cities[idx];
		const vec2_t pos = {city->pos[0], city->pos[1]};
		qboolean cityCanBeUsed = qfalse;
		qboolean parametersFit = qfalse;
		int ufoTypes[UFO_MAX];
		int numTypes;

		if (!city->name) {
			error++;
			Com_Printf("...... city '%s' has no name\n", city->id);
		}

		numTypes = CP_TerrorMissionAvailableUFOs(NULL, ufoTypes);

		for (mapIdx = 0; mapIdx < csi.numMDs; mapIdx++) {
			const mapDef_t const *md = &csi.mds[mapIdx];

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
					if (LIST_ContainsString(md->ufos, UFO_TypeToShortName(ufoTypes[i]))) {
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
					Com_Printf(" %s", UFO_TypeToShortName(ufoTypes[i]));
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

	MN_PushMenu("nations", NULL);
	Cbuf_AddText(va("nation_select %i;", num));
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
 * @sa NAT_GetFunding
 */
static int CL_NationsMaxFunding (void)
{
	int m, n;
	int max = 0;

	for (n = 0; n < gd.numNations; n++) {
		nation_t *nation = &gd.nations[n];
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

/**
 * @brief Draws a graph for the funding values over time.
 * @param[in] nation The nation to draw the graph for.
 * @param[in] node A pointer to a "linestrip" node that we want to draw in.
 * @param[in] maxFunding The upper limit of the graph - all values willb e scaled to this one.
 * @param[in] color If this is -1 draw the line for the current selected nation
 * @todo Somehow the display of the months isnt really correct right now (straight line :-/)
 */
static void CL_NationDrawStats (const nation_t *nation, menuNode_t *node, int maxFunding, int color)
{
	int width, height, x, y, dx;
	int m;
	int minFunding = 0;
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
			const int funding = NAT_GetFunding(nation, m);
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
	if (node->u.linestrip.numStrips >= MAX_LINESTRIPS - 1)
		return;

	/* Link graph to node */
	node->u.linestrip.pointList[node->u.linestrip.numStrips] = (int*)fundingPts[usedFundPtslist];
	node->u.linestrip.numPoints[node->u.linestrip.numStrips] = ptsNumber;
	if (color < 0) {
		Cvar_Set("mn_nat_symbol", va("nations/%s", gd.nations[selectedNation].id));
		Vector4Copy(graphColorSelected, node->u.linestrip.color[node->u.linestrip.numStrips]);
	} else {
		Vector4Copy(graphColors[color], node->u.linestrip.color[node->u.linestrip.numStrips]);
	}
	node->u.linestrip.numStrips++;

	usedFundPtslist++;
}

/**
 * @brief Shows the current nation list + statistics.
 * @note See menu_stats.ufo
 */
static void CL_NationStatsUpdate_f (void)
{
	int i;
	int funding, maxFunding;
	menuNode_t *colorNode;
	menuNode_t *graphNode;
	const vec4_t colorAxes = {1, 1, 1, 0.5};
	int dy = 10;

	usedColPtslists = 0;

	colorNode = MN_GetNodeFromCurrentMenu("nation_graph_colors");
	if (colorNode) {
		dy = (int)(colorNode->size[1] / MAX_NATIONS);
		colorNode->u.linestrip.numStrips = 0;
	}

	for (i = 0; i < gd.numNations; i++) {
		funding = NAT_GetFunding(&(gd.nations[i]), 0);

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

			colorNode->u.linestrip.pointList[colorNode->u.linestrip.numStrips] = (int*)colorLinePts[usedColPtslists];
			colorNode->u.linestrip.numPoints[colorNode->u.linestrip.numStrips] = 2;

			if (i == selectedNation) {
				Vector4Copy(graphColorSelected, colorNode->u.linestrip.color[colorNode->u.linestrip.numStrips]);
			} else {
				Vector4Copy(graphColors[i], colorNode->u.linestrip.color[colorNode->u.linestrip.numStrips]);
			}

			usedColPtslists++;
			colorNode->u.linestrip.numStrips++;
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
		graphNode->u.linestrip.numStrips = 0;

		/* Generate axes & link to node. */
		/** @todo Maybe create a margin toward the axes? */
		coordAxesPts[0].x = graphNode->pos[0];	/* x */
		coordAxesPts[0].y = graphNode->pos[1] - (int)graphNode->size[1];	/* y - height */
		coordAxesPts[1].x = graphNode->pos[0];	/* x */
		coordAxesPts[1].y = graphNode->pos[1];	/* y */
		coordAxesPts[2].x = graphNode->pos[0] + (int)graphNode->size[0];	/* x + width */
		coordAxesPts[2].y = graphNode->pos[1];	/* y */
		graphNode->u.linestrip.pointList[graphNode->u.linestrip.numStrips] = (int*)coordAxesPts;
		graphNode->u.linestrip.numPoints[graphNode->u.linestrip.numStrips] = 3;
		Vector4Copy(colorAxes, graphNode->u.linestrip.color[graphNode->u.linestrip.numStrips]);
		graphNode->u.linestrip.numStrips++;

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

#ifdef DEBUG
/**
 * @brief Debug function to list all cities in game.
 * @note Called with debug_listcities.
 */
static void CL_ListCities_f (void)
{
	int idx;

	for (idx = 0; idx < gd.numCities; idx++) {
		const city_t const *city = &gd.cities[idx];

		Com_Printf("City '%s' -- position (%0.1f, %0.1f)\n", city->id, city->pos[0], city->pos[1]);
		MAP_PrintParameterStringByPos(city->pos);
	}
}
#endif

void NAT_InitStartup (void)
{
	Cmd_AddCommand("nation_stats_click", CP_NationStatsClick_f, NULL);
	Cmd_AddCommand("nation_update", CL_NationStatsUpdate_f, "Shows the current nation list + statistics.");
	Cmd_AddCommand("nation_select", CL_NationSelect_f, "Select nation and display all relevant information for it.");
#ifdef DEBUG
	Cmd_AddCommand("debug_listcities", CL_ListCities_f, "Debug function to list all cities in game.");
#endif
}
