/**
 * @file scp_parse.c
 * @brief Singleplayer static campaign script parser code
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

#include "scp_parse.h"
#include "../../cl_shared.h"
#include "../campaign/cp_campaign.h"
#include "../../../shared/parse.h"
#include "scp_shared.h"

static const value_t mission_vals[] = {
	{"pos", V_POS, offsetof(staticMission_t, pos), 0},

	{NULL, 0, 0, 0}
};

static void SCP_ParseMission (const char *name, const char **text)
{
	const char *errhead = "SCP_ParseMission: unexpected end of file (mission ";
	staticMission_t *ms;
	const value_t *vp;
	const char *token;
	int i;
	size_t writtenBytes;

	token = Com_Parse(text);
	if (token[0] == '\0' || text[0] == '\0') {
		Com_Printf("SCP_ParseMission: mission def \"%s\" without name ignored\n", name);
		return;
	}

	/* search for missions with same name */
	for (i = 0; i < scd->numMissions; i++)
		if (Q_streq(token, scd->missions[i].id))
			break;

	if (i < scd->numMissions) {
		Com_Printf("SCP_ParseMission: mission def \"%s\" with same name found, second ignored\n", token);
		return;
	}

	if (scd->numMissions >= MAX_STATIC_MISSIONS) {
		Com_Printf("SCP_ParseMission: Too many missions, ignore '%s'\n", token);
		return;
	}

	/* initialize the menu */
	ms = &scd->missions[scd->numMissions];
	OBJZERO(*ms);

	Q_strncpyz(ms->id, token, sizeof(ms->id));

	/* get it's body */
	token = Com_Parse(text);

	if (text[0] == '\0' || !Q_streq(token, "{")) {
		Com_Printf("SCP_ParseMission: mission def \"%s\" without body ignored\n", ms->id);
		return;
	}

	do {
		token = Com_EParse(text, errhead, ms->id);
		if (text[0] == '\0')
			break;
		if (token[0] == '}')
			break;

		for (vp = mission_vals; vp->string; vp++)
			if (Q_streq(token, vp->string)) {
				/* found a definition */
				token = Com_EParse(text, errhead, ms->id);
				if (text[0] == '\0')
					return;

				if (vp->ofs) {
					Com_ParseValue(ms, token, vp->type, vp->ofs, vp->size, &writtenBytes);
				} else {
					Com_Printf("SCP_ParseMission: unknown token '%s'\n", token);
				}
				break;
			}

		if (!vp->string) {
			if (Q_streq(token, "city")) {
				const city_t *city;
				token = Com_EParse(text, errhead, ms->id);
				if (text[0] == '\0')
					return;
				city = CP_GetCity(token);
				if (city == NULL)
					Com_Printf("SCP_ParseMission: unknown city \"%s\" ignored (mission %s)\n", token, ms->id);
				else
					Vector2Copy(city->pos, ms->pos);
			} else {
				Com_Printf("SCP_ParseMission: unknown token \"%s\" ignored (mission %s)\n", token, ms->id);
			}
		}

	} while (*text);

	mapDef_t *mapDef = cgi->Com_GetMapDefinitionByID(ms->id);
	if (mapDef == NULL) {
		Com_Printf("SCP_ParseMission: invalid mapdef for '%s'\n", ms->id);
		return;
	}

	if (Vector2Empty(ms->pos)) {
		if (!CP_GetRandomPosOnGeoscapeWithParameters(ms->pos, mapDef->terrains, mapDef->cultures, mapDef->populations, NULL)) {
			Com_Printf("SCP_ParseMission: could not find a valid position for '%s'\n", ms->id);
			return;
		}
	}

	scd->numMissions++;
}

static const value_t stageset_vals[] = {
	{"needed", V_STRING, offsetof(stageSet_t, needed), 0},
	{"delay", V_DATE, offsetof(stageSet_t, delay), 0},
	{"frame", V_DATE, offsetof(stageSet_t, frame), 0},
	{"expire", V_DATE, offsetof(stageSet_t, expire), 0},
	{"number", V_INT, offsetof(stageSet_t, number), MEMBER_SIZEOF(stageSet_t, number)},
	{"quota", V_INT, offsetof(stageSet_t, quota), MEMBER_SIZEOF(stageSet_t, quota)},
	{"ufos", V_INT, offsetof(stageSet_t, ufos), MEMBER_SIZEOF(stageSet_t, ufos)},
	{"nextstage", V_STRING, offsetof(stageSet_t, nextstage), 0},
	{"endstage", V_STRING, offsetof(stageSet_t, endstage), 0},
	{"commands", V_STRING, offsetof(stageSet_t, cmds), 0},

	{NULL, 0, 0, 0}
};

