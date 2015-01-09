/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../../ui/ui_dataids.h" /* TEXT_STANDARD */
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_event_callbacks.h"

/**
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddTechAsResearchable_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <tech>\n", cgi->Cmd_Argv(0));
		return;
	}

	const char* techID = cgi->Cmd_Argv(1);
	technology_t* tech = RS_GetTechByID(techID);
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
	base_t* base;
	const objDef_t* item;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <item>\n", cgi->Cmd_Argv(0));
		return;
	}

	id = cgi->Cmd_Argv(1);
	baseID = atoi(cgi->Cmd_Argv(2));
	base = B_GetBaseByIDX(baseID);
	if (base == nullptr)
		return;

	/* i = item index */
	item = INVSH_GetItemByIDSilent(id);
	if (item) {
		technology_t* tech = RS_GetTechForItem(item);
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
	nation_t* nation;
	const nationInfo_t* stats;
	const mission_t* mission = GEO_GetSelectedMission();

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <absolute change value>\n", cgi->Cmd_Argv(0));
		return;
	}
	change = atof(cgi->Cmd_Argv(1));

	if (!mission) {
		Com_Printf("No mission selected - could not determine nation to use\n");
		return;
	}

	nation = GEO_GetNation(mission->pos);
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
	cgi->UI_RegisterText(TEXT_STANDARD, _("Congratulations! You have reached the end of the UFO:AI campaign.\n"
		"However, this is not the end of the road. The game remains in development.\n"
		"The campaign will be expanded with new missions, new enemies, "
		"new UFOs, new player controllable craft and more research.\n\n"
		"And YOU can help make it happen! Visit our forums or IRC channel to find\n"
		"out what you can do to help finish this game. Alternatively, you can just\n"
		"come by and talk about the game, or find other players for a multiplayer game.\n\n"
		"Thank you for playing, and we hope to see you around.\n\n"
		"   - The UFO:AI development team"));
	CP_EndCampaign(true);
}

/** @brief mission trigger functions */
static const cmdList_t cp_commands[] = {
	{"cp_add_researchable", CP_AddTechAsResearchable_f, "Add a tech as researchable"},
	{"cp_add_item", CP_AddItemAsCollected_f, "Add an item as collected"},
	{"cp_changehappiness", CP_ChangeNationHappiness_f, "Function to raise or lower nation happiness."},
	{"cp_endgame", CP_EndGame_f, "This command will end the current campaign"},

	{nullptr, nullptr, nullptr}
};

/**
 * @brief Add/Remove temporary mission trigger functions
 * @note These function can be defined via onwin/onlose parameters in missions.ufo
 * @param[in] add If true, add the trigger functions, otherwise delete them
 */
void CP_CampaignTriggerFunctions (bool add)
{
	const cmdList_t* commands;

	for (commands = cp_commands; commands->name; commands++)
		if (add)
			cgi->Cmd_AddCommand(commands->name, commands->function, commands->description);
		else
			cgi->Cmd_RemoveCommand(commands->name);
}
