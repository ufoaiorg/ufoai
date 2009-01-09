/**
 * @file cl_basesummary.c
 * @brief Deals with the Base Summary report.
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

#include "../client.h"
#include "../cl_global.h"
#include "../cl_game.h"
#include "cl_basesummary.h"

/**
 * @brief Open menu for base.
 */
static void BaseSummary_SelectBase_f (void)
{
	int i;

	/* Can be called from everywhere. */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <baseid>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push basesummary\n", i));
}

/**
 * @brief Base Summary menu init function.
 * @note Command to call this: basesummary_init
 * @note Should be called whenever the Base Summary menu gets active.
 */
static void BaseSummary_Init_f (void)
{
	static char textStatsBuffer[1024];
	static char textInfoBuffer[256];
	int i;
	base_t *base = baseCurrent;

	if (!base) {
		Com_Printf("No base selected\n");
		return;
	} else {
		baseCapacities_t cap;
		building_t* b;
		production_queue_t *queue;
		technology_t *tech;
		int totalEmployees = 0;
		int tmp;

		/* wipe away old buffers */
		textStatsBuffer[0] = textInfoBuffer[0] = 0;

		Q_strcat(textInfoBuffer, _("^BAircraft\n"), sizeof(textInfoBuffer));
		for (i = 0; i <= MAX_HUMAN_AIRCRAFT_TYPE; i++)
			Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i\n", AIR_GetAircraftString(i),
				AIR_CountTypeInBase(base, i)), sizeof(textInfoBuffer));

		Q_strcat(textInfoBuffer, "\n", sizeof(textInfoBuffer));

		Q_strcat(textInfoBuffer, _("^BEmployees\n"), sizeof(textInfoBuffer));
		for (i = 0; i < MAX_EMPL; i++) {
			tmp = E_CountHired(base, i);
			totalEmployees += tmp;
			Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i\n", E_GetEmployeeString(i), tmp), sizeof(textInfoBuffer));
		}

		/* link into the menu */
		mn.menuText[TEXT_STANDARD] = textInfoBuffer;

		Q_strcat(textStatsBuffer, _("^BBuildings\t\t\t\t\t\tCapacity\t\t\t\tAmount\n"), sizeof(textStatsBuffer));
		for (i = 0; i < gd.numBuildingTemplates; i++) {
			b = &gd.buildingTemplates[i];

			/* only show already researched buildings */
			if (!RS_IsResearched_ptr(b->tech))
				continue;

			cap = B_GetCapacityFromBuildingType(b->buildingType);
			if (cap == MAX_CAP)
				continue;

			/* Check if building is functional (see comments in B_UpdateBaseCapacities) */
			if (B_GetBuildingStatus(base, b->buildingType)) {
				Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i/%i", _(b->name),
					base->capacities[cap].cur, base->capacities[cap].max), sizeof(textStatsBuffer));
			} else {
				if (b->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
					const int daysLeft = b->timeStart + b->buildTime - ccs.date.day;
					/* if there is no day left the status should not be B_STATUS_UNDER_CONSTRUCTION */
					assert(daysLeft);
					Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i %s", _(b->name), daysLeft, ngettext("day", "days", daysLeft)), sizeof(textStatsBuffer));
				} else {
					Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i/%i", _(b->name), base->capacities[cap].cur, 0), sizeof(textStatsBuffer));
				}
			}
			Q_strcat(textStatsBuffer, va("\t\t\t\t%i\n", B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType)), sizeof(textStatsBuffer));
		}

		Q_strcat(textStatsBuffer, "\n", sizeof(textStatsBuffer));

		Q_strcat(textStatsBuffer, _("^BProduction\t\t\t\t\t\tQuantity\t\t\t\tPercent\n"), sizeof(textStatsBuffer));
		queue = &gd.productions[base->idx];
		if (queue->numItems > 0) {
			for (i = 0; i < queue->numItems; i++) {
				const production_t *production = &queue->items[i];
				const objDef_t *objDef = production->item;
				const aircraft_t *aircraft = production->aircraft;
				const char *name = (objDef) ? objDef->name : _(aircraft->name);
				assert(name);

				/** @todo use the same method as we do in PR_ProductionInfo */
				Q_strcat(textStatsBuffer, va(_("%s\t\t\t\t\t\t%d\t\t\t\t%.2f%%\n"), name,
					production->amount, production->percentDone * 100), sizeof(textStatsBuffer));
			}
		} else {
			Q_strcat(textStatsBuffer, _("Nothing\n"), sizeof(textStatsBuffer));
		}

		Q_strcat(textStatsBuffer, "\n", sizeof(textStatsBuffer));

		Q_strcat(textStatsBuffer, _("^BResearch\t\t\t\t\t\tScientists\t\t\t\tPercent\n"), sizeof(textStatsBuffer));
		tmp = 0;
		for (i = 0; i < gd.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
			if (tech->base == base && (tech->statusResearch == RS_RUNNING || tech->statusResearch == RS_PAUSED)) {
				Q_strcat(textStatsBuffer, va(_("%s\t\t\t\t\t\t%d\t\t\t\t%1.2f%%\n"), _(tech->name),
					tech->scientists, (1 - tech->time / tech->overalltime) * 100), sizeof(textStatsBuffer));
				tmp++;
			}
		}
		if (!tmp)
			Q_strcat(textStatsBuffer, _("Nothing\n"), sizeof(textStatsBuffer));

		/* link into the menu */
		mn.menuText[TEXT_STATS_BASESUMMARY] = textStatsBuffer;
	}
}

/**
 * @brief Defines commands and cvars for the base statistics menu(s).
 * @sa MN_InitStartup
 */
void BSUM_InitStartup (void)
{
	/* add commands */
	Cmd_AddCommand("basesummary_init", BaseSummary_Init_f, "Init function for Base Statistics menu");
	Cmd_AddCommand("basesummary_selectbase", BaseSummary_SelectBase_f, "Opens Base Statistics menu in base");
}