static void SCP_ParseStageSet (const char *name, const char **text)
{
	const char *errhead = "SCP_ParseStageSet: unexpected end of file (stageset ";
	stageSet_t *sp;
	const value_t *vp;
	char missionstr[256];
	const char *token, *misp;
	int j;
	size_t writtenBytes;

	/* initialize the stage */
	sp = &scd->stageSets[scd->numStageSets++];
	OBJZERO(*sp);
	Q_strncpyz(sp->name, name, sizeof(sp->name));

	/* get it's body */
	token = Com_Parse(text);
	if (text[0] == '\0' || !Q_streq(token, "{")) {
		Com_Printf("SCP_ParseStageSet: stageset def \"%s\" without body ignored\n", sp->name);
		scd->numStageSets--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, sp->name);
		if (!*text)
			break;
		if (token[0] == '}')
			break;

		/* check for some standard values */
		for (vp = stageset_vals; vp->string; vp++)
			if (Q_streq(token, vp->string)) {
				/* found a definition */
				token = Com_EParse(text, errhead, sp->name);
				if (!*text)
					return;

				Com_ParseValue(sp, token, vp->type, vp->ofs, vp->size, &writtenBytes);
				break;
			}
		if (vp->string)
			continue;

		/* get mission set */
		if (Q_streq(token, "missions")) {
			token = Com_EParse(text, errhead, sp->name);
			if (!*text)
				return;
			Q_strncpyz(missionstr, token, sizeof(missionstr));
			misp = missionstr;

			/* add mission options */
			sp->numMissions = 0;
			do {
				token = Com_Parse(&misp);
				if (!misp)
					break;

				for (j = 0; j < scd->numMissions; j++) {
					if (Q_streq(token, scd->missions[j].id)) {
						sp->missions[sp->numMissions++] = j;
						break;
					}
				}

				if (j == scd->numMissions) {
					const mapDef_t *mapDef = cgi->Com_GetMapDefinitionByID(token);
					if (mapDef != NULL) {
						if (j < MAX_STATIC_MISSIONS - 1) {
							/* we don't need and mission definition in the scripts if we just would like to link a mapdef */
							staticMission_t *mis = &scd->missions[j];
							OBJZERO(*mis);
							Q_strncpyz(mis->id, token, sizeof(mis->id));
							if (!CP_GetRandomPosOnGeoscapeWithParameters(mis->pos, mapDef->terrains, mapDef->cultures, mapDef->populations, NULL)) {
								Com_Printf("SCP_ParseMission: could not find a valid position for '%s'\n", mis->id);
								continue;
							}
							sp->missions[sp->numMissions++] = scd->numMissions++;
						}
					} else {
						Com_Printf("SCP_ParseStageSet: unknown mission \"%s\" ignored (stageset %s)\n", token, sp->name);
					}
				}
			} while (misp && sp->numMissions < MAX_SETMISSIONS);
			continue;
		}

		Com_Printf("SCP_ParseStageSet: unknown token \"%s\" ignored (stageset %s)\n", token, sp->name);
	} while (*text);
}

static void SCP_ParseStage (const char *name, const char **text)
{
	const char *errhead = "SCP_ParseStage: unexpected end of file (stage ";
	stage_t *sp;
	const char *token;
	int i;

	token = Com_Parse(text);
	if (token[0] == '\0' || text[0] == '\0') {
		Com_Printf("SCP_ParseStage: stage def \"%s\" without name ignored\n", name);
		return;
	}

	/* search for campaigns with same name */
	for (i = 0; i < scd->numStages; i++)
		if (Q_streq(token, scd->stages[i].name))
			break;

	if (i < scd->numStages) {
		Com_Printf("SCP_ParseStage: stage def \"%s\" with same name found, second ignored\n", token);
		return;
	}

	/* initialize the stage */
	sp = &scd->stages[scd->numStages++];
	OBJZERO(*sp);
	Q_strncpyz(sp->name, token, sizeof(sp->name));

	/* get it's body */
	token = Com_Parse(text);
	if (text[0] == '\0' || !Q_streq(token, "{")) {
		Com_Printf("SCP_ParseStage: stage def \"%s\" without body ignored\n", sp->name);
		scd->numStages--;
		return;
	}

	sp->first = scd->numStageSets;

	do {
		token = Com_EParse(text, errhead, sp->name);
		if (text[0] == '\0')
			break;
		if (token[0] == '}')
			break;

		if (Q_streq(token, "set")) {
			token = Com_EParse(text, errhead, sp->name);
			SCP_ParseStageSet(token, text);
		} else
			Com_Printf("SCP_ParseStage: unknown token \"%s\" ignored (stage %s)\n", token, sp->name);
	} while (*text);

	sp->num = scd->numStageSets - sp->first;
}

static void SCP_ParseStaticCampaignData (const char *name, const char **text)
{
	const char *errhead = "SCP_ParseStaticCampaignData: unexpected end of file (names ";
	const char *token;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("SCP_ParseStaticCampaignData: staticcampaign \"%s\" without body ignored\n", name);
		return;
	}

	do {
		/* get the id */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (Q_streq(token, "mission")) {
			SCP_ParseMission(name, text);
		} else if (Q_streq(token, "stage")) {
			SCP_ParseStage(name, text);
		} else {
			Com_Printf("SCP_ParseStaticCampaignData: unknown token '%s'\n", token);
		}
	} while (*text);
}

void SCP_Parse (void)
{
	const char *type, *name, *text;

	cgi->FS_BuildFileList("ufos/*.ufo");
	cgi->FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	while ((type = cgi->FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL) {
		if (Q_streq(type, "staticcampaign")) {
			const char **textPtr = &text;
			SCP_ParseStaticCampaignData(name, textPtr);
		}
	}
}
