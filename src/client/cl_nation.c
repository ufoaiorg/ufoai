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

#include "client.h"
#include "cl_global.h"
#include "cl_map.h"

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
		/* Some non-water location have no nation */
		if (nation) {
			float happinessFactor;
			switch (mission->stage) {
			case STAGE_TERROR_MISSION:
			case STAGE_SUBVERT_GOV:
				happinessFactor = (4.0f - difficulty->integer) / 40.0f;
				break;
			case STAGE_RECON_GROUND:
			case STAGE_SPREAD_XVI:
			case STAGE_HARVEST:
				happinessFactor = (4.0f - difficulty->integer) / 80.0f;
				break;
			default:
				/* mission is not active on earth, skip this mission */
				continue;
			}

			NAT_SetHappiness(nation, nation->stats[0].happiness - happinessFactor);
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

	nation->stats[0].happiness = happiness;

	if (nation->stats[0].happiness < 0.0f)
		nation->stats[0].happiness = 0.0f;
	else if (nation->stats[0].happiness > 1.0f)
		nation->stats[0].happiness = 1.0f;

	newString = NAT_GetHappinessString(nation);
	if (oldString != newString) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer),
			_("Nation %s changed happiness from %s to %s"), _(nation->name), oldString, newString);
		MN_AddNewMessage(_("Nation changed happiness"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
	}
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

	if (gd.numNations >= MAX_NATIONS) {
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
	} while (*text);
}
