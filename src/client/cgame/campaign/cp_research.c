/**
 * @file cp_research.c
 * @brief Technology research.
 *
 * Handles everything related to the research-tree.
 * Provides information if items/buildings/etc.. can be researched/used/displayed etc...
 * Implements the research-system (research new items/etc...)
 * See base/ufos/research.ufo and base/ufos/ui/research.ufo for the underlying content.
 * @todo comment on used global variables.
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
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_research.h"
#include "cp_popup.h"
#include "cp_time.h"
#include "save/save_research.h"

#define TECH_HASH_SIZE 64
static technology_t *techHash[TECH_HASH_SIZE];
static technology_t *techHashProvided[TECH_HASH_SIZE];

/**
 * @brief Sets a technology status to researched and updates the date.
 * @param[in] tech The technology that was researched.
 */
void RS_ResearchFinish (technology_t* tech)
{
	/* Remove all scientists from the technology. */
	RS_StopResearch(tech);

	/** At this point we define what research-report description is used when displayed. (i.e. "usedDescription" is set here).
	 * That's because this is the first the player will see the research result
	 * and any later changes may make the content inconsistent for the player.
	 * @sa RS_MarkOneResearchable */
	RS_GetDescription(&tech->description);
	if (tech->preDescription.usedDescription < 0) {
		/* For some reason the research proposal description was not set at this point - we just make sure it _is_ set. */
		RS_GetDescription(&tech->preDescription);
	}

	/* execute the trigger only if the tech is not yet researched */
	if (tech->finishedResearchEvent && tech->statusResearch != RS_FINISH)
		Cmd_ExecuteString(tech->finishedResearchEvent);

	tech->statusResearch = RS_FINISH;
	tech->researchedDate = ccs.date;
	if (!tech->statusResearchable) {
		tech->statusResearchable = qtrue;
		tech->preResearchedDate = ccs.date;
	}

	/* send a new message and add it to the mailclient */
	if (tech->mailSent < MAILSENT_FINISHED && tech->type != RS_LOGIC) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A research project has been completed: %s\n"), _(tech->name));
		MSO_CheckAddNewMessage( NT_RESEARCH_COMPLETED, _("Research finished"), cp_messageBuffer, qfalse, MSG_RESEARCH_FINISHED, tech);
		tech->mailSent = MAILSENT_FINISHED;
	}
}

/**
 * @brief Stops a research (Removes scientists from it)
 * @param[in] tech The technology that is being researched.
 */
void RS_StopResearch (technology_t* tech)
{
	assert(tech);
	while (tech->scientists > 0)
		RS_RemoveScientist(tech, NULL);
}

/**
 * @brief Marks one tech as researchable.
 * @param tech The technology to be marked.
 * @sa RS_MarkCollected
 * @sa RS_MarkResearchable
 */
void RS_MarkOneResearchable (technology_t* tech)
{
	if (!tech)
		return;

	Com_DPrintf(DEBUG_CLIENT, "RS_MarkOneResearchable: \"%s\" marked as researchable.\n", tech->id);

	/* Don't do anything for not researchable techs. */
	if (tech->time == -1)
		return;

	/* Don't send mail for automatically completed techs. */
	if (tech->time == 0)
		tech->mailSent = MAILSENT_FINISHED;

	/** At this point we define what research proposal description is used when displayed. (i.e. "usedDescription" is set here).
	 * That's because this is the first the player will see anything from
	 * the tech and any later changes may make the content (proposal) of the tech inconsistent for the player.
	 * @sa RS_ResearchFinish */
	RS_GetDescription(&tech->preDescription);
	/* tech->description is checked before a research is finished */

	if (tech->mailSent < MAILSENT_PROPOSAL) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("New research proposal: %s\n"), _(tech->name));
		MSO_CheckAddNewMessage(NT_RESEARCH_PROPOSED, _("Unknown Technology researchable"), cp_messageBuffer, qfalse, MSG_RESEARCH_PROPOSAL, tech);
		tech->mailSent = MAILSENT_PROPOSAL;
	}

	tech->statusResearchable = qtrue;

	/* only change the date if it wasn't set before */
	if (tech->preResearchedDate.day == 0) {
		tech->preResearchedDate = ccs.date;
	}
}

/**
 * @brief Checks if all requirements of a tech have been met so that it becomes researchable.
 * @note If there are NO requirements defined at all it will always return true.
 * @param[in] requiredAND Pointer to a list of AND-related requirements.
 * @param[in] requiredOR Pointer to a list of OR-related requirements.
 * @param[in] base In what base to check the "collected" items etc..
 * @return @c true if all requirements are satisfied otherwise @c false.
 * @todo Add support for the "delay" value.
 */
qboolean RS_RequirementsMet (const requirements_t *requiredAND, const requirements_t *requiredOR, const base_t *base)
{
	int i;
	qboolean metAND = qfalse;
	qboolean metOR = qfalse;

	if (!requiredAND && !requiredOR) {
		Com_Printf("RS_RequirementsMet: No requirement list(s) given as parameter.\n");
		return qfalse;
	}

	/* If there are no requirements defined at all we have 'met' them by default. */
	if (requiredAND->numLinks == 0 && requiredOR->numLinks == 0) {
		Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: No requirements set for this tech. They are 'met'.\n");
		return qtrue;
	}

	if (requiredAND->numLinks) {
		metAND = qtrue;
		for (i = 0; i < requiredAND->numLinks; i++) {
			const requirement_t *req = &requiredAND->links[i];
			switch (req->type) {
			case RS_LINK_TECH:
				if (!RS_IsResearched_ptr(req->link.tech))
					metAND = qfalse;
				break;
			case RS_LINK_TECH_NOT:
				if (RS_IsResearched_ptr(req->link.tech))
					metAND = qfalse;
				break;
			case RS_LINK_ITEM:
				/* The same code is used in "PR_RequirementsMet" */
				if (!base || B_ItemInBase(req->link.od, base) < req->amount)
					metAND = qfalse;
				break;
			case RS_LINK_ALIEN_DEAD:
			case RS_LINK_ALIEN:
				if (!base || AL_GetAlienAmount(req->link.td, req->type, base) < req->amount)
					metAND = qfalse;
				break;
			case RS_LINK_ALIEN_GLOBAL:
				if (AL_CountAll() < req->amount)
					metAND = qfalse;
				break;
			case RS_LINK_UFO:
				if (US_UFOsInStorage(req->link.aircraft, NULL) < req->amount)
					metAND = qfalse;
				break;
			case RS_LINK_ANTIMATTER:
				if (!base || B_AntimatterInBase(base) < req->amount)
					metAND = qfalse;
				break;
			default:
				break;
			}

			if (!metAND)
				break;
		}
	}

	if (requiredOR->numLinks)
		for (i = 0; i < requiredOR->numLinks; i++) {
			const requirement_t *req = &requiredOR->links[i];
			switch (req->type) {
			case RS_LINK_TECH:
				if (RS_IsResearched_ptr(req->link.tech))
					metOR = qtrue;
				break;
			case RS_LINK_TECH_NOT:
				if (!RS_IsResearched_ptr(req->link.tech))
					metOR = qtrue;
				break;
			case RS_LINK_ITEM:
				/* The same code is used in "PR_RequirementsMet" */
				if (base && B_ItemInBase(req->link.od, base) >= req->amount)
					metOR = qtrue;
				break;
			case RS_LINK_ALIEN:
			case RS_LINK_ALIEN_DEAD:
				if (base && AL_GetAlienAmount(req->link.td, req->type, base) >= req->amount)
					metOR = qtrue;
				break;
			case RS_LINK_ALIEN_GLOBAL:
				if (AL_CountAll() >= req->amount)
					metOR = qtrue;
				break;
			case RS_LINK_UFO:
				if (US_UFOsInStorage(req->link.aircraft, NULL) >= req->amount)
					metOR = qtrue;
				break;
			case RS_LINK_ANTIMATTER:
				if (base && B_AntimatterInBase(base) >= req->amount)
					metOR = qtrue;
				break;
			default:
				break;
			}

			if (metOR)
				break;
		}
	Com_DPrintf(DEBUG_CLIENT, "met_AND is %i, met_OR is %i\n", metAND, metOR);

	return (metAND || metOR);
}

