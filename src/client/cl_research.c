/**
 * @file cl_research.c
 * @brief Player research.
 *
 * Handles everything related to the research-tree.
 * Provides information if items/buildings/etc.. can be researched/used/displayed etc...
 * Implements the research-system (research new items/etc...)
 * See base/ufos/research.ufo and base/ufos/menu_research.ufo for the underlying content.
 * TODO: comment on used global variables.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"

qboolean RS_TechIsResearchable(technology_t *t);

/* A (local) list of displayed technology-entries (the research list in the base) */
static technology_t *researchList[MAX_RESEARCHLIST];

/* The number of entries in the above list. */
static int researchListLength;

/* The currently selected entry in the above list. */
static int researchListPos;

static stringlist_t curRequiredList;

/**
 * @brief Marks one tech as researchable.
 * @param id unique id of a technology_t
 */
void RS_MarkOneResearchable(int tech_idx)
{
	Com_DPrintf("RS_MarkOneResearchable: \"%s\" marked as researchable.\n", gd.technologies[tech_idx].id);
	gd.technologies[tech_idx].statusResearchable = qtrue;
}

#if DEPENDENCIES_OVERHAUL
/**
 * @brief Checks if all requirements of a tech have been met.
 * @param[in] require_AND pointer to a list of AND-related requirements.
 * @param[in] require_OR pointer to a list of OR-related requirements.
 * @return qtrue if all requirements are satisfied otherwise qfalse.
 */
static qboolean RS_RequirementsMet(requirements_t *required_AND, requirements_t *required_OR)
{
	int i;
	qboolean met_AND = qtrue;
	qboolean met_OR = qfalse;

	if (!required_AND && !required_AND) {
		Com_Printf("RS_RequirementsMet: No requirement list(s) given as parameter.\n");
		return qfalse;
	}
	
	if (required_AND) {
		for (i = 0; i < required_AND->numLinks; i++) {
			switch (required_AND->type[i]) {
			case RS_LINK_TECH:
				/* Com_DPrintf("RS_RequirementsMet: ANDtech: %s / %i\n", required_AND->id[i], required_AND->idx[i]); */
				if ((!RS_TechIsResearched(required_AND->idx[i]))
					|| (!Q_strncmp(required_AND->id[i], "nothing", MAX_VAR))) {
					met_AND = qfalse;
				}
				break;
			case RS_LINK_ITEM:
				/* Com_DPrintf("RS_RequirementsMet: ANDitem: %s / %i\n", required_AND->id[i], required_AND->idx[i]); */
				if (required_AND->collected[i] < required_AND->amount[i]) {
					met_AND = qfalse;
				}
				break;
			case RS_LINK_EVENT:
				break;
			default:
				break;
			}

			if (!met_AND)
				break;
		}
	} else {
		met_AND = qfalse;
	}

	if (required_OR)
		for (i = 0; i < required_OR->numLinks; i++) {
			switch (required_OR->type[i]) {
			case RS_LINK_TECH:
				Com_DPrintf("RS_RequirementsMet: ORtech: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				if ((!RS_TechIsResearched(required_OR->idx[i]))
					|| (!Q_strncmp(required_OR->id[i], "nothing", MAX_VAR))) {
					met_OR = qtrue;
				}
				break;
			case RS_LINK_ITEM:
				Com_DPrintf("RS_RequirementsMet: ORitem: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				if (required_OR->collected[i] < required_OR->amount[i]) {
					met_OR = qtrue;
				}
				break;
			case RS_LINK_EVENT:
				break;
			default:
				break;
			}

			if (met_OR)
				break;
		}

	return (met_AND || met_OR);
}

/**
 * @brief Check if the item has been collected (in storage or quarantine) in the giveb base.
 * @param[in] item_idx The index of the item in the inv.
 * @param[in] base The base to searrch in.
 * @return qboolean
 * @todo quarantine
 */
int RS_ItemInBase(int item_idx, base_t *base)
{
	equipDef_t *ed = NULL;
	
	if (!base)
		return -1;
	
	ed = &base->storage;
	
	if (!ed)
		return -1;
	
	Com_DPrintf("RS_ItemInBase: DEBUG idx %s\n",  csi.ods[item_idx].kurz);
	
	return ed->num[item_idx];
}

/**
 * @brief Checks if any items have been collected (in the current base) and correct the value for each requirement.
 */
qboolean RS_CheckCollected(requirements_t *required)
{
	int i;
	int item_amount;
	qboolean all_collected = qtrue;
	
	if (!required)
		return qfalse;
	
	if (!baseCurrent)
		return qfalse;
	
	for (i = 0; i < required->numLinks; i++) {
		if (required->type[i] == RS_LINK_ITEM) {
			item_amount = RS_ItemInBase(required->idx[i], baseCurrent);
			if (item_amount > 0) {
				required->collected[i] = item_amount;
			} else {
				required->collected[i] = 0;
				all_collected = qfalse;
			}
		}
	}
	return all_collected;
}

/**
 * @brief Checks if any items have been collected int he current base and correct the values for each requirement.
 */
void RS_CheckAllCollected(void)
{
	int i;
	technology_t *tech = NULL;
	requirements_t *required = NULL;

	if (!required)
		return;
	
	if (!baseCurrent)
		return;
	
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (RS_CheckCollected(&tech->require_AND)) {
			tech->statusCollected = qtrue;
		}
		
	}
}

/**
 * @brief Marks all the techs that can be researched.
 * Automatically researches 'free' techs such as ammo for a weapon.
 * Should be called when a new item is researched (RS_MarkResearched) and after
 * the tree-initialisation (RS_InitTree)
 */
void RS_MarkResearchable(void)
{
	int i;
	technology_t *tech = NULL;

	/* Set all entries to initial value. */
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		tech->statusResearchable = qfalse;
	}
	RS_CheckAllCollected();
	
	for (i = 0; i < gd.numTechnologies; i++) {	/* i = tech-index */
		tech = &gd.technologies[i];
		if (!tech->statusResearchable) {	/* Redundant, since we set them all to false, but you never know. */
			/* Check for collected items/aliens/etc... */
			
			if (tech->statusResearch != RS_FINISH) {
				/* Com_DPrintf("RS_MarkResearchable: handling \"%s\".\n", tech->id); */
				/* If required techs are all researched and all other requirements are met, mark this as researchable. */
				
				/* All requirements are met. */
				if (RS_RequirementsMet(&tech->require_AND, &tech->require_OR)) {
					Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:requirements.\n", tech->id);
					tech->statusResearchable = qtrue;
				}

				/* If the tech is a 'free' one (such as ammo for a weapon),
				   mark it as researched and loop back to see if it unlocks
				   any other techs */
				if (tech->statusResearchable && tech->time <= 0) {
					tech->statusResearch = RS_FINISH;
					Com_DPrintf("RS_MarkResearchable: automatically researched \"%s\"\n", tech->id);
					/* restart the loop as this may have unlocked new possibilities */
					i = 0;
				}
			}
		}
	}
	Com_DPrintf("RS_MarkResearchable: Done.\n");
}
#else /* overhaul */
/**
 * @brief Marks all the techs that can be researched.
 * Automatically researches 'free' techs such as ammo for a weapon.
 * Should be called when a new item is researched (RS_MarkResearched) and after
 * the tree-initialisation (RS_InitTree)
 */
