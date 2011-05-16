/**
 * @file cp_mission_triggers.c
 * @brief Campaign mission triggers
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
#include "../../ui/ui_main.h" /* TEXT_STANDARD */
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"

/**
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddTechAsResearchable_f (void)
{
	const char *techID;
	technology_t *tech;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <tech>\n", Cmd_Argv(0));
		return;
	}

	techID = Cmd_Argv(1);
	tech = RS_GetTechByID(techID);
	RS_MarkOneResearchable(tech);
}

/**
 * @brief For things like craft_ufo_scout that are no real items this function will
 * increase the collected counter by one
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddItemAsCollected_f (void)
{
	int baseID;
	const char* id;
	base_t *base;
	const objDef_t *item;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <item>\n", Cmd_Argv(0));
		return;
	}

	id = Cmd_Argv(1);
	baseID = atoi(Cmd_Argv(2));
	base = B_GetBaseByIDX(baseID);
	if (base == NULL)
		return;

	/* i = item index */
	item = INVSH_GetItemByIDSilent(id);
	if (item) {
		technology_t *tech = RS_GetTechForItem(item);
		base->storage.numItems[item->idx]++;
		Com_DPrintf(DEBUG_CLIENT, "add item: '%s'\n", item->id);
		RS_MarkCollected(tech);
	}
}

/**
 * @brief Changes nation happiness by given value.
 * @note There must be argument passed to this function being converted to float.
 */
static void CP_ChangeNationHappiness_f (void)
{
	float change;
	nation_t *nation;
	const nationInfo_t *stats;
	const mission_t *mission = MAP_GetSelectedMission();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <absolute change value>\n", Cmd_Argv(0));
		return;
	}
	change = atof(Cmd_Argv(1));

	if (!mission) {
		Com_Printf("No mission selected - could not determine nation to use\n");
		return;
	}

	nation = MAP_GetNation(mission->pos);
	assert(nation);

	stats = NAT_GetCurrentMonthInfo(nation);
	NAT_SetHappiness(ccs.curCampaign->minhappiness, nation, stats->happiness + change);
}

/**
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_EndGame_f (void)
{
	UI_RegisterText(TEXT_STANDARD, _("Congratulations! You have reached the end of the UFO:AI campaign.\n"
		"However, this is not the end of the road. The game remains in development.\n"
		"The campaign will be expanded with new missions, new enemies, "
		"new UFOs, new player controllable craft and more research.\n\n"
		"And YOU can help make it happen! Visit our forums or IRC channel to find\n"
		"out what you can do to help finish this game. Alternatively, you can just\n"
		"come by and talk about the game, or find other players for a multiplayer game.\n\n"
		"Thank you for playing, and we hope to see you around.\n\n"
		"   - The UFO:AI development team"));
	CP_EndCampaign(qtrue);
}

/** @brief mission trigger functions */
static const cmdList_t cp_commands[] = {
	{"cp_add_researchable", CP_AddTechAsResearchable_f, "Add a tech as researchable"},
	{"cp_add_item", CP_AddItemAsCollected_f, "Add an item as collected"},
	{"cp_changehappiness", CP_ChangeNationHappiness_f, "Function to raise or lower nation happiness."},
	{"cp_endgame", CP_EndGame_f, "This command will end the current campaign"},

	{NULL, NULL, NULL}
};

/**
 * @brief Add/Remove temporary mission trigger functions
 * @note These function can be defined via onwin/onlose parameters in missions.ufo
 * @param[in] add If true, add the trigger functions, otherwise delete them
 */
static void CP_MissionTriggerFunctions (qboolean add)
{
	const cmdList_t *commands;

	for (commands = cp_commands; commands->name; commands++)
		if (add)
			Cmd_AddCommand(commands->name, commands->function, commands->description);
		else
			Cmd_RemoveCommand(commands->name);
}

/**
 * @brief Executes console commands after a mission
 *
 * @param[in] mission Pointer to the mission to execute the triggers for
 * @param[in] won Int value that is one when you've won the game, and zero when the game was lost
 * Can execute console commands (triggers) on win and lose
 * This can be used for story dependent missions
 */
void CP_ExecuteMissionTrigger (const mission_t *mission, qboolean won)
{
	Com_DPrintf(DEBUG_CLIENT, "Execute mission triggers\n");

	if (mission == NULL)
		return;

	/* we add them only here - and remove them afterwards to prevent cheating */
	CP_MissionTriggerFunctions(qtrue);

	if (won) {
		if (mission->onwin[0] != '\0') {
			Com_DPrintf(DEBUG_CLIENT, "...won - executing '%s'\n", mission->onwin);
			Cmd_ExecuteString(mission->onwin);
		}
		if (mission->mapDef && mission->mapDef->onwin != NULL) {
			Com_DPrintf(DEBUG_CLIENT, "...won - executing '%s'\n", mission->mapDef->onwin);
			Cmd_ExecuteString(mission->mapDef->onwin);
		}
	} else {
		if (mission->onlose[0] != '\0') {
			Com_DPrintf(DEBUG_CLIENT, "...lost - executing '%s'\n", mission->onlose);
			Cmd_ExecuteString(mission->onlose);
		}
		if (mission->mapDef && mission->mapDef->onlose != NULL) {
			Com_DPrintf(DEBUG_CLIENT, "...lost - executing '%s'\n", mission->mapDef->onlose);
			Cmd_ExecuteString(mission->mapDef->onlose);
		}
	}

	CP_MissionTriggerFunctions(qfalse);
}
