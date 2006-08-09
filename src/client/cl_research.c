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

void RS_GetFirstRequired(int tech_idx, stringlist_t * required);
qboolean RS_TechIsResearchable(technology_t * t);

/* A (local) list of displayed technology-entries (the research list in the base) */
static technology_t *researchList[MAX_RESEARCHLIST];

/* The number of entries in the above list. */
static int researchListLength;

/* The currently selected entry in the above list. */
static int researchListPos;


/**
  * @brief Marks one tech as 'collected'  if an item it 'provides' (= id) has been collected.
  * @param id unique id of a provided item (can be item/building/craft/etc..)
  */
void RS_MarkOneCollected(char *id)
{
	int i;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!Q_strncmp(tech->provides, id, MAX_VAR)) {	/* provided item found */
			tech->statusCollected++;
			return;
		}
	}
	Com_Printf("RS_MarkOneCollected: \"%s\" not found. No tech provides this.\n", id);
}

/**
  * @brief Marks all techs if an item they 'provide' have been collected.
  * Should be run after items have been collected/looted from the
  * battlefield (cl_campaign.c -> "CL_CollectItems") and after techtree/inventory init (for all).
  */
void RS_MarkCollected(void)
{
	int i;

	for (i = 0; i < MAX_OBJDEFS; i++) {
		if (ccs.eCampaign.num[i]) {
			Com_DPrintf("RS_MarkCollected: %s marked as collected.\n", csi.ods[i].kurz);
			RS_MarkOneCollected(csi.ods[i].kurz);
		}
	}
}

/**
  * @brief Marks one tech as researchedable.
  * @param id unique id of a technology_t
  */
void RS_MarkOneResearchable(int tech_idx)
{
	Com_DPrintf("RS_MarkOneResearchable: \"%s\" marked as researchable.\n", gd.technologies[tech_idx].id);
	gd.technologies[tech_idx].statusResearchable = qtrue;
}