void RS_MarkResearchable(void)
{
	int i, j;
	technology_t *tech = NULL;
	stringlist_t firstrequired;
	stringlist_t *required = NULL;
	byte required_are_researched;

	/* set all entries to initial value */
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		tech->statusResearchable = qfalse;
	}

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!tech->statusResearchable) {	/* Redundant, since we set them all to false, but you never know. */
			if (tech->statusResearch != RS_FINISH) {
				Com_DPrintf("RS_MarkResearchable: handling \"%s\".\n", tech->id);
				firstrequired.numEntries = 0;
				RS_GetFirstRequired(tech->idx, &firstrequired);

				/* TODO doesn't work yet? */
				/* If the tech has an collected item, mark the first-required techs as researchable */
				if (tech->statusCollected) {
					for (j = 0; j < firstrequired.numEntries; j++) {
						Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:firstrequired of collected.\n", firstrequired.string[j]);
						RS_MarkOneResearchable(firstrequired.idx[j]);
					}
				}

				/* if needed/required techs are all researched, mark this as researchable. */
				required = &tech->requires;
				required_are_researched = qtrue;
				Com_DPrintf("RS_MarkResearchable: %i required entries\n", required->numEntries);
				for (j = 0; j < required->numEntries; j++) {
					Com_DPrintf("RS_MarkResearchable: entry: %s / %i\n", required->string[j], required->idx[j]);
					if ((!RS_TechIsResearched(required->idx[j]))
						|| (!Q_strncmp(required->string[j], "nothing", MAX_VAR))) {
						required_are_researched = qfalse;
						break;
					}
				}

				/* all required items are researched */
				if (required_are_researched && ((tech->needsCollected && tech->statusCollected) || !tech->needsCollected)) {	/* AND ( all needed collected OR no collected needed ) */
					Com_DPrintf("RS_MarkResearchable: %i - %i - %i\n", required_are_researched, tech->needsCollected, tech->statusCollected);
					Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:required.\n", tech->id);
					tech->statusResearchable = qtrue;
				}

#if 0
				/* If the tech is an initial one,  mark it as as researchable. */
				for (j = 0; j < required->numEntries; j++) {
					if (!Q_strncmp(required->string[j], "initial", MAX_VAR)) {
						Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable - reason:isinitial.\n", tech->id);
						tech->statusResearchable = qtrue;
						break;
					}
				}
#endif

#if 1
				/* If the tech is a 'free' one (such as ammo for a weapon),
				   mark it as researched and loop back to see if it unlocks
				   any other techs */
				if (tech->statusResearchable && tech->time <= 0) {
					tech->statusResearch = RS_FINISH;
					Com_DPrintf("RS_MarkResearchable: automatically researched \"%s\"\n", tech->id);
					/* restart the loop as this may have unlocked new possibilities */
					i = 0;
				}
#endif
			}
		}
	}
	Com_DPrintf("RS_MarkResearchable: Done.\n");
}
#endif /* overhaul */

/**
 * @brief Link the tech pointers to object definitions
 */
void RS_AddObjectTechs(void)
{
	objDef_t *od;
	int i;

	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		od->tech = RS_GetTechByProvided(od->kurz);
#ifdef DEBUG
		if (!od->tech)
			Sys_Error("Com_AddObjectTechs: Could not find a valid tech for item %s\n", od->kurz);
#endif /* DEBUG */
	}
}

#if DEPENDENCIES_OVERHAUL
static void RS_InitRequirementList(requirements_t *required)
{
	int i, k;
	technology_t *tech_required = NULL;
	objDef_t *item = NULL;

	for (i = 0; i < required->numLinks; i++) {	/* i = link index */
		switch (required->type[i]) {
		case RS_LINK_TECH:
			/* Get index of technology by its id-name. */
			tech_required = RS_GetTechByID(required->id[i]);
			required->idx[i] = -1;
			if (tech_required) {
				required->idx[i] = tech_required->idx;
			}
			break;
		case RS_LINK_ITEM:
			/* Get index of item by its id-name. */
			required->idx[i] = -1;
			for (k = 0; k < csi.numODs; k++) {	/* k = item index */
				item = &csi.ods[k];
				if (!Q_strncmp(required->id[i], item->kurz, MAX_VAR)) {
					required->idx[i] = k;
					break;
				}
			}
			break;
		case RS_LINK_EVENT:
			/* TODO: get index of event */
			break;
		default:
			break;
		}
		
	}

}


/**
 * @brief Gets all needed names/file-paths/etc... for each technology entry.
 * Should be executed after the parsing of _all_ the ufo files and e.g. the
 * research tree/inventory/etc... are initialised.
 * TODO: add a function to reset ALL research-stati to RS_NONE; -> to be called after start of a new game.
 */