/**
 * @brief returns the currently used description for a technology.
 * @param[in] desc A list of possible descriptions (with tech-links that decide which one is the correct one)
 */
const char *RS_GetDescription (descriptions_t *desc)
{
	int i;

	/* Return (unparsed) default description (0) if nothing is defined.
	 * it is _always_ set, even if numDescriptions is zero. See RS_ParseTechnologies (standard values). */
	if (desc->numDescriptions == 0)
		return desc->text[0];

	/* Return already used description if it's defined. */
	if (desc->usedDescription >= 0)
		return desc->text[desc->usedDescription];

	/* Search for useable description text (first match is returned => order is important)
	 * The default (0) entry is skipped here. */
	for (i = 1; i < desc->numDescriptions; i++) {
		const technology_t *tech = RS_GetTechByID(desc->tech[i]);
		if (!tech)
			continue;

		if (RS_IsResearched_ptr(tech)) {
			desc->usedDescription = i;	/**< Stored used description */
			return desc->text[i];
		}
	}

	return desc->text[0];
}

/**
 * @brief Marks a give technology as collected
 * @sa CP_AddItemAsCollected_f
 * @sa MS_AddNewMessage
 * @sa RS_MarkOneResearchable
 * @param[in] tech The technology pointer to mark collected
 */
void RS_MarkCollected (technology_t* tech)
{
	assert(tech);

	if (tech->time == 0) /* Don't send mail for automatically completed techs. */
		tech->mailSent = MAILSENT_FINISHED;

	if (tech->mailSent < MAILSENT_PROPOSAL) {
		if (tech->statusResearch < RS_FINISH) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("New research proposal: %s\n"), _(tech->name));
			MSO_CheckAddNewMessage(NT_RESEARCH_PROPOSED, _("Unknown Technology found"), cp_messageBuffer, qfalse, MSG_RESEARCH_PROPOSAL, tech);
		}
		tech->mailSent = MAILSENT_PROPOSAL;
	}

	/* only change the date if it wasn't set before */
	if (tech->preResearchedDate.day == 0) {
		tech->preResearchedDate = ccs.date;
	}

	tech->statusCollected = qtrue;
}

/**
 * @brief Marks all the techs that can be researched.
 * Automatically researches 'free' techs such as ammo for a weapon. Not "researchable"-related.
 * Should be called when a new item is researched (RS_MarkResearched) and after
 * the tree-initialisation (RS_InitTree)
 * @sa RS_MarkResearched
 */
void RS_MarkResearchable (qboolean init, const base_t* base)
{
	int i;
	const base_t *thisBase = base;

	/* Set all entries to initial value. */
	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		tech->statusResearchable = qfalse;
	}

	for (i = 0; i < ccs.numTechnologies; i++) {	/* i = tech-index */
		technology_t *tech = RS_GetTechByIDX(i);
		if (!tech->statusResearchable) { /* In case we loopback we need to check for already marked techs. */
			/* Check for collected items/aliens/etc... */
			if (tech->statusResearch != RS_FINISH) {
				Com_DPrintf(DEBUG_CLIENT, "RS_MarkResearchable: handling \"%s\".\n", tech->id);
				/* If required techs are all researched and all other requirements are met, mark this as researchable. */

				if (tech->base)
					base = tech->base;
				else
					base = thisBase;

				/* All requirements are met. */
				if (RS_RequirementsMet(&tech->requireAND, &tech->requireOR, base)) {
					Com_DPrintf(DEBUG_CLIENT, "RS_MarkResearchable: \"%s\" marked researchable. reason:requirements.\n", tech->id);
					if (init && tech->time == 0)
						tech->mailSent = MAILSENT_PROPOSAL;
					RS_MarkOneResearchable(tech);
				}

				/* If the tech is a 'free' one (such as ammo for a weapon),
				 * mark it as researched and loop back to see if it unlocks
				 * any other techs */
				if (tech->statusResearchable && tech->time == 0) {
					if (init)
						tech->mailSent = MAILSENT_FINISHED;
					RS_ResearchFinish(tech);
					Com_DPrintf(DEBUG_CLIENT, "RS_MarkResearchable: automatically researched \"%s\"\n", tech->id);
					/* Restart the loop as this may have unlocked new possibilities. */
					i = -1;
				}
			}
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "RS_MarkResearchable: Done.\n");
}

/**
 * @brief Assign required tech/item/etc... pointers for a single requirements list.
 * @note A function with the same behaviour was formerly also known as RS_InitRequirementList
 */
static void RS_AssignTechLinks (requirements_t *reqs)
{
	int i;
	requirement_t *req;

	for (i = 0; i < reqs->numLinks; i++) {
		req = &reqs->links[i];
		switch (req->type) {
		case RS_LINK_TECH:
		case RS_LINK_TECH_NOT:
			/* Get the index in the techtree. */
			req->link.tech = RS_GetTechByID(req->id);
			if (!req->link.tech)
				Com_Error(ERR_DROP, "RS_AssignTechLinks: Could not get tech definition for '%s'", req->id);
			break;
		case RS_LINK_ITEM:
			/* Get index in item-list. */
			req->link.od = INVSH_GetItemByID(req->id);
			if (!req->link.od)
				Com_Error(ERR_DROP, "RS_AssignTechLinks: Could not get item definition for '%s'", req->id);
			break;
		case RS_LINK_ALIEN:
		case RS_LINK_ALIEN_DEAD:
			req->link.td = Com_GetTeamDefinitionByID(req->id);
			if (!req->link.td)
				Com_Error(ERR_DROP, "RS_AssignTechLinks: Could not get alien type (alien or alien_dead) definition for '%s'", req->id);
			break;
		case RS_LINK_UFO:
			req->link.aircraft = AIR_GetAircraft(req->id);
			break;
		default:
			break;
		}
	}
}

static linkedList_t *redirectedTechs;

/**
 * @brief Assign Link pointers to all required techs/items/etc...
 * @note This replaces the RS_InitRequirementList function (since the switch to the _OR and _AND list)
 */
void RS_RequiredLinksAssign (void)
{
	linkedList_t* ll = redirectedTechs;	/**< Use this so we do not change the original redirectedTechs pointer. */
	technology_t *redirectedTech;
	int i;

	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		if (tech->requireAND.numLinks)
			RS_AssignTechLinks(&tech->requireAND);
		if (tech->requireOR.numLinks)
			RS_AssignTechLinks(&tech->requireOR);
		if (tech->requireForProduction.numLinks)
			RS_AssignTechLinks(&tech->requireForProduction);
	}

	/* Link the redirected technologies to their correct "parents" */
	while (ll) {
		/* Get the data stored in the linked list. */
		assert(ll);
		redirectedTech = (technology_t *) ll->data;
		ll = ll->next;

		assert(redirectedTech);

		assert(ll);
		redirectedTech->redirect = RS_GetTechByID((char*)ll->data);
		ll = ll->next;
	}

	/* clean up redirected techs list as it is no longer needed */
	LIST_Delete(&redirectedTechs);
}

