/**
 * @file cl_research.c
 * @brief Player research.
 *
 * Handles everything related to the research-tree.
 * Provides information if items/buildings/etc.. can be researched/used/displayed etc...
 * Implements the research-system (research new items/etc...)
 * See base/ufos/research.ufo and base/ufos/menu_research.ufo for the underlying content.
 * @todo: comment on used global variables.
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

#include "client.h"
#include "cl_global.h"

#define TECH_HASH_SIZE 64
static technology_t *tech_hash[TECH_HASH_SIZE];

qboolean RS_TechIsResearchable(technology_t *t);

/* A (local) list of displayed technology-entries (the research list in the base) */
static technology_t *researchList[MAX_RESEARCHLIST];

/* The number of entries in the above list. */
static int researchListLength;

/* The currently selected entry in the above list. */
static int researchListPos;

static stringlist_t curRequiredList;

/**
 * @brief Push a news about this tech when researched.
 * @param[in] tech_idx Technology index in global data.
 * @sa RS_ResearchFinish
 */
static void RS_PushNewsWhenResearched (technology_t* tech)
{
	assert(tech->pushnews);
	/* @todo */
}

/**
 * @brief Sets a technology status to researched and updates the date.
 * @param[in] tech The technology that was researched.
 */
void RS_ResearchFinish (technology_t* tech)
{
	tech->statusResearch = RS_FINISH;
	CL_DateConvert(&ccs.date, &tech->researchedDateDay, &tech->researchedDateMonth);
	tech->researchedDateYear = ccs.date.day / 365;
	if (!tech->statusResearchable) {
		tech->statusResearchable = qtrue;
		CL_DateConvert(&ccs.date, &tech->preResearchedDateDay, &tech->preResearchedDateMonth);
		tech->preResearchedDateYear = ccs.date.day / 365;
	}
	if (tech->pushnews)
		RS_PushNewsWhenResearched(tech);

	/* send a new message and add it to the mailclient */
	if ((tech->mailSent < MAILSENT_FINISHED) && (tech->time != 0)) { /* No mail sent for finished research. */
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("A research project has been completed: %s\n"), _(tech->name));
		MN_AddNewMessage(_("Research finished"), messageBuffer, qfalse, MSG_RESEARCH_FINISHED, tech);
		tech->mailSent = MAILSENT_FINISHED;
	}
}

/**
 * @brief Marks one tech as researchable.
 * @param tech The technology to be marked.
 * @sa RS_MarkCollected
 */
void RS_MarkOneResearchable (technology_t* tech)
{
	if (!tech)
		return;

	Com_DPrintf("RS_MarkOneResearchable: \"%s\" marked as researchable.\n", tech->id);

	if ((tech->mailSent < MAILSENT_PROPOSAL) && (tech->time != 0)) { /* No mail sent for research proposal. */
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("New research proposal: %s\n"), _(tech->name));
		MN_AddNewMessage(_("Unknown Technology researchable"), messageBuffer, qfalse, MSG_RESEARCH_PROPOSAL, tech);
		tech->mailSent = MAILSENT_PROPOSAL;
	}

	tech->statusResearchable = qtrue;
	CL_DateConvert(&ccs.date, &tech->preResearchedDateDay, &tech->preResearchedDateMonth);
	tech->preResearchedDateYear = ccs.date.day / 365;
}

/**
 * @brief Checks if all requirements of a tech have been met so that it becomes researchable.
 * @param[in] require_AND pointer to a list of AND-related requirements.
 * @param[in] require_OR pointer to a list of OR-related requirements.
 * @return Returns qtrue if all requirements are satisfied otherwise qfalse.
 * @todo Add support for the "delay" value.
 */