void RS_InitTree(void)
{
	int i, j, k;
	technology_t *tech = NULL;
	objDef_t *item = NULL;
	objDef_t *item_ammo = NULL;
	building_t *building = NULL;
	aircraft_t *air_samp = NULL;
	byte found;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];

		/* Save the idx to the id-names of the different requirement-types for quicker access. The id-strings themself are not really needed afterwards :-/ */
		RS_InitRequirementList(&tech->require_AND);
		RS_InitRequirementList(&tech->require_OR);

		/* Search in correct data/.ufo */
		switch (tech->type) {
		case RS_CRAFTWEAPON:
		case RS_CRAFTSHIELD:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A type craftshield or craftweapon item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_TECH:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A 'type tech' item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_WEAPON:
		case RS_ARMOR:
			found = qfalse;
			for (j = 0; j < csi.numODs; j++) {	/* j = item index */
				item = &csi.ods[j];

				/* This item has been 'provided' -> get the correct data. */
				if (!Q_strncmp(tech->provides, item->kurz, MAX_VAR)) {
					found = qtrue;
					if (!*tech->name)
						Com_sprintf(tech->name, MAX_VAR, item->name);
					if (!*tech->mdl_top)
						Com_sprintf(tech->mdl_top, MAX_VAR, item->model);
					if (!*tech->image_top)
						Com_sprintf(tech->image_top, MAX_VAR, item->image);
					if (!*tech->mdl_bottom) {
						if (tech->type == RS_WEAPON) {
							/* Find ammo for weapon. */
							for (k = 0; k < csi.numODs; k++) {
								item_ammo = &csi.ods[k];
								if (j == item_ammo->link) {
									Com_sprintf(tech->mdl_bottom, MAX_VAR, item_ammo->model);
									break;
								}
							}
						}
					}
					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			/*no id found in csi.ods */
			if (!found) {
				Com_sprintf(tech->name, MAX_VAR, tech->id);
				Com_Printf("RS_InitTree: \"%s\" - Linked weapon or armor (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
			}
			break;
		case RS_BUILDING:
			found = qfalse;
			for (j = 0; j < gd.numBuildingTypes; j++) {
				building = &gd.buildingTypes[j];
				/* This building has been 'provided'  -> get the correct data. */
				if (!Q_strncmp(tech->provides, building->id, MAX_VAR)) {
					found = qtrue;
					if (!*tech->name)
						Com_sprintf(tech->name, MAX_VAR, building->name);
					if (!*tech->image_top)
						Com_sprintf(tech->image_top, MAX_VAR, building->image);

					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			if (!found) {
				Com_sprintf(tech->name, MAX_VAR, tech->id);
				Com_DPrintf("RS_InitTree: \"%s\" - Linked building (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
			}
			break;
		case RS_CRAFT:
			found = qfalse;
			for (j = 0; j < numAircraft_samples; j++) {
				air_samp = &aircraft_samples[j];
				/* This aircraft has been 'provided'  -> get the correct data. */
				if (!Q_strncmp(tech->provides, air_samp->id, MAX_VAR)) {
					found = qtrue;
					if (!*tech->name)
						Com_sprintf(tech->name, MAX_VAR, air_samp->name);
					if (!*tech->mdl_top) {	/* DEBUG testing */
						Com_sprintf(tech->mdl_top, MAX_VAR, air_samp->model);
						Com_DPrintf("RS_InitTree: aircraft model \"%s\" \n", air_samp->model);
					}
					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			if (!found)
				Com_DPrintf("RS_InitTree: \"%s\" - Linked aircraft or craft-upgrade (provided=\"%s\") not found.\n", tech->id, tech->provides);
			break;
		case RS_ALIEN:
			/* does nothing right now */
			break;
		case RS_UGV:
			/* TODO: Implement me */
			break;
		} /* switch */
	}
	/*
	for (i = 0; i < gd.numBases; i++)
		if (gd.bases[i].founded)
			RS_MarkCollected(&gd.bases[i].storage);
	*/
	RS_MarkResearchable();

	memset(&curRequiredList, 0, sizeof(stringlist_t));

	Com_DPrintf("RS_InitTree: Technology tree initialised. %i entries found.\n", i);
}
#else /*overhaul */
#endif /* overhaul */

/**
 * @brief Return "name" if present, otherwise enter the correct .ufo file and get it from the definition there.
 * @param[in] id unique id of a technology_t
 * @param[out] name Full name of this technology_t (technology_t->name) - defaults to id if nothing is found.
 * @note name has a maxlength of MAX_VAR
 */
void RS_GetName(char *id, char *name)
{
	technology_t *tech = NULL;

	tech = RS_GetTechByID(id);
	if (!tech) {
		/* set the name to the id. */
		Com_sprintf(name, MAX_VAR, id);
		return;
	}

	if (*tech->name) {
		Com_sprintf(name, MAX_VAR, _(tech->name));
		return;
	} else {
		/* FIXME: Do we need to translate the id? */
		/* set the name to the id. */
		Com_sprintf(name, MAX_VAR, _(id));
		return;
	}
}

/**
 * @brief Displays the informations of the current selected technology in the description-area.
 * See menu_research.ufo for the layout/called functions.
 */
static void RS_ResearchDisplayInfo(void)
{
#if DEPENDENCIES_OVERHAUL
#else /* overhaul */
	int i;
	static char dependencies[256];
	char tempstring[MAX_VAR];
#endif /* overhaul */
	technology_t *tech = NULL;
	base_t *base = NULL;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	if (researchListLength <= 0 || researchListPos >= researchListLength)
		return;

	tech = researchList[researchListPos];

	/* Display total number of free labs in current base. */
	Cvar_Set("mn_research_scis", va(_("Available scientists in this base: %i"), E_CountUnassigned(baseCurrent, EMPL_SCIENTIST)));
	Cvar_Set("mn_research_selbase", _("Not researched in any base."));

	/* Display the base this tech is researched in. */
	if (tech->scientists >= 0) {
		if (tech->base_idx != baseCurrent->idx) {
			base = &gd.bases[tech->base_idx];
			Cvar_Set("mn_research_selbase", va(_("Researched in %s"), base->name));
		} else
			Cvar_Set("mn_research_selbase", _("Researched in this base."));
	}

	Cvar_Set("mn_research_selname", _(tech->name));
	if (tech->overalltime) {
		if (tech->time > tech->overalltime) {
			Com_Printf("RS_ResearchDisplayInfo: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", tech->id, tech->time,
					tech->overalltime);
			/* just in case the values got messed up */
			tech->time = tech->overalltime;
		}
		Cvar_Set("mn_research_seltime", va(_("Progress: %.1f%%"), 100 - (tech->time * 100 / tech->overalltime)));
	} else
		Cvar_Set("mn_research_seltime", _("Progress: Not available."));

	switch (tech->statusResearch) {
	case RS_RUNNING:
		Cvar_Set("mn_research_selstatus", _("Status: Under research"));
		break;
	case RS_PAUSED:
		Cvar_Set("mn_research_selstatus", _("Status: Research paused"));
		break;
	case RS_FINISH:
		Cvar_Set("mn_research_selstatus", _("Status: Research finished"));
		break;
	case RS_NONE:
		Cvar_Set("mn_research_selstatus", _("Status: Unknown technology"));
		break;
	default:
		break;
	}

#if DEPENDENCIES_OVERHAUL
#else /* overhaul */
	memset(&curRequiredList, 0, sizeof(stringlist_t));
	RS_GetFirstRequired(tech->idx, &curRequiredList);

	/* clear the list */
	*dependencies = '\0';

	for (i = 0; i < curRequiredList.numEntries; i++) {
		/* name_temp gets initialised in getname. */
		RS_GetName(curRequiredList.string[i], tempstring);
		Q_strcat(dependencies, tempstring, sizeof(dependencies));
		Q_strcat(dependencies, "\n", sizeof(dependencies));
	}

	menuText[TEXT_RESEARCH_INFO] = dependencies;
	Cvar_Set("mn_research_seldep", _("Dependencies"));
#endif /* overhaul */
}

/**
 * @brief Changes the active research-list entry to the currently selected.
 * See menu_research.ufo for the layout/called functions.
 */
static void CL_ResearchSelectCmd(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: research_select <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= researchListLength) {
		menuText[TEXT_STANDARD] = NULL;
		return;
	}

	/* call researchselect function from menu_research.ufo */
	researchListPos = num;
	Cbuf_AddText(va("researchselect%i\n", researchListPos));

	/*RS_ResearchDisplayInfo(); */
	RS_UpdateData();
}

/**
 * @brief Assigns scientist to the selected research-project.
 * @note The lab will be automatically selected (the first one that has still free space).
 * @param[in] tech What technology you want to assign the scientist to.
 * @sa RS_AssignScientist_f
 */
void RS_AssignScientist(technology_t* tech)
{
	building_t *building = NULL;
	employee_t *employee = NULL;
	base_t *base = NULL;

	Com_DPrintf("RS_AssignScientist: %i | %s \n", tech->idx, tech->name);

	if  (tech->base_idx >= 0) {
		base = &gd.bases[tech->base_idx];
	} else {
		base = baseCurrent;
	}

	employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);

	if (!employee) {
		/* No scientists are free in this base. */
		return;
	}

	if (tech->statusResearchable) {
		/* Get a free lab from the base. */
		building = B_GetLab(base->idx);
		if (building) {
			/* Assign the tech to a lab&base. */
			tech->scientists++;
			tech->base_idx = building->base_idx;
			employee->buildingID = building->idx;
			/* TODO: use
			E_AssignEmployeeToBuilding(employee, building);
			instead. */
		} else {
			MN_Popup(_("Notice"),
				_("There is no free lab available.\nYou need to build one or free another\nin order to assign scientists to research this technology.\n"));
			return;
		}

		tech->statusResearch = RS_RUNNING;

		/* Update display-list and display-info. */
		RS_ResearchDisplayInfo();
		RS_UpdateData();
	}
}

/**
 * @brief Script function to add a scientist to  the technology entry in the research-list.
 * @sa RS_AssignScientist
 * @sa RS_RemoveScientist_f
 */
static void RS_AssignScientist_f(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_add <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num > researchListLength)
		return;

	Com_DPrintf("RS_AssignScientist_f: num %i\n", num);
	RS_AssignScientist(researchList[num]);
}


/**
 * @brief Remove a scientist from a technology.
 * @param[in] tech The technology you want to remove the scientist from.
 * @sa RS_RemoveScientist_f
 * @sa RS_AssignScientist
 */
static void RS_RemoveScientist(technology_t* tech)
{
	employee_t *employee = NULL;

	assert(tech);

	if (tech->scientists > 0) {
		employee = E_GetAssignedEmployee(&gd.bases[tech->base_idx], EMPL_SCIENTIST);
		if (employee) {
			employee->buildingID = -1; /* See also E_RemoveEmployeeFromBuilding */
			tech->scientists--;
		}
	}

	if (tech->scientists == 0) {
		tech->base_idx = -1;
	}
}

/**
 * @brief Script function to remove a scientist from the technology entry in the research-list.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist
 */
static void RS_RemoveScientist_f(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_remove <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num > researchListLength)
		return;

	RS_RemoveScientist(researchList[num]);

	/* Update display-list and display-info. */
	RS_ResearchDisplayInfo();
	RS_UpdateData();
}

/**
 * @brief Starts the research of the selected research-list entry.
 * TODO: Check if laboratory is available.
 */
static void RS_ResearchStart(void)
{
	technology_t *tech = NULL;

	/* We are not in base view. */
	if (!baseCurrent)
		return;

#if 0
	/* Check if laboratory is available. */
	if (!baseCurrent->hasLab)
		return;
#endif

	/* get the currently selected research-item */
	tech = researchList[researchListPos];

	/************
		TODO:
		Check for collected items that are needed by the tech to be researchable.
		If there are enough items add them to the tech, otherwise pop an errormessage telling the palyer what is missing.
	*/
	if (!tech->statusResearchable) {
		if (RS_CheckCollected(&tech->require_AND) && RS_CheckCollected(&tech->require_OR))
			tech->statusCollected = qtrue;
		RS_MarkResearchable();
	}
	/************/
	
	if (tech->statusResearchable) {
		switch (tech->statusResearch) {
		case RS_RUNNING:
			MN_Popup(_("Notice"), _("This item is already under research by your scientists."));
			break;
		case RS_PAUSED:
			MN_Popup(_("Notice"), _("The research on this item continues."));
			tech->statusResearch = RS_RUNNING;
			break;
		case RS_FINISH:
			MN_Popup(_("Notice"), _("The research on this item is complete."));
			break;
		case RS_NONE:
			if (tech->scientists <= 0) {
				/* Add scientists to tech. */
				RS_AssignScientist(tech);
			}
			tech->statusResearch = RS_RUNNING;
			break;
		default:
			break;
		}
	} else
		MN_Popup(_("Notice"), _("The research on this item is not yet possible.\nYou need to research the technologies it's based on first."));

	RS_UpdateData();
}

/**
 * @brief Pauses the research of the selected research-list entry.
 * TODO: Check if laboratory is available
 */
static void RS_ResearchStop(void)
{
	technology_t *tech = NULL;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	/* get the currently selected research-item */
	tech = researchList[researchListPos];

	switch (tech->statusResearch) {
	case RS_RUNNING:
		/* TODO: remove lab from technology and scientists from lab */
		tech->statusResearch = RS_PAUSED;
		break;
	case RS_PAUSED:
		tech->statusResearch = RS_RUNNING;
		break;
	case RS_FINISH:
		MN_Popup(_("Notice"), _("The research on this item is complete."));
		break;
	case RS_NONE:
		Com_Printf("Can't pause research. Research not started.\n");
		break;
	default:
		break;
	}
	RS_UpdateData();
}

#if DEPENDENCIES_OVERHAUL
/**
 * @brief Loops trough the research-list and updates the displayed text+color of each research-item according to it's status.
 * @note See menu_research.ufo for the layout/called functions.
 * @todo Display free space in all labs in the current base for each item.
 */
void RS_UpdateData(void)
{
	char name[MAX_VAR];
	int i, j;
	int available[MAX_BASES];
	technology_t *tech = NULL;

	/* Make everything the same (predefined in the ufo-file) color. */
	Cbuf_AddText("research_clear\n");

	for (i=0; i < gd.numBases; i++) {
		available[i] = E_CountUnassigned(&gd.bases[i], EMPL_SCIENTIST);
	}
	RS_CheckAllCollected();
	for (i = 0, j = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_sprintf(name, MAX_VAR, tech->name);

		/* TODO: add check for collected items */
		
		if (tech->statusCollected && !tech->statusResearchable && (tech->statusResearch != RS_FINISH)) {
			/* An unresearched collected item that cannot yet be researched. */
			Q_strcat(name, _(" [not yet researchable]"), MAX_VAR);
			/* Color the item 'unresearchable' */
			Cbuf_AddText(va("researchunresearchable%i\n", j));
			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), name);

			Cvar_Set(va("mn_researchassigned%i", j), "--");
			Cvar_Set(va("mn_researchavailable%i", j), "--");
			Cvar_Set(va("mn_researchmax%i", j), "--");

			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		} else if ((tech->statusResearch != RS_FINISH) && (tech->statusResearchable)) {
			/* An item that can be researched. */
			/* How many scis are assigned to this tech. */
			Cvar_SetValue(va("mn_researchassigned%i", j), tech->scientists);
			if ((tech->base_idx == baseCurrent->idx) || (tech->base_idx < 0) ) {
				/* Maximal available scientists in the base the tech is researched. */
				Cvar_SetValue(va("mn_researchavailable%i", j), available[baseCurrent->idx]);
			} else {
				/* Display available scientists of other base here. */
				Cvar_SetValue(va("mn_researchavailable%i", j), available[tech->base_idx]);
			}
			/* TODO: Free space in all labs in this base. */
			/* Cvar_SetValue(va("mn_researchmax%i", j), available); */
			Cvar_Set(va("mn_researchmax%i", j), "mx.");
			/* Set the text of the research items and mark them if they are currently researched. */
			switch (tech->statusResearch) {
			case RS_RUNNING:
				/* Color the item with 'research running'-color. */
				Cbuf_AddText(va("researchrunning%i\n", j));
				break;
			case RS_PAUSED:
				/* Color the item with 'research paused'-color. */
				Cbuf_AddText(va("researchpaused%i\n", j));
				break;
			case RS_NONE:
				/* The color is defined in menu research.ufo by  "confunc research_clear". See also above. */
				break;
			case RS_FINISH:
			default:
				break;
			}

			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), _(name));
			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		}
	}

	researchListLength = j;

	/* Set rest of the list-entries to have no text at all. */
	for (; j < MAX_RESEARCHDISPLAY; j++) {
		Cvar_Set(va("mn_researchitem%i", j), "");
		Cvar_Set(va("mn_researchassigned%i", j), "");
		Cvar_Set(va("mn_researchavailable%i", j), "");
		Cvar_Set(va("mn_researchmax%i", j), "");
	}

	/* Select last selected item if possible or the very first one if not. */
	if (researchListLength) {
		Com_DPrintf("RS_UpdateData: Pos%i Len%i\n", researchListPos, researchListLength);
		if ((researchListPos < researchListLength) && (researchListLength < MAX_RESEARCHDISPLAY)) {
			Cbuf_AddText(va("researchselect%i\n", researchListPos));
		} else {
			Cbuf_AddText("researchselect0\n");
		}
	} else {
		/* No display list abailable (zero items) - > Reset description. */
		Cvar_Set("mn_researchitemname", "");
		Cvar_Set("mn_researchitem", "");
		Cvar_Set("mn_researchweapon", "");
		Cvar_Set("mn_researchammo", "");
		menuText[TEXT_STANDARD] = NULL;
	}

	/* Update the description field/area. */
	RS_ResearchDisplayInfo();
}
#else /* overhaul */
/**
 * @brief Loops trough the research-list and updates the displayed text+color of each research-item according to it's status.
 * @note See menu_research.ufo for the layout/called functions.
 * @todo Display free space in all labs in the current base for each item.
 */