technology_t* RS_GetTechForItem (const objDef_t *item)
{
	if (item == NULL)
		Com_Error(ERR_DROP, "RS_GetTechForItem: No item given");
	if (item->idx < 0 || item->idx > lengthof(ccs.objDefTechs))
		Com_Error(ERR_DROP, "RS_GetTechForItem: Buffer overflow");
	if (ccs.objDefTechs[item->idx] == NULL)
		Com_Error(ERR_DROP, "RS_GetTechForItem: No technology for item %s", item->id);
	return ccs.objDefTechs[item->idx];
}

/**
 * @brief Gets all needed names/file-paths/etc... for each technology entry.
 * Should be executed after the parsing of _all_ the ufo files and e.g. the
 * research tree/inventory/etc... are initialised.
 * @param[in] campaign The campaign data structure
 * @param[in] load qtrue if we are loading a game, qfalse otherwise
 * @todo Add a function to reset ALL research-stati to RS_NONE; -> to be called after start of a new game.
 * @todo Enhance ammo model display (see comment in code).
 * @sa CP_CampaignInit
 */
void RS_InitTree (const campaign_t *campaign, qboolean load)
{
	int i, j;
	technology_t *tech;
	byte found;
	const objDef_t *od;

	/* Add links to technologies. */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		ccs.objDefTechs[od->idx] = RS_GetTechByProvided(od->id);
		if (!ccs.objDefTechs[od->idx])
			Com_Error(ERR_DROP, "RS_InitTree: Could not find a valid tech for item %s", od->id);
	}

	for (i = 0, tech = ccs.technologies; i < ccs.numTechnologies; i++, tech++) {
		for (j = 0; j < tech->markResearched.numDefinitions; j++) {
			if (tech->markResearched.markOnly[j] && Q_streq(tech->markResearched.campaign[j], campaign->researched)) {
				Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
				RS_ResearchFinish(tech);
				break;
			}
		}

		/* Save the idx to the id-names of the different requirement-types for quicker access.
		 * The id-strings themself are not really needed afterwards :-/ */
		RS_AssignTechLinks(&tech->requireAND);
		RS_AssignTechLinks(&tech->requireOR);

		/* Search in correct data/.ufo */
		switch (tech->type) {
		case RS_CRAFTITEM:
			if (!tech->name)
				Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: \"%s\" A type craftitem needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_NEWS:
			if (!tech->name)
				Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: \"%s\" A 'type news' item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_TECH:
			if (!tech->name)
				Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: \"%s\" A 'type tech' item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_WEAPON:
		case RS_ARMOUR:
			found = qfalse;
			for (j = 0; j < csi.numODs; j++) {	/* j = item index */
				const objDef_t *item = INVSH_GetItemByIDX(j);

				/* This item has been 'provided' -> get the correct data. */
				if (Q_streq(tech->provides, item->id)) {
					found = qtrue;
					if (!tech->name)
						tech->name = Mem_PoolStrDup(item->name, cp_campaignPool, 0);
					if (!tech->mdl)
						tech->mdl = Mem_PoolStrDup(item->model, cp_campaignPool, 0);
					if (!tech->image)
						tech->image = Mem_PoolStrDup(item->image, cp_campaignPool, 0);
					break;
				}
			}
			/* No id found in csi.ods */
			if (!found) {
				tech->name = Mem_PoolStrDup(tech->id, cp_campaignPool, 0);
				Com_Printf("RS_InitTree: \"%s\" - Linked weapon or armour (provided=\"%s\") not found. Tech-id used as name.\n",
						tech->id, tech->provides);
			}
			break;
		case RS_BUILDING:
			found = qfalse;
			for (j = 0; j < ccs.numBuildingTemplates; j++) {
				building_t *building = &ccs.buildingTemplates[j];
				/* This building has been 'provided'  -> get the correct data. */
				if (Q_streq(tech->provides, building->id)) {
					found = qtrue;
					if (!tech->name)
						tech->name = Mem_PoolStrDup(building->name, cp_campaignPool, 0);
					if (!tech->image)
						tech->image = Mem_PoolStrDup(building->image, cp_campaignPool, 0);
					break;
				}
			}
			if (!found) {
				tech->name = Mem_PoolStrDup(tech->id, cp_campaignPool, 0);
				Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: \"%s\" - Linked building (provided=\"%s\") not found. Tech-id used as name.\n",
						tech->id, tech->provides);
			}
			break;
		case RS_CRAFT:
			found = qfalse;
			for (j = 0; j < ccs.numAircraftTemplates; j++) {
				aircraft_t *aircraftTemplate = &ccs.aircraftTemplates[j];
				/* This aircraft has been 'provided'  -> get the correct data. */
				if (!tech->provides)
					Com_Error(ERR_FATAL, "RS_InitTree: \"%s\" - No linked aircraft or craft-upgrade.\n", tech->id);
				if (Q_streq(tech->provides, aircraftTemplate->id)) {
					found = qtrue;
					if (!tech->name)
						tech->name = Mem_PoolStrDup(aircraftTemplate->name, cp_campaignPool, 0);
					if (!tech->mdl) {	/* DEBUG testing */
						tech->mdl = Mem_PoolStrDup(aircraftTemplate->model, cp_campaignPool, 0);
						Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: aircraft model \"%s\" \n", aircraftTemplate->model);
					}
					aircraftTemplate->tech = tech;
					break;
				}
			}
			if (!found)
				Com_Printf("RS_InitTree: \"%s\" - Linked aircraft or craft-upgrade (provided=\"%s\") not found.\n", tech->id, tech->provides);
			break;
		case RS_ALIEN:
			/* does nothing right now */
			break;
		case RS_UGV:
			/** @todo Implement me */
			break;
		case RS_LOGIC:
			/* Does not need any additional data. */
			break;
		}

		/* Check if we finally have a name for the tech. */
		if (!tech->name) {
			if (tech->type != RS_LOGIC)
				Com_Error(ERR_DROP, "RS_InitTree: \"%s\" - no name found!", tech->id);
		} else {
			/* Fill in subject lines of tech-mails.
			 * The tech-name is copied if nothing is defined. */
			for (j = 0; j < TECHMAIL_MAX; j++) {
				/* Check if no subject was defined (but it is supposed to be sent) */
				if (!tech->mail[j].subject && tech->mail[j].to) {
					tech->mail[j].subject = tech->name;
				}
			}
		}

		if (!tech->image && !tech->mdl)
			Com_DPrintf(DEBUG_CLIENT, "Tech %s of type %i has no image (%p) and no model (%p) assigned.\n",
					tech->id, tech->type, tech->image, tech->mdl);
	}

	if (load) {
		/* when you load a savegame right after starting UFO, the aircraft in bases
		 * and installations don't have any tech assigned */
		AIR_Foreach(aircraft) {
			/* if you already played before loading the game, tech are already defined for templates */
			if (!aircraft->tech)
				aircraft->tech = RS_GetTechByProvided(aircraft->id);
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: Technology tree initialised. %i entries found.\n", i);
}

/**
 * @brief Assigns scientist to the selected research-project.
 * @note The lab will be automatically selected (the first one that has still free space).
 * @param[in] tech What technology you want to assign the scientist to.
 * @param[in] base Pointer to base where the research is ongoing.
 * @param[in] employee Pointer to the scientist to assign. It can be NULL! That means "any".
 * @note if employee is NULL, te system selects an unassigned scientist on the selected (or tech-) base
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist
 */
void RS_AssignScientist (technology_t* tech, base_t *base, employee_t *employee)
{
	assert(tech);
	Com_DPrintf(DEBUG_CLIENT, "RS_AssignScientist: %i | %s \n", tech->idx, tech->name);

	/* if the tech is already assigned to a base, use that one */
	if (tech->base)
		base = tech->base;

	assert(base);

	if (!employee)
		employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
	if (!employee) {
		/* No scientists are free in this base. */
		Com_DPrintf(DEBUG_CLIENT, "No free scientists in this base (%s) to assign to tech '%s'\n", base->name, tech->id);
		return;
	}

	if (tech->statusResearchable) {
		if (CAP_GetFreeCapacity(base, CAP_LABSPACE) > 0) {
			tech->scientists++;
			tech->base = base;
			CAP_AddCurrent(base, CAP_LABSPACE, 1);
			employee->assigned = qtrue;
		} else {
			CP_Popup(_("Not enough laboratories"), _("No free space in laboratories left.\nBuild more laboratories.\n"));
			return;
		}

		tech->statusResearch = RS_RUNNING;
	}
}

/**
 * @brief Remove a scientist from a technology.
 * @param[in] tech The technology you want to remove the scientist from.
 * @param[in] employee Employee you want to remove (NULL if you don't care which one should be removed).
 * @sa RS_RemoveScientist_f
 * @sa RS_AssignScientist
 * @sa E_RemoveEmployeeFromBuildingOrAircraft
 */
void RS_RemoveScientist (technology_t* tech, employee_t *employee)
{
	assert(tech);

	/* no need to remove anything, but we can do some check */
	if (tech->scientists == 0) {
		assert(tech->base == NULL);
		assert(tech->statusResearch == RS_PAUSED);
		return;
	}

	if (!employee)
		employee = E_GetAssignedEmployee(tech->base, EMPL_SCIENTIST);
	if (employee) {
		/* Remove the scientist from the tech. */
		tech->scientists--;
		/* Update capacity. */
		CAP_AddCurrent(tech->base, CAP_LABSPACE, -1);
		employee->assigned = qfalse;
	} else {
		Com_Error(ERR_DROP, "No assigned scientists found - serious inconsistency.");
	}

	assert(tech->scientists >= 0);

	if (tech->scientists == 0) {
		/* Remove the tech from the base if no scientists are left to research it. */
		tech->base = NULL;
		tech->statusResearch = RS_PAUSED;
	}
}

/**
 * @brief Remove one scientist from research project if needed.
 * @param[in] base Pointer to base where a scientist should be removed.
 * @param[in] employee Pointer to the employee that is fired.
 * @note used when a scientist is fired.
 * @sa E_RemoveEmployeeFromBuildingOrAircraft
 * @note This function is called before the employee is actually fired.
 */
void RS_RemoveFiredScientist (base_t *base, employee_t *employee)
{
	technology_t *tech;
	employee_t *freeScientist = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);

	assert(base);
	assert(employee);

	/* Get a tech where there is at least one scientist working on (unless no scientist working in this base) */
	tech = RS_GetTechWithMostScientists(base);

	/* tech should never be NULL, as there is at least 1 scientist working in base */
	assert(tech);
	RS_RemoveScientist(tech, employee);

	/* if there is at least one scientist not working on a project, make this one replace removed employee */
	if (freeScientist)
		RS_AssignScientist(tech, base, freeScientist);
}

