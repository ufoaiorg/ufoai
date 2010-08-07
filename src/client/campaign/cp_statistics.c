/**
 * @file cp_statistics.c
 * @brief Campaign statistics.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../cl_shared.h"
#include "../ui/ui_data.h"
#include "cp_campaign.h"
#include "cp_xvi.h"
#include "save/save_campaign.h"

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 */
void CL_StatsUpdate_f (void)
{
	char *pos;
	static char statsBuffer[MAX_STATS_BUFFER];
	int hired[MAX_EMPL];
	int i, costs = 0, sum = 0, totalfunds = 0;
	employee_t *employee;

	/* delete buffer */
	memset(statsBuffer, 0, sizeof(statsBuffer));
	memset(hired, 0, sizeof(hired));

	pos = statsBuffer;

	/* missions */
	UI_RegisterText(TEXT_STATS_MISSION, pos);
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), ccs.campaignStats.missionsWon, ccs.campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_BASES, pos);
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
			ccs.campaignStats.basesBuilt, ccs.numBases, ccs.campaignStats.basesAttacked),

	/* installations */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_INSTALLATIONS, pos);
	for (i = 0; i < ccs.numInstallations; i++) {
		const installation_t *inst = &ccs.installations[i];
		Q_strcat(pos, va("%s\n", inst->name), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* nations */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_NATIONS, pos);
	for (i = 0; i < ccs.numNations; i++) {
		const nation_t *nation = &ccs.nations[i];
		Q_strcat(pos, va(_("%s\t%s\n"), _(nation->name), NAT_GetHappinessString(nation)), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		totalfunds += NAT_GetFunding(nation, 0);
	}
	Q_strcat(pos, va(_("\nFunding this month:\t%d"), totalfunds), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* costs */
	for (i = 0; i < MAX_EMPL; i++) {
		employee = NULL;
		while ((employee = E_GetNextHired(i, employee))) {
			costs += CP_GetSalaryBaseEmployee(i) + employee->chr.score.rank * CP_GetSalaryRankBonusEmployee(i);
			hired[employee->type]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_EMPLOYEES, pos);
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_STATS_COSTS, pos);
	Q_strcat(pos, va(_("Employees:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	costs = 0;
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		aircraft_t *aircraft;

		aircraft = NULL;
		while ((aircraft = AIR_GetNextFromBase(base, aircraft)) != NULL)
			costs += aircraft->price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
	}
	Q_strcat(pos, va(_("Aircraft:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		costs = CP_GetSalaryUpKeepBase(base);
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), base->name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = CP_GetSalaryAdministrative();
	Q_strcat(pos, va(_("Administrative costs:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	if (ccs.credits < 0) {
		const float interest = ccs.credits * SALARY_DEBT_INTEREST;

		costs = (int)ceil(interest);
		Q_strcat(pos, va(_("Debt:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}
	Q_strcat(pos, va(_("\n\t-------\nSum:\t%i c\n"), sum), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* campaign */
	pos += (strlen(pos) + 1);
	UI_RegisterText(TEXT_GENERIC, pos);
	Q_strcat(pos, va(_("Max. allowed debts: %ic\n"), ccs.curCampaign->negativeCreditsUntilLost),
		(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* only show the xvi spread data when it's available */
	if (CP_IsXVIResearched()) {
		Q_strcat(pos, va(_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			ccs.curCampaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate()),
			(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}
}

/**
 * @brief Load mapDef statistics
 * @param[in] parent XML Node structure, where we get the information from
 */
qboolean CP_LoadMapDefStatXML (mxml_node_t *parent)
{
	mxml_node_t *node;

	for (node = mxml_GetNode(parent, SAVE_CAMPAIGN_MAPDEF); node; node = mxml_GetNextNode(node, parent, SAVE_CAMPAIGN_MAPDEF)) {
		const char *s = mxml_GetString(node, SAVE_CAMPAIGN_MAPDEF_ID);
		mapDef_t *map;

		if (s[0] == '\0') {
			Com_Printf("Warning: MapDef with no id in xml!\n");
			continue;
		}
		map = Com_GetMapDefinitionByID(s);
		if (!map) {
			Com_Printf("Warning: No MapDef with id '%s'!\n", s);
			continue;
		}
		map->timesAlreadyUsed = mxml_GetInt(node, SAVE_CAMPAIGN_MAPDEF_COUNT, 0);
	}

	return qtrue;
}

#ifdef DEBUG
/**
 * @brief Show campaign stats in console
 */
static void CP_CampaignStats_f (void)
{
	campaign_t *campaign = ccs.curCampaign;

	if (!CP_IsRunning()) {
		Com_Printf("No campaign active\n");
		return;
	}

	Com_Printf("Campaign id: %s\n", campaign->id);
	Com_Printf("..research list: %s\n", campaign->researched);
	Com_Printf("..equipment: %s\n", campaign->equipment);
	Com_Printf("..team: %i\n", campaign->team);

	Com_Printf("..salaries:\n");
	Com_Printf("...soldier_base: %i\n", CP_GetSalaryBaseEmployee(EMPL_SOLDIER));
	Com_Printf("...soldier_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(EMPL_SOLDIER));
	Com_Printf("...worker_base: %i\n", CP_GetSalaryBaseEmployee(EMPL_WORKER));
	Com_Printf("...worker_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(EMPL_WORKER));
	Com_Printf("...scientist_base: %i\n", CP_GetSalaryBaseEmployee(EMPL_SCIENTIST));
	Com_Printf("...scientist_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(EMPL_SCIENTIST));
	Com_Printf("...pilot_base: %i\n", CP_GetSalaryBaseEmployee(EMPL_PILOT));
	Com_Printf("...pilot_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(EMPL_PILOT));
	Com_Printf("...robot_base: %i\n", CP_GetSalaryBaseEmployee(EMPL_ROBOT));
	Com_Printf("...robot_rankbonus: %i\n", CP_GetSalaryRankBonusEmployee(EMPL_ROBOT));
	Com_Printf("...aircraft_factor: %i\n", SALARY_AIRCRAFT_FACTOR);
	Com_Printf("...aircraft_divisor: %i\n", SALARY_AIRCRAFT_DIVISOR);
	Com_Printf("...base_upkeep: %i\n", SALARY_BASE_UPKEEP);
	Com_Printf("...admin_initial: %i\n", SALARY_ADMIN_INITIAL);
	Com_Printf("...admin_soldier: %i\n", CP_GetSalaryAdminEmployee(EMPL_SOLDIER));
	Com_Printf("...admin_worker: %i\n", CP_GetSalaryAdminEmployee(EMPL_WORKER));
	Com_Printf("...admin_scientist: %i\n", CP_GetSalaryAdminEmployee(EMPL_SCIENTIST));
	Com_Printf("...admin_pilot: %i\n", CP_GetSalaryAdminEmployee(EMPL_PILOT));
	Com_Printf("...admin_robot: %i\n", CP_GetSalaryAdminEmployee(EMPL_ROBOT));
	Com_Printf("...debt_interest: %.5f\n", SALARY_DEBT_INTEREST);
}
#endif /* DEBUG */

void STATS_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listcampaign", CP_CampaignStats_f, "Print campaign stats to game console");
#endif
}