void RS_UpdateData(void)
{
	char name[MAX_VAR];
	int i, j;
	int available[MAX_BASES];
	technology_t *tech = NULL;

	/* Make everything the same (predefined in the ufo-file) color. */
	Cbuf_AddText("research_clear\n");

	for (i=0; i < gd.numBases; i++) {
		available[i] = E_CountUnassigned(&gd.bases[i], EMPL_SCIENTIST);
	}

	for (i = 0, j = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_sprintf(name, MAX_VAR, tech->name);

		if (tech->statusCollected && !tech->statusResearchable && (tech->statusResearch != RS_FINISH)) {
			/* An unresearched collected item that cannot yet be researched. */
			Q_strcat(name, _(" [not yet researchable]"), MAX_VAR);
			/* Color the item 'unresearchable' */
			Cbuf_AddText(va("researchunresearchable%i\n", j));
			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), name);

			Cvar_Set(va("mn_researchassigned%i", j), "--");
			Cvar_Set(va("mn_researchavailable%i", j), "--");
			Cvar_Set(va("mn_researchmax%i", j), "--");

			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		} else if ((tech->statusResearch != RS_FINISH) && (tech->statusResearchable)) {
			/* An item that can be researched. */
			/* How many scis are assigned to this tech. */
			Cvar_SetValue(va("mn_researchassigned%i", j), tech->scientists);
			if ((tech->base_idx == baseCurrent->idx) || (tech->base_idx < 0) ) {
				/* Maximal available scientists in the base the tech is researched. */
				Cvar_SetValue(va("mn_researchavailable%i", j), available[baseCurrent->idx]);
			} else {
				/* Display available scientists of other base here. */
				Cvar_SetValue(va("mn_researchavailable%i", j), available[tech->base_idx]);
			}
			/* TODO: Free space in all labs in this base. */
			/* Cvar_SetValue(va("mn_researchmax%i", j), available); */
			Cvar_Set(va("mn_researchmax%i", j), "mx.");
			/* Set the text of the research items and mark them if they are currently researched. */
			switch (tech->statusResearch) {
			case RS_RUNNING:
				/* Color the item with 'research running'-color. */
				Cbuf_AddText(va("researchrunning%i\n", j));
				break;
			case RS_PAUSED:
				/* Color the item with 'research paused'-color. */
				Cbuf_AddText(va("researchpaused%i\n", j));
				break;
			case RS_NONE:
				/* The color is defined in menu research.ufo by  "confunc research_clear". See also above. */
				break;
			case RS_FINISH:
			default:
				break;
			}

			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), _(name));
			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		}
	}

	researchListLength = j;

	/* Set rest of the list-entries to have no text at all. */
	for (; j < MAX_RESEARCHDISPLAY; j++) {
		Cvar_Set(va("mn_researchitem%i", j), "");
		Cvar_Set(va("mn_researchassigned%i", j), "");
		Cvar_Set(va("mn_researchavailable%i", j), "");
		Cvar_Set(va("mn_researchmax%i", j), "");
	}

	/* Select last selected item if possible or the very first one if not. */
	if (researchListLength) {
		Com_DPrintf("RS_UpdateData: Pos%i Len%i\n", researchListPos, researchListLength);
		if ((researchListPos < researchListLength) && (researchListLength < MAX_RESEARCHDISPLAY)) {
			Cbuf_AddText(va("researchselect%i\n", researchListPos));
		} else {
			Cbuf_AddText("researchselect0\n");
		}
	} else {
		/* No display list abailable (zero items) - > Reset description. */
		Cvar_Set("mn_researchitemname", "");
		Cvar_Set("mn_researchitem", "");
		Cvar_Set("mn_researchweapon", "");
		Cvar_Set("mn_researchammo", "");
		menuText[TEXT_STANDARD] = NULL;
	}

	/* Update the description field/area. */
	RS_ResearchDisplayInfo();
}
#endif /* overhaul */