/**
 * @brief Mark technologies as researched. This includes techs that depends on "tech" and have time=0
 * @param[in] tech Pointer to a technology_t struct.
 * @param[in] base Pointer to base where we did research.
 * @todo Base shouldn't be needed here - check RS_MarkResearchable() for that.
 * @sa RS_ResearchRun
 */
static void RS_MarkResearched (technology_t *tech, const base_t *base)
{
	RS_ResearchFinish(tech);
	Com_DPrintf(DEBUG_CLIENT, "Research of \"%s\" finished.\n", tech->id);
	RS_MarkResearchable(qfalse, base);
}

/**
 * Pick a random base to research a story line event tech
 * @param techID The event technology script id to research
 * @note If there is no base available the tech is not marked as researched, too
 */
qboolean RS_MarkStoryLineEventResearched (const char *techID)
{
	technology_t* tech = RS_GetTechByID(techID);
	if (!RS_IsResearched_ptr(tech)) {
		const base_t *base = B_GetNext(NULL);
		if (base != NULL) {
			RS_MarkResearched(tech, base);
			return qtrue;
		}
	}
	return qfalse;
}


/**
 * @brief Checks the research status
 * @todo Needs to check on the exact time that elapsed since the last check of the status.
 * @sa RS_MarkResearched
 */
int RS_ResearchRun (void)
{
	int i, newResearch = 0;

	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);

		if (tech->statusResearch != RS_RUNNING)
			continue;

		if (!RS_RequirementsMet(&tech->requireAND, &tech->requireOR, tech->base)) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Research prerequisites of %s do not met at %s. Research halted!"), _(tech->name), tech->base->name);
			MSO_CheckAddNewMessage(NT_RESEARCH_HALTED, _("Research halted"), cp_messageBuffer, qfalse, MSG_RESEARCH_HALTED, NULL);

			RS_StopResearch(tech);
			continue;
		}

		if (tech->time > 0 && tech->scientists > 0) {
			/* If there are scientists there _has_ to be a base. */
			const base_t *base = tech->base;
			assert(tech->base);
			if (RS_ResearchAllowed(base)) {
				/** @todo Just for testing, better formular may be needed. Include employee-skill in calculation. */
				tech->time -= tech->scientists * 0.8;
				/* Will be a good thing (think of percentage-calculation) once non-integer values are used. */
				if (tech->time <= 0) {
					RS_MarkResearched(tech, base);

					newResearch++;
					tech->time = 0;
				}
			}
		}
	}

	return newResearch;
}

