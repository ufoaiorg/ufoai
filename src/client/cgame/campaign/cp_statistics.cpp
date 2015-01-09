/**
 * @file
 * @brief Campaign statistics.
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
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_xvi.h"
#include "save/save_statistics.h"

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 * @todo This is very redundant with NAT_HandleBudget Investigate and clean up.
 */
void CP_StatsUpdate_f (void)
{
	static char statsBuffer[MAX_STATS_BUFFER];
	const campaign_t* campaign = ccs.curCampaign;

	/* delete buffer */
	OBJZERO(statsBuffer);

	char* pos = statsBuffer;

	/* missions */
	cgi->UI_RegisterText(TEXT_STATS_MISSION, pos);
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), ccs.campaignStats.missionsWon, ccs.campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	cgi->UI_RegisterText(TEXT_STATS_BASES, pos);
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
			ccs.campaignStats.basesBuilt, B_GetCount(), ccs.campaignStats.basesAttacked),

	/* installations */
	pos += (strlen(pos) + 1);
	cgi->UI_RegisterText(TEXT_STATS_INSTALLATIONS, pos);

	INS_Foreach(inst) {
		Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), "%s\n", inst->name);
	}

	/* nations */
	pos += (strlen(pos) + 1);
	int totalfunds = 0;
	cgi->UI_RegisterText(TEXT_STATS_NATIONS, pos);
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("%s\t%s\n"), _(nation->name), NAT_GetHappinessString(nation));
		totalfunds += NAT_GetFunding(nation, 0);
	}
	Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("\nFunding this month:\t%d"), totalfunds);

	/* costs */
	int costs = 0;
	const salary_t* salary = &campaign->salaries;
	int hired[MAX_EMPL];
	OBJZERO(hired);
	for (int i = 0; i < MAX_EMPL; i++) {
		E_Foreach(i, employee) {
			const employeeType_t type = (employeeType_t)i;
			if (!employee->isHired())
				continue;
			const rank_t* rank = CL_GetRankByIdx(employee->chr.score.rank);
			costs += CP_GetSalaryBaseEmployee(salary, type) + rank->level * CP_GetSalaryRankBonusEmployee(salary, type);
			hired[employee->getType()]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	cgi->UI_RegisterText(TEXT_STATS_EMPLOYEES, pos);
	for (int i = 0; i < MAX_EMPL; i++) {
		const employeeType_t type = (employeeType_t)i;
		Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("%s\t%i\n"), E_GetEmployeeString(type, hired[i]), hired[i]);
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	cgi->UI_RegisterText(TEXT_STATS_COSTS, pos);
	Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Employees:\t%i c\n"), costs);
	int sum = costs;

	costs = 0;
	AIR_Foreach(aircraft) {
		if (aircraft->status == AIR_CRASHED)
			continue;
		costs += aircraft->price * salary->aircraftFactor / salary->aircraftDivisor;
	}
	Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Aircraft:\t%i c\n"), costs);
	sum += costs;

	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		costs = CP_GetSalaryUpKeepBase(salary, base);
		Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Base (%s):\t%i c\n"), base->name, costs);
		sum += costs;
	}

	costs = CP_GetSalaryAdministrative(salary);
	Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Administrative costs:\t%i c\n"), costs);
	sum += costs;

	if (ccs.credits < 0) {
		const float interest = ccs.credits * campaign->salaries.debtInterest;

		costs = (int)ceil(interest);
		Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Debt:\t%i c\n"), costs);
		sum += costs;
	}
	Q_strcat(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("\n\t-------\nSum:\t%i c\n"), sum);

	/* campaign */
	pos += (strlen(pos) + 1);
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
		Com_Printf("Did not find stats entry in xml!\n");
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


#ifdef DEBUG
/**
 * @brief Show campaign stats in console
 */
static void CP_CampaignStats_f (void)
{
	campaign_t* campaign = ccs.curCampaign;
	const salary_t* salary;

	if (!CP_IsRunning()) {
		Com_Printf("No campaign active\n");
		return;
	}

	salary = &campaign->salaries;

	Com_Printf("Campaign id: %s\n", campaign->id);
	Com_Printf("..research list: %s\n", campaign->researched);
	Com_Printf("..equipment: %s\n", campaign->equipment);
	Com_Printf("..team: %i\n", campaign->team);

	Com_Printf("..salaries:\n");
	Com_Printf("...soldier_base: %i\n", CP_GetSalaryBaseEmployee(salary, EMPL_SOLDIER));
	Com_Printf("...soldier_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(salary, EMPL_SOLDIER));
	Com_Printf("...worker_base: %i\n", CP_GetSalaryBaseEmployee(salary, EMPL_WORKER));
	Com_Printf("...worker_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(salary, EMPL_WORKER));
	Com_Printf("...scientist_base: %i\n", CP_GetSalaryBaseEmployee(salary, EMPL_SCIENTIST));
	Com_Printf("...scientist_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(salary, EMPL_SCIENTIST));
	Com_Printf("...pilot_base: %i\n", CP_GetSalaryBaseEmployee(salary, EMPL_PILOT));
	Com_Printf("...pilot_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(salary, EMPL_PILOT));
	Com_Printf("...robot_base: %i\n", CP_GetSalaryBaseEmployee(salary, EMPL_ROBOT));
	Com_Printf("...robot_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(salary, EMPL_ROBOT));
	Com_Printf("...aircraft_factor: %i\n", salary->aircraftFactor);
	Com_Printf("...aircraft_divisor: %i\n", salary->aircraftDivisor);
	Com_Printf("...base_upkeep: %i\n", salary->baseUpkeep);
	Com_Printf("...admin_initial: %i\n", salary->adminInitial);
	Com_Printf("...admin_soldier: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_SOLDIER));
	Com_Printf("...admin_worker: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_WORKER));
	Com_Printf("...admin_scientist: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_SCIENTIST));
	Com_Printf("...admin_pilot: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_PILOT));
	Com_Printf("...admin_robot: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_ROBOT));
	Com_Printf("...debt_interest: %.5f\n", salary->debtInterest);
}
#endif /* DEBUG */

void STATS_InitStartup (void)
{
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_listcampaign", CP_CampaignStats_f, "Print campaign stats to game console");
#endif
}
