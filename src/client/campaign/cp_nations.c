/**
 * @file cp_nations.c
 * @brief Campaign nations code
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
#include "cp_campaign.h"
#include "cp_nations.h"
#include "cp_time.h"

/**
 * @brief Update the nation data from all parsed nation each month
 * @note give us nation support by:
 * * credits
 * * new soldiers
 * * new scientists
 * @note Called from CL_CampaignRun
 * @sa CL_CampaignRun
 * @sa B_CreateEmployee
 */
void CP_NationHandleBudget (void)
{
	int i, j;
	char message[1024];
	int cost;
	int totalIncome = 0;
	int totalExpenditure = 0;
	int initialCredits = ccs.credits;
	employee_t *employee;

	/* Refreshes the pilot global list.  Pilots who are already hired are unchanged, but all other
	 * pilots are replaced.  The new pilots is evenly distributed between the nations that are happy (happiness > 0). */
	E_RefreshUnhiredEmployeeGlobalList(EMPL_PILOT, qtrue);

	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = &ccs.nations[i];
		const int funding = NAT_GetFunding(nation, 0);
		int newScientists = 0, newSoldiers = 0, newWorkers = 0;

		totalIncome += funding;

		for (j = 0; 0.25 + j < (float) nation->maxScientists * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
			/* Create a scientist, but don't auto-hire her. */
			E_CreateEmployee(EMPL_SCIENTIST, nation, NULL);
			newScientists++;
		}


		if (nation->stats[0].happiness > 0) {
			for (j = 0; 0.25 + j < (float) nation->maxSoldiers * nation->stats[0].happiness * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
				/* Create a soldier. */
				E_CreateEmployee(EMPL_SOLDIER, nation, NULL);
				newSoldiers++;
			}
		}

		for (j = 0; 0.25 + j * 2 < (float) nation->maxSoldiers * nation->stats[0].happiness; j++) {
			/* Create a worker. */
			E_CreateEmployee(EMPL_WORKER, nation, NULL);
			newWorkers++;
		}

		Com_sprintf(message, sizeof(message), _("Gained %i %s, %i %s, %i %s, and %i %s from nation %s (%s)"),
					funding, ngettext("credit", "credits", funding),
					newScientists, ngettext("scientist", "scientists", newScientists),
					newSoldiers, ngettext("soldier", "soldiers", newSoldiers),
					newWorkers, ngettext("worker", "workers", newWorkers),
					_(nation->name), NAT_GetHappinessString(nation));
		MS_AddNewMessageSound(_("Notice"), message, qfalse, MSG_STANDARD, NULL, qfalse);
	}

	for (i = 0; i < MAX_EMPL; i++) {
		employee = NULL;
		cost = 0;
		while ((employee = E_GetNextHired(i, employee))) {
			cost += CP_GetSalaryBaseEmployee(employee->type)
					+ employee->chr.score.rank * CP_GetSalaryRankBonusEmployee(employee->type);
		}
		totalExpenditure += cost;
		Com_sprintf(message, sizeof(message), _("Paid %i credits to: %s"), cost, E_GetEmployeeString(i));
		MS_AddNewMessageSound(_("Notice"), message, qfalse, MSG_STANDARD, NULL, qfalse);
	}

	cost = 0;
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		aircraft_t *aircraft;

		if (!base)
			continue;

		aircraft = NULL;
		while ((aircraft = AIR_GetNextFromBase(base, aircraft)) != NULL)
			cost += aircraft->price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
	}
	totalExpenditure += cost;

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for aircraft"), cost);
		MS_AddNewMessageSound(_("Notice"), message, qfalse, MSG_STANDARD, NULL, qfalse);
	}

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		cost = CP_GetSalaryUpKeepBase(base);
		totalExpenditure += cost;

		Com_sprintf(message, sizeof(message), _("Paid %i credits for upkeep of %s"), cost, base->name);
		MS_AddNewMessageSound(_("Notice"), message, qfalse, MSG_STANDARD, NULL, qfalse);
	}

	cost = CP_GetSalaryAdministrative();
	Com_sprintf(message, sizeof(message), _("Paid %i credits for administrative overhead."), cost);
	totalExpenditure += cost;
	MS_AddNewMessageSound(_("Notice"), message, qfalse, MSG_STANDARD, NULL, qfalse);

	if (initialCredits < 0) {
		float interest = initialCredits * SALARY_DEBT_INTEREST;

		cost = (int)ceil(interest);
		Com_sprintf(message, sizeof(message), _("Paid %i credits in interest on your debt."), cost);
		totalExpenditure += cost;
		MS_AddNewMessageSound(_("Notice"), message, qfalse, MSG_STANDARD, NULL, qfalse);
	}
	CL_UpdateCredits(ccs.credits - totalExpenditure + totalIncome);
	CL_GameTimeStop();
}

/**
 * @brief Backs up each nation's relationship values.
 * @note Right after the copy the stats for the current month are the same as the ones from the (end of the) previous month.
 * They will change while the curent month is running of course :)
 * @todo other stuff to back up?
 */
void CP_NationBackupMonthlyData (void)
{
	int i, nat;

	/**
	 * Back up nation relationship .
	 * "inuse" is copied as well so we do not need to set it anywhere.
	 */
	for (nat = 0; nat < ccs.numNations; nat++) {
		nation_t *nation = &ccs.nations[nat];

		for (i = MONTHS_PER_YEAR - 1; i > 0; i--) {	/* Reverse copy to not overwrite with wrong data */
			nation->stats[i] = nation->stats[i - 1];
		}
	}
}