#ifdef DEBUG
/** @todo use Com_RegisterConstInt(); */
static const char *RS_TechTypeToName (researchType_t type)
{
	switch(type) {
	case RS_TECH:
		return "tech";
	case RS_WEAPON:
		return "weapon";
	case RS_ARMOUR:
		return "armour";
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

static const char *RS_TechReqToName (requirement_t *req)
{
	switch(req->type) {
	case RS_LINK_TECH:
		return req->link.tech->id;
	case RS_LINK_TECH_NOT:
		return va("not %s", req->link.tech->id);
	case RS_LINK_ITEM:
		return req->link.od->id;
	case RS_LINK_ALIEN:
		return req->link.td->id;
	case RS_LINK_ALIEN_DEAD:
		return req->link.td->id;
	case RS_LINK_ALIEN_GLOBAL:
		return "global alien count";
	case RS_LINK_UFO:
		return req->link.aircraft->id;
	case RS_LINK_ANTIMATTER:
		return "antimatter";
	default:
		return "unknown";
	}
}

/** @todo use Com_RegisterConstInt(); */
static const char *RS_TechLinkTypeToName (requirementType_t type)
{
	switch(type) {
	case RS_LINK_TECH:
		return "tech";
	case RS_LINK_TECH_NOT:
		return "tech (not)";
	case RS_LINK_ITEM:
		return "item";
	case RS_LINK_ALIEN:
		return "alien";
	case RS_LINK_ALIEN_DEAD:
		return "alien_dead";
	case RS_LINK_ALIEN_GLOBAL:
		return "alienglobal";
	case RS_LINK_UFO:
		return "ufo";
	case RS_LINK_ANTIMATTER:
		return "antimatter";
	default:
		return "unknown";
	}
}

/**
 * @brief List all parsed technologies and their attributes in commandline/console.
 * @note called with debug_listtech
 */
static void RS_TechnologyList_f (void)
{
	int i, j;
	technology_t *tech;
	requirements_t *reqs;
	dateLong_t date;

	Com_Printf("#techs: %i\n", ccs.numTechnologies);
	for (i = 0; i < ccs.numTechnologies; i++) {
		tech = RS_GetTechByIDX(i);
		Com_Printf("Tech: %s\n", tech->id);
		Com_Printf("... time      -> %.2f\n", tech->time);
		Com_Printf("... name      -> %s\n", tech->name);
		reqs = &tech->requireAND;
		Com_Printf("... requires ALL  ->");
		for (j = 0; j < reqs->numLinks; j++)
			Com_Printf(" %s (%s) %s", reqs->links[j].id, RS_TechLinkTypeToName(reqs->links[j].type), RS_TechReqToName(&reqs->links[j]));
		reqs = &tech->requireOR;
		Com_Printf("\n");
		Com_Printf("... requires ANY  ->");
		for (j = 0; j < reqs->numLinks; j++)
			Com_Printf(" %s (%s) %s", reqs->links[j].id, RS_TechLinkTypeToName(reqs->links[j].type), RS_TechReqToName(&reqs->links[j]));
		Com_Printf("\n");
		Com_Printf("... provides  -> %s", tech->provides);
		Com_Printf("\n");

		Com_Printf("... type      -> ");
		Com_Printf("%s\n", RS_TechTypeToName(tech->type));

		Com_Printf("... researchable -> %i\n", tech->statusResearchable);

		if (tech->statusResearchable) {
			CP_DateConvertLong(&tech->preResearchedDate, &date);
			Com_Printf("... researchable date: %02i %02i %i\n", date.day, date.month, date.year);
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
			CP_DateConvertLong(&tech->researchedDate, &date);
			Com_Printf("... research date: %02i %02i %i\n", date.day, date.month, date.year);
			break;
		default:
			Com_Printf("unknown\n");
			break;
		}
	}
}

/**
 * @brief Mark everything as researched
 * @sa UI_StartServer
 */
static void RS_DebugMarkResearchedAll (void)
{
	int i;

	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
		RS_MarkOneResearchable(tech);
		RS_ResearchFinish(tech);
		/** @todo Set all "collected" entries in the requirements to the "amount" value. */
	}
}

/**
 * @brief Set all items to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchAll_f (void)
{
	if (Cmd_Argc() != 2) {
		RS_DebugMarkResearchedAll();
	} else {
		technology_t *tech = RS_GetTechByID(Cmd_Argv(1));
		if (!tech)
			return;
		Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
		RS_MarkOneResearchable(tech);
		RS_ResearchFinish(tech);
	}
}

/**
 * @brief Set all item to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchableAll_f (void)
{
	int i;

	if (Cmd_Argc() != 2) {
		for (i = 0; i < ccs.numTechnologies; i++) {
			technology_t *tech = RS_GetTechByIDX(i);
			Com_Printf("...mark %s as researchable\n", tech->id);
			RS_MarkOneResearchable(tech);
			RS_MarkCollected(tech);
		}
	} else {
		technology_t *tech = RS_GetTechByID(Cmd_Argv(1));
		if (tech) {
			Com_Printf("...mark %s as researchable\n", tech->id);
			RS_MarkOneResearchable(tech);
			RS_MarkCollected(tech);
		}
	}
}

static void RS_DebugFinishResearches_f (void)
{
	int i;

	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		if (tech->statusResearch == RS_RUNNING) {
			assert(tech->base);
			Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
			RS_MarkResearched(tech, tech->base);
		}
	}
}
#endif


/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from UI_InitStartup resp. CL_InitLocal
 */
void RS_InitStartup (void)
{
	/* add commands and cvars */
#ifdef DEBUG
	Cmd_AddCommand("debug_listtech", RS_TechnologyList_f, "Print the current parsed technologies to the game console");
	Cmd_AddCommand("debug_researchall", RS_DebugResearchAll_f, "Mark all techs as researched");
	Cmd_AddCommand("debug_researchableall", RS_DebugResearchableAll_f, "Mark all techs as researchable");
	Cmd_AddCommand("debug_finishresearches", RS_DebugFinishResearches_f, "Mark all running researches as finished");
#endif
}

/**
 * @brief This is called everytime RS_ParseTechnologies is called - to prevent cyclic hash tables
 */
void RS_ResetTechs (void)
{
	/* they are static - but i'm paranoid - this is called before the techs were parsed */
	OBJZERO(techHash);
	OBJZERO(techHashProvided);

	/* delete redirectedTechs, will be filled during parse */
	LIST_Delete(&redirectedTechs);
}

/**
 * @brief The valid definition names in the research.ufo file.
 * @note Handled in parser below.
 * description, preDescription, requireAND, requireOR, up_chapter
 */
static const value_t valid_tech_vars[] = {
	{"name", V_TRANSLATION_STRING, offsetof(technology_t, name), 0},
	{"provides", V_HUNK_STRING, offsetof(technology_t, provides), 0},
	{"event", V_HUNK_STRING, offsetof(technology_t, finishedResearchEvent), 0},
	{"delay", V_INT, offsetof(technology_t, delay), MEMBER_SIZEOF(technology_t, delay)},
	{"producetime", V_INT, offsetof(technology_t, produceTime), MEMBER_SIZEOF(technology_t, produceTime)},
	{"time", V_FLOAT, offsetof(technology_t, time), MEMBER_SIZEOF(technology_t, time)},
	{"image", V_HUNK_STRING, offsetof(technology_t, image), 0},
	{"mdl", V_HUNK_STRING, offsetof(technology_t, mdl), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief The valid definition names in the research.ufo file for tech mails
 */
static const value_t valid_techmail_vars[] = {
	{"from", V_TRANSLATION_STRING, offsetof(techMail_t, from), 0},
	{"to", V_TRANSLATION_STRING, offsetof(techMail_t, to), 0},
	{"subject", V_TRANSLATION_STRING, offsetof(techMail_t, subject), 0},
	{"date", V_TRANSLATION_STRING, offsetof(techMail_t, date), 0},
	{"icon", V_HUNK_STRING, offsetof(techMail_t, icon), 0},
	{"model", V_HUNK_STRING, offsetof(techMail_t, model), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses one "tech" entry in the research.ufo file and writes it into the next free entry in technologies (technology_t).
 * @param[in] name Unique id of a technology_t. This is parsed from "tech xxx" -> id=xxx
 * @param[in] text the whole following text that is part of the "tech" item definition in research.ufo.
 * @sa CL_ParseScriptFirst
 * @sa GAME_SetMode
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void RS_ParseTechnologies (const char *name, const char **text)
{
	technology_t *tech;
	unsigned hash;
	const char *errhead = "RS_ParseTechnologies: unexpected end of file.";
	const char *token;
	requirements_t *requiredTemp;
	descriptions_t *descTemp;
	int i;

	for (i = 0; i < ccs.numTechnologies; i++) {
		if (Q_streq(ccs.technologies[i].id, name)) {
			Com_Printf("RS_ParseTechnologies: Second tech with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	if (ccs.numTechnologies >= MAX_TECHNOLOGIES) {
		Com_Printf("RS_ParseTechnologies: too many technology entries. limit is %i.\n", MAX_TECHNOLOGIES);
		return;
	}

	/* get body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("RS_ParseTechnologies: \"%s\" technology def without body ignored.\n", name);
		return;
	}

	/* New technology (next free entry in global tech-list) */
	tech = &ccs.technologies[ccs.numTechnologies];
	ccs.numTechnologies++;

	OBJZERO(*tech);

	/*
	 * Set standard values
	 */
	tech->idx = ccs.numTechnologies - 1;
	tech->id = Mem_PoolStrDup(name, cp_campaignPool, 0);
	hash = Com_HashKey(tech->id, TECH_HASH_SIZE);

	/* Set the default string for descriptions (available even if numDescriptions is 0) */
	tech->description.text[0] = _("No description available.");
	tech->preDescription.text[0] = _("No research proposal available.");
	/* Set desc-indices to undef. */
	tech->description.usedDescription = -1;
	tech->preDescription.usedDescription = -1;

	/* link the variable in */
	/* tech_hash should be null on the first run */
	tech->hashNext = techHash[hash];
	/* set the techHash pointer to the current tech */
	/* if there were already others in techHash at position hash, they are now
	 * accessible via tech->next - loop until tech->next is null (the first tech
	 * at that position)
	 */
	techHash[hash] = tech;

	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		/* get values */
		if (Q_streq(token, "type")) {
			/* what type of tech this is */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return;
			/** @todo use Com_RegisterConstInt(); */
			/* redundant, but oh well. */
			if (Q_streq(token, "tech"))
				tech->type = RS_TECH;
			else if (Q_streq(token, "weapon"))
				tech->type = RS_WEAPON;
			else if (Q_streq(token, "news"))
				tech->type = RS_NEWS;
			else if (Q_streq(token, "armour"))
				tech->type = RS_ARMOUR;
			else if (Q_streq(token, "craft"))
				tech->type = RS_CRAFT;
			else if (Q_streq(token, "craftitem"))
				tech->type = RS_CRAFTITEM;
			else if (Q_streq(token, "building"))
				tech->type = RS_BUILDING;
			else if (Q_streq(token, "alien"))
				tech->type = RS_ALIEN;
			else if (Q_streq(token, "ugv"))
				tech->type = RS_UGV;
			else if (Q_streq(token, "logic"))
				tech->type = RS_LOGIC;
			else
				Com_Printf("RS_ParseTechnologies: \"%s\" unknown techtype: \"%s\" - ignored.\n", name, token);
		} else {
			if (Q_streq(token, "description") || Q_streq(token, "pre_description")) {
				/* Parse the available descriptions for this tech */

				/* Link to correct list. */
				if (Q_streq(token, "pre_description")) {
					descTemp = &tech->preDescription;
				} else {
					descTemp = &tech->description;
				}

				token = Com_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;
				if (*token == '}')
					break;

				do {	/* Loop through all descriptions in the list.*/
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token == '}')
						break;

					if (descTemp->numDescriptions < MAX_DESCRIPTIONS) {
						/* Copy tech string into entry. */
						descTemp->tech[descTemp->numDescriptions] = Mem_PoolStrDup(token, cp_campaignPool, 0);

						/* Copy description text into the entry. */
						token = Com_EParse(text, errhead, name);
						/* skip translation marker */
						if (*token == '_')
							token++;
						else
							Com_Error(ERR_DROP, "RS_ParseTechnologies: '%s' No gettext string for description '%s'. Abort.\n", name, descTemp->tech[descTemp->numDescriptions]);

						descTemp->text[descTemp->numDescriptions] = Mem_PoolStrDup(token, cp_campaignPool, 0);
						descTemp->numDescriptions++;
					}
				} while (*text);

			} else if (Q_streq(token, "redirect")) {
				token = Com_EParse(text, errhead, name);
				/* Store this tech and the parsed tech-id of the target of the redirection for later linking. */
				LIST_AddPointer(&redirectedTechs, tech);
				LIST_AddString(&redirectedTechs, token);
			} else if (Q_streq(token, "require_AND") || Q_streq(token, "require_OR") || Q_streq(token, "require_for_production")) {
				/* Link to correct list. */
				if (Q_streq(token, "require_AND")) {
					requiredTemp = &tech->requireAND;
				} else if (Q_streq(token, "require_OR")) {
					requiredTemp = &tech->requireOR;
				} else { /* It's "requireForProduction" */
					requiredTemp = &tech->requireForProduction;
				}

				token = Com_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;
				if (*token == '}')
					break;

				do {	/* Loop through all 'require' entries.*/
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token == '}')
						break;

					if (Q_streq(token, "tech") || Q_streq(token, "tech_not")) {
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (Q_streq(token, "tech_not"))
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_TECH_NOT;
							else
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_TECH;

							/* Set requirement-name (id). */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cp_campaignPool, 0);

							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-tech ('tech' or 'tech_not')- %s\n", requiredTemp->links[requiredTemp->numLinks].id);

							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (Q_streq(token, "item")) {
						/* Defines what items need to be collected for this item to be researchable. */
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ITEM;
							/* Set requirement-name (id). */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cp_campaignPool, 0);
							/* Set requirement-amount of item. */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-item - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (Q_streq(token, "alienglobal")) {
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ALIEN_GLOBAL;
							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies:  require-alienglobal - %i\n", requiredTemp->links[requiredTemp->numLinks].amount);

							/* Set requirement-amount of item. */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (Q_streq(token, "alien_dead") || Q_streq(token, "alien")) { /* Does this only check the beginning of the string? */
						/* Defines what live or dead aliens need to be collected for this item to be researchable. */
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (Q_streq(token, "alien_dead")) {
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ALIEN_DEAD;
								Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies:  require-alien dead - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							} else {
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ALIEN;
								Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies:  require-alien alive - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							}
							/* Set requirement-name (id). */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cp_campaignPool, 0);
							/* Set requirement-amount of item. */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (Q_streq(token, "ufo")) {
						/* Defines what ufos need to be collected for this item to be researchable. */
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_UFO;
							/* Set requirement-name (id). */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cp_campaignPool, 0);
							/* Set requirement-amount of item. */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-ufo - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							requiredTemp->numLinks++;
						}
					} else if (Q_streq(token, "antimatter")) {
						/* Defines what ufos need to be collected for this item to be researchable. */
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ANTIMATTER;
							/* Set requirement-amount of item. */
							token = Com_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-antimatter - %i\n", requiredTemp->links[requiredTemp->numLinks].amount);
							requiredTemp->numLinks++;
						}
					} else {
						Com_Printf("RS_ParseTechnologies: \"%s\" unknown requirement-type: \"%s\" - ignored.\n", name, token);
					}
				} while (*text);
			} else if (Q_streq(token, "up_chapter")) {
				/* UFOpaedia chapter */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				if (*token) {
					/* find chapter */
					for (i = 0; i < ccs.numChapters; i++) {
						if (Q_streq(token, ccs.upChapters[i].id)) {
							/* add entry to chapter */
							tech->upChapter = &ccs.upChapters[i];
							if (!ccs.upChapters[i].first) {
								ccs.upChapters[i].first = tech;
								ccs.upChapters[i].last = tech;
								tech->upPrev = NULL;
								tech->upNext = NULL;
							} else {
								/* get "last entry" in chapter */
								technology_t *techOld = ccs.upChapters[i].last;
								ccs.upChapters[i].last = tech;
								techOld->upNext = tech;
								ccs.upChapters[i].last->upPrev = techOld;
								ccs.upChapters[i].last->upNext = NULL;
							}
							break;
						}
						if (i == ccs.numChapters)
							Com_Printf("RS_ParseTechnologies: \"%s\" - chapter \"%s\" not found.\n", name, token);
					}
				}
			} else if (Q_streq(token, "mail") || Q_streq(token, "mail_pre")) {
				techMail_t* mail;

				/* how many mails found for this technology
				 * used in UFOpaedia to check which article to display */
				tech->numTechMails++;

				if (tech->numTechMails > TECHMAIL_MAX)
					Com_Printf("RS_ParseTechnologies: more techmail-entries found than supported. \"%s\"\n",  name);

				if (Q_streq(token, "mail_pre")) {
					mail = &tech->mail[TECHMAIL_PRE];
				} else {
					mail = &tech->mail[TECHMAIL_RESEARCHED];
				}
				token = Com_EParse(text, errhead, name);
				if (!*text || *token != '{')
					return;

				/* grab the initial mail entry */
				token = Com_EParse(text, errhead, name);
				if (!*text || *token == '}')
					return;
				do {
					Com_ParseBlockToken(name, text, mail, valid_techmail_vars, cp_campaignPool, token);

					/* grab the next entry */
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;
				} while (*text && *token != '}');
				/* default model is navarre */
				if (mail->model == NULL)
					mail->model = "characters/navarre";
			} else {
				if (!Com_ParseBlockToken(name, text, tech, valid_tech_vars, cp_campaignPool, token))
					Com_Printf("RS_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n", token, name);
			}
		}
	} while (*text);

	if (tech->provides) {
		hash = Com_HashKey(tech->provides, TECH_HASH_SIZE);
		/* link the variable in */
		/* techHashProvided should be null on the first run */
		tech->hashProvidedNext = techHashProvided[hash];
		/* set the techHashProvided pointer to the current tech */
		/* if there were already others in techHashProvided at position hash, they are now
		* accessable via tech->next - loop until tech->next is null (the first tech
		* at that position)
		*/
		techHashProvided[hash] = tech;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "tech '%s' doesn't have a provides string\n", tech->id);
	}

	/* set the overall reseach time to the one given in the ufo-file. */
	tech->overallTime = tech->time;
}

static inline qboolean RS_IsValidTechIndex (int techIdx)
{
	if (techIdx == TECH_INVALID)
		return qfalse;
	if (techIdx < 0 || techIdx >= ccs.numTechnologies)
		return qfalse;
	if (techIdx >= MAX_TECHNOLOGIES)
		return qfalse;

	return qtrue;
}

/**
 * @brief Checks if the technology (tech-index) has been researched.
 * @param[in] techIdx index of the technology.
 * @return qboolean Returns qtrue if the technology has been researched, otherwise (or on error) qfalse;
 * @sa RS_IsResearched_ptr
 */
qboolean RS_IsResearched_idx (int techIdx)
{
	if (!RS_IsValidTechIndex(techIdx))
		return qfalse;

	if (ccs.technologies[techIdx].statusResearch == RS_FINISH)
		return qtrue;

	return qfalse;
}

/**
 * @brief Checks whether an item is already researched
 * @sa RS_IsResearched_idx
 * Call this function if you already hold a tech pointer
 */
qboolean RS_IsResearched_ptr (const technology_t * tech)
{
	if (tech && tech->statusResearch == RS_FINISH)
		return qtrue;
	return qfalse;
}

/**
 * @brief Returns the technology pointer for a tech index.
 * You can use this instead of "&ccs.technologies[techIdx]" to avoid having to check valid indices.
 * @param[in] techIdx Index in the global ccs.technologies[] array.
 * @return technology_t pointer or NULL if an error occurred.
 */
technology_t* RS_GetTechByIDX (int techIdx)
{
	if (!RS_IsValidTechIndex(techIdx))
		return NULL;
	else
		return &ccs.technologies[techIdx];
}


/**
 * @brief return a pointer to the technology identified by given id string
 * @param[in] id Unique identifier of the tech as defined in the research.ufo file (e.g. "tech xxxx").
 * @return technology_t pointer or NULL if an error occured.
 */
technology_t *RS_GetTechByID (const char *id)
{
	unsigned hash;
	technology_t *tech;

	if (Q_strnull(id))
		return NULL;

	hash = Com_HashKey(id, TECH_HASH_SIZE);
	for (tech = techHash[hash]; tech; tech = tech->hashNext)
		if (!Q_strcasecmp(id, tech->id))
			return tech;

	Com_Printf("RS_GetTechByID: Could not find a technology with id \"%s\"\n", id);
	return NULL;
}

/**
 * @brief returns a pointer to the item tech (as listed in "provides")
 * @param[in] idProvided Unique identifier of the object the tech is providing
 * @return The tech for the given object id or NULL if not found
 */
technology_t *RS_GetTechByProvided (const char *idProvided)
{
	unsigned hash;
	technology_t *tech;

	if (!idProvided)
		return NULL;
	/* catch empty strings */
	if (idProvided[0] == '\0')
		return NULL;

	hash = Com_HashKey(idProvided, TECH_HASH_SIZE);
	for (tech = techHashProvided[hash]; tech; tech = tech->hashProvidedNext)
		if (!Q_strcasecmp(idProvided, tech->provides))
			return tech;

	Com_DPrintf(DEBUG_CLIENT, "RS_GetTechByProvided: %s\n", idProvided);
	/* if a building, probably needs another building */
	/* if not a building, catch NULL where function is called! */
	return NULL;
}

/**
 * @brief Searches for the technology that has the most scientists assigned in a given base.
 * @param[in] base In what base the tech should be researched.
 * @sa E_RemoveEmployeeFromBuildingOrAircraft
 */
technology_t *RS_GetTechWithMostScientists (const struct base_s *base)
{
	technology_t *tech;
	int i, max;

	if (!base)
		return NULL;

	tech = NULL;
	max = 0;
	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech_temp = RS_GetTechByIDX(i);
		if (tech_temp->statusResearch == RS_RUNNING && tech_temp->base == base) {
			if (tech_temp->scientists > max) {
				tech = tech_temp;
				max = tech->scientists;
			}
		}
	}

	/* this tech has at least one assigned scientist or is a NULL pointer */
	return tech;
}