/**
 * @brief
 * TODO: document this
 */
void CL_ResearchType(void)
{
	/* Update and display the list. */
	RS_UpdateData();

	/* Nothing to research here. */
	if (!researchListLength || !gd.numBases) {
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
	} else if (baseCurrent && !baseCurrent->hasLab) {
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
		MN_Popup(_("Notice"), _("Build a laboratory first"));
	}
}

#if 0
/**
 * @brief Checks if the research item id1 depends on (requires) id2
 * @param[in] id1 Unique id of a technology_t that may or may not depend on id2.
 * @param[in] id2 Unique id of a technology_t
 * @return qboolean
 */
static qboolean RS_DependsOn(char *id1, char *id2)
{
	int i;
	technology_t *tech = NULL;
	stringlist_t required;

	tech = RS_GetTechByID(id1);
	if (!tech)
		return qfalse;

	/* research item found */
	required = tech->requires;
	for (i = 0; i < required.numEntries; i++) {
		/* Current item (=id1) depends on id2. */
		if (!Q_strncmp(required.string[i], id2, MAX_VAR))
			return qtrue;
	}
	return qfalse;
}
#endif

/**
 * @brief Mark technologies as researched. This includes techs that depends in "id" and have time=0
 * @param[in] id Unique id of a technology_t
 */
void RS_MarkResearched(char *id)
{
	int i;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!Q_strncmp(id, tech->id, MAX_VAR)) {
			tech->statusResearch = RS_FINISH;
			Com_DPrintf("Research of \"%s\" finished.\n", tech->id);
#if 0
		} else if (RS_DependsOn(tech->id, id) && (tech->time <= 0) && RS_TechIsResearchable(tech)) {
			tech->statusResearch = RS_FINISH;
			Com_DPrintf("Depending tech \"%s\" has been researched as well.\n", tech->id);
#endif
		}
	}
	RS_MarkResearchable();
}

/**
 * @brief Checks the research status
 * @todo Needs to check on the exact time that elapsed since the last check fo the status.
 *
 */
