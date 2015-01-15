/**
 * @file
 * @brief Menu related functions for research.
 *
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
#include "cp_campaign.h"
#include "cp_research_callbacks.h"

/**
 * @brief Assign as many scientists to the research project as possible.
 */
static void RS_Max_f (void)
{
	/* The base the tech is researched in. */
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <tech_id>\n", cgi->Cmd_Argv(0));
		return;
	}
	/* The technology you want to max out. */
	technology_t* tech = RS_GetTechByID(cgi->Cmd_Argv(1));
	if (!tech) {
		Com_Printf("RS_Max_f: Invalid tech '%s'\n", cgi->Cmd_Argv(1));
		return;
	}
	if (tech->base && tech->base != base) {
		Com_Printf("RS_Max_f: Tech '%s' is not researched in this base\n", cgi->Cmd_Argv(1));
		return;
	}

	/* Add as many scientists as possible to this tech. */
	while (CAP_GetFreeCapacity(base, CAP_LABSPACE) > 0) {
		Employee* employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
		if (!employee)
			break;
		RS_AssignScientist(tech, base, employee);
		if (!employee->isAssigned())
			break;
	}

	cgi->UI_ExecuteConfunc("ui_research_update_topic %s %d", tech->id, tech->scientists);
	cgi->UI_ExecuteConfunc("ui_research_update_caps %d %d %d %d", E_CountUnassigned(base, EMPL_SCIENTIST),
		E_CountHired(base, EMPL_SCIENTIST), CAP_GetFreeCapacity(base, CAP_LABSPACE), CAP_GetMax(base, CAP_LABSPACE));
}

/**
 * @brief Script function to add and remove a scientist to the technology entry in the research-list.
 */
static void RS_Change_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <tech_id>\n", cgi->Cmd_Argv(0));
		return;
	}
	technology_t* tech = RS_GetTechByID(cgi->Cmd_Argv(1));
	if (!tech) {
		Com_Printf("RS_ChangeScientist_f: Invalid tech '%s'\n", cgi->Cmd_Argv(1));
		return;
	}
	if (tech->base && tech->base != base) {
		Com_Printf("RS_ChangeScientist_f: Tech '%s' is not researched in this base\n", cgi->Cmd_Argv(1));
		return;
	}
	const int diff = atoi(cgi->Cmd_Argv(2));
	if (diff == 0)
		return;

	if (diff > 0) {
		RS_AssignScientist(tech, base);
	} else if (tech->base) {
		RS_RemoveScientist(tech, nullptr);
	}

	cgi->UI_ExecuteConfunc("ui_research_update_topic %s %d", tech->id, tech->scientists);
	cgi->UI_ExecuteConfunc("ui_research_update_caps %d %d %d %d", E_CountUnassigned(base, EMPL_SCIENTIST),
		E_CountHired(base, EMPL_SCIENTIST), CAP_GetFreeCapacity(base, CAP_LABSPACE), CAP_GetMax(base, CAP_LABSPACE));
}

/**
 * @brief Removes all scientists from the selected research-list entry.
 */
static void RS_Stop_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <tech_id>\n", cgi->Cmd_Argv(0));
		return;
	}
	technology_t* tech = RS_GetTechByID(cgi->Cmd_Argv(1));
	if (!tech) {
		Com_Printf("RS_Stop_f: Invalid tech '%s'\n", cgi->Cmd_Argv(1));
		return;
	}
	if (!tech->base) {
		return;
	}
	if (tech->base != base) {
		Com_Printf("RS_Stop_f: Tech '%s' is not researched in this base\n", cgi->Cmd_Argv(1));
		return;
	}

	RS_StopResearch(tech);

	cgi->UI_ExecuteConfunc("ui_research_update_topic %s %d", tech->id, tech->scientists);
	cgi->UI_ExecuteConfunc("ui_research_update_caps %d %d %d %d", E_CountUnassigned(base, EMPL_SCIENTIST),
		E_CountHired(base, EMPL_SCIENTIST), CAP_GetFreeCapacity(base, CAP_LABSPACE), CAP_GetMax(base, CAP_LABSPACE));
}

/**
 * @brief Shows research image/model and title on the research screen
 */
static void RS_GetDetails_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <tech_id>\n", cgi->Cmd_Argv(0));
		return;
	}
	const technology_t* tech = RS_GetTechByID(cgi->Cmd_Argv(1));
	if (!tech) {
		Com_Printf("RS_GetDetails_f: Invalid tech '%s'\n", cgi->Cmd_Argv(1));
		return;
	}
	cgi->UI_ExecuteConfunc("ui_research_details \"%s\" \"%s\" \"%s\"", _(tech->name), tech->image ? tech->image : "", tech->mdl ? tech->mdl : "");
}

/**
 * @brief Fills technology list on research UI
 */
static void RS_FillTechnologyList_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	RS_MarkResearchable(base);

	cgi->UI_ExecuteConfunc("ui_research_update_caps %d %d %d %d", E_CountUnassigned(base, EMPL_SCIENTIST),
		E_CountHired(base, EMPL_SCIENTIST), CAP_GetFreeCapacity(base, CAP_LABSPACE), CAP_GetMax(base, CAP_LABSPACE));
	cgi->UI_ExecuteConfunc("ui_techlist_clear");
	for (int i = 0; i < ccs.numTechnologies; i++) {
		technology_t* tech = RS_GetTechByIDX(i);
		/* Don't show technologies with time == 0 - those are NOT separate research topics. */
		if (tech->time == 0)
			continue;
		/* hide finished research */
		if (tech->statusResearch == RS_FINISH)
			continue;

		int percentage = 0;
		if (tech->overallTime > 0.0) {
			percentage = std::min(100, std::max(0, 100 - int(round(tech->time * 100.0 / tech->overallTime))));
		}

		/* show researches that are running */
		if (tech->base && tech->scientists > 0) {
			if (tech->base == base) {
				cgi->UI_ExecuteConfunc("ui_techlist_add %s \"%s\" %d %d", tech->id, _(tech->name), tech->scientists, percentage);
			} else {
				cgi->UI_ExecuteConfunc("ui_techlist_add %s \"%s\" %d %d base %d \"%s\"", tech->id, _(tech->name), tech->scientists,
					percentage, tech->base->idx, tech->base->name);
			}
			continue;
		}
		/* show topics that are researchable on this base */
		const bool req = RS_RequirementsMet(tech, base);
		if (tech->statusResearchable && req) {
			cgi->UI_ExecuteConfunc("ui_techlist_add %s \"%s\" %d %d", tech->id, _(tech->name), tech->scientists, percentage);
			continue;
		}
		if (tech->statusCollected && !req) {
			cgi->UI_ExecuteConfunc("ui_techlist_add %s \"%s\" %d %d missing", tech->id, _(tech->name), tech->scientists, percentage);
			continue;
		}
	}
}

static const cmdList_t research_commands[] = {
	{"ui_research_fill", RS_FillTechnologyList_f, "Fill research screen with list of researchable technologies"},
	{"ui_research_getdetails", RS_GetDetails_f, "Show technology image/model in reseach screen"},
	{"ui_research_stop", RS_Stop_f, "Stops the research"},
	{"ui_research_change", RS_Change_f, "Change number of scientists working on the research"},
	{"ui_research_max", RS_Max_f, "Allocates as many scientists on the research as possible"},
	{nullptr, nullptr, nullptr}
};

void RS_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(research_commands);
}

void RS_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(research_commands);
}