/**
 * @brief Returns the index (idx) of a "tech" entry given it's name.
 * @param[in] name the name of the tech
 */
int RS_GetTechIdxByName (const char *name)
{
	technology_t *tech;
	const unsigned hash = Com_HashKey(name, TECH_HASH_SIZE);

	for (tech = techHash[hash]; tech; tech = tech->hashNext)
		if (!Q_strcasecmp(name, tech->id))
			return tech->idx;

	Com_Printf("RS_GetTechIdxByName: Could not find tech '%s'\n", name);
	return TECH_INVALID;
}

/**
 * @brief Returns the number of employees searching in labs in given base.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 * @note must not return 0 if hasBuilding[B_LAB] is qfalse: used to update capacity
 */
int RS_CountScientistsInBase (const base_t *base)
{
	int i, counter = 0;

	for (i = 0; i < ccs.numTechnologies; i++) {
		const technology_t *tech = &ccs.technologies[i];
		if (tech->base == base) {
			/* Get a free lab from the base. */
			counter += tech->scientists;
		}
	}

	return counter;
}

/**
 * @brief Remove all exceeding scientist.
 * @param[in, out] base Pointer to base where a scientist should be removed.
 */
void RS_RemoveScientistsExceedingCapacity (base_t *base)
{
	assert(base);

	/* Make sure current CAP_LABSPACE capacity is set to proper value */
	CAP_SetCurrent(base, CAP_LABSPACE, RS_CountScientistsInBase(base));

	while (CAP_GetFreeCapacity(base, CAP_LABSPACE) < 0) {
		technology_t *tech = RS_GetTechWithMostScientists(base);
		RS_RemoveScientist(tech, NULL);
	}
}

