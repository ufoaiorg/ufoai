/**
 * @file
 * @brief Campaign statistics
 */

/*
Copyright (C) 2002-2020 UFO: Alien Invasion.

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
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_xvi.h"
#include "save/save_statistics.h"

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 * @todo This is very redundant with NAT_HandleBudget Investigate and clean up.
 */
static void STATS_Update_f (void)
{
	const campaign_t* campaign = ccs.curCampaign;
	static char statsBuffer[MAX_STATS_BUFFER];
	OBJZERO(statsBuffer);
	char* pos = statsBuffer;
	/* campaign */
	cgi->UI_RegisterText(TEXT_GENERIC, pos);
	Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Max. allowed debts: %ic\n"), campaign->negativeCreditsUntilLost);

	/* only show the xvi spread data when it's available */
	if (CP_IsXVIVisible()) {
		Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos),
			_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			campaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate());
	}
}

/**
 * @brief Console command for UI to gather expenses
 */
static void STAT_GetExpenses_f (void)
{
	const int argCount = cgi->Cmd_Argc();
	if (argCount < 2) {
		cgi->Com_Printf("Usage: %s <confunc>\n", cgi->Cmd_Argv(0));
		return;
	}
	char callback[MAX_VAR];
	Q_strncpyz(callback, cgi->Cmd_Argv(1), sizeof(callback));

	/** @todo should have something like CP_GetCurrentCampaign */
	const campaign_t* campaign = ccs.curCampaign;
	const salary_t* salary = &campaign->salaries;

	int count = 0;
	int cost = 0;
	for (int i = 0; i < MAX_EMPL; i++) {
		E_Foreach(i, employee) {
			if (!employee->isHired())
				continue;
			cost += employee->salary();
			count++;
		}

	}
	cgi->UI_ExecuteConfunc("%s %s \"%s\" %d",
		callback,
		"employee",
		ngettext("Salary", "Salaries", count),
		cost
	);

	count = 0;
	cost = 0;
	AIR_Foreach(aircraft) {
		if (aircraft->status == AIR_CRASHED)
			continue;
		cost += aircraft->price * salary->aircraftFactor / salary->aircraftDivisor;
		count++;
	}
	cgi->UI_ExecuteConfunc("%s %s \"%s\" %d",
		callback,
		"aircraft",
		ngettext("Aircraft", "Aircraft", count),
		cost
	);

	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		cost = CP_GetSalaryUpKeepBase(salary, base);
		cgi->UI_ExecuteConfunc("%s base_%d \"%s: %s\" %d",
			callback,
			base->idx,
			_("Base"),
			base->name,
			cost
		);
	}

	if (ccs.credits < 0) {
		const float interest = ccs.credits * campaign->salaries.debtInterest;
		cgi->UI_ExecuteConfunc("%s %s \"%s\" %d",
			callback,
			"interest",
			_("Debt interest:"),
			(int)ceil(interest)
		);
	}
}


/**
 * @brief Save callback for savegames in XML Format
 * @param[out] parent XML Node structure, where we write the information to
 */
bool STATS_SaveXML (xmlNode_t* parent)
{
	xmlNode_t* stats;

	stats = cgi->XML_AddNode(parent, SAVE_STATS_STATS);

	cgi->XML_AddIntValue(stats, SAVE_STATS_MISSIONS, ccs.campaignStats.missions);
	cgi->XML_AddIntValue(stats, SAVE_STATS_MISSIONSWON, ccs.campaignStats.missionsWon);
	cgi->XML_AddIntValue(stats, SAVE_STATS_MISSIONSLOST, ccs.campaignStats.missionsLost);
	cgi->XML_AddIntValue(stats, SAVE_STATS_BASESBUILT, ccs.campaignStats.basesBuilt);
	cgi->XML_AddIntValue(stats, SAVE_STATS_BASESATTACKED, ccs.campaignStats.basesAttacked);
	cgi->XML_AddIntValue(stats, SAVE_STATS_INSTALLATIONSBUILT, ccs.campaignStats.installationsBuilt);
	cgi->XML_AddIntValue(stats, SAVE_STATS_INTERCEPTIONS, ccs.campaignStats.interceptions);
	cgi->XML_AddIntValue(stats, SAVE_STATS_SOLDIERSLOST, ccs.campaignStats.soldiersLost);
	cgi->XML_AddIntValue(stats, SAVE_STATS_SOLDIERSNEW, ccs.campaignStats.soldiersNew);
	cgi->XML_AddIntValue(stats, SAVE_STATS_KILLEDALIENS, ccs.campaignStats.killedAliens);
	cgi->XML_AddIntValue(stats, SAVE_STATS_CAPTUREDALIENS, ccs.campaignStats.capturedAliens);
	cgi->XML_AddIntValue(stats, SAVE_STATS_RESCUEDCIVILIANS, ccs.campaignStats.rescuedCivilians);
	cgi->XML_AddIntValue(stats, SAVE_STATS_RESEARCHEDTECHNOLOGIES, ccs.campaignStats.researchedTechnologies);
	cgi->XML_AddIntValue(stats, SAVE_STATS_MONEYINTERCEPTIONS, ccs.campaignStats.moneyInterceptions);
	cgi->XML_AddIntValue(stats, SAVE_STATS_MONEYBASES, ccs.campaignStats.moneyBases);
	cgi->XML_AddIntValue(stats, SAVE_STATS_MONEYRESEARCH, ccs.campaignStats.moneyResearch);
	cgi->XML_AddIntValue(stats, SAVE_STATS_MONEYWEAPONS, ccs.campaignStats.moneyWeapons);
	cgi->XML_AddIntValue(stats, SAVE_STATS_UFOSDETECTED, ccs.campaignStats.ufosDetected);
	cgi->XML_AddIntValue(stats, SAVE_STATS_ALIENBASESBUILT, ccs.campaignStats.alienBasesBuilt);
	cgi->XML_AddIntValue(stats, SAVE_STATS_UFOSSTORED, ccs.campaignStats.ufosStored);
	cgi->XML_AddIntValue(stats, SAVE_STATS_AIRCRAFTHAD, ccs.campaignStats.aircraftHad);

	return true;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] parent XML Node structure, where we get the information from
 */