static qboolean RS_RequirementsMet (requirements_t *required_AND, requirements_t *required_OR)
{
	int i;
	qboolean met_AND = qfalse;
	qboolean met_OR = qfalse;

	if (!required_AND && !required_OR) {
		Com_Printf("RS_RequirementsMet: No requirement list(s) given as parameter.\n");
		return qfalse;
	}

	if (required_AND->numLinks) {
		met_AND = qtrue;
		for (i = 0; i < required_AND->numLinks; i++) {
			switch (required_AND->type[i]) {
			case RS_LINK_TECH:
				Com_DPrintf("RS_RequirementsMet: ANDtech: %s / %i\n", required_AND->id[i], required_AND->idx[i]);
				if (!RS_TechIsResearched(required_AND->idx[i])
					&& Q_strncmp(required_AND->id[i], "nothing", MAX_VAR)) {
					Com_DPrintf("RS_RequirementsMet: this tech not researched ----> %s \n", required_AND->id[i]);
					met_AND = qfalse;
				}
				break;
			case RS_LINK_ITEM:
				/* The same code is used in "PR_RequirementsMet" */
				Com_DPrintf("RS_RequirementsMet: ANDitem: %s / %i\n", required_AND->id[i], required_AND->idx[i]);
				if (B_ItemInBase(required_AND->idx[i], baseCurrent) < required_AND->amount[i]) {
					met_AND = qfalse;
				}
				break;
			case RS_LINK_EVENT:
				break;
			case RS_LINK_ALIEN_DEAD:
			case RS_LINK_ALIEN:
				if (AL_GetAlienAmount(required_AND->idx[i], required_AND->type[i]) < required_AND->amount[i])
					met_AND = qfalse;
				break;
			case RS_LINK_ALIEN_GLOBAL:
				if (AL_CountAll() < required_AND->amount[i])
					met_AND = qfalse;
				break;
			default:
				break;
			}

			if (!met_AND)
				break;
		}
	}

	if (required_OR->numLinks)
		for (i = 0; i < required_OR->numLinks; i++) {
			switch (required_OR->type[i]) {
			case RS_LINK_TECH:
				Com_DPrintf("RS_RequirementsMet: ORtech: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				if (RS_TechIsResearched(required_OR->idx[i]))
					met_OR = qtrue;
				break;
			case RS_LINK_ITEM:
				/* The same code is used in "PR_RequirementsMet" */
				Com_DPrintf("RS_RequirementsMet: ORitem: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				if (B_ItemInBase(required_OR->idx[i], baseCurrent) >= required_OR->amount[i])
					met_OR = qtrue;
				break;
			case RS_LINK_EVENT:
				break;
			case RS_LINK_ALIEN:
				if (AL_GetAlienAmount(required_OR->idx[i], RS_LINK_ALIEN) >= required_OR->amount[i])
					met_OR = qtrue;
				break;
			case RS_LINK_ALIEN_DEAD:
				if (AL_GetAlienAmount(required_OR->idx[i], RS_LINK_ALIEN_DEAD) >= required_OR->amount[i])
					met_OR = qtrue;
				break;
			case RS_LINK_ALIEN_GLOBAL:
				if (AL_CountAll() >= required_OR->amount[i])
					met_OR = qtrue;
				break;
			default:
				break;
			}

			if (met_OR)
				break;
		}
	Com_DPrintf("met_AND is %i, met_OR is %i\n", met_AND, met_OR);

	return (met_AND || met_OR);
}

/**
 * @brief Marks a give technology as collected
 * @sa CP_AddItemAsCollected
 * @sa MN_AddNewMessage
 * @sa RS_MarkOneResearchable
 */
void RS_MarkCollected (technology_t* tech)
{
	assert(tech);
	if ((tech->mailSent < MAILSENT_PROPOSAL) && (tech->time != 0)) { /* No mail sent for research proposal. */
		if (tech->statusResearch < RS_FINISH) {
			Com_sprintf(messageBuffer, sizeof(messageBuffer), _("New research proposal: %s\n"), _(tech->name));
			MN_AddNewMessage(_("Unknown Technology found"), messageBuffer, qfalse, MSG_RESEARCH_PROPOSAL, tech);
		}
		tech->mailSent = MAILSENT_PROPOSAL;
	}
	tech->statusCollected = qtrue;
}

/**
 * @brief Checks if anything has been collected (in the current base) and correct the value for each requirement.
 * @note Does not check if the collected items satisfy the needed "amount". This is done in RS_RequirementsMet. tech->statusCollected is just needed so the item is at least displayed somewhere.
 * @return Returns qtrue if there are is ANYTHING collected for each entry otherwise qfalse.
 * @todo Get rid (or improve) this statusCollected stuff.
 */
qboolean RS_CheckCollected (requirements_t *required)
{
	int i;
	int amount;
	qboolean something_collected_from_each = qtrue;
	technology_t *tech = NULL;

	if (!required)
		return qfalse;

	if (!baseCurrent)
		return qfalse;

	for (i = 0; i < required->numLinks; i++) {
		switch (required->type[i]) {
		case RS_LINK_ITEM:
			amount = B_ItemInBase(required->idx[i], baseCurrent);
			if (amount > 0) {
				required->collected[i] = amount;
			} else {
				required->collected[i] = 0;
				something_collected_from_each = qfalse;
			}
			break;
		case RS_LINK_TECH:
			tech = &gd.technologies[required->idx[i]];
			/* Check if it is a logic block (RS_LOGIC) and interate into it if so.*/
			if (tech->type == RS_LOGIC) {
				if (!RS_CheckCollected(&tech->require_AND)) {
					tech->statusCollected = qfalse;
					something_collected_from_each = qfalse;
				} else {
					RS_MarkCollected(tech);
				}
			}
			break;
		case RS_LINK_ALIEN:
		case RS_LINK_ALIEN_DEAD:
			/* Use alien=index and RS_LINK_ALIEN (or RS_LINK_ALIEN_DEAD) to get correct amount.*/
			amount = AL_GetAlienAmount(required->idx[i], required->type[i]);
			if (amount > 0) {
				required->collected[i] = amount;
			} else {
				required->collected[i] = 0;
				something_collected_from_each = qfalse;
			}
			break;
		case RS_LINK_ALIEN_GLOBAL:
			amount = AL_CountAll();
			if (amount > 0) {
				required->collected[i] = amount;
			} else {
				required->collected[i] = 0;
				something_collected_from_each = qfalse;
			}
			break;
		case RS_LINK_EVENT:
			break;
		default:
			break;
		}
	}
	return something_collected_from_each;
}

/**
 * @brief Checks if any items have been collected in the current base and correct the values for each requirement.
 * @todo Add support for items in the require_OR list.
 */
void RS_CheckAllCollected (void)
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

		if (RS_CheckCollected(&tech->require_AND) || RS_CheckCollected(&tech->require_OR)) {
			RS_MarkCollected(tech);
		}
	}
}

/**
 * @brief Marks all the techs that can be researched.
 * Automatically researches 'free' techs such as ammo for a weapon.
 * Should be called when a new item is researched (RS_MarkResearched) and after
 * the tree-initialisation (RS_InitTree)
 */
void RS_MarkResearchable (qboolean init)
{
	int i;
	technology_t *tech = NULL;

	/* Set all entries to initial value. */
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		tech->statusResearchable = qfalse;
	}

	/* Update the "amount" and statusCollected values in the requirement-trees. */
	RS_CheckAllCollected();

	for (i = 0; i < gd.numTechnologies; i++) {	/* i = tech-index */
		tech = &gd.technologies[i];
		if (!tech->statusResearchable) { /* In case we loopback we need to check for already marked techs. */
			/* Check for collected items/aliens/etc... */

			if (tech->statusResearch != RS_FINISH) {
				Com_DPrintf("RS_MarkResearchable: handling \"%s\".\n", tech->id);
				/* If required techs are all researched and all other requirements are met, mark this as researchable. */

				/* All requirements are met. */
				if (RS_RequirementsMet(&tech->require_AND, &tech->require_OR)) {
					Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:requirements.\n", tech->id);
					if (init && tech->time <= 0)
						tech->mailSent = MAILSENT_PROPOSAL;
					RS_MarkOneResearchable(tech);
				}

				/* If the tech is a 'free' one (such as ammo for a weapon),
				   mark it as researched and loop back to see if it unlocks
				   any other techs */
				if (tech->statusResearchable && tech->time <= 0) {
					if (init)
						tech->mailSent = MAILSENT_FINISHED;
					RS_ResearchFinish(tech);
					Com_DPrintf("RS_MarkResearchable: automatically researched \"%s\"\n", tech->id);
					/* Restart the loop as this may have unlocked new possibilities. */
					i = 0;
				}
			}
		}
	}
	Com_DPrintf("RS_MarkResearchable: Done.\n");
}

/**
 * @brief Assign required tech/item/etc... IDXs for a single requirements list.
 * @note A function with the same behaviour was formerly also known as RS_InitRequirementList
 */
void RS_AssignTechIdxs (requirements_t *req)
{
	int i;

	for (i = 0; i < req->numLinks; i++) {
		switch (req->type[i]) {
		case RS_LINK_TECH:
			/* Get the index in the techtree. */
			req->idx[i] = RS_GetTechIdxByName(req->id[i]);
			break;
		case RS_LINK_ITEM:
			/* Get index in item-list. */
			req->idx[i] = Com_GetItemByID(req->id[i]);
			break;
		case RS_LINK_EVENT:
			/* @todo: Get index of event in event-list. */
			break;
		case RS_LINK_ALIEN:
		case RS_LINK_ALIEN_DEAD:
			req->idx[i] = AL_GetAlienIdx(req->id[i]);
			break;
#if 0
		case RS_LINK_ALIEN_GLOBAL:
			/* not linked */
			break;
#endif
		default:
			break;
		}
	}
}

/**
 * @brief Assign IDXs to all required techs/items/etc...
 * @note This replaces the RS_InitRequirementList function (since the switch to the _OR and _AND list)
 */