/**
 * @brief Save callback for research and technologies
 * @param[out] parent XML Node structure, where we write the information to
 * @sa RS_LoadXML
 */
qboolean RS_SaveXML (xmlNode_t *parent)
{
	int i;
	xmlNode_t *node;

	Com_RegisterConstList(saveResearchConstants);
	node = XML_AddNode(parent, SAVE_RESEARCH_RESEARCH);
	for (i = 0; i < ccs.numTechnologies; i++) {
		int j;
		const technology_t *t = RS_GetTechByIDX(i);

		xmlNode_t * snode = XML_AddNode(node, SAVE_RESEARCH_TECH);
		XML_AddString(snode, SAVE_RESEARCH_ID, t->id);
		XML_AddBoolValue(snode, SAVE_RESEARCH_STATUSCOLLECTED, t->statusCollected);
		XML_AddFloatValue(snode, SAVE_RESEARCH_TIME, t->time);
		XML_AddString(snode, SAVE_RESEARCH_STATUSRESEARCH, Com_GetConstVariable(SAVE_RESEARCHSTATUS_NAMESPACE, t->statusResearch));
		if (t->base)
			XML_AddInt(snode, SAVE_RESEARCH_BASE, t->base->idx);
		XML_AddIntValue(snode, SAVE_RESEARCH_SCIENTISTS, t->scientists);
		XML_AddBool(snode, SAVE_RESEARCH_STATUSRESEARCHABLE, t->statusResearchable);
		XML_AddDate(snode, SAVE_RESEARCH_PREDATE, t->preResearchedDate.day, t->preResearchedDate.sec);
		XML_AddDate(snode, SAVE_RESEARCH_DATE, t->researchedDate.day, t->researchedDate.sec);
		XML_AddInt(snode, SAVE_RESEARCH_MAILSENT, t->mailSent);

		/* save which techMails were read */
		/** @todo this should be handled by the mail system */
		for (j = 0; j < TECHMAIL_MAX; j++) {
			if (t->mail[j].read) {
				xmlNode_t * ssnode = XML_AddNode(snode, SAVE_RESEARCH_MAIL);
				XML_AddInt(ssnode, SAVE_RESEARCH_MAIL_ID, j);
			}
		}
	}
	Com_UnregisterConstList(saveResearchConstants);

	return qtrue;
}