void CL_CheckResearchStatus(void)
{
	int i, newResearch = 0;
	technology_t *tech = NULL;

	if (!researchListLength)
		return;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (tech->statusResearch == RS_RUNNING) {
			if ( ( tech->time > 0 ) && ( tech->scientists >= 0 ) ) {
				Com_DPrintf("timebefore %.2f\n", tech->time);
				Com_DPrintf("timedelta %.2f\n", tech->scientists * 0.8);
				/* TODO: Just for testing, better formular may be needed. */
				tech->time -= tech->scientists * 0.8;
				Com_DPrintf("timeafter %.2f\n", tech->time);
				/* TODO include employee-skill in calculation. */
				/* Will be a good thing (think of percentage-calculation) once non-integer values are used. */
				if (tech->time <= 0) {
					Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Research of %s finished\n"), tech->name);
					MN_AddNewMessage(_("Research finished"), messageBuffer, qfalse, MSG_RESEARCH, tech);

					/* Remove all scientists from the technology. */
					while (tech->scientists > 0)
						RS_RemoveScientist(tech);

					RS_MarkResearched(tech->id);
					researchListLength = 0;
					researchListPos = 0;
					newResearch++;
					tech->time = 0;
				}
			}
		}
	}

	if (newResearch) {
		CL_GameTimeStop();
		RS_UpdateData();
	}
}

#ifdef DEBUG
/**
 * @brief List all parsed technologies and their attributes in commandline/console.
 * Command to call this: techlist
 */
static void RS_TechnologyList_f(void)
{
	int i, j;
	technology_t *tech = NULL;
	requirements_t *req = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_Printf("Tech: %s\n", tech->id);
		Com_Printf("... time      -> %.2f\n", tech->time);
		Com_Printf("... name      -> %s\n", tech->name);
		req = &tech->require_AND;
		Com_Printf("... requires ALL  ->");
		for (j = 0; j < req->numLinks; j++)
			Com_Printf(" %i %s %i", req->type[j], req->id[j], req->idx[j]);
		req = &tech->require_OR;
		Com_Printf("... requires ANY  ->");
		for (j = 0; j < req->numLinks; j++)
			Com_Printf(" %i %s %i", req->type[j], req->id[j], req->idx[j]);
		Com_Printf("\n");
		Com_Printf("... provides  -> %s", tech->provides);
		Com_Printf("\n");

		Com_Printf("... type      -> ");
		switch (tech->type) {
		case RS_TECH:
			Com_Printf("tech");
			break;
		case RS_WEAPON:
			Com_Printf("weapon");
			break;
		case RS_CRAFT:
			Com_Printf("craft");
			break;
		case RS_CRAFTWEAPON:
			Com_Printf("craftweapon");
			break;
		case RS_CRAFTSHIELD:
			Com_Printf("craftshield");
			break;
		case RS_ARMOR:
			Com_Printf("armor");
			break;
		case RS_BUILDING:
			Com_Printf("building");
			break;
		case RS_UGV:
			Com_Printf("ugv");
			break;
		default:
			break;
		}
		Com_Printf("\n");

		Com_Printf("... research  -> ");
		switch (tech->type) {
		case RS_NONE:
			Com_Printf("unknown tech");
			break;
		case RS_RUNNING:
			Com_Printf("running");
			break;
		case RS_PAUSED:
			Com_Printf("paused");
			break;
		case RS_FINISH:
			Com_Printf("done");
			break;
		default:
			break;
		}

		Com_Printf("\n");
	}
}
#endif /* DEBUG */

/**
 * @brief
 * Command to call this: research_init
 *
 * Should be called whenever the research menu
 * gets active
 */
void MN_ResearchInit(void)
{
	CL_ResearchType();
}

#ifdef DEBUG
/**
 * @brief Set all item to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchAll(void)
{
	int i;

	for (i = 0; i < gd.numTechnologies; i++) {
		Com_Printf("...mark %s as researched\n", gd.technologies[i].id);
		gd.technologies[i].statusResearchable = qtrue;
		gd.technologies[i].statusResearch = RS_FINISH;
		/* TODO: Set all "collected" entries in the requirements to the "amount" value. */
	}
}
#endif

/**
 * @brief Opens UFOpedia by clicking dependency list
 */
