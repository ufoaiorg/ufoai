/**
 * @file cl_rank.c
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
#include "cl_game.h"
#include "cl_rank.h"

/**
 * @brief Get the index number of the given rankID
 */
int CL_GetRankIdx (const char* rankID)
{
	int i;

	/* only check in singleplayer */
	if (!GAME_IsCampaign())
		return -1;

	for (i = 0; i < gd.numRanks; i++) {
		if (!Q_strncmp(gd.ranks[i].id, rankID, MAX_VAR))
			return i;
	}
	Com_Printf("Could not find rank '%s'\n", rankID);
	return -1;
}

static const value_t rankValues[] = {
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(rank_t, name), 0},
	{"shortname", V_TRANSLATION_MANUAL_STRING,	offsetof(rank_t, shortname), 0},
	{"image", V_CLIENT_HUNK_STRING, offsetof(rank_t, image), 0},
	{"mind", V_INT, offsetof(rank_t, mind), MEMBER_SIZEOF(rank_t, mind)},
	{"killed_enemies", V_INT, offsetof(rank_t, killed_enemies), MEMBER_SIZEOF(rank_t, killed_enemies)},
	{"killed_others", V_INT, offsetof(rank_t, killed_others), MEMBER_SIZEOF(rank_t, killed_others)},
	{"factor", V_FLOAT, offsetof(rank_t, factor), MEMBER_SIZEOF(rank_t, factor)},
	{NULL, 0, 0, 0}
};

/**
 * @brief Parse medals and ranks defined in the medals.ufo file.
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseRanks (const char *name, const char **text)
{
	rank_t *rank;
	const char *errhead = "CL_ParseRanks: unexpected end of file (medal/rank ";
	const char *token;
	const value_t *v;
	int i;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseRanks: rank/medal \"%s\" without body ignored\n", name);
		return;
	}

	for (i = 0; i < gd.numRanks; i++) {
		if (!Q_strcmp(name, gd.ranks[i].name)) {
			Com_Printf("CL_ParseRanks: Rank with same name '%s' already loaded.\n", name);
			return;
		}
	}
	/* parse ranks */
	if (gd.numRanks >= MAX_RANKS) {
		Com_Printf("CL_ParseRanks: Too many rank descriptions, '%s' ignored.\n", name);
		gd.numRanks = MAX_RANKS;
		return;
	}

	rank = &gd.ranks[gd.numRanks++];
	memset(rank, 0, sizeof(*rank));
	rank->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		for (v = rankValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				switch (v->type) {
				case V_TRANSLATION_MANUAL_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)rank + (int)v->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				default:
					Com_EParseValue(rank, token, v->type, v->ofs, v->size);
					break;
				}
				break;
			}

		if (!Q_strncmp(token, "type", 4)) {
			/* employeeType_t */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			/* error check is performed in E_GetEmployeeType function */
			rank->type = E_GetEmployeeType(token);
		} else if (!v->string)
			Com_Printf("CL_ParseRanks: unknown token \"%s\" ignored (medal/rank %s)\n", token, name);
	} while (*text);
}