/**
 * @brief Load callback for research and technologies
 * @param[in] parent XML Node structure, where we get the information from
 * @sa RS_SaveXML
 */
qboolean RS_LoadXML (xmlNode_t *parent)
{
	xmlNode_t *topnode;
	xmlNode_t *snode;
	qboolean success = qtrue;

	topnode = XML_GetNode(parent, SAVE_RESEARCH_RESEARCH);
	if (!topnode)
		return qfalse;

	Com_RegisterConstList(saveResearchConstants);
	for (snode = XML_GetNode(topnode, SAVE_RESEARCH_TECH); snode; snode = XML_GetNextNode(snode, topnode, "tech")) {
		const char *techString = XML_GetString(snode, SAVE_RESEARCH_ID);
		xmlNode_t * ssnode;
		int baseIdx;
		technology_t *t = RS_GetTechByID(techString);
		const char *type = XML_GetString(snode, SAVE_RESEARCH_STATUSRESEARCH);

		if (!t) {
			Com_Printf("......your game doesn't know anything about tech '%s'\n", techString);
			continue;
		}

		if (!Com_GetConstIntFromNamespace(SAVE_RESEARCHSTATUS_NAMESPACE, type, (int*) &t->statusResearch)) {
			Com_Printf("Invalid research status '%s'\n", type);
			success = qfalse;
			break;
		}

		t->statusCollected = XML_GetBool(snode, SAVE_RESEARCH_STATUSCOLLECTED, qfalse);
		t->time = XML_GetFloat(snode, SAVE_RESEARCH_TIME, 0.0);
		/* Prepare base-index for later pointer-restoration in RS_PostLoadInit. */
		baseIdx = XML_GetInt(snode, SAVE_RESEARCH_BASE, -1);
		if (baseIdx >= 0)
			/* even if the base is not yet loaded we can set the pointer already */
			t->base = B_GetBaseByIDX(baseIdx);
		t->scientists = XML_GetInt(snode, SAVE_RESEARCH_SCIENTISTS, 0);
		t->statusResearchable = XML_GetBool(snode, SAVE_RESEARCH_STATUSRESEARCHABLE, qfalse);
		XML_GetDate(snode, SAVE_RESEARCH_PREDATE, &t->preResearchedDate.day, &t->preResearchedDate.sec);
		XML_GetDate(snode, SAVE_RESEARCH_DATE, &t->researchedDate.day, &t->researchedDate.sec);
		t->mailSent = XML_GetInt(snode, SAVE_RESEARCH_MAILSENT, 0);

		/* load which techMails were read */
		/** @todo this should be handled by the mail system */
		for (ssnode = XML_GetNode(snode, SAVE_RESEARCH_MAIL); ssnode; ssnode = XML_GetNextNode(ssnode, snode, SAVE_RESEARCH_MAIL)) {
			const int j= XML_GetInt(ssnode, SAVE_RESEARCH_MAIL_ID, TECHMAIL_MAX);
			if (j < TECHMAIL_MAX) {
				const techMailType_t mailType = j;
				t->mail[mailType].read = qtrue;
			} else
				Com_Printf("......your save game contains unknown techmail ids... \n");
		}

#ifdef DEBUG
		if (t->statusResearch == RS_RUNNING && t->scientists > 0) {
			if (!t->base) {
				Com_Printf("No base but research is running and scientists are assigned");
				success = qfalse;
				break;
			}
		}
#endif
	}
	Com_UnregisterConstList(saveResearchConstants);

	return success;
}

/**
 * @brief Returns true if the current base is able to handle research
 * @sa B_BaseInit_f
 * probably menu function, but not for research gui
 */
qboolean RS_ResearchAllowed (const base_t* base)
{
	assert(base);
	return !B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_LAB) && E_CountHired(base, EMPL_SCIENTIST) > 0;
}

/**
 * @brief Checks the parsed tech data for errors
 * @return false if there are errors - true otherwise
 */
qboolean RS_ScriptSanityCheck (void)
{
	int i, error = 0;
	technology_t *t;

	for (i = 0, t = ccs.technologies; i < ccs.numTechnologies; i++, t++) {
		if (!t->name) {
			error++;
			Com_Printf("...... technology '%s' has no name\n", t->id);
		}
		if (!t->provides) {
			switch (t->type) {
			case RS_TECH:
			case RS_NEWS:
			case RS_LOGIC:
			case RS_ALIEN:
				break;
			default:
				error++;
				Com_Printf("...... technology '%s' doesn't provide anything\n", t->id);
			}
		}

		if (t->produceTime == 0) {
			switch (t->type) {
			case RS_TECH:
			case RS_NEWS:
			case RS_LOGIC:
			case RS_BUILDING:
			case RS_ALIEN:
				break;
			default:
				/** @todo error++; Crafts still give errors - are there any definitions missing? */
				Com_Printf("...... technology '%s' has zero (0) produceTime, is this on purpose?\n", t->id);
			}
		}

		if (t->type != RS_LOGIC && (!t->description.text[0] || t->description.text[0][0] == '_')) {
			if (!t->description.text[0])
				Com_Printf("...... technology '%s' has a strange 'description' value '%s'.\n", t->id, t->description.text[0]);
			else
				Com_Printf("...... technology '%s' has no 'description' value.\n", t->id);
		}
	}

	if (!error)
		return qtrue;

	return qfalse;
}