void RS_DependenciesClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: research_dependencies_click <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= curRequiredList.numEntries)
		return;

	UP_OpenWith(curRequiredList.string[num]);
}

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
void RS_ResetResearch(void)
{
	researchListLength = 0;
	/* add commands and cvars */
	Cmd_AddCommand("research_init", MN_ResearchInit, NULL);
	Cmd_AddCommand("research_select", CL_ResearchSelectCmd, NULL);
	Cmd_AddCommand("research_type", CL_ResearchType, NULL);
	Cmd_AddCommand("mn_start_research", RS_ResearchStart, NULL);
	Cmd_AddCommand("mn_stop_research", RS_ResearchStop, NULL);
	Cmd_AddCommand("mn_rs_add", RS_AssignScientist_f, NULL);
	Cmd_AddCommand("mn_rs_remove", RS_RemoveScientist_f, NULL);
	Cmd_AddCommand("research_update", RS_UpdateData, NULL);
	Cmd_AddCommand("invlist", Com_InventoryList_f, NULL);
	Cmd_AddCommand("research_dependencies_click", RS_DependenciesClick_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("techlist", RS_TechnologyList_f, NULL);
	Cmd_AddCommand("research_all", RS_DebugResearchAll, NULL);
#endif
}

/**
 * @brief The valid definition names in the research.ufo file.
 */
static value_t valid_tech_vars[] = {
	/*name of technology */
	{"name", V_TRANSLATION2_STRING, offsetof(technology_t, name)},
	{"description", V_TRANSLATION2_STRING, offsetof(technology_t, description)},
	{"description_pre", V_TRANSLATION2_STRING, offsetof(technology_t, description_pre)},
	/*what does this research provide */
	{"provides", V_STRING, offsetof(technology_t, provides)},
	/* ("require")	Handled in parser below. */
	/* ("delay", V_INT, offsetof(technology_t, require->delay)}, can this work? */

	{"producetime", V_INT, offsetof(technology_t, produceTime)},
	{"time", V_FLOAT, offsetof(technology_t, time)},
	{"image_top", V_STRING, offsetof(technology_t, image_top)},
	{"image_bottom", V_STRING, offsetof(technology_t, image_bottom)},
	{"mdl_top", V_STRING, offsetof(technology_t, mdl_top)},
	{"mdl_bottom", V_STRING, offsetof(technology_t, mdl_bottom)},
	{NULL, 0, 0}
};

/**
 * @brief Parses one "tech" entry in the research.ufo file and writes it into the next free entry in technologies (technology_t).
 * @param[in] id Unique id of a technology_t. This is parsed from "tech xxx" -> id=xxx
 * @param[in] text the whole following text that is part of the "tech" item definition in research.ufo.
 */
void RS_ParseTechnologies(char *id, char **text)
{
	value_t *var = NULL;
	technology_t *tech = NULL;
	int tech_old;
	char *errhead = "RS_ParseTechnologies: unexptected end of file.";
	char *token = NULL;
	requirements_t *required_temp = NULL;

	int i;

	/* get body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("RS_ParseTechnologies: \"%s\" technology def without body ignored.\n", id);
		return;
	}
	if (gd.numTechnologies >= MAX_TECHNOLOGIES) {
		Com_Printf("RS_ParseTechnologies: too many technology entries. limit is %i.\n", MAX_TECHNOLOGIES);
		return;
	}

	/* New technology (next free entry in global tech-list) */
	tech = &gd.technologies[gd.numTechnologies];
	gd.numTechnologies++;

	memset(tech, 0, sizeof(technology_t));

	/*set standard values */
	tech->idx = gd.numTechnologies - 1;
	Com_sprintf(tech->id, MAX_VAR, id);
	Com_sprintf(tech->description, MAX_VAR, _("No description available."));
	/* tech->description_pre should be NULL in the beginning so we can check on that. TODO: this may change int he future.*/
	*tech->provides = '\0';
	*tech->image_top = '\0';
	*tech->image_bottom = '\0';
	*tech->mdl_top = '\0';
	*tech->mdl_bottom = '\0';
	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;
	tech->time = 0;
	tech->overalltime = 0;
	tech->scientists = 0;
	tech->prev = -1;
	tech->next = -1;
	tech->base_idx = -1;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, id);
		if (!*text)
			break;
		if (*token == '}')
			break;
		/* get values */
		if (!Q_strncmp(token, "type", MAX_VAR)) {
			/* what type of tech this is */
			token = COM_EParse(text, errhead, id);
			if (!*text)
				return;
			/* redundant, but oh well. */
			if (!Q_strncmp(token, "tech", MAX_VAR))
				tech->type = RS_TECH;
			else if (!Q_strncmp(token, "weapon", MAX_VAR))
				tech->type = RS_WEAPON;
			else if (!Q_strncmp(token, "armor", MAX_VAR))
				tech->type = RS_ARMOR;
			else if (!Q_strncmp(token, "craft", MAX_VAR))
				tech->type = RS_CRAFT;
			else if (!Q_strncmp(token, "craftweapon", MAX_VAR))
				tech->type = RS_CRAFTWEAPON;
			else if (!Q_strncmp(token, "craftshield", MAX_VAR))
				tech->type = RS_CRAFTSHIELD;
			else if (!Q_strncmp(token, "building", MAX_VAR))
				tech->type = RS_BUILDING;
			else if (!Q_strncmp(token, "alien", MAX_VAR))
				tech->type = RS_ALIEN;
			else if (!Q_strncmp(token, "ugv", MAX_VAR))
				tech->type = RS_UGV;
			else
				Com_Printf("RS_ParseTechnologies: \"%s\" unknown techtype: \"%s\" - ignored.\n", id, token);
		} else {
			
			if ((!Q_strncmp(token, "require_AND", MAX_VAR)) || (!Q_strncmp(token, "require_OR", MAX_VAR))) {
				/* Link to correct list. */
				if (!Q_strncmp(token, "require_AND", MAX_VAR)) {
					required_temp = &tech->require_AND;
				} else {
					required_temp = &tech->require_OR;
				}
				
				token = COM_EParse(text, errhead, id);
				if (!*text)
					break;
				if (*token != '{')
					break;
				if (*token == '}')
					break;

				do {	/* Loop through all 'require' entries.*/
					token = COM_EParse(text, errhead, id);
					if (!*text)
						return;
					if (*token == '}')
						break;

					if (!Q_strcmp(token, "tech")) {
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							required_temp->type[required_temp->numLinks] = RS_LINK_TECH;
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							Q_strncpyz(required_temp->id[required_temp->numLinks], token, MAX_VAR);
							Com_DPrintf("RS_ParseTechnologies: tech - %s\n", required_temp->id[required_temp->numLinks]);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", id, MAX_TECHLINKS);
						}
					} else if (!Q_strcmp(token, "item")) {
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							required_temp->type[required_temp->numLinks] = RS_LINK_ITEM;
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							Q_strncpyz(required_temp->id[required_temp->numLinks], token, MAX_VAR);
							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							required_temp->amount[required_temp->numLinks] = atoi(token);
							Com_DPrintf("RS_ParseTechnologies: item - %s - %i\n", required_temp->id[required_temp->numLinks], required_temp->amount[required_temp->numLinks]);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", id, MAX_TECHLINKS);
						}
					} else if (!Q_strcmp(token, "event")) {
						token = COM_Parse(text);
						Com_DPrintf("RS_ParseTechnologies: event - %s\n", token);
						/* Get name/id & amount of required item. */
						/* TODO: Implement final event esystem, so this can work 100% */
					} else {
						Com_Printf("RS_ParseTechnologies: \"%s\" unknown requirement-type: \"%s\" - ignored.\n", id, token);
					}
				} while (*text);
			} else if (!Q_strncmp(token, "delay", MAX_VAR)) {
				/* TODO: Move this to the default parser? See valid_tech_vars. */
				token = COM_Parse(text);
				Com_DPrintf("RS_ParseTechnologies: delay - %s\n", token);
			} else if (!Q_strncmp(token, "up_chapter", MAX_VAR)) {
				/* ufopedia chapter */
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;

				if (*token) {
					/* find chapter */
					for (i = 0; i < gd.numChapters; i++) {
						if (!Q_strncmp(token, gd.upChapters[i].id, MAX_VAR)) {
							/* add entry to chapter */
							tech->up_chapter = i;
							if (!gd.upChapters[i].first) {
								gd.upChapters[i].first = tech->idx;
								gd.upChapters[i].last = tech->idx;
								tech->prev = -1;
								tech->next = -1;
							} else {
								/* get "last entry" in chapter */
								tech_old = gd.upChapters[i].last;
								gd.upChapters[i].last = tech->idx;
								gd.technologies[tech_old].next = tech->idx;
								gd.technologies[gd.upChapters[i].last].prev = tech_old;
								gd.technologies[gd.upChapters[i].last].next = -1;
							}
							break;
						}
						if (i == gd.numChapters)
							Com_Printf("RS_ParseTechnologies: \"%s\" - chapter \"%s\" not found.\n", id, token);
					}
				}
			} else {
				for (var = valid_tech_vars; var->string; var++)
					if (!Q_strncmp(token, var->string, sizeof(var->string))) {
						/* found a definition */
						token = COM_EParse(text, errhead, id);
						if (!*text)
							return;

						if (var->ofs && var->type != V_NULL)
							Com_ParseValue(tech, token, var->type, var->ofs);
						else
							/* NOTE: do we need a buffer here? for saving or something like that? */
							Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed\n");
						break;
					}
				/*TODO: escape "type weapon/tech/etc.." here */
				if (!var->string)
					Com_Printf("RS_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n", token, id);
			}
		}
	} while (*text);

	/* set the overall reseach time to the one given in the ufo-file. */
	tech->overalltime = tech->time;
}

#if DEPENDENCIES_OVERHAUL
#else /* overhaul */
/**
 * @brief Returns the list of required (by id) items.
 * @param[in] id Unique id of a technology_t.
 * @param[out] required a list of strings with the unique ids of items/buildings/etc..
 * TODO: out of order ... seems to produce garbage
 */
void RS_GetRequired(char *id, stringlist_t * required)
{
	technology_t *tech = NULL;

	tech = RS_GetTechByID(id);
	if (!tech)
		return;

	/* research item found */
	required = &tech->requires;	/* is linking a good idea? */
}
#endif /* overhaul */

/**
 * @brief Checks whether an item is already researched
 * @sa RS_IsResearched_ptr
 */
qboolean RS_IsResearched_idx(int idx)
{
	if (ccs.singleplayer == qfalse)
		return qtrue;
	if (idx >= 0 && gd.technologies[idx].statusResearch == RS_FINISH)
		return qtrue;
	return qfalse;
}

/**
 * @brief Checks whether an item is already researched
 * @sa RS_IsResearched_idx
 * Call this function if you already hold a tech pointer
 */
qboolean RS_IsResearched_ptr(technology_t * tech)
{
	if (ccs.singleplayer == qfalse)
		return qtrue;
	if (tech && tech->statusResearch == RS_FINISH)
		return qtrue;
	return qfalse;
}