/**
  * @brief Marks all the techs that can be researched.
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

				/* If the tech is an initial one,  mark it as as researchable. */
				for (j = 0; j < required->numEntries; j++) {
					if (!Q_strncmp(required->string[j], "initial", MAX_VAR)) {
						Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable - reason:isinitial.\n", tech->id);
						tech->statusResearchable = qtrue;
						break;
					}
				}
			}
		}
	}
	Com_DPrintf("RS_MarkResearchable: Done.\n");
}

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
	technology_t *tech_required = NULL;
	stringlist_t *required = NULL;
	objDef_t *item = NULL;
	objDef_t *item_ammo = NULL;
	building_t *building = NULL;
	aircraft_t *air_samp = NULL;
	byte found;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		/* set the overall reseach time (now fixed) to the one given in the ufo-file. */
		tech->overalltime = tech->time;

		/* link idx entries to tech provided by id */
		required = &tech->requires;
		for (j = 0; j < required->numEntries; j++) {
			tech_required = RS_GetTechByID(required->string[j]);
			if (tech_required) {
				required->idx[j] = tech_required->idx;
			} else {
				required->idx[j] = -1;
			}
		}

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
			for (j = 0; j < csi.numODs; j++) {
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
				Com_DPrintf("RS_InitTree: \"%s\" - Linked weapon or armor (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
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
	RS_MarkCollected();
	RS_MarkResearchable();
	Com_DPrintf("RS_InitTree: Technology tree initialised. %i entries found.\n", i);
}

/**
  * @brief Return "name" if present, otherwise enter the correct .ufo file and get it from the definition there.
  * @param[in] id unique id of a technology_t
  * @param[out] name Full name of this technology_t (technology_t->name) - defaults to id if nothing is found.
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
	int i;
	technology_t *tech = NULL;
	base_t *base = NULL;
	char dependencies[MAX_VAR];
	char tempstring[MAX_VAR];
	stringlist_t req_temp;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	if (researchListLength <= 0)
		return;

	tech = researchList[researchListPos];

	/* Display total number of free labs in current base. */
	Cvar_Set("mn_research_scis", va(_("Available scientists in this base: %i"), E_GetUnassingedEmployee(baseCurrent, EMPL_SCIENTIST )));
	Cvar_Set("mn_research_selbase", _("Not researched in any base."));

	/* Display the base this tech is researched in. */
	if (tech->scientists >= 0) {
		if (tech->base_idx != baseCurrent->idx) {
			base = &gd.bases[tech->base_idx];
			Cvar_Set("mn_research_selbase", va(_("Researched in %s"), base->name));
		} else {
			Cvar_Set("mn_research_selbase", _("Researched in this base."));
		}
	}

	Cvar_Set("mn_research_selname", tech->name);
	if (tech->overalltime) {
		if (tech->time > tech->overalltime) {
			Com_Printf("RS_ResearchDisplayInfo: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", tech->id, tech->time,
					   tech->overalltime);
			/* just in case the values got messed up */
			tech->time = tech->overalltime;
		}
		Cvar_Set("mn_research_seltime", va(_("Progress: %.1f%%"), 100 - (tech->time * 100 / tech->overalltime)));
	} else {
		Cvar_Set("mn_research_seltime", _("Progress: Not available."));
	}

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

	req_temp.numEntries = 0;
	RS_GetFirstRequired(tech->idx, &req_temp);
	Com_sprintf(dependencies, MAX_VAR, _("Dependencies: "));
	if (req_temp.numEntries > 0) {
		for (i = 0; i < req_temp.numEntries; i++) {
			/* name_temp gets initialised in getname. */
			RS_GetName(req_temp.string[i], tempstring);
			Q_strcat(dependencies, tempstring, MAX_VAR);

			if (i < req_temp.numEntries - 1)
				Q_strcat(dependencies, ", ", MAX_VAR);
		}
	} else {
		*dependencies = '\0';
	}
	Cvar_Set("mn_research_seldep", dependencies);
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
static void RS_AssignScientist(technology_t* tech)
{
	building_t *building = NULL;
	employee_t *employee = NULL;

	employee = E_GetUnassingedEmployee(baseCurrent, EMPL_SCIENTIST);
	
	if (!employee) {
		/* No scientists are free in this base. */
		return;
	}

	if (tech->statusResearchable) {
		/* Get a free lab from the currently active base. */
		building = B_GetLab(baseCurrent->idx);
		if (building) {
			/* Assign the tech to a lab&base. */
			tech->scientists++;
			tech->base_idx = building->base_idx;
			employee->buildingID = building->idx;
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
  * @brief
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

	RS_AssignScientist(researchList[num]);
}


/**
  * @brief
  * @sa RS_RemoveScientist
  * @sa RS_AssignScientist_f
  */
static void RS_RemoveScientist_f(void)
{
	int num;
	employee_t *employee = NULL;
	
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_remove <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num > researchListLength)
		return;

	if (researchList[num]->scientists >= 0) {
		employee = E_GetAssingedEmployee(&gd.bases[researchList[num]->base_idx], EMPL_SCIENTIST);
		employee->buildingID = -1;
		researchList[num]->scientists--;
	}

	/* Update display-list and display-info. */
	RS_ResearchDisplayInfo();
	RS_UpdateData();
}

/**
  * @brief Starts the research of the selected research-list entry.
  * TODO: Check if laboratory is available
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
	} else {
		MN_Popup(_("Notice"), _("The research on this item is not yet possible.\nYou need to research the technologies it's based on first."));
	}
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

	/* TODO: remove lab from technology and scientists from lab */
	switch (tech->statusResearch) {
	case RS_RUNNING:
		tech->statusResearch = RS_PAUSED;
		break;
	case RS_PAUSED:
		MN_Popup(_("Notice"), _("The research on this item continues."));
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

/**
  * @brief Loops trough the research-list and updates the displayed text+color of each research-item according to it's status.
  * See menu_research.ufo for the layout/called functions.
  */
void RS_UpdateData(void)
{
	char name[MAX_VAR];
	int i, j, available;
	technology_t *tech = NULL;

	/* Make everything the same (predefined in the ufo-file) color. */
	Cbuf_AddText("research_clear\n");

	available = E_CountUnassinged(baseCurrent, EMPL_SCIENTIST);

	for (i = 0, j = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_sprintf(name, MAX_VAR, tech->name);

		/* An unresearched collected item that cannot yet be researched. */
		if (tech->statusCollected && !tech->statusResearchable && (tech->statusResearch != RS_FINISH)) {
			Q_strcat(name, _(" [not yet researchable]"), MAX_VAR);
			/* Color the item 'unresearchable' */
			Cbuf_AddText(va("researchunresearchable%i\n", j));
			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), name);
			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		} else if ((tech->statusResearch != RS_FINISH) && (tech->statusResearchable)) {
#if 0
			if (tech->lab >= 0) {
				/* Display the assigned/free/max numbers of scientists for this tech. */
				building = &gd.buildings[tech->base_idx][tech->lab];
				employees_in_building = &building->assigned_employees;
				Com_sprintf(tempstring, MAX_VAR, _("%i max.\n"), employees_in_building->maxEmployees);
				/* Maximum number of employees in this base. */
				Cvar_Set(va("mn_researchmax%i", j), tempstring);
				Com_sprintf(tempstring, MAX_VAR, "%i\n", employees_in_building->numEmployees);
				/* Assigned employees to the technology. */
				Cvar_Set(va("mn_researchassigned%i", j), tempstring);
			} else
#endif
			{
				Cvar_SetValue(va("mn_researchassigned%i", j), 0);
				Cvar_Set(va("mn_researchmax%i", j), "mx.");
			}
			/* Maximal available scientists in the base the tech is reseearched. */
			Cvar_SetValue(va("mn_researchavailable%i", j), available);

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

/**
  * @brief
  * TODO: document this
  */
void CL_ResearchType(void)
{
	/* Update and display the list. */
	RS_UpdateData();

	/* Nothing to research here. */
	if (!researchListLength || !gd.numBases)
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
	else if (baseCurrent && !baseCurrent->hasLab)
		MN_Popup(_("Notice"), _("Build a laboratory first"));
}

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
			Com_Printf("Research of \"%s\" finished.\n", tech->id);
		} else if (RS_DependsOn(tech->id, id) && (tech->time <= 0)) {
			tech->statusResearch = RS_FINISH;
			Com_Printf("Depending tech \"%s\" has been researched as well.\n", tech->id);
		}
	}
	RS_MarkResearchable();
}

/**
  * @brief Checks the research status
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
			if (tech->time <= 0) {
				Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Research of %s finished\n"), tech->name);
				MN_AddNewMessage(_("Research finished"), messageBuffer, qfalse, MSG_RESEARCH, tech);

/*				B_ClearBuilding(&gd.buildings[tech->base_idx][tech->lab]);*/
				tech->base_idx = -1;
				tech->scientists = 0;
				RS_MarkResearched(tech->id);
				researchListPos = 0;
				newResearch++;
				tech->time = 0;
			} else {
#if 0
				if (tech->scientists >= 0) {
					building = &gd.buildings[tech->base_idx][tech->lab];
					Com_DPrintf("building found %s\n", building->name);
					employees_in_building = &building->assigned_employees;
					if (employees_in_building->numEmployees > 0) {
						Com_DPrintf("employees are there\n");
#if 0
						/* OLD */
						/* reduce one time-unit */
						tech->time--;
#endif
						Com_DPrintf("timebefore %.2f\n", tech->time);
						Com_DPrintf("timedelta %.2f\n", employees_in_building->numEmployees * 0.8);
						/* TODO: Just for testing, better formular may be needed. */
						tech->time -= employees_in_building->numEmployees * 0.8;
						Com_DPrintf("timeafter %.2f\n", tech->time);
						/* TODO include employee-skill in calculation. */
						/* Will be a good thing (think of percentage-calculation) once non-integer values are used. */
						if (tech->time < 0)
							tech->time = 0;
					}
				}
#endif
			}
		}
	}

	if (newResearch) {
		CL_GameTimeStop();
		CL_ResearchType();
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
	stringlist_t *req = NULL;
	stringlist_t req_temp;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		req = &tech->requires;
		Com_Printf("Tech: %s\n", tech->id);
		Com_Printf("... time      -> %.2f\n", tech->time);
		Com_Printf("... name      -> %s\n", tech->name);
		Com_Printf("... requires  ->");
		for (j = 0; j < req->numEntries; j++)
			Com_Printf(" %s", req->string[j]);
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

		Com_Printf("... Res.able  -> %i\n", tech->statusResearchable);
		Com_Printf("... Collected -> %i\n", tech->statusCollected);


		Com_Printf("... req_first ->");
		req_temp.numEntries = 0;
		RS_GetFirstRequired(tech->idx, &req_temp);
		for (j = 0; j < req_temp.numEntries; j++)
			Com_Printf(" %s", req_temp.string[j]);

		Com_Printf("\n");
	}
}
#endif

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
		gd.technologies[i].needsCollected = qfalse;
	}
}
#endif

