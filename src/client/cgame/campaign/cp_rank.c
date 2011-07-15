/**
 * @file cp_rank.c
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

#include "../../cl_shared.h"
#include "../../../shared/parse.h"
#include "cp_rank.h"
#include "cp_campaign.h"

/**
 * @brief Get the index of the given rankID in ccs.ranks array
 * @param[in] rankID Script id of the rank
 * @return -1 if no rank with rankID found
 */
int CL_GetRankIdx (const char* rankID)
{
	int i;

	for (i = 0; i < ccs.numRanks; i++) {
		if (Q_streq(ccs.ranks[i].id, rankID))
			return i;
	}

	return -1;
}

/**
 * @brief Returns a rank at an index
 * @param[in] index Index of rank in ccs.ranks
 * @return NULL on invalid index
 * @return pointer to the rank definition otherwise
 */
rank_t *CL_GetRankByIdx (const int index)
{
	if (index < 0 || index >= ccs.numRanks)
		return NULL;

	return &ccs.ranks[index];
}

static const value_t rankValues[] = {
	{"name", V_TRANSLATION_STRING, offsetof(rank_t, name), 0},
	{"shortname", V_TRANSLATION_STRING,	offsetof(rank_t, shortname), 0},
	{"image", V_HUNK_STRING, offsetof(rank_t, image), 0},
	{"mind", V_INT, offsetof(rank_t, mind), MEMBER_SIZEOF(rank_t, mind)},
	{"killed_enemies", V_INT, offsetof(rank_t, killedEnemies), MEMBER_SIZEOF(rank_t, killedEnemies)},
	{"killed_others", V_INT, offsetof(rank_t, killedOthers), MEMBER_SIZEOF(rank_t, killedOthers)},
	{"factor", V_FLOAT, offsetof(rank_t, factor), MEMBER_SIZEOF(rank_t, factor)},
	{NULL, 0, 0, 0}
};

/**
 * @brief Parse medals and ranks defined in the medals.ufo file.
 * @sa CL_ParseScriptFirst
 */
void CL_ParseRanks (const char *name, const char **text)
{
	rank_t *rank;
	const char *errhead = "CL_ParseRanks: unexpected end of file (medal/rank ";
	const char *token;
	int i;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseRanks: rank/medal \"%s\" without body ignored\n", name);
		return;
	}

	for (i = 0; i < ccs.numRanks; i++) {
		if (Q_streq(name, ccs.ranks[i].name)) {
			Com_Printf("CL_ParseRanks: Rank with same name '%s' already loaded.\n", name);
			return;
		}
	}
	/* parse ranks */
	if (ccs.numRanks >= MAX_RANKS) {
		Com_Printf("CL_ParseRanks: Too many rank descriptions, '%s' ignored.\n", name);
		ccs.numRanks = MAX_RANKS;
		return;
	}

	rank = &ccs.ranks[ccs.numRanks++];
	OBJZERO(*rank);
	rank->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (Com_ParseBlockToken(name, text, rank, rankValues, cp_campaignPool, token)) {
			continue;
		} else if (Q_streq(token, "type")) {
			/* employeeType_t */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return;
			/* error check is performed in E_GetEmployeeType function */
			rank->type = E_GetEmployeeType(token);
		} else
			Com_Printf("CL_ParseRanks: unknown token \"%s\" ignored (medal/rank %s)\n", token, name);
	} while (*text);

	if (rank->image == NULL || !strlen(rank->image))
		Com_Error(ERR_DROP, "CL_ParseRanks: image is missing for rank %s", rank->id);

	if (rank->name == NULL || !strlen(rank->name))
		Com_Error(ERR_DROP, "CL_ParseRanks: name is missing for rank %s", rank->id);

	if (rank->shortname == NULL || !strlen(rank->shortname))
		rank->shortname = rank->name;
}