bool STATS_LoadXML (xmlNode_t* parent)
{
	xmlNode_t* stats;
	bool success = true;

	stats = cgi->XML_GetNode(parent, SAVE_STATS_STATS);
	if (!stats) {
		cgi->Com_Printf("Did not find stats entry in xml!\n");
		return false;
	}
	ccs.campaignStats.missions = cgi->XML_GetInt(stats, SAVE_STATS_MISSIONS, 0);
	ccs.campaignStats.missionsWon = cgi->XML_GetInt(stats, SAVE_STATS_MISSIONSWON, 0);
	ccs.campaignStats.missionsLost = cgi->XML_GetInt(stats, SAVE_STATS_MISSIONSLOST, 0);
	ccs.campaignStats.basesBuilt = cgi->XML_GetInt(stats, SAVE_STATS_BASESBUILT, 0);
	ccs.campaignStats.basesAttacked = cgi->XML_GetInt(stats, SAVE_STATS_BASESATTACKED, 0);
	ccs.campaignStats.installationsBuilt = cgi->XML_GetInt(stats, SAVE_STATS_INSTALLATIONSBUILT, 0);
	ccs.campaignStats.interceptions = cgi->XML_GetInt(stats, SAVE_STATS_INTERCEPTIONS, 0);
	ccs.campaignStats.soldiersLost = cgi->XML_GetInt(stats, SAVE_STATS_SOLDIERSLOST, 0);
	ccs.campaignStats.soldiersNew = cgi->XML_GetInt(stats, SAVE_STATS_SOLDIERSNEW, 0);
	ccs.campaignStats.killedAliens = cgi->XML_GetInt(stats, SAVE_STATS_KILLEDALIENS, 0);
	ccs.campaignStats.capturedAliens = cgi->XML_GetInt(stats, SAVE_STATS_CAPTUREDALIENS, 0);
	ccs.campaignStats.rescuedCivilians = cgi->XML_GetInt(stats, SAVE_STATS_RESCUEDCIVILIANS, 0);
	ccs.campaignStats.researchedTechnologies = cgi->XML_GetInt(stats, SAVE_STATS_RESEARCHEDTECHNOLOGIES, 0);
	ccs.campaignStats.moneyInterceptions = cgi->XML_GetInt(stats, SAVE_STATS_MONEYINTERCEPTIONS, 0);
	ccs.campaignStats.moneyBases = cgi->XML_GetInt(stats, SAVE_STATS_MONEYBASES, 0);
	ccs.campaignStats.moneyResearch = cgi->XML_GetInt(stats, SAVE_STATS_MONEYRESEARCH, 0);
	ccs.campaignStats.moneyWeapons = cgi->XML_GetInt(stats, SAVE_STATS_MONEYWEAPONS, 0);
	ccs.campaignStats.ufosDetected = cgi->XML_GetInt(stats, SAVE_STATS_UFOSDETECTED, 0);
	ccs.campaignStats.alienBasesBuilt = cgi->XML_GetInt(stats, SAVE_STATS_ALIENBASESBUILT, 0);
	ccs.campaignStats.ufosStored = cgi->XML_GetInt(stats, SAVE_STATS_UFOSSTORED, 0);
	ccs.campaignStats.aircraftHad = cgi->XML_GetInt(stats, SAVE_STATS_AIRCRAFTHAD, 0);

	/* freeing the memory below this node */
	mxmlDelete(stats);
	return success;
}

static const cmdList_t statisticsCallbacks[] = {
	{"stats_update", STATS_Update_f, "Update capaign statistics UI"},
	{"stats_getexpenses", STAT_GetExpenses_f, "Gather expenses for the statistics UI"},
	{nullptr, nullptr, nullptr}
};


void STATS_InitStartup (void)
{
	cgi->Cmd_TableAddList(statisticsCallbacks);
}

void STATS_ShutDown (void)
{
	cgi->Cmd_TableRemoveList(statisticsCallbacks);
}