/**
  * @brief This is more or less the initial
  * Bind some of the functions in this file to console-commands that you can call ingame.
  * Called from MN_ResetMenus resp. CL_InitLocal
  */
void RS_ResetResearch(void)
{
	researchListLength = 0;
	/* add commands and cvars */
	Cmd_AddCommand("research_init", MN_ResearchInit);
	Cmd_AddCommand("research_select", CL_ResearchSelectCmd);
	Cmd_AddCommand("research_type", CL_ResearchType);
	Cmd_AddCommand("mn_start_research", RS_ResearchStart);
	Cmd_AddCommand("mn_stop_research", RS_ResearchStop);
	Cmd_AddCommand("mn_rs_add", RS_AssignScientist_f);
	Cmd_AddCommand("mn_rs_remove", RS_RemoveScientist_f);
	Cmd_AddCommand("research_update", RS_UpdateData);
	Cmd_AddCommand("invlist", Com_InventoryList_f);
#ifdef DEBUG
	Cmd_AddCommand("techlist", RS_TechnologyList_f);
	Cmd_AddCommand("research_all", RS_DebugResearchAll);
#endif
}

/**
  * @brief The valid definition names in the research.ufo file.
  */
static value_t valid_tech_vars[] = {
	/*name of technology */
	{"name", V_TRANSLATION2_STRING, offsetof(technology_t, name)},
	{"description", V_TRANSLATION2_STRING, offsetof(technology_t, description)},
	/*what does this research provide */
	{"provides", V_STRING, offsetof(technology_t, provides)},
	/* to be able to research this tech zou need all "required" and at least one collected "provides" item. */
	{"needscollected", V_BOOL, offsetof(technology_t, needsCollected)},
	/*how long will this research last */
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
	char *misp = NULL;
	char temp_text[MAX_VAR];
	stringlist_t *required = NULL;
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

	required = &tech->requires;
	memset(tech, 0, sizeof(technology_t));

	/*set standard values */
	tech->idx = gd.numTechnologies - 1;
	Com_sprintf(tech->id, MAX_VAR, id);
	Com_sprintf(tech->description, MAX_VAR, _("No description available."));
	*tech->provides = '\0';
	*tech->image_top = '\0';
	*tech->image_bottom = '\0';
	*tech->mdl_top = '\0';
	*tech->mdl_bottom = '\0';
	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;
	tech->statusCollected = 0;
	tech->time = 0;
	tech->overalltime = 0;
	tech->scientists = 0;
	tech->prev = -1;
	tech->next = -1;


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
			if (!Q_strncmp(token, "requires", MAX_VAR)) {
				/* what other techs this one requires */
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;
				Q_strncpyz(temp_text, token, MAX_VAR);
				misp = temp_text;

				required->numEntries = 0;
				do {
					token = COM_Parse(&misp);
					if (!misp)
						break;
					if (required->numEntries < MAX_TECHLINKS)
						Q_strncpyz(required->string[required->numEntries++], token, MAX_VAR);
					else
						Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", id, MAX_TECHLINKS);

				}
				while (misp && required->numEntries < MAX_TECHLINKS);
			} else if (!Q_strncmp(token, "researched", MAX_VAR)) {
				/* tech alreadyy researched? */
				token = COM_EParse(text, errhead, id);
				if (!Q_strncmp(token, "true", MAX_VAR) || *token == '1')
					tech->statusResearch = RS_FINISH;
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
}

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

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		/* Provided item found. */
		if (!Q_strncmp(id_provided, tech->provides, MAX_VAR))
			return RS_IsResearched_ptr(tech);
	}
	/* no research needed */
	return qtrue;
}