/**
 * @brief Checks if the item (as listed in "provides") has been researched
 * @param[in] id_provided Unique id of an item/building/etc.. that is provided by a technology_t
 * @return qboolean
 * @sa RS_IsResearched_ptr
 */
qboolean RS_ItemIsResearched(char *id_provided)
{
	int i;
	technology_t *tech = NULL;

	/* in multiplaer everyting is researched */
	if (!ccs.singleplayer)
		return qtrue;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		/* Provided item found. */
		if (!Q_strncmp(id_provided, tech->provides, MAX_VAR))
			return RS_IsResearched_ptr(tech);
	}
	/* no research needed */
	return qtrue;
}

#if DEPENDENCIES_OVERHAUL
/**
 * @brief
 * @sa RS_ItemCollected
 * Call this function if you already hold a tech pointer.
 */
int RS_Collected_(technology_t * tech)
{
	if (tech)
		return tech->statusCollected;

	Com_DPrintf("RS_Collected_: NULL technology given.\n");
	return -1;
}
#else /* overhaul */

/**
 * @brief Returns the number of collected ("provided") items.
 * @param[in] id_provided Unique id of an item/building/etc.. that is provided by a technology_t
 * @return qboolean
 * @sa RS_TechIsResearched
 * @sa RS_TechIsResearchable
 */
qboolean RS_ItemCollected(char *id_provided)
{
	int i = 0;

	if (!id_provided)
		return qfalse;

	for (; i < gd.numTechnologies; i++)
		if (!Q_strncmp((char *) id_provided, gd.technologies[i].provides, MAX_VAR))
			return RS_Collected_(&gd.technologies[i]);

	Com_DPrintf("RS_ItemCollected: \"%s\" <- research item that 'provides' this item not found.\n", id_provided);
	return qfalse;
}
#endif /* overhaul */

/**
 * @brief Checks if the technology (tech-id) has been researched.
 * @param[in] tech_idx index of the technology.
 * @return qboolean
 */
qboolean RS_TechIsResearched(int tech_idx)
{
	if (tech_idx < 0)
		return qfalse;

	/* research item found */
	if (gd.technologies[tech_idx].statusResearch == RS_FINISH)
		return qtrue;

	return qfalse;
}

#if DEPENDENCIES_OVERHAUL
/**
 * @brief Checks if the technology (tech-id) is researchable.
 * @param[in] tech pointer to technology_t.
 * @return qboolean
 * @sa RS_TechIsResearched
 */
qboolean RS_TechIsResearchable(technology_t * tech)
{
	if (!tech)
		return qfalse;

	/* Research item found */
	if (tech->statusResearch == RS_FINISH)
		return qfalse;

	if (tech->statusResearchable)
		return qtrue;
	
	return RS_RequirementsMet(&tech->require_AND, &tech->require_OR);
}

#else /* overhaul */
/**
 * @brief Checks if the technology (tech-id) is researchable.
 * @param[in] tech pointer to technology_t.
 * @return qboolean
 * @sa RS_TechIsResearched
 */
qboolean RS_TechIsResearchable(technology_t * tech)
{
	int i;
	stringlist_t *required = NULL;

	if (!tech)
		return qfalse;

	/* research item found */
	if (tech->statusResearch == RS_FINISH)
		return qfalse;

	if (tech->statusResearchable)
		return qtrue;

	required = &tech->requires;

	for (i = 0; i < required->numEntries; i++)
		/* Research of "id" not finished (RS_FINISH) at this point. */
		if (!RS_TechIsResearched(required->idx[i]))
			return qfalse;

	return qtrue;

}
#endif /* overhaul */

/**
 * @brief Returns a list of .ufo items that are produceable when this item has been researched (=provided)
 * This list also incldues other items that "require" this one (id) and have a reseach_time of 0.
 */
#if 0
void RS_GetProvided(char *id, char *provided)
{
	int i, j;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!strcmp(id, tech->id)) {
			for (j = 0; j < MAX_TECHLINKS; j++)
				Com_sprintf(provided[j], MAX_VAR, tech->provides);
			/*TODO: search for dependent items. */
			for (j = 0; j < gd.numTechnologies; j++) {
				if (RS_DependsOn(tech->id, id)) {
					/* TODO: append researchtree[j]->provided to *provided */
				}
			}
			return;
		}
	}
	Com_Printf("RS_GetProvided: research item \"%s\" not found.\n", id);
}
#endif


/**
 * @brief Returns the tech pointer
 * @param id unique id of a technology_t
 */
technology_t* RS_GetTechByIDX(int tech_idx)
{
	if (tech_idx < 0 || tech_idx >= gd.numTechnologies)
		return NULL;
	else
		return &gd.technologies[tech_idx];
}


/**
 * @brief return a pointer to the technology identified by given id string
 */
technology_t *RS_GetTechByID(const char *id)
{
	int i = 0;

	if (!id || !*id)
		return NULL;

	if (!Q_strncmp((char *) id, "nothing", MAX_VAR))
		return NULL;

	for (; i < gd.numTechnologies; i++) {
		if (!Q_strncmp((char *) id, gd.technologies[i].id, MAX_VAR))
			return &gd.technologies[i];
	}
	Com_DPrintf("RS_GetTechByID: Could not find a technology with id \"%s\"\n", id);
	return NULL;
}

/**
 * @brief returns a pointer to the item tech (as listed in "provides")
 */
technology_t *RS_GetTechByProvided(const char *id_provided)
{
	int i;

	for (i = 0; i < gd.numTechnologies; i++)
		if (!Q_strncmp((char *) id_provided, gd.technologies[i].provides, MAX_VAR))
			return &gd.technologies[i];

	/* if a building, probably needs another building */
	/* if not a building, catch NULL where function is called! */
	return NULL;
}

/**
 * @brief Returns a list of technologies for the given type
 * @note this list is terminated by a NULL pointer
 */
technology_t **RS_GetTechsByType(researchType_t type)
{
	static technology_t *techList[MAX_TECHNOLOGIES];
	int i, j = 0;

	for (i=0; i<gd.numTechnologies;i++) {
		if (gd.technologies[i].type == type ) {
			techList[j] = &gd.technologies[i];
			j++;
			/* j+1 because last item have to be NULL */
			if ( j+1 >= MAX_TECHNOLOGIES ) {
				Com_Printf("RS_GetTechsByType: MAX_TECHNOLOGIES limit hit\n");
				break;
			}
		}
	}
	techList[j] = NULL;
	Com_DPrintf("techlist with %i entries\n", j);
	return techList;
}

/**
 * @brief Searches for the technology that has teh most scientists assigned in a given base.
 * @param[in] base_idx In what base the tech shoudl be researched.
 * @sa E_RemoveEmployeeFromBuilding
 */
technology_t *RS_GetTechWithMostScientists( int base_idx )
{
	technology_t *tech = NULL;
	technology_t *tech_temp = NULL;
	int i = 0;
	int max = 0;

	for (i=0; i<gd.numTechnologies;i++) {
		tech_temp = &gd.technologies[i];
		if ( (tech_temp->statusResearch == RS_RUNNING) && (tech_temp->base_idx == base_idx) ) {
			if (tech_temp->scientists > max) {
				tech = tech_temp;
				max = tech->scientists;
			}
		}
	}

	return tech;
}
