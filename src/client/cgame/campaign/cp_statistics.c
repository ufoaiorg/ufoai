/**
 * @file cp_statistics.c
 * @brief Campaign statistics.
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
#include "../../ui/ui_data.h"
#include "cp_campaign.h"
#include "cp_xvi.h"
#include "save/save_statistics.h"

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 * @todo This is very redundant with NAT_HandleBudget Ivestigate and clean up.
 */
void CP_StatsUpdate_f (void)
{
	char *pos;
	static char statsBuffer[MAX_STATS_BUFFER];
	int hired[MAX_EMPL];
	int i, costs = 0, sum = 0, totalfunds = 0;
	base_t *base;
	const campaign_t *campaign = ccs.curCampaign;
	const salary_t *salary = &campaign->salaries;

	/* delete buffer */
	OBJZERO(statsBuffer);
	OBJZERO(hired);

	pos = statsBuffer;

	/* missions */
	UI_RegisterText(TEXT_STATS_MISSION, pos);
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), ccs.campaignStats.missionsWon, ccs.campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_BASES, pos);
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
			ccs.campaignStats.basesBuilt, B_GetCount(), ccs.campaignStats.basesAttacked),

	/* installations */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_INSTALLATIONS, pos);

	INS_Foreach(inst) {
		Q_strcat(pos, va("%s\n", inst->name), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* nations */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_NATIONS, pos);
	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = NAT_GetNationByIDX(i);
		Q_strcat(pos, va(_("%s\t%s\n"), _(nation->name), NAT_GetHappinessString(nation)), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		totalfunds += NAT_GetFunding(nation, 0);
	}
	Q_strcat(pos, va(_("\nFunding this month:\t%d"), totalfunds), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* costs */
	for (i = 0; i < MAX_EMPL; i++) {
		E_Foreach(i, employee) {
			if (!E_IsHired(employee))
				continue;
			costs += CP_GetSalaryBaseEmployee(salary, i) + employee->chr.score.rank * CP_GetSalaryRankBonusEmployee(salary, i);
			hired[employee->type]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_EMPLOYEES, pos);
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i, hired[i]), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_COSTS, pos);
	Q_strcat(pos, va(_("Employees:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	costs = 0;
	AIR_Foreach(aircraft) {
			if (aircraft->status == AIR_CRASHED)
				continue;
			costs += aircraft->price * salary->aircraftFactor / salary->aircraftDivisor;
	}
	Q_strcat(pos, va(_("Aircraft:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		costs = CP_GetSalaryUpKeepBase(salary, base);
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), base->name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = CP_GetSalaryAdministrative(salary);
	Q_strcat(pos, va(_("Administrative costs:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	if (ccs.credits < 0) {
		const float interest = ccs.credits * campaign->salaries.debtInterest;

		costs = (int)ceil(interest);
		Q_strcat(pos, va(_("Debt:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}
	Q_strcat(pos, va(_("\n\t-------\nSum:\t%i c\n"), sum), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* campaign */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_GENERIC, pos);
	Q_strcat(pos, va(_("Max. allowed debts: %ic\n"), campaign->negativeCreditsUntilLost),
		(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* only show the xvi spread data when it's available */
	if (CP_IsXVIResearched()) {
		Q_strcat(pos, va(_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			campaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate()),
			(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] parent XML Node structure, where we write the information to
 */
qboolean STATS_SaveXML (xmlNode_t *parent)
{
	xmlNode_t * stats;

	stats = XML_AddNode(parent, SAVE_STATS_STATS);

	XML_AddIntValue(stats, SAVE_STATS_MISSIONS, ccs.campaignStats.missions);
	XML_AddIntValue(stats, SAVE_STATS_MISSIONSWON, ccs.campaignStats.missionsWon);
	XML_AddIntValue(stats, SAVE_STATS_MISSIONSLOST, ccs.campaignStats.missionsLost);
	XML_AddIntValue(stats, SAVE_STATS_BASESBUILT, ccs.campaignStats.basesBuilt);
	XML_AddIntValue(stats, SAVE_STATS_BASESATTACKED, ccs.campaignStats.basesAttacked);
	XML_AddIntValue(stats, SAVE_STATS_INSTALLATIONSBUILT, ccs.campaignStats.installationsBuilt);
	XML_AddIntValue(stats, SAVE_STATS_INTERCEPTIONS, ccs.campaignStats.interceptions);
	XML_AddIntValue(stats, SAVE_STATS_SOLDIERSLOST, ccs.campaignStats.soldiersLost);
	XML_AddIntValue(stats, SAVE_STATS_SOLDIERSNEW, ccs.campaignStats.soldiersNew);
	XML_AddIntValue(stats, SAVE_STATS_KILLEDALIENS, ccs.campaignStats.killedAliens);
	XML_AddIntValue(stats, SAVE_STATS_RESCUEDCIVILIANS, ccs.campaignStats.rescuedCivilians);
	XML_AddIntValue(stats, SAVE_STATS_RESEARCHEDTECHNOLOGIES, ccs.campaignStats.researchedTechnologies);
	XML_AddIntValue(stats, SAVE_STATS_MONEYINTERCEPTIONS, ccs.campaignStats.moneyInterceptions);
	XML_AddIntValue(stats, SAVE_STATS_MONEYBASES, ccs.campaignStats.moneyBases);
	XML_AddIntValue(stats, SAVE_STATS_MONEYRESEARCH, ccs.campaignStats.moneyResearch);
	XML_AddIntValue(stats, SAVE_STATS_MONEYWEAPONS, ccs.campaignStats.moneyWeapons);
	XML_AddIntValue(stats, SAVE_STATS_UFOSDETECTED, ccs.campaignStats.ufosDetected);
	XML_AddIntValue(stats, SAVE_STATS_ALIENBASESBUILT, ccs.campaignStats.alienBasesBuilt);
	XML_AddIntValue(stats, SAVE_STATS_UFOSSTORED, ccs.campaignStats.ufosStored);
	XML_AddIntValue(stats, SAVE_STATS_AIRCRAFTHAD, ccs.campaignStats.aircraftHad);

	return qtrue;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] parent XML Node structure, where we get the information from
 */
qboolean STATS_LoadXML (xmlNode_t *parent)
{
	xmlNode_t * stats;
	qboolean success = qtrue;

	stats = XML_GetNode(parent, SAVE_STATS_STATS);
	if (!stats) {
		Com_Printf("Did not find stats entry in xml!\n");
		return qfalse;
	}
	ccs.campaignStats.missions = XML_GetInt(stats, SAVE_STATS_MISSIONS, 0);
	ccs.campaignStats.missionsWon = XML_GetInt(stats, SAVE_STATS_MISSIONSWON, 0);
	ccs.campaignStats.missionsLost = XML_GetInt(stats, SAVE_STATS_MISSIONSLOST, 0);
	ccs.campaignStats.basesBuilt = XML_GetInt(stats, SAVE_STATS_BASESBUILT, 0);
	ccs.campaignStats.basesAttacked = XML_GetInt(stats, SAVE_STATS_BASESATTACKED, 0);
	ccs.campaignStats.installationsBuilt = XML_GetInt(stats, SAVE_STATS_INSTALLATIONSBUILT, 0);
	ccs.campaignStats.interceptions = XML_GetInt(stats, SAVE_STATS_INTERCEPTIONS, 0);
	ccs.campaignStats.soldiersLost = XML_GetInt(stats, SAVE_STATS_SOLDIERSLOST, 0);
	ccs.campaignStats.soldiersNew = XML_GetInt(stats, SAVE_STATS_SOLDIERSNEW, 0);
	ccs.campaignStats.killedAliens = XML_GetInt(stats, SAVE_STATS_KILLEDALIENS, 0);
	ccs.campaignStats.rescuedCivilians = XML_GetInt(stats, SAVE_STATS_RESCUEDCIVILIANS, 0);
	ccs.campaignStats.researchedTechnologies = XML_GetInt(stats, SAVE_STATS_RESEARCHEDTECHNOLOGIES, 0);
	ccs.campaignStats.moneyInterceptions = XML_GetInt(stats, SAVE_STATS_MONEYINTERCEPTIONS, 0);
	ccs.campaignStats.moneyBases = XML_GetInt(stats, SAVE_STATS_MONEYBASES, 0);
	ccs.campaignStats.moneyResearch = XML_GetInt(stats, SAVE_STATS_MONEYRESEARCH, 0);
	ccs.campaignStats.moneyWeapons = XML_GetInt(stats, SAVE_STATS_MONEYWEAPONS, 0);
	ccs.campaignStats.ufosDetected = XML_GetInt(stats, SAVE_STATS_UFOSDETECTED, 0);
	ccs.campaignStats.alienBasesBuilt = XML_GetInt(stats, SAVE_STATS_ALIENBASESBUILT, 0);
	ccs.campaignStats.ufosStored = XML_GetInt(stats, SAVE_STATS_UFOSSTORED, 0);
	ccs.campaignStats.aircraftHad = XML_GetInt(stats, SAVE_STATS_AIRCRAFTHAD, 0);

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
	campaign_t *campaign = ccs.curCampaign;
	const salary_t *salary;

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
	Com_Printf("...aircraft_factor: %i\n", salary->aircraftFactor);
	Com_Printf("...aircraft_divisor: %i\n", salary->aircraftDivisor);
	Com_Printf("...base_upkeep: %i\n", salary->baseUpkeep);
	Com_Printf("...admin_initial: %i\n", salary->adminInitial);
	Com_Printf("...admin_soldier: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_SOLDIER));
	Com_Printf("...admin_worker: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_WORKER));
	Com_Printf("...admin_scientist: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_SCIENTIST));
	Com_Printf("...admin_pilot: %i\n", CP_GetSalaryAdminEmployee(salary, EMPL_PILOT));
	Com_Printf("...debt_interest: %.5f\n", salary->debtInterest);
}
#endif /* DEBUG */

void STATS_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listcampaign", CP_CampaignStats_f, "Print campaign stats to game console");
#endif
}