/**
  * @brief
  * @sa RS_ItemCollected
  * call this function if you already hold a tech pointer
  */
int RS_Collected_(technology_t * tech)
{
	if (tech)
		return tech->statusCollected;

	Com_DPrintf("RS_Collected_: NULL technology given.\n");
	return -1;
}

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

/**
  * @brief Checks if the technology (tech-id) has been researched.
  * @param[in] tech_idx index of the technology.
  * @return qboolean
  */
qboolean RS_TechIsResearched(int tech_idx)
{
	if (tech_idx < 0)
		return qfalse;

#if 0
	/* DEBUG: still needed? */
	/* initial and nothing are always researched. as they are just starting "technologys" that are never used. */
	if (!Q_strncmp(gd.technologies[tech_idx].id, "initial", MAX_VAR)
		|| !Q_strncmp(gd.technologies[tech_idx].id, "nothing", MAX_VAR))
		return qtrue;
#endif

	/* research item found */
	if (gd.technologies[tech_idx].statusResearch == RS_FINISH)
		return qtrue;

	return qfalse;
}

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

	if ((!Q_strncmp(tech->id, "initial", MAX_VAR))
		|| (!Q_strncmp(tech->id, "nothing", MAX_VAR)))
		return qtrue;

	required = &tech->requires;

	for (i = 0; i < required->numEntries; i++)
		/* Research of "id" not finished (RS_FINISH) at this point. */
		if (!RS_TechIsResearched(required->idx[i]))
			return qfalse;

	return qtrue;

}