void RS_RequiredIdxAssign (void)
{
	int i;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (&tech->require_AND.numLinks)
			RS_AssignTechIdxs(&tech->require_AND);
		if (&tech->require_OR.numLinks)
			RS_AssignTechIdxs(&tech->require_OR);
		if (&tech->require_for_production.numLinks)
			RS_AssignTechIdxs(&tech->require_for_production);
	}
}

/**
 * @brief Gets all needed names/file-paths/etc... for each technology entry.
 * Should be executed after the parsing of _all_ the ufo files and e.g. the
 * research tree/inventory/etc... are initialised.
 * @todo Add a function to reset ALL research-stati to RS_NONE; -> to be called after start of a new game.
 * @todo Enhance ammo model display (see comment in code).
 * @sa CL_GameInit
 */
void RS_InitTree (void)
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

		for (j = 0; j < tech->markResearched.numDefinitions; j++) {
			if (tech->markResearched.markOnly[j] && !Q_strncmp(tech->markResearched.campaign[j], curCampaign->id, MAX_VAR)) {
				Com_Printf("Mark '%s' as researched\n", tech->id);
				RS_ResearchFinish(tech);
				break;
			}
		}

		/* Save the idx to the id-names of the different requirement-types for quicker access. The id-strings themself are not really needed afterwards :-/ */
		RS_AssignTechIdxs(&tech->require_AND);
		RS_AssignTechIdxs(&tech->require_OR);

		/* Search in correct data/.ufo */
		switch (tech->type) {
		case RS_CRAFTITEM:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A type craftitem needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_NEWS:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A 'type news' item needs to have a 'name\txxx' defined.", tech->id);
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
				if (!Q_strncmp(tech->provides, item->id, MAX_VAR)) {
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
							/* @todo: Add code+structure to display several ammo-types (even reseachable ones). */
							for (k = 0; k < csi.numODs; k++) {
								item_ammo = &csi.ods[k];
								if ( INV_LoadableInWeapon(item_ammo, j) ) {
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
			/* No id found in csi.ods */
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
			/* @todo: Implement me */
			break;
		case RS_LOGIC:
			/* Does not need any additional data. */
			break;

		} /* switch */
	}

	RS_MarkResearchable(qtrue);

	memset(&curRequiredList, 0, sizeof(stringlist_t));

	Com_DPrintf("RS_InitTree: Technology tree initialised. %i entries found.\n", i);
}

/**
 * @brief Return "name" if present, otherwise enter the correct .ufo file and get it from the definition there.
 * @param[in] id unique id of a technology_t
 * @param[out] name Full name of this technology_t (technology_t->name) - defaults to id if nothing is found.
 * @note name has a maxlength of MAX_VAR
 */
void RS_GetName (char *id, char *name)
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
static void RS_ResearchDisplayInfo (void)
{
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
	if (tech->scientists > 0) {
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
}

/**
 * @brief Changes the active research-list entry to the currently selected.
 * See menu_research.ufo for the layout/called functions.
 */
static void CL_ResearchSelect_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: research_select <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength) {
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
void RS_AssignScientist (technology_t* tech)
{
	building_t *building = NULL;
	employee_t *employee = NULL;
	base_t *base = NULL;

	assert(tech);
	Com_DPrintf("RS_AssignScientist: %i | %s \n", tech->idx, tech->name);

	if (tech->base_idx >= 0) {
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
			/* Check the capacity. */
			if (base->capacities[CAP_LABSPACE].max > base->capacities[CAP_LABSPACE].cur) {
				tech->scientists++;			/* Assign a scientist to this tech. */
				tech->base_idx = building->base_idx;	/* Make sure this tech has proper base_idx. */
				base->capacities[CAP_LABSPACE].cur++;	/* Set the amount of currently assigned in capacities. */

				/* Assign the sci to the lab and set number of used lab-space. */
				employee->buildingID = building->idx;
#ifdef DEBUG
				if (base->capacities[CAP_LABSPACE].cur > base->capacities[CAP_LABSPACE].max)
					Com_DPrintf("RS_AssignScientist: more lab-space used (%i) than available (%i) - please investigate.\n", base->capacities[CAP_LABSPACE].cur, base->capacities[CAP_LABSPACE].max);
#endif

			} else {
				MN_Popup(_("Not enough laboratories"), _("No free space in laboratories left.\nBuild more laboratories.\n"));
				return;
			}
		} else {
			MN_Popup(_("Not enough laboratories"), _("No free space in laboratories left.\nBuild more laboratories.\n"));
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
static void RS_AssignScientist_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_add <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
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
static void RS_RemoveScientist (technology_t* tech)
{
	employee_t *employee = NULL;

	assert(tech);

	if (tech->scientists > 0) {
		employee = E_GetAssignedEmployee(&gd.bases[tech->base_idx], EMPL_SCIENTIST);
		if (employee) {
			/* Remove the sci from the tech. */
			tech->scientists--;
			/* Update capacity. */
			gd.bases[tech->base_idx].capacities[CAP_LABSPACE].cur--;
			/* Remove the sci from the lab and set number of used lab-space. */
			employee->buildingID = -1; /* See also E_RemoveEmployeeFromBuilding */
		}
	}

	if (tech->scientists == 0) {
		/* Remove the tech from the base if no scis are left to research it. */
		tech->base_idx = -1;
		tech->statusResearch = RS_PAUSED;
	}
}


/**
 * @brief Assign as many scientists to the research project as possible.
 * @param[in] base The base the tech is researched in.
 * @param[in] tech The technology you want to max out.
 * @sa RS_AssignScientist
 */
static void RS_MaxOutResearch (base_t *base, technology_t* tech)
{
	employee_t *employee = NULL;

	if (!base || !tech)
		return;

	if (tech->scientists <= 0)
		tech->scientists = 0; /* Just in case it's negative. */

	/* Add as many scientists as possible to this tech. */
	do {
		if (base->capacities[CAP_LABSPACE].cur < base->capacities[CAP_LABSPACE].max) {
			employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
			if (employee)
				RS_AssignScientist(tech);
		} else {
			/* No free lab-space left. */
			break;
		}
	} while (employee);
}

/**
 * @brief Script function to remove a scientist from the technology entry in the research-list.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist
 */
static void RS_RemoveScientist_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_remove <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	RS_RemoveScientist(researchList[num]);

	/* Update display-list and display-info. */
	RS_ResearchDisplayInfo();
	RS_UpdateData();
}

/**
 * @brief Starts the research of the selected research-list entry.
 */
static void RS_ResearchStart_f (void)
{
	technology_t *tech = NULL;

	/* We are not in base view. */
	if (!baseCurrent)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* Check if laboratory is available. */
	if (!baseCurrent->hasLab)
		return;

	/* get the currently selected research-item */
	tech = researchList[researchListPos];

	/************
		@todo:
		If there are enough items add them to the tech (i.e. block them from selling or for other research), otherwise pop an errormessage telling the palyer what is missing.
	*/
	if (!tech->statusResearchable) {
		Com_DPrintf("RS_ResearchStart_f: %s was not researchable yet. re-checking\n",tech->id);
		/* If all requiremnts are met (includes a check for "enough-collected") mark this tech as researchable.*/
		if (RS_RequirementsMet(&tech->require_AND, &tech->require_OR))
			RS_MarkOneResearchable(tech);
		RS_MarkResearchable(qfalse);	/* Re-check all other techs in case they depend on the marked one. */
	}
	/************/

	if (tech->statusResearchable) {
		switch (tech->statusResearch) {
		case RS_RUNNING:
			if (tech->base_idx == baseCurrent->idx) {
				/* Research already running in current base ... try to add max amount of scis. */
				RS_MaxOutResearch(baseCurrent, tech);
			}else {
				/* Research already running in another base. */
				MN_Popup(_("Notice"), _("This item is currently under research in another base."));
			}
			break;
		case RS_PAUSED:
		case RS_NONE:
			if (tech->statusResearch == RS_PAUSED) {
				/* MN_Popup(_("Notice"), _("The research on this item continues.")); Removed because it isn't really needed.*/
				Com_Printf("RS_ResearchStart_f: The research on this item continues.\n");
			}
			/* Add as many scientists as possible to this tech. */
			RS_MaxOutResearch(baseCurrent, tech);

			if (tech->scientists > 0) {
				tech->statusResearch = RS_RUNNING;
			}
			break;
		case RS_FINISH:
			/* Should never be executed. */
			MN_Popup(_("Notice"), _("The research on this item is complete."));
			break;
		default:
			break;
		}
	} else
		MN_Popup(_("Notice"), _("The research on this item is not yet possible.\nYou need to research the technologies it's based on first."));

	RS_UpdateData();
}

/**
 * @brief Removes all scientists from the selected research-list entry.
 */
static void RS_ResearchStop_f (void)
{
	technology_t *tech = NULL;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* Check if laboratory is available. */
	if (!baseCurrent->hasLab)
		return;

	/* get the currently selected research-item */
	tech = researchList[researchListPos];

	switch (tech->statusResearch) {
	case RS_RUNNING:
		/* Remove all scis from it and set the status to paused (i.e. it's differnet from a RS_NONE since it may have a little bit of progress already). */
		while (tech->scientists > 0)
			RS_RemoveScientist(tech);
		tech->statusResearch = RS_PAUSED; /* This is redundant since it's done in RS_RemoveScientist aready. But just in case :) */
		break;
	case RS_PAUSED:
		/* @todo: remove? Popup info how much is already researched? */
		/* tech->statusResearch = RS_RUNNING; */
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
 * @brief Switches to the ufopedia entry of the currently selected research entry.
 */
static void RS_ShowPedia_f (void)
{
	technology_t *tech = NULL;

	/* We are not in base view. */
	if (!baseCurrent)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* get the currently selected research-item */

	tech = researchList[researchListPos];
	if (*tech->pre_description) {
		UP_OpenCopyWith(tech->id);
	} else {
		MN_Popup(_("Notice"), _("No research proposal available for this project."));
	}
}

/**
 * @brief Loops trough the research-list and updates the displayed text+color of each research-item according to it's status.
 * @note See menu_research.ufo for the layout/called functions.
 * @todo Display free space in all labs in the current base for each item.
 */
void RS_UpdateData (void)
{
	char name[MAX_VAR];
	int i, j;
	int available[MAX_BASES];
	technology_t *tech = NULL;

	/* Make everything the same (predefined in the ufo-file) color. */
	Cbuf_AddText("research_clear\n");

	for (i = 0; i < gd.numBases; i++) {
		available[i] = E_CountUnassigned(&gd.bases[i], EMPL_SCIENTIST);
	}
	RS_CheckAllCollected();
	RS_MarkResearchable(qfalse);
	for (i = 0, j = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];

		/* Don't show technologies with time == 0 - those are NOT separate research topics. */
		if (tech->time == 0)
			continue;

		Com_sprintf(name, sizeof(name), tech->name);

		/* @todo: add check for collected items */

		if (tech->statusCollected && !tech->statusResearchable && (tech->statusResearch != RS_FINISH)) {
			/* An unresearched collected item that cannot yet be researched. */
			Q_strcat(name, _(" [not yet researchable]"), sizeof(name));
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
			/* @todo: Free space in all labs in this base. */
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

			/* Display the concated text in the correct list-entry.
			 * But embed it in brackets if it isn't researched in the current base. */
			if ((tech->scientists > 0) && (tech->base_idx != baseCurrent->idx)) {
				Com_sprintf(name, sizeof(name), "(%s)", _(name));
			}
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
 * @brief Checks whether there are items in the research list and there is a base
 * otherwise leave the research menu again
 * @note if there is a base but no lab a popup appears
 * @sa RS_UpdateData
 * @sa MN_ResearchInit_f
 */
static void CL_ResearchType_f (void)
{
	/* Update and display the list. */
	RS_UpdateData();

	/* Nothing to research here. */
	if (!researchListLength || !gd.numBases) {
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
		if (!researchListLength)
			MN_Popup(_("Notice"), _("Nothing to research"));
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
 * @sa CL_CheckResearchStatus
 */
static void RS_MarkResearched (technology_t *tech)
{
	RS_ResearchFinish(tech);
	Com_DPrintf("Research of \"%s\" finished.\n", tech->id);
	INV_EnableAutosell(tech);
#if 0
	if (RS_DependsOn(tech->id, id) && (tech->time <= 0) && RS_TechIsResearchable(tech)) {
		RS_ResearchFinish(tech);
		Com_DPrintf("Depending tech \"%s\" has been researched as well.\n", tech->id);
	}
#endif
	RS_MarkResearchable(qfalse);
}

/**
 * @brief Checks the research status
 * @todo Needs to check on the exact time that elapsed since the last check of the status.
 * @sa RS_MarkResearched
 */
void CL_CheckResearchStatus (void)
{
	int i, newResearch = 0;
	technology_t *tech = NULL;

	if (!researchListLength)
		return;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (tech->statusResearch == RS_RUNNING) {
			if ((tech->time > 0) && (tech->scientists >= 0)) {
				Com_DPrintf("timebefore %.2f\n", tech->time);
				Com_DPrintf("timedelta %.2f\n", tech->scientists * 0.8);
				/* @todo: Just for testing, better formular may be needed. */
				tech->time -= tech->scientists * 0.8;
				Com_DPrintf("timeafter %.2f\n", tech->time);
				/* @todo include employee-skill in calculation. */
				/* Will be a good thing (think of percentage-calculation) once non-integer values are used. */
				if (tech->time <= 0) {
					/* Remove all scientists from the technology. */
					while (tech->scientists > 0)
						RS_RemoveScientist(tech);

					RS_MarkResearched(tech);
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
 * @brief Returns a list of technologies for the given type
 * @note this list is terminated by a NULL pointer
 */
static char *RS_TechTypeToName (researchType_t type)
{
	switch(type) {
	case RS_TECH:
		return "tech";
	case RS_WEAPON:
		return "weapon";
	case RS_ARMOR:
		return "armor";
	case RS_CRAFT:
		return "craft";
	case RS_CRAFTITEM:
		return "craftitem";
	case RS_BUILDING:
		return "building";
	case RS_ALIEN:
		return "alien";
	case RS_UGV:
		return "ugv";
	case RS_NEWS:
		return "news";
	case RS_LOGIC:
		return "logic";
	default:
		return "unknown";
	}
}

/**
 * @brief List all parsed technologies and their attributes in commandline/console.
 * Command to call this: techlist
 */
static void RS_TechnologyList_f (void)
{
	int i, j;
	technology_t *tech = NULL;
	requirements_t *req = NULL;

	Com_Printf("#techs: %i\n", gd.numTechnologies);
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_Printf("Tech: %s\n", tech->id);
		Com_Printf("... time      -> %.2f\n", tech->time);
		Com_Printf("... name      -> %s\n", tech->name);
		req = &tech->require_AND;
		Com_Printf("... requires ALL  ->");
		for (j = 0; j < req->numLinks; j++)
			Com_Printf(" %s (%s) %i", req->id[j], RS_TechTypeToName(req->type[j]), req->idx[j]);
		req = &tech->require_OR;
		Com_Printf("\n");
		Com_Printf("... requires ANY  ->");
		for (j = 0; j < req->numLinks; j++)
			Com_Printf(" %s (%s) %i", req->id[j], RS_TechTypeToName(req->type[j]), req->idx[j]);
		Com_Printf("\n");
		Com_Printf("... provides  -> %s", tech->provides);
		Com_Printf("\n");

		Com_Printf("... type      -> ");
		Com_Printf("%s\n", RS_TechTypeToName(tech->type));

		Com_Printf("... researchable -> %i\n", tech->statusResearchable);
		if (tech->statusResearchable) {
			Com_Printf("... researchable date: %02i %02i %i\n", tech->preResearchedDateDay, tech->preResearchedDateMonth,
				tech->preResearchedDateYear);
		}

		Com_Printf("... research  -> ");
		switch (tech->statusResearch) {
		case RS_NONE:
			Com_Printf("nothing\n");
			break;
		case RS_RUNNING:
			Com_Printf("running\n");
			break;
		case RS_PAUSED:
			Com_Printf("paused\n");
			break;
		case RS_FINISH:
			Com_Printf("done\n");
			Com_Printf("... research date: %02i %02i %i\n", tech->researchedDateDay, tech->researchedDateMonth,
				tech->researchedDateYear);
			break;
		default:
			Com_Printf("unknown\n");
			break;
		}
	}
}
#endif /* DEBUG */

/**
 * @brief Research menu init function binding
 * @note Command to call this: research_init
 *
 * @note Should be called whenever the research menu gets active
 * @sa CL_ResearchType_f
 */
static void MN_ResearchInit_f (void)
{
	CL_ResearchType_f();
}

/**
 * @brief Mark everything as researched
 * @sa CL_Connect_f
 * @sa MN_StartServer
 */
void RS_MarkResearchedAll (void)
{
	int i;

	for (i = 0; i < gd.numTechnologies; i++) {
		Com_DPrintf("...mark %s as researched\n", gd.technologies[i].id);
		RS_MarkOneResearchable(&gd.technologies[i]);
		RS_ResearchFinish(&gd.technologies[i]);
		/* @todo: Set all "collected" entries in the requirements to the "amount" value. */
	}
}

#ifdef DEBUG
/**
 * @brief Set all item to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchAll (void)
{
	technology_t *tech = NULL;

	if (Cmd_Argc() != 2) {
		RS_MarkResearchedAll();
	} else {
		tech= RS_GetTechByID(Cmd_Argv(1));
		Com_DPrintf("...mark %s as researched\n", tech->id);
		RS_MarkOneResearchable(tech);
		RS_ResearchFinish(tech);
	}
}

/**
 * @brief Set all item to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchableAll (void)
{
	int i;
	technology_t *tech = NULL;

	if (Cmd_Argc() != 2) {
		for (i = 0; i < gd.numTechnologies; i++) {
			tech = &gd.technologies[i];
			Com_Printf("...mark %s as researchable\n", tech->id);
			RS_MarkOneResearchable(tech);
			RS_MarkCollected(tech);
		}
	} else {
		tech = RS_GetTechByID(Cmd_Argv(1));
		if (tech) {
			Com_Printf("...mark %s as researchable\n", tech->id);
			RS_MarkOneResearchable(tech);
			RS_MarkCollected(tech);
		}
	}
}
#endif

/**
 * @brief Opens UFOpedia by clicking dependency list
 */
static void RS_DependenciesClick_f (void)
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
extern void RS_ResetResearch (void)
{
	researchListLength = 0;
	/* add commands and cvars */
	Cmd_AddCommand("research_init", MN_ResearchInit_f, "Research menu init function binding");
	Cmd_AddCommand("research_select", CL_ResearchSelect_f, "Update current selection with the one that has been clicked");
	Cmd_AddCommand("research_type", CL_ResearchType_f, "Switch between different research types");
	Cmd_AddCommand("mn_start_research", RS_ResearchStart_f, "Start the research of the selected entry");
	Cmd_AddCommand("mn_stop_research", RS_ResearchStop_f, "Pause the research of the selected entry");
	Cmd_AddCommand("mn_show_ufopedia", RS_ShowPedia_f, "Show the entry in the ufopedia for the selected research topic");
	Cmd_AddCommand("mn_rs_add", RS_AssignScientist_f, "Assign one scientist to this entry");
	Cmd_AddCommand("mn_rs_remove", RS_RemoveScientist_f, "Remove one scientist from this entry");
	Cmd_AddCommand("research_update", RS_UpdateData, NULL);
	Cmd_AddCommand("invlist", Com_InventoryList_f, NULL);
	Cmd_AddCommand("research_dependencies_click", RS_DependenciesClick_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("techlist", RS_TechnologyList_f, NULL);
	Cmd_AddCommand("research_all", RS_DebugResearchAll, NULL);
	Cmd_AddCommand("researchable_all", RS_DebugResearchableAll, NULL);
#endif
}

/**
 * @brief This i called everytime RS_ParseTechnologies i called - to prevent cyclic hash tables
 */
extern void RS_ResetHash (void)
{
	/* they are static - but i'm paranoid - this is called before the techs were parsed */
	memset(tech_hash, 0, sizeof(tech_hash));
}

/**
 * @brief The valid definition names in the research.ufo file.
 */
static const value_t valid_tech_vars[] = {
	/*name of technology */
	{"name", V_TRANSLATION2_STRING, offsetof(technology_t, name)},
	{"description", V_TRANSLATION2_STRING, offsetof(technology_t, description)},
	{"pre_description", V_TRANSLATION2_STRING, offsetof(technology_t, pre_description)},
	/*what does this research provide */
	{"provides", V_STRING, offsetof(technology_t, provides)},
	/* ("require_AND")	Handled in parser below. */
	/* ("require_OR")	Handled in parser below. */
	/* ("up_chapter")	Handled in parser below. */
	{"delay", V_INT, offsetof(technology_t, delay)},
	{"producetime", V_INT, offsetof(technology_t, produceTime)},
	{"time", V_FLOAT, offsetof(technology_t, time)},
	{"pushnews", V_BOOL, offsetof(technology_t, pushnews)},
	{"image_top", V_STRING, offsetof(technology_t, image_top)},
	{"image_bottom", V_STRING, offsetof(technology_t, image_bottom)},
	{"mdl_top", V_STRING, offsetof(technology_t, mdl_top)},
	{"mdl_bottom", V_STRING, offsetof(technology_t, mdl_bottom)},

	{NULL, 0, 0}
};

/**
 * @brief The valid definition names in the research.ufo file for tech mails
 */
static const value_t valid_techmail_vars[] = {
	{"from", V_TRANSLATION2_STRING, offsetof(techMail_t, from)},
	{"to", V_TRANSLATION2_STRING, offsetof(techMail_t, to)},
	{"subject", V_TRANSLATION2_STRING, offsetof(techMail_t, subject)},
	{"date", V_TRANSLATION2_STRING, offsetof(techMail_t, date)},

	{NULL, 0, 0}
};

/**
 * @brief Parses one "tech" entry in the research.ufo file and writes it into the next free entry in technologies (technology_t).
 * @param[in] id Unique id of a technology_t. This is parsed from "tech xxx" -> id=xxx
 * @param[in] text the whole following text that is part of the "tech" item definition in research.ufo.
 */
extern void RS_ParseTechnologies (const char *name, char **text)
{
	const value_t *var = NULL;
	technology_t *tech = NULL;
	unsigned hash;
	int tech_old;
	const char *errhead = "RS_ParseTechnologies: unexpected end of file.";
	char *token = NULL;
	requirements_t *required_temp = NULL;

	int i;

	/* get body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("RS_ParseTechnologies: \"%s\" technology def without body ignored.\n", name);
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
	Q_strncpyz(tech->id, name, sizeof(tech->id));
	hash = Com_HashKey(tech->id, TECH_HASH_SIZE);
	Q_strncpyz(tech->description, _("No description available."), sizeof(tech->description));

	/* link the variable in */
	/* tech_hash should be null on the first run */
	tech->hash_next = tech_hash[hash];
	/* set the tech_hash pointer to the current tech */
	/* if there were already others in tech_hash at position hash, they are now
	 * accessable via tech->next - loop until tech->next is null (the first tech
	 * at that position)
	 */
	tech_hash[hash] = tech;

	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;
	tech->time = 0;
	tech->overalltime = 0;
	tech->scientists = 0;
	tech->prev = -1;
	tech->next = -1;
	tech->base_idx = -1;
	tech->up_chapter = -1;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		/* get values */
		if (!Q_strncmp(token, "type", MAX_VAR)) {
			/* what type of tech this is */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			/* redundant, but oh well. */
			if (!Q_strncmp(token, "tech", MAX_VAR))
				tech->type = RS_TECH;
			else if (!Q_strncmp(token, "weapon", MAX_VAR))
				tech->type = RS_WEAPON;
			else if (!Q_strncmp(token, "news", MAX_VAR))
				tech->type = RS_NEWS;
			else if (!Q_strncmp(token, "armor", MAX_VAR))
				tech->type = RS_ARMOR;
			else if (!Q_strncmp(token, "craft", MAX_VAR))
				tech->type = RS_CRAFT;
			else if (!Q_strncmp(token, "craftitem", MAX_VAR))
				tech->type = RS_CRAFTITEM;
			else if (!Q_strncmp(token, "building", MAX_VAR))
				tech->type = RS_BUILDING;
			else if (!Q_strncmp(token, "alien", MAX_VAR))
				tech->type = RS_ALIEN;
			else if (!Q_strncmp(token, "ugv", MAX_VAR))
				tech->type = RS_UGV;
			else if (!Q_strncmp(token, "logic", MAX_VAR))
				tech->type = RS_LOGIC;
			else
				Com_Printf("RS_ParseTechnologies: \"%s\" unknown techtype: \"%s\" - ignored.\n", name, token);
		} else {

			if ((!Q_strncmp(token, "require_AND", MAX_VAR))
			 || (!Q_strncmp(token, "require_OR", MAX_VAR))
			 || (!Q_strncmp(token, "require_for_production", MAX_VAR))) {
				/* Link to correct list. */
				if (!Q_strncmp(token, "require_AND", MAX_VAR)) {
					required_temp = &tech->require_AND;
				} else if (!Q_strncmp(token, "require_OR", MAX_VAR)) {
					required_temp = &tech->require_OR;
				} else { /* It's "require_for_production" */
					required_temp = &tech->require_for_production;
				}

				token = COM_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;
				if (*token == '}')
					break;

				do {	/* Loop through all 'require' entries.*/
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token == '}')
						break;

					if (!Q_strncmp(token, "tech", MAX_VAR)) {
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							required_temp->type[required_temp->numLinks] = RS_LINK_TECH;

							/* Set requirement-name (id). */
							token = COM_Parse(text);
							required_temp->id[required_temp->numLinks] = CL_ClientHunkUse(token, MAX_VAR);

							Com_DPrintf("RS_ParseTechnologies: require-tech - %s\n", required_temp->id[required_temp->numLinks]);

							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (!Q_strncmp(token, "item", MAX_VAR)) {
						/* Defines what items need to be collected for this item to be researchable. */
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							required_temp->type[required_temp->numLinks] = RS_LINK_ITEM;
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							required_temp->id[required_temp->numLinks] = CL_ClientHunkUse(token, MAX_VAR);
							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							required_temp->amount[required_temp->numLinks] = atoi(token);
							Com_DPrintf("RS_ParseTechnologies: require-item - %s - %i\n", required_temp->id[required_temp->numLinks], required_temp->amount[required_temp->numLinks]);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (!Q_strncmp(token, "event", MAX_VAR)) {
						token = COM_Parse(text);
						Com_DPrintf("RS_ParseTechnologies: require-event - %s\n", token);
						required_temp->type[required_temp->numLinks] = RS_LINK_EVENT;
						/* Get name/id & amount of required item. */
						/* @todo: Implement final event system, so this can work 100% */
					} else if (!Q_strncmp(token, "alienglobal", MAX_VAR)) {
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							required_temp->type[required_temp->numLinks] = RS_LINK_ALIEN_GLOBAL;
							Com_DPrintf("RS_ParseTechnologies:  require-alienglobal - %i\n", required_temp->amount[required_temp->numLinks]);

							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							required_temp->amount[required_temp->numLinks] = atoi(token);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if ( (!Q_strncmp(token, "alien_dead", MAX_VAR)) ||  (!Q_strncmp(token, "alien", MAX_VAR)) ) { /* Does this only check the beginning of the string? */
						/* Defines what live or dead aliens need to be collected for this item to be researchable. */
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (!Q_strncmp(token, "alien_dead", MAX_VAR)) {
								required_temp->type[required_temp->numLinks] = RS_LINK_ALIEN_DEAD;
								Com_DPrintf("RS_ParseTechnologies:  require-alien dead - %s - %i\n", required_temp->id[required_temp->numLinks], required_temp->amount[required_temp->numLinks]);
							} else {
								required_temp->type[required_temp->numLinks] = RS_LINK_ALIEN;
								Com_DPrintf("RS_ParseTechnologies:  require-alien alive - %s - %i\n", required_temp->id[required_temp->numLinks], required_temp->amount[required_temp->numLinks]);
							}
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							required_temp->id[required_temp->numLinks] = CL_ClientHunkUse(token, MAX_VAR);
							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							required_temp->amount[required_temp->numLinks] = atoi(token);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else {
						Com_Printf("RS_ParseTechnologies: \"%s\" unknown requirement-type: \"%s\" - ignored.\n", name, token);
					}
				} while (*text);
			} else if (!Q_strncmp(token, "up_chapter", MAX_VAR)) {
				/* ufopedia chapter */
				token = COM_EParse(text, errhead, name);
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
							Com_Printf("RS_ParseTechnologies: \"%s\" - chapter \"%s\" not found.\n", name, token);
					}
				}
			} else if (!Q_strncmp(token, "mail", 4)) { /* also mail_pre */
				techMail_t* mail;

				/* how many mails found for this technology
				 * used in ufopedia to check which article to display */
				tech->numTechMails++;

				if (tech->numTechMails > TECHMAIL_MAX)
					Com_Printf("RS_ParseTechnologies: more techmail-entries found than supported. \"%s\"\n",  name);

				if (!Q_strncmp(token, "mail_pre", 8)) {
					mail = &tech->mail[TECHMAIL_PRE];
				} else {
					mail = &tech->mail[TECHMAIL_RESEARCHED];
				}
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{')
					return;

				/* grab the initial mail entry */
				token = COM_EParse(text, errhead, name);
				if (!*text || *token == '}')
					return;
				do {
					for (var = valid_techmail_vars; var->string; var++)
						if (!Q_strncmp(token, var->string, sizeof(var->string))) {
							/* found a definition */
							token = COM_EParse(text, errhead, name);
							if (!*text)
								return;

							if (var->type != V_NULL)
								Com_ParseValue(mail, token, var->type, var->ofs);
							else
								/* NOTE: do we need a buffer here? for saving or something like that? */
								Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed (token: '%s') entry: '%s' - offset: %Zu\n", token, name, var->ofs);
							break;
						}
					/* grab the next entry */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
				} while (*text && *token != '}');
			} else {
				for (var = valid_tech_vars; var->string; var++)
					if (!Q_strncmp(token, var->string, sizeof(var->string))) {
						/* found a definition */
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;

						if (var->ofs && var->type != V_NULL)
							Com_ParseValue(tech, token, var->type, var->ofs);
						else
							/* NOTE: do we need a buffer here? for saving or something like that? */
							Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed (token: '%s') entry: '%s'\n", token, name);
						break;
					}
				/*@todo: escape "type weapon/tech/etc.." here */
				if (!var->string)
					Com_Printf("RS_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n", token, name);
			}
		}
	} while (*text);

	/* set the overall reseach time to the one given in the ufo-file. */
	tech->overalltime = tech->time;
}

/**
 * @brief Checks whether an item is already researched
 * @sa RS_IsResearched_ptr
 */
qboolean RS_IsResearched_idx (int idx)
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
qboolean RS_IsResearched_ptr (technology_t * tech)
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
qboolean RS_ItemIsResearched (const char *id_provided)
{
	technology_t *tech = NULL;

	/* in multiplaer everyting is researched */
	if (!ccs.singleplayer)
		return qtrue;

	tech = RS_GetTechByProvided(id_provided);
	if (!tech)
		return qtrue;

	return RS_IsResearched_ptr(tech);
}

/**
 * @brief
 * @sa RS_ItemCollected
 * Call this function if you already hold a tech pointer.
 */
int RS_Collected_ (technology_t * tech)
{
	if (tech)
		return tech->statusCollected;

	Com_DPrintf("RS_Collected_: NULL technology given.\n");
	return -1;
}

/**
 * @brief Checks if the technology (tech-id) has been researched.
 * @param[in] tech_idx index of the technology.
 * @return qboolean Returns qtrue if the technology has been researched, otherwise qfalse;
 */
qboolean RS_TechIsResearched (int tech_idx)
{
	if (tech_idx < 0)
		return qfalse;

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
qboolean RS_TechIsResearchable (technology_t * tech)
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

/**
 * @brief Returns a list of .ufo items that are produceable when this item has been researched (=provided)
 * This list also incldues other items that "require" this one (id) and have a reseach_time of 0.
 */
#if 0
void RS_GetProvided (char *id, char *provided)
{
	int i, j;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!Q_strncmp(id, tech->id, MAX_VAR)) {
			for (j = 0; j < MAX_TECHLINKS; j++)
				Com_sprintf(provided[j], MAX_VAR, tech->provides);
			/*@todo: search for dependent items. */
			for (j = 0; j < gd.numTechnologies; j++) {
				if (RS_DependsOn(tech->id, id)) {
					/* @todo: append researchtree[j]->provided to *provided */
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
technology_t* RS_GetTechByIDX (int tech_idx)
{
	if (tech_idx < 0 || tech_idx >= gd.numTechnologies)
		return NULL;
	else
		return &gd.technologies[tech_idx];
}


/**
 * @brief return a pointer to the technology identified by given id string
 */
technology_t *RS_GetTechByID (const char *id)
{
	unsigned hash;
	technology_t *tech;

	if (!id || !*id)
		return NULL;

	if (!Q_strncmp((char *) id, "nothing", MAX_VAR))
		return NULL;

	hash = Com_HashKey(id, TECH_HASH_SIZE);
	for (tech = tech_hash[hash]; tech; tech = tech->hash_next)
		if (!Q_stricmp(id, tech->id))
			return tech;

	Com_Printf("RS_GetTechByID: Could not find a technology with id \"%s\"\n", id);
	return NULL;
}

/**
 * @brief returns a pointer to the item tech (as listed in "provides")
 */
technology_t *RS_GetTechByProvided (const char *id_provided)
{
	int i;
#if 0
	unsigned hash;

	hash = Com_HashKey(id_provided, TECH_HASH_SIZE);
	/* @todo */
#endif
	for (i = 0; i < gd.numTechnologies; i++)
		if (!Q_strncmp((char *) id_provided, gd.technologies[i].provides, MAX_VAR))
			return &gd.technologies[i];

	/* if a building, probably needs another building */
	/* if not a building, catch NULL where function is called! */
	return NULL;
}

#if 0
/* Not really used anywhere, but i;ll just leave it in here if it is needed again*/
 /**
 * @brief Returns a list of technologies for the given type
 * @note This list is terminated by a NULL pointer.
* @sa * AC_GetTechsByType
 */
technology_t **RS_GetTechsByType (researchType_t type)
{
	static technology_t *techList[MAX_TECHNOLOGIES];
	int i, j = 0;

	for (i = 0; i < gd.numTechnologies; i++) {
		if (gd.technologies[i].type == type) {
			techList[j] = &gd.technologies[i];
			j++;
			/* j+1 because last item have to be NULL */
			if (j + 1 >= MAX_TECHNOLOGIES) {
				Com_Printf("RS_GetTechsByType: MAX_TECHNOLOGIES limit hit\n");
				break;
			}
		}
	}
	techList[j] = NULL;
	Com_DPrintf("techlist with %i entries\n", j);
	return techList;
}
#endif


/**
 * @brief Searches for the technology that has the most scientists assigned in a given base.
 * @param[in] base_idx In what base the tech shoudl be researched.
 * @sa E_RemoveEmployeeFromBuilding
 */
technology_t *RS_GetTechWithMostScientists (int base_idx)
{
	technology_t *tech = NULL;
	technology_t *tech_temp = NULL;
	int i = 0;
	int max = 0;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech_temp = &gd.technologies[i];
		if ((tech_temp->statusResearch == RS_RUNNING) && (tech_temp->base_idx == base_idx)) {
			if (tech_temp->scientists > max) {
				tech = tech_temp;
				max = tech->scientists;
			}
		}
	}

	return tech;
}

/**
 * @brief Returns the index (idx) of a "tech" entry given it's name.
 * @param[in] name the name of the tech
 * @todo: this method is extremely inefficient... it could be dramatically improved
 */
int RS_GetTechIdxByName (const char *name)
{
	technology_t *tech;
	unsigned hash;

	/* return -1 if tech matches "nothing" */
	if (!Q_strncmp(name, "nothing", MAX_VAR))
		return -1;

	hash = Com_HashKey(name, TECH_HASH_SIZE);
	for (tech = tech_hash[hash]; tech; tech = tech->hash_next)
		if (!Q_stricmp(name, tech->id))
			return tech->idx;

	Com_Printf("RS_GetTechIdxByName: Could not find tech '%s'\n", name);
	/* return -1 if not found */
	return -1;
}

/**
 * @brief
 */
extern qboolean RS_Save (sizebuf_t* sb, void* data)
{
	int i, j;
	technology_t *t;

	MSG_WriteShort(sb, gd.numTechnologies);
	for (i = 0; i < gd.numTechnologies; i++) {
		t = &gd.technologies[i];
		MSG_WriteString(sb, t->id);
		MSG_WriteByte(sb, t->statusCollected);
		MSG_WriteFloat(sb, t->time);
		MSG_WriteByte(sb, t->statusResearch);
		MSG_WriteShort(sb, t->base_idx); /* may be -1 */
		MSG_WriteByte(sb, t->scientists);
		MSG_WriteByte(sb, t->statusResearchable);
		MSG_WriteShort(sb, t->preResearchedDateDay);
		MSG_WriteShort(sb, t->preResearchedDateMonth);
		MSG_WriteShort(sb, t->preResearchedDateYear);
		MSG_WriteShort(sb, t->researchedDateDay);
		MSG_WriteShort(sb, t->researchedDateMonth);
		MSG_WriteShort(sb, t->researchedDateYear);
		MSG_WriteByte(sb, t->mailSent);
		for (j = 0; j < TECHMAIL_MAX; j++) {
			/* only save the already read mails */
			MSG_WriteByte(sb, j);
			MSG_WriteByte(sb, t->mail[j].read);
		}
	}

	return qtrue;
}

/**
 * @brief
 */
extern qboolean RS_Load (sizebuf_t* sb, void* data)
{
	int i, j, k;
	technology_t *t;
	const char *techString;
	techMailType_t mailType;

	j = MSG_ReadShort(sb);
	if (j != gd.numTechnologies)
		Com_Printf("......different amount of technologies found - resave this game %i, %i\n", j, gd.numTechnologies);
	for (i = 0; i < j; i++) {
		techString = MSG_ReadString(sb);
		t = RS_GetTechByID(techString);
		if (!t) {
			Com_Printf("......your game doesn't know anything about tech '%s'\n", techString);
			/* We now read dummy data to skip the unknown tech. */
			MSG_ReadByte(sb);	/* statusCollected */
			MSG_ReadFloat(sb);	/* time */
			MSG_ReadByte(sb);	/* statusResearch */
			MSG_ReadShort(sb);	/* base_idx */
			MSG_ReadByte(sb);	/* scientists */
			MSG_ReadByte(sb);	/* statusResearchable */
			MSG_ReadShort(sb);	/* preResearchedDateDay */
			MSG_ReadShort(sb);	/* preResearchedDateMonth */
			MSG_ReadShort(sb);	/* preResearchedDateYear */
			MSG_ReadShort(sb);	/* researchedDateDay */
			MSG_ReadShort(sb);	/* researchedDateMonth */
			MSG_ReadShort(sb);	/* researchedDateYear */
			MSG_ReadByte(sb);	/* mailSent */
			for (k = 0; k < TECHMAIL_MAX; k++) {
				MSG_ReadByte(sb);	/* mailType */
				MSG_ReadByte(sb);	/* t->mail[mailType].read */
			}
			continue;
		}
		t->statusCollected = MSG_ReadByte(sb);
		t->time = MSG_ReadFloat(sb);
		t->statusResearch = MSG_ReadByte(sb);
		t->base_idx = MSG_ReadShort(sb);
		t->scientists = MSG_ReadByte(sb);
		t->statusResearchable = MSG_ReadByte(sb);
		t->preResearchedDateDay = MSG_ReadShort(sb);
		t->preResearchedDateMonth = MSG_ReadShort(sb);
		t->preResearchedDateYear = MSG_ReadShort(sb);
		t->researchedDateDay = MSG_ReadShort(sb);
		t->researchedDateMonth = MSG_ReadShort(sb);
		t->researchedDateYear = MSG_ReadShort(sb);
		t->mailSent = MSG_ReadByte(sb);
		for (k = 0; k < TECHMAIL_MAX; k++) {
			mailType = MSG_ReadByte(sb);
			t->mail[mailType].read = MSG_ReadByte(sb);
		}
	}

	return qtrue;
}