/**
  * @brief Returns the first required (yet unresearched) technologies that are needed by "tech_idx".
  * That means you need to research the result to be able to research (and maybe use) "id".
  */
static void RS_GetFirstRequired2(int tech_idx, int first_tech_idx, stringlist_t * required)
{
	int i;
	stringlist_t *required_temp = NULL;
	technology_t *tech = NULL;

	if (tech_idx < 0)
		return;

	required_temp = &gd.technologies[tech_idx].requires;

	if (!Q_strncmp(required_temp->string[0], "initial", MAX_VAR) || !Q_strncmp(required_temp->string[0], "nothing", MAX_VAR)) {
		if (tech_idx == first_tech_idx)
			return;
		if (required->numEntries < MAX_TECHLINKS) {
			/* TODO: check if the firstrequired tech has already been added (e.g indirectly from another tech) */
			required->idx[required->numEntries] = tech_idx;
			Q_strncpyz(required->string[required->numEntries], gd.technologies[tech_idx].id, MAX_VAR);
			required->numEntries++;
			Com_DPrintf("RS_GetFirstRequired2: \"%s\" - requirement 'initial' or 'nothing' found.\n", gd.technologies[tech_idx].id);
		}
		return;
	}
	for (i = 0; i < required_temp->numEntries; i++) {
		tech = RS_GetTechByID(required_temp->string[i]);
		if (RS_IsResearched_ptr(tech)) {
			if (required->numEntries < MAX_TECHLINKS) {
				required->idx[required->numEntries] = tech->idx;
				Q_strncpyz(required->string[required->numEntries], tech->id, MAX_VAR);
				required->numEntries++;
				Com_DPrintf("RS_GetFirstRequired2: \"%s\" - next item \"%s\" already researched.\n", gd.technologies[tech_idx].id, tech->id);
			}
		} else {
			RS_GetFirstRequired2(tech->idx, first_tech_idx, required);
		}
	}
}

/**
 * @brief
 * @param
 * @sa RS_GetTechByProvided
 * @sa RS_GetRequired
 */
void RS_GetFirstRequired(int tech_idx, stringlist_t * required)
{
	RS_GetFirstRequired2(tech_idx, tech_idx, required);
}

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

	if (!Q_strncmp((char *) id, "nothing", MAX_VAR)
		|| !Q_strncmp((char *) id, "initial", MAX_VAR))
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
