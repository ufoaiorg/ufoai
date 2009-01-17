/**
 * @file cl_research.c
 * @brief Player research.
 *
 * Handles everything related to the research-tree.
 * Provides information if items/buildings/etc.. can be researched/used/displayed etc...
 * Implements the research-system (research new items/etc...)
 * See base/ufos/research.ufo and base/ufos/menu_research.ufo for the underlying content.
 * @todo comment on used global variables.
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
#include "../menu/m_popup.h"

#define TECH_HASH_SIZE 64
static technology_t *techHash[TECH_HASH_SIZE];
static technology_t *techHashProvided[TECH_HASH_SIZE];

typedef enum {
	RSGUI_NOTHING,
	RSGUI_RESEARCH,
	RSGUI_BASETITLE,
	RSGUI_BASEINFO,
	RSGUI_RESEARCHOUT,
	RSGUI_MISSINGITEM,
	RSGUI_MISSINGITEMTITLE,
	RSGUI_UNRESEARCHABLEITEM,
	RSGUI_UNRESEARCHABLEITEMTITLE
} guiResearchElementType_t;

/**
 */
typedef struct {
	base_t *base;
	technology_t *tech;
	guiResearchElementType_t type;
} guiResearchElement_t;

/**
 * @brief A (local) list of displayed technology-entries (the research list in the base)
 */
static guiResearchElement_t researchList2[MAX_RESEARCHLIST + MAX_BASES + MAX_BASES];

/* The number of entries in the above list. */
static int researchListLength;

/* The currently selected entry in the above list. */
static int researchListPos;

static stringlist_t curRequiredList;

/* prototype */
static void RS_InitGUI(base_t* base, qboolean update);

/**
 * @brief Push a news about this tech when researched.
 * @param[in] tech Technology pointer.
 * @sa RS_ResearchFinish
 */
static void RS_PushNewsWhenResearched (technology_t* tech)
{
	assert(tech->pushnews);
	/** @todo */
}

/**
 * @brief Sets a technology status to researched and updates the date.
 * @param[in] tech The technology that was researched.
 */
void RS_ResearchFinish (technology_t* tech)
{
	/** At this point we define what research-report description is used when displayed. (i.e. "usedDescription" is set here).
	 * That's because this is the first the player will see the research result
	 * and any later changes may make the content inconsistent for the player.
	 * @sa RS_MarkOneResearchable */
	RS_GetDescription(&tech->description);
	if (tech->pre_description.usedDescription < 0) {
		/* For some reason the research proposal description was not set at this point - we just make sure it _is_ set. */
		RS_GetDescription(&tech->pre_description);
	}

	if (tech->finishedResearchEvent && tech->statusResearch != RS_FINISH)
		Cmd_ExecuteString(tech->finishedResearchEvent);
	tech->statusResearch = RS_FINISH;
	tech->researchedDate = ccs.date;
	if (!tech->statusResearchable) {
		tech->statusResearchable = qtrue;
		tech->preResearchedDate = ccs.date;
	}
	if (tech->pushnews)
		RS_PushNewsWhenResearched(tech);

	/* send a new message and add it to the mailclient */
	if ((tech->mailSent < MAILSENT_FINISHED) && (tech->type != RS_LOGIC)) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A research project has been completed: %s\n"), _(tech->name));
		MSO_CheckAddNewMessage( NT_RESEARCH_COMPLETED, _("Research finished"), mn.messageBuffer, qfalse, MSG_RESEARCH_FINISHED, tech);
		tech->mailSent = MAILSENT_FINISHED;
	}
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
	RS_GetDescription(&tech->pre_description);
	/* tech->description is checked before a research is finished */

	if (tech->mailSent < MAILSENT_PROPOSAL) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("New research proposal: %s\n"), _(tech->name));
		MSO_CheckAddNewMessage(NT_RESEARCH_PROPOSED, _("Unknown Technology researchable"), mn.messageBuffer, qfalse, MSG_RESEARCH_PROPOSAL, tech);
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
 * @param[in] required_AND Pointer to a list of AND-related requirements.
 * @param[in] required_OR Pointer to a list of OR-related requirements.
 * @param[in] base In what base to check the "collected" items etc..
 * @return Returns qtrue if all requirements are satisfied otherwise qfalse.
 * @todo Add support for the "delay" value.
 */
static qboolean RS_RequirementsMet (const requirements_t *required_AND, const requirements_t *required_OR, const base_t* base)
{
	int i;
	qboolean met_AND = qfalse;
	qboolean met_OR = qfalse;

	if (!required_AND && !required_OR) {
		Com_Printf("RS_RequirementsMet: No requirement list(s) given as parameter.\n");
		return qfalse;
	}

	/* If there are no requirements defined at all we have 'met' them by default. */
	if (required_AND->numLinks == 0 && required_OR->numLinks == 0) {
		Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: No requirements set for this tech. They are 'met'.\n");
		return qtrue;
	}

	if (required_AND->numLinks) {
		met_AND = qtrue;
		for (i = 0; i < required_AND->numLinks; i++) {
			const requirement_t *req = &required_AND->links[i];
			switch (req->type) {
			case RS_LINK_TECH:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ANDtech: %s / %i\n", req->id, ((technology_t*)req->link)->idx);
				if (!RS_IsResearched_ptr(req->link)) {
					Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: this tech not researched ----> %s \n", req->id);
					met_AND = qfalse;
				}
				break;
			case RS_LINK_TECH_NOT:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ANDtech NOT: %s / %i\n", req->id, ((technology_t*)req->link)->idx);
				if (RS_IsResearched_ptr(req->link)) {
					Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: this tech researched but it  must not be ----> %s \n", req->id);
					met_AND = qfalse;
				}
				break;
#if 0
			case RS_LINK_TECH_BEFORE:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ANDtech: BEFORE %s / %i\n", req->id, ((technology_t*)req->link)->idx);

				if (/**@todo I think we can use Date_LaterThan(now,compare) for this */) {
					Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: this dependecy not met ----> %s researched before %s \n", req->id, req->id2);
					met_AND = qfalse;
				}
				break;
				break;
			case RS_LINK_TECH_XOR:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ANDtech: %s XOR %s / %i\n", req->id, req->id2, ((technology_t*)req->link)->idx);

				if ((!RS_IsResearched_ptr(req->link) && !RS_IsResearched_ptr(req->link2))
				 || (RS_IsResearched_ptr(req->link) && RS_IsResearched_ptr(req->link2))) {
					Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: this dependecy not met ----> %s xor %s \n", req->id, req->id2);
					met_AND = qfalse;
				}
				break;
#endif
			case RS_LINK_ITEM:
				assert(req->link);
				assert(base);
				/* The same code is used in "PR_RequirementsMet" */
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ANDitem: %s / %i\n", req->id, ((objDef_t*)req->link)->idx);
				if (B_ItemInBase(req->link, base) < req->amount)
					met_AND = qfalse;
				break;
			case RS_LINK_EVENT:
				break;
			case RS_LINK_ALIEN_DEAD:
			case RS_LINK_ALIEN:
				assert(req->link);
				assert(base);
				if (AL_GetAlienAmount(req->link, req->type, base) < req->amount)
					met_AND = qfalse;
				break;
			case RS_LINK_ALIEN_GLOBAL:
				if (AL_CountAll() < req->amount)
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
			const requirement_t *req = &required_OR->links[i];
			switch (req->type) {
			case RS_LINK_TECH:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ORtech: %s / %i\n", req->id, ((technology_t*)req->link)->idx);
				if (RS_IsResearched_ptr(req->link))
					met_OR = qtrue;
				break;
			case RS_LINK_TECH_NOT:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ORtech: NOT %s / %i\n", req->id, ((technology_t*)req->link)->idx);
				if (!RS_IsResearched_ptr(req->link))
					met_OR = qtrue;
				break;
#if 0
			case RS_LINK_TECH_BEFORE:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ORtech: %s BEFORE %s / %i\n", req->id, req->id2, ((technology_t*)req->link)->idx);
				if (/**@todo I think we can use Date_LaterThan(now,compare) for this */)
					met_OR = qtrue;
				break;
			case RS_LINK_TECH_XOR:
				assert(req->link);
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ORtech: %s XOR %s / %i\n", req->id, req->id2, ((technology_t*)req->link)->idx);
				if ((RS_IsResearched_ptr(req->link) && !RS_IsResearched_ptr(req->link2))
				|| (!RS_IsResearched_ptr(req->link) && RS_IsResearched_ptr(req->link2)))
					met_OR = qtrue;
				break;
#endif
			case RS_LINK_ITEM:
				assert(req->link);
				assert(base);
				/* The same code is used in "PR_RequirementsMet" */
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsMet: ORitem: %s / %i\n", req->id, ((objDef_t*)req->link)->idx);
				if (B_ItemInBase(req->link, base) >= req->amount)
					met_OR = qtrue;
				break;
			case RS_LINK_EVENT:
				break;
			case RS_LINK_ALIEN:
			case RS_LINK_ALIEN_DEAD:
				assert(req->link);
				assert(base);
				if (AL_GetAlienAmount(req->link, req->type, base) >= req->amount)
					met_OR = qtrue;
				break;
			case RS_LINK_ALIEN_GLOBAL:
				if (AL_CountAll() >= req->amount)
					met_OR = qtrue;
				break;
			default:
				break;
			}

			if (met_OR)
				break;
		}
	Com_DPrintf(DEBUG_CLIENT, "met_AND is %i, met_OR is %i\n", met_AND, met_OR);

	return (met_AND || met_OR);
}

#if 0
/**
 * @todo Just here because it might be useful later on. See "note" below.
 * Winter and me (hoehrer) decided to go the "generic message" way for now.
 */

/**
 * @brief Checks if the required techs (no recursive lookup) are researchABLE.
 * @param[in] require_AND Pointer to a list of AND-related requirements.
 * @param[in] require_OR Pointer to a list of OR-related requirements.
 * @param[in] base In what base to check.
 * @return Returns qtrue if the requitred techs are researchable  otherwise qfalse.
 * @note The basic use of this function is to prevent "grey" entries in the research-list if no researchable precursor exists.
 * @sa RS_MarkResearchable
 */
static qboolean RS_RequirementsResearchable (requirements_t *required_AND, requirements_t *required_OR, base_t* base)
{
	int i;
	qboolean met_AND = qfalse;
	qboolean met_OR = qfalse;
	technology_t *tech;

	if (!required_AND && !required_OR) {
		Com_Printf("RS_RequirementsResearchable: No requirement list(s) given as parameter.\n");
		return qfalse;
	}

	if (required_AND->numLinks) {
		met_AND = qtrue;
		for (i = 0; i < required_AND->numLinks; i++) {
			if (required_AND->type[i] == RS_LINK_TECH) {
				/** @todo support for RS_LINK_TECH_BEFORE and RS_LINK_TECH_XOR? */
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsResearchable: ANDtech: %s / %i\n", required_AND->id[i], required_AND->idx[i]);
				tech = RS_GetTechByIDX(required_AND->idx[i]);

				if (!RS_TechIsResearchable(tech, base)) {
					Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsResearchable: this tech not researchable ----> %s \n", required_AND->id[i]);
					met_AND = qfalse;
				}
			}
			if (!met_AND)
				break;
		}
	}

	if (required_OR->numLinks) {
		for (i = 0; i < required_OR->numLinks; i++) {
			if (required_OR->type[i] == RS_LINK_TECH) {
				/** @todo support for RS_LINK_TECH_BEFORE and RS_LINK_TECH_XOR? */
				Com_DPrintf(DEBUG_CLIENT, "RS_RequirementsResearchable: ORtech: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				tech = RS_GetTechByIDX(required_OR->idx[i]);
				if (RS_TechIsResearchable(tech, base))
					met_OR = qtrue;
			}

			if (met_OR)
				break;
		}
	}
	/* Com_DPrintf(DEBUG_CLIENT, "met_AND is %i, met_OR is %i\n", met_AND, met_OR); */

	return (met_AND || met_OR);
}

/**
 * @brief Checks if the technology (tech-id) is researchable.
 * @param[in] tech Pointer to technology_t to be checked.
 * @param[in] base In what base to check reseachability.
 * "base" can be NULL (i.e. everything related to a base is ignored). See code in RS_RequirementsMet for details.
 * @return qboolean
 * @sa RS_IsResearched_idx
 */
static qboolean RS_TechIsResearchable (const technology_t * tech, const base_t *base)
{
	if (!tech)
		return qfalse;

	/* Research item found */
	if (tech->statusResearch == RS_FINISH)
		return qfalse;

	if (tech->statusResearchable)
		return qtrue;

	return RS_RequirementsMet(&tech->require_AND, &tech->require_OR, base);
}
#endif

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
		const base_t* base;
		if (!tech)
			continue;
		if (tech->base)
			base = tech->base;
		else
			base = baseCurrent;

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
 * @sa MN_AddNewMessage
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
			Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("New research proposal: %s\n"), _(tech->name));
			MSO_CheckAddNewMessage(NT_RESEARCH_PROPOSED,_("Unknown Technology found"), mn.messageBuffer, qfalse, MSG_RESEARCH_PROPOSAL, tech);
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
	for (i = 0; i < gd.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		tech->statusResearchable = qfalse;
	}

	for (i = 0; i < gd.numTechnologies; i++) {	/* i = tech-index */
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

				assert(base);

				/* All requirements are met. */
				if (RS_RequirementsMet(&tech->require_AND, &tech->require_OR, base)) {
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
					i = 0;
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
			req->link = RS_GetTechByID(req->id);
			if (!req->link)
				Sys_Error("RS_AssignTechLinks: Could not get tech definition for '%s'", req->id);
			break;
#if 0
		case RS_LINK_TECH_BEFORE:
		case RS_LINK_TECH_XOR:
			/* Get the index in the techtree. */
			req->link = RS_GetTechByID(req->id);
			if (!req->link)
				Sys_Error("RS_AssignTechLinks: Could not get tech XOR/Before definition for '%s'", req->id);

			req->link2 = RS_GetTechByID(req->id2);
			if (!req->link2)
				Sys_Error("RS_AssignTechLinks: Could not get tech XOR/Before definition for '%s'", req->id2);
			break;
#endif
		case RS_LINK_ITEM:
			/* Get index in item-list. */
			req->link = INVSH_GetItemByID(req->id);
			if (!req->link)
				Sys_Error("RS_AssignTechLinks: Could not get item definition for '%s'", req->id);
			break;
		case RS_LINK_EVENT:
			/** @todo Get index of event in event-list. */
			break;
		case RS_LINK_ALIEN:
		case RS_LINK_ALIEN_DEAD:
			req->link = Com_GetTeamDefinitionByID(req->id);
			if (!req->link)
				Sys_Error("RS_AssignTechLinks: Could not get alien type (alien or alien_dead) definition for '%s'", req->id);
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

	for (i = 0; i < gd.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		if (&tech->require_AND.numLinks)
			RS_AssignTechLinks(&tech->require_AND);
		if (&tech->require_OR.numLinks)
			RS_AssignTechLinks(&tech->require_OR);
		if (&tech->require_for_production.numLinks)
			RS_AssignTechLinks(&tech->require_for_production);
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

/**
 * @brief Gets all needed names/file-paths/etc... for each technology entry.
 * Should be executed after the parsing of _all_ the ufo files and e.g. the
 * research tree/inventory/etc... are initialised.
 * @param[in] load qtrue if we are loading a game, qfalse otherwise
 * @todo Add a function to reset ALL research-stati to RS_NONE; -> to be called after start of a new game.
 * @todo Enhance ammo model display (see comment in code).
 * @sa CP_CampaignInit
 */
void RS_InitTree (qboolean load)
{
	int i, j;
	technology_t *tech;
	byte found;
	objDef_t *od;

	/* Add links to technologies. */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		tech = RS_GetTechByProvided(od->id);
		od->tech = tech;
		if (!od->tech) {
			Com_Printf("RS_InitTree: Could not find a valid tech for item %s\n", od->id);
		}
	}

	for (i = 0, tech = gd.technologies; i < gd.numTechnologies; i++, tech++) {
		if (GAME_IsCampaign()) {
			assert(GAME_CP_IsRunning());
			for (j = 0; j < tech->markResearched.numDefinitions; j++) {
				if (tech->markResearched.markOnly[j] && !Q_strncmp(tech->markResearched.campaign[j], curCampaign->researched, MAX_VAR)) {
					Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
					RS_ResearchFinish(tech);
					break;
				}
			}
		} else {
			Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
			RS_ResearchFinish(tech);
		}

		/* Save the idx to the id-names of the different requirement-types for quicker access. The id-strings themself are not really needed afterwards :-/ */
		RS_AssignTechLinks(&tech->require_AND);
		RS_AssignTechLinks(&tech->require_OR);

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
				objDef_t *item = &csi.ods[j];

				/* This item has been 'provided' -> get the correct data. */
				if (!Q_strncmp(tech->provides, item->id, MAX_VAR)) {
					found = qtrue;
					if (!tech->name)
						tech->name = Mem_PoolStrDup(item->name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					if (!tech->mdl)
						tech->mdl = Mem_PoolStrDup(item->model, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					if (!tech->image)
						tech->image = Mem_PoolStrDup(item->image, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			/* No id found in csi.ods */
			if (!found) {
				tech->name = Mem_PoolStrDup(tech->id, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
				Com_Printf("RS_InitTree: \"%s\" - Linked weapon or armour (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
			}
			break;
		case RS_BUILDING:
			found = qfalse;
			for (j = 0; j < gd.numBuildingTemplates; j++) {
				building_t *building = &gd.buildingTemplates[j];
				/* This building has been 'provided'  -> get the correct data. */
				if (!Q_strncmp(tech->provides, building->id, MAX_VAR)) {
					found = qtrue;
					if (!tech->name)
						tech->name = Mem_PoolStrDup(building->name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					if (!tech->image)
						tech->image = Mem_PoolStrDup(building->image, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			if (!found) {
				tech->name = Mem_PoolStrDup(tech->id, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
				Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: \"%s\" - Linked building (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
			}
			break;
		case RS_CRAFT:
			found = qfalse;
			for (j = 0; j < numAircraftTemplates; j++) {
				aircraft_t *air_samp = &aircraftTemplates[j];
				/* This aircraft has been 'provided'  -> get the correct data. */
				if (!Q_strncmp(tech->provides, air_samp->id, MAX_VAR)) {
					found = qtrue;
					if (!tech->name)
						tech->name = Mem_PoolStrDup(air_samp->name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					if (!tech->mdl) {	/* DEBUG testing */
						tech->mdl = Mem_PoolStrDup(air_samp->model, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
						Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: aircraft model \"%s\" \n", air_samp->model);
					}
					air_samp->tech = tech;
					/* Should return to CASE RS_xxx. */
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
		} /* switch */

		/* Check if we finally have a name for the tech. */
		if (!tech->name) {
			if (tech->type != RS_LOGIC)
				Com_Printf("RS_InitTree: \"%s\" - no name found!\n", tech->id);
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
	}

	if (load) {
		/* when you load a savegame right after starting UFO, the aircraft in bases
		 * and installations don't have any tech assigned */
		int k;

		for (j = 0; j < gd.numBases; j++) {
			base_t *b = B_GetFoundedBaseByIDX(j);
			if (!b)
				continue;
			for (k = 0; k < b->numAircraftInBase; k++) {
				aircraft_t *aircraft = &b->aircraft[k];
				/* if you already played before loading the game, tech are already defined for templates */
				if (!aircraft->tech)
					aircraft->tech = RS_GetTechByProvided(aircraft->id);
			}
		}
	}

	memset(&curRequiredList, 0, sizeof(curRequiredList));

	Com_DPrintf(DEBUG_CLIENT, "RS_InitTree: Technology tree initialised. %i entries found.\n", i);
}

/**
 * @brief Displays the informations of the current selected technology in the description-area.
 * See menu_research.ufo for the layout/called functions.
 */
static void RS_UpdateInfo (const base_t* base)
{
	char tmpbuf[128];
	technology_t *tech;
	int type;

	/* reset cvars */
	Cvar_Set("mn_research_imagetop", "");
	Cvar_Set("mn_researchitemname", "");
	Cvar_Set("mn_researchitem", "");
	Cvar_Set("mn_researchweapon", "");	/**< @todo Do we even need/use mn_researchweapon and mn_researchammo (now or in the future?) */
	Cvar_Set("mn_researchammo", "");
	MN_MenuTextReset(TEXT_STANDARD);

	if (researchListLength <= 0 || researchListPos >= researchListLength)
		return;

	/* selection is not a research */
	/** @todo it can be an assert */
	type = researchList2[researchListPos].type;
	if (type != RSGUI_RESEARCH && type != RSGUI_RESEARCHOUT && type != RSGUI_UNRESEARCHABLEITEM)
		return;

	tech = researchList2[researchListPos].tech;
	if (tech == NULL)
		return;

	/* Display laboratories limits. */
	Com_sprintf(tmpbuf, sizeof(tmpbuf), _("Laboratory space (used/all): %i/%i"),
		base->capacities[CAP_LABSPACE].cur,
		base->capacities[CAP_LABSPACE].max);
	Cvar_Set("mn_research_labs", tmpbuf);

	/* Display scientists amounts. */
	Com_sprintf(tmpbuf, sizeof(tmpbuf), _("Scientists (available/all): %i/%i"),
		E_CountUnassigned(base, EMPL_SCIENTIST),
		E_CountHired(base, EMPL_SCIENTIST));
	Cvar_Set("mn_research_scis", tmpbuf);

	Cvar_Set("mn_research_selbase", _("Not researched in any base."));

	/* Display the base this tech is researched in. */
	if (tech->scientists > 0) {
		assert(tech->base);
		if (tech->base != base)
			Cvar_Set("mn_research_selbase", va(_("Researched in %s"), tech->base->name));
		else
			Cvar_Set("mn_research_selbase", _("Researched in this base."));
	}

	Cvar_Set("mn_research_selname", _(tech->name));
	if (tech->overalltime) {
		if (tech->time > tech->overalltime) {
			Com_Printf("RS_UpdateInfo: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", tech->id, tech->time,
					tech->overalltime);
			/* just in case the values got messed up */
			tech->time = tech->overalltime;
		}
		Cvar_SetValue("mn_research_seltimebar", 100 - (tech->time * 100 / tech->overalltime));
		Cvar_Set("mn_research_seltime", va(_("Progress: %.1f%%"), 100 - (tech->time * 100 / tech->overalltime)));
	} else {
		Cvar_SetValue("mn_research_seltimebar", 0);
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
		if (tech->statusCollected && !tech->statusResearchable) {
			/** @sa RS_UpdateData -> "--" */
			Cvar_Set("mn_research_selstatus", _("Status: We don't currently have all the materials or background knowledge needed to research this topic."));
		} else {
			Cvar_Set("mn_research_selstatus", _("Status: Unknown technology"));
		}
		break;
	default:
		break;
	}

	/* Set image cvar. */
	if (tech->image)
		Cvar_Set("mn_research_imagetop", tech->image);

	/** @todo Should we really show the model before it is researched? If not we'd need a pic/model to show a research proposal image.
	else if (tech->provides)
		switch (tech->type) {
			case RS_WEAPON:
			case RS_ARMOUR:
				Cvar_Set("mn_researchitem", tech->provides);
				break;
			default:
				break;
		} * switch *
	*/
}

/**
 * @brief Changes the active research-list entry to the currently selected.
 * See menu_research.ufo for the layout/called functions.
 */
static void CL_ResearchSelect_f (void)
{
	int num;
	int type;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength) {
		MN_MenuTextReset(TEXT_STANDARD);
		return;
	}

	type = researchList2[num].type;

	/* switch to another team */
	if (type == RSGUI_BASETITLE) {
		base_t *b = researchList2[num].base;
		if (b != NULL && b != baseCurrent) {
			MN_ExecuteConfunc("research_changebase %i %i", b->idx, researchListPos);
			return;
		}
	}

	if (type == RSGUI_RESEARCH || type == RSGUI_RESEARCHOUT || type == RSGUI_UNRESEARCHABLEITEM) {
		/* update the selected row */
		researchListPos = num;
	} else {
		return;
	}

	/** @todo improve that, dont need to update every thing */
	/* need to set previous selected tech to proper color */
	RS_InitGUI(baseCurrent, qtrue);
}

/**
 * @brief Assigns scientist to the selected research-project.
 * @note The lab will be automatically selected (the first one that has still free space).
 * @param[in] tech What technology you want to assign the scientist to.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist
 */
void RS_AssignScientist (technology_t* tech, base_t *base)
{
	employee_t *employee;

	assert(tech);
	Com_DPrintf(DEBUG_CLIENT, "RS_AssignScientist: %i | %s \n", tech->idx, tech->name);

	/* if the tech is already assigned to a base, use that one */
	if (tech->base)
		base = tech->base;

	assert(base);

	employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
	if (!employee) {
		/* No scientists are free in this base. */
		Com_DPrintf(DEBUG_CLIENT, "No free scientists in this base (%s) to assign to tech '%s'\n", base->name, tech->id);
		return;
	}

	if (tech->statusResearchable) {
		/* Get a free lab from the base. */
		building_t *building = B_GetBuildingInBaseByType(base, B_LAB, qtrue);
		if (building) {
			/* Check the capacity. */
			if (base->capacities[CAP_LABSPACE].max > base->capacities[CAP_LABSPACE].cur) {
				tech->scientists++;			/* Assign a scientist to this tech. */
				tech->base = base;			/* Make sure this tech has proper base pointer. */
				base->capacities[CAP_LABSPACE].cur++;	/* Set the amount of currently assigned in capacities. */

				/* Assign the sci to the lab and set number of used lab-space. */
				employee->building = building;
			} else {
				MN_Popup(_("Not enough laboratories"), _("No free space in laboratories left.\nBuild more laboratories.\n"));
				return;
			}
		} else {
			MN_Popup(_("Not enough laboratories"), _("No free space in laboratories left.\nBuild more laboratories.\n"));
			return;
		}

		tech->statusResearch = RS_RUNNING;
	}
}

/**
 * @brief Script function to add and remove a scientist to  the technology entry in the research-list.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist_f
 * @todo use the diff as a value, not only as a +1 or -1
 */
static void RS_ChangeScientist_f ()
{
	int num;
	float diff;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <num_in_list> <diff>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	diff = atof(Cmd_Argv(2));
	if (diff == 0)
		return;

	Com_DPrintf(DEBUG_CLIENT, "RS_ChangeScientist_f: num %i, diff %i\n", num, (int) diff);
	if (diff > 0) {
		RS_AssignScientist(researchList2[num].tech, baseCurrent);
	} else {
		RS_RemoveScientist(researchList2[num].tech, NULL);
	}

	/* Update display-list and display-info. */
	RS_InitGUI(baseCurrent, qtrue);
}

/**
 * @brief Script function to add a scientist to  the technology entry in the research-list.
 * @sa RS_AssignScientist
 * @sa RS_RemoveScientist_f
 */
static void RS_AssignScientist_f (void)
{
	int num;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num_in_list>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	Com_DPrintf(DEBUG_CLIENT, "RS_AssignScientist_f: num %i\n", num);
	RS_AssignScientist(researchList2[num].tech, baseCurrent);

	/* Update display-list and display-info. */
	RS_InitGUI(baseCurrent, qtrue);
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
	base_t *base;
	assert(tech);

	/* no need to remove anything, but we can do some check */
	if (tech->scientists == 0) {
		assert(tech->base == NULL);
		assert(tech->statusResearch == RS_PAUSED);
		return;
	}

	assert(tech->base);
	base = tech->base;

	if (!employee)
		employee = E_GetAssignedEmployee(tech->base, EMPL_SCIENTIST);
	if (employee) {
		/* Remove the sci from the tech. */
		tech->scientists--;
		/* Update capacity. */
		tech->base->capacities[CAP_LABSPACE].cur--;
		/* Remove the scientist from the lab and set number of used lab-space. */
		employee->building = NULL; /* See also E_RemoveEmployeeFromBuildingOrAircraft */
	} else {
		/* No assigned scientists found - serious inconsistency. */
		/** @todo add proper handling of this case. */
	}

	assert(tech->scientists >= 0);

	if (tech->scientists == 0) {
		/* Remove the tech from the base if no scis are left to research it. */
		tech->base = NULL;
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
	if (!base || !tech)
		return;

	assert(tech->scientists >= 0);

	/* Add as many scientists as possible to this tech. */
	do {
		if (base->capacities[CAP_LABSPACE].cur < base->capacities[CAP_LABSPACE].max) {
			const employee_t *employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
			if (!employee)
				break;
			RS_AssignScientist(tech, base);
		} else {
			/* No free lab-space left. */
			break;
		}
	} while (qtrue);
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
		Com_Printf("Usage: %s <num_in_list>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	RS_RemoveScientist(researchList2[num].tech, NULL);

	/* Update display-list and display-info. */
	RS_InitGUI(baseCurrent, qtrue);
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

	assert(base);
	assert(employee);

	/* Get a tech where there is at least one scientist working on (unless no scientist working in this base) */
	tech = RS_GetTechWithMostScientists(base);

	/* if there is at least one scientist not working on a project, make this one replace removed employee */
	if (E_CountUnassigned(base, EMPL_SCIENTIST)) {
		if (employee->building) {
			RS_AssignScientist(tech, base);
			RS_RemoveScientist(tech, employee);
		}
		return;
	}

	/* tech should never be NULL, as there is at least 1 scientist working in base */
	assert(tech);
	RS_RemoveScientist(tech, employee);
}

/**
 * @brief Starts the research of the selected research-list entry.
 */
static void RS_ResearchStart_f (void)
{
	technology_t *tech;

	/* We are not in base view. */
	if (!baseCurrent)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* Check if laboratory is available. */
	if (!B_GetBuildingStatus(baseCurrent, B_LAB))
		return;

	/* get the currently selected research-item */
	tech = researchList2[researchListPos].tech;

	/** @todo If there are enough items add them to the tech (i.e. block them from selling or for other research),
	 * otherwise pop an errormessage telling the palyer what is missing */
	if (!tech->statusResearchable) {
		Com_DPrintf(DEBUG_CLIENT, "RS_ResearchStart_f: %s was not researchable yet. re-checking\n", tech->id);
		/* If all requirements are met (includes a check for "enough-collected") mark this tech as researchable.*/
		if (RS_RequirementsMet(&tech->require_AND, &tech->require_OR, baseCurrent))
			RS_MarkOneResearchable(tech);
		RS_MarkResearchable(qfalse, baseCurrent);	/* Re-check all other techs in case they depend on the marked one. */
	}

	/* statusResearchable might have changed - check it again */
	if (tech->statusResearchable) {
		switch (tech->statusResearch) {
		case RS_RUNNING:
			if (tech->base == baseCurrent) {
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

	RS_InitGUI(baseCurrent, qtrue);
}

/**
 * @brief Removes all scientists from the selected research-list entry.
 */
static void RS_ResearchStop_f (void)
{
	technology_t *tech;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* Check if laboratory is available. */
	if (!B_GetBuildingStatus(baseCurrent, B_LAB))
		return;

	/* get the currently selected research-item */
	tech = researchList2[researchListPos].tech;

	switch (tech->statusResearch) {
	case RS_RUNNING:
		/* Remove all scis from it and set the status to paused (i.e. it's differnet from a RS_NONE since it may have a little bit of progress already). */
		while (tech->scientists > 0)
			RS_RemoveScientist(tech, NULL);
		assert(tech->statusResearch == RS_PAUSED);
		break;
	case RS_PAUSED:
		/** @todo remove? Popup info how much is already researched? */
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
	RS_InitGUI(baseCurrent, qtrue);
}

/**
 * @brief Switches to the UFOpaedia entry of the currently selected research entry.
 */
static void RS_ShowPedia_f (void)
{
	const technology_t *tech;

	/* We are not in base view. */
	if (!baseCurrent)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* get the currently selected research-item */
	tech = researchList2[researchListPos].tech;
	if (tech->pre_description.numDescriptions > 0) {
		UP_OpenCopyWith(tech->id);
	} else {
		MN_Popup(_("Notice"), _("No research proposal available for this project."));
	}
}

/**
 * @brief Create a GUI view of the current research in a base
 * @param[in] base Pointer to the base where item list is updated
 * @note call when we open the GUI
 */
static void RS_InitGUIData (base_t* base)
{
	int i, j;
	int row;
	int available[MAX_BASES];
	qboolean first;

	assert(base);

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *b = B_GetFoundedBaseByIDX(i);
		if (!b)
			continue;
		available[i] = E_CountUnassigned(b, EMPL_SCIENTIST);
	}

	RS_MarkResearchable(qfalse, base);

	/* update tech of the base */
	row = 0;
	for (i = 0; i < gd.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);

		/* Don't show technologies with time == 0 - those are NOT separate research topics. */
		if (tech->time == 0)
			continue;

		/* hide finished research */
		if (tech->statusResearch == RS_FINISH)
			continue;

		/* hide tech we can't search */
		if (!tech->statusResearchable)
			continue;

		/* In this base or nowhere */
		if (tech->base != NULL && tech->base != base)
			continue;

		/* Assign the current tech in the global list to the correct entry in the displayed list. */
		researchList2[row].tech = tech;
		researchList2[row].base = base;
		researchList2[row].type = RSGUI_RESEARCH;
		row++;
	}
	researchList2[row].base = base;
	researchList2[row].type = RSGUI_BASEINFO;
	row++;

	/* Items collected but not yet researchable. */
	first = qtrue;
	for (i = 0; i < gd.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);

		/* Don't show technologies with time == 0 - those are NOT separate research topics. */
		if (tech->time == 0)
			continue;

		/* hide finished research */
		if (tech->statusResearch == RS_FINISH)
			continue;

		/* Hide searchable or uncollected tech */
		if (tech->statusResearchable || !tech->statusCollected)
			continue;

		/* title */
		if (first) {
			researchList2[row].type = RSGUI_NOTHING;
			row++;
			researchList2[row].base = base;
			researchList2[row].type = RSGUI_UNRESEARCHABLEITEMTITLE;
			row++;
			first = qfalse;
		}

		/* Assign the current tech in the global list to the correct entry in the displayed list. */
		researchList2[row].tech = tech;
		researchList2[row].base = base;
		researchList2[row].type = RSGUI_UNRESEARCHABLEITEM;
		row++;
	}


	/* research from another bases */
	for (j = 0; j < MAX_BASES; j++) {
		base_t *b = B_GetFoundedBaseByIDX(j);
		if (!b || b == base)
			continue;

		/* skip bases without labs */
		if (B_GetBuildingInBaseByType(b, B_LAB, qtrue) == NULL)
			continue;

		researchList2[row].type = RSGUI_NOTHING;
		row++;
		researchList2[row].type = RSGUI_BASETITLE;
		researchList2[row].base = b;
		row++;

		for (i = 0; i < gd.numTechnologies; i++) {
			technology_t *tech = RS_GetTechByIDX(i);

			/* Don't show technologies with time == 0 - those are NOT separate research topics. */
			if (tech->time == 0)
				continue;

			if (tech->base != b)
				continue;

			/* hide finished research */
			if (tech->statusResearch == RS_FINISH)
				continue;

			/* hide tech we can't search */
			if (!tech->statusResearchable)
				continue;

			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList2[row].tech = tech;
			researchList2[row].base = b;
			researchList2[row].type = RSGUI_RESEARCHOUT;

			/* counting the numbers of display-list entries. */
			row++;
		}
		researchList2[row].base = b;
		researchList2[row].type = RSGUI_BASEINFO;
		row++;
	}

	/** @todo add missingitem, unsearchableitem  */

	researchListLength = row;
}

/**
 * @brief Update the research status of a row
 */
static void RS_UpdateResearchStatus (int row)
{
	guiResearchElement_t *element = &researchList2[row];
	assert(element->type == RSGUI_RESEARCH || element->type == RSGUI_RESEARCHOUT);

	switch (element->tech->statusResearch) {
	case RS_RUNNING:
		/* Color the item with 'research running'-color. */
		MN_ExecuteConfunc("research_running %i", row);
		break;
	case RS_PAUSED:
		/* Color the item with 'research paused'-color. */
		MN_ExecuteConfunc("research_paused %i", row);
		break;
	case RS_NONE:
		/* Color the item with 'research normal'-color. */
		MN_ExecuteConfunc("research_normal %i", row);
		break;
	case RS_FINISH:
	default:
		break;
	}
}

/**
 * @brief Initialize/Update all the GUI according to the current view
 * @param[in] base Pointer to the base where item list is updated
 * @param[in] update If true, only update editable content
 * @note See menu_research.ufo for the layout/called functions.
 * @todo Display free space in all labs in the current base for each item.
 */
static void RS_InitGUI (base_t* base, qboolean update)
{
	int i = 0;
	int available[MAX_BASES];

	assert(base);

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *b = B_GetFoundedBaseByIDX(i);
		if (!b)
			continue;
		available[i] = E_CountUnassigned(b, EMPL_SCIENTIST);
	}

	for (i = 0; i < MAX_RESEARCHDISPLAY && i < researchListLength; i++) {
		guiResearchElement_t *element = &researchList2[i];

		/* only element of the current base can change */
		if (update && (element->base != base && element->type != RSGUI_RESEARCHOUT))
			continue;

		switch (element->type) {
		case RSGUI_NOTHING:
			MN_ExecuteConfunc("research_hide %i", i);
			Cvar_Set(va("mn_researchitem%i", i), "");
			break;
		case RSGUI_RESEARCH:
			{
				const int value = element->tech->scientists;
				const int max = available[element->base->idx] + element->tech->scientists;
				MN_ExecuteConfunc("research_research %i", i);
				if (!update) {
					Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
				}
				MN_ExecuteConfunc("research_updateitem %i %i %i", i, value, max);
				/* How many scis are assigned to this tech. */
				Cvar_SetValue(va("mn_researchassigned%i", i), element->tech->scientists);

				RS_UpdateResearchStatus(i);
			}
			break;
		case RSGUI_BASETITLE:
			MN_ExecuteConfunc("research_basetitle %i", i);
			Cvar_Set(va("mn_researchitem%i", i), element->base->name);
			break;
		case RSGUI_BASEINFO:
			MN_ExecuteConfunc("research_baseinfo %i", i);
			Cvar_Set(va("mn_researchitem%i", i), _("Unassigned scientists"));
			/* How many scis are unassigned */
			Cvar_SetValue(va("mn_researchassigned%i", i), available[element->base->idx]);
			break;
		case RSGUI_RESEARCHOUT:
			MN_ExecuteConfunc("research_outterresearch %i", i);
			Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
			/* How many scis are assigned to this tech. */
			Cvar_SetValue(va("mn_researchassigned%i", i), element->tech->scientists);
			RS_UpdateResearchStatus(i);
			break;
		case RSGUI_MISSINGITEM:
			MN_ExecuteConfunc("research_missingitem %i", i);
			Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
			break;
		case RSGUI_MISSINGITEMTITLE:
			MN_ExecuteConfunc("research_missingitemtitle %i", i);
			Cvar_Set(va("mn_researchitem%i", i), _("Missing an artifact"));
			break;
		case RSGUI_UNRESEARCHABLEITEM:
			MN_ExecuteConfunc("research_unresearchableitem %i", i);
			Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
			break;
		case RSGUI_UNRESEARCHABLEITEMTITLE:
			MN_ExecuteConfunc("research_unresearchableitemtitle %i", i);
			Cvar_Set(va("mn_researchitem%i", i), _("Unresearchable collected items"));
			break;
		default:
			assert(qfalse);
		}
	}

	/* Set rest of the list-entries to have no text at all. */
	if (!update) {
		for (; i < MAX_RESEARCHDISPLAY; i++) {
			MN_ExecuteConfunc("research_hide %i", i);
		}
	}

	/* Select last selected item if possible or the very first one if not. */
	if (researchListLength) {
		Com_DPrintf(DEBUG_CLIENT, "RS_UpdateData: Pos%i Len%i\n", researchListPos, researchListLength);
		if (researchListPos >= 0 && researchListPos < researchListLength && researchListLength < MAX_RESEARCHDISPLAY) {
			int t = researchList2[researchListPos].type;
			/* is it a tech row */
			if (t == RSGUI_RESEARCH || t == RSGUI_RESEARCHOUT || t == RSGUI_UNRESEARCHABLEITEM) {
				MN_ExecuteConfunc("research_selected %i", researchListPos);
			}
		}
	}

	/* Update the description field/area. */
	RS_UpdateInfo(base);
}

/**
 * @brief Script binding for RS_UpdateData
 */
static void RS_UpdateData_f (void)
{
	if (!baseCurrent)
		return;
	RS_InitGUI(baseCurrent, qtrue);
}

/**
 * @brief Checks whether there are items in the research list and there is a base
 * otherwise leave the research menu again
 * @note if there is a base but no lab a popup appears
 * @sa RS_UpdateData
 * @sa MN_ResearchInit_f
 * @todo wrong computation: researchListLength dont say if there are research on this base
 */
static void CL_ResearchType_f (void)
{
	if (!baseCurrent)
		return;

	/* Update and display the list. */
	RS_InitGUIData(baseCurrent);

	/* Nothing to research here. */
	if (!researchListLength || !gd.numBases) {
		MN_PopMenu(qfalse);
		if (!researchListLength)
			MN_Popup(_("Notice"), _("Nothing to research"));
	} else if (!B_GetBuildingStatus(baseCurrent, B_LAB)) {
		MN_PopMenu(qfalse);
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
static qboolean RS_DependsOn (char *id1, char *id2)
{
	int i;
	technology_t *tech;
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
 * @brief Enable autosell option.
 * @param[in] tech Pointer to newly researched technology.
 * @sa RS_MarkResearched
 */
static void RS_EnableAutosell (const technology_t *tech)
{
	int i;

	/* If the tech leads to weapon or armour, find related item and enable autosell. */
	if ((tech->type == RS_WEAPON) || (tech->type == RS_ARMOUR)) {
		const objDef_t *obj = NULL;
		for (i = 0; i < csi.numODs; i++) {
			obj = &csi.ods[i];
			if (!Q_strncmp(tech->provides, obj->id, MAX_VAR)) {
				gd.autosell[i] = qtrue;
				break;
			}
		}
		if (i == csi.numODs)
			return;

		/* If the weapon has ammo, enable autosell for proper ammo as well. */
		if ((tech->type == RS_WEAPON) && (obj->reload)) {
			for (i = 0; i < obj->numAmmos; i++) {
				const objDef_t *ammo = obj->ammos[i];
				const technology_t *ammotech = ammo->tech;
				if (ammotech && (ammotech->produceTime < 0))
					continue;
				gd.autosell[ammo->idx] = qtrue;
			}
		}
	}
}

/**
 * @brief Mark technologies as researched. This includes techs that depends on "tech" and have time=0
 * @param[in] tech Pointer to a technology_t struct.
 * @sa RS_ResearchRun
 */
static void RS_MarkResearched (technology_t *tech, const base_t *base)
{
	RS_ResearchFinish(tech);
	Com_DPrintf(DEBUG_CLIENT, "Research of \"%s\" finished.\n", tech->id);
	RS_EnableAutosell(tech);
	RS_MarkResearchable(qfalse, base);
}

/**
 * @brief Checks the research status
 * @todo Needs to check on the exact time that elapsed since the last check of the status.
 * @sa RS_MarkResearched
 */
void RS_ResearchRun (void)
{
	int i, newResearch = 0;
	base_t* checkBases[MAX_BASES];

	if (!researchListLength)
		return;

	memset(checkBases, 0, sizeof(checkBases));

	for (i = 0; i < gd.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);
		if (tech->statusResearch == RS_RUNNING) {
			/* the test hasBuilding[B_LAB] is needed to make sure that labs are active (their dependences are OK) */
			if (tech->time > 0 && tech->scientists > 0) {
				base_t *base = tech->base;
				assert(tech->base);	/**< If there are scientitsts there _has_ to be a base. */
				if (RS_ResearchAllowed(base)) {
					Com_DPrintf(DEBUG_CLIENT, "timebefore %.2f\n", tech->time);
					Com_DPrintf(DEBUG_CLIENT, "timedelta %.2f\n", tech->scientists * 0.8);
					/** @todo Just for testing, better formular may be needed. */
					tech->time -= tech->scientists * 0.8;
					Com_DPrintf(DEBUG_CLIENT, "timeafter %.2f\n", tech->time);
					/** @todo include employee-skill in calculation. */
					/* Will be a good thing (think of percentage-calculation) once non-integer values are used. */
					if (tech->time <= 0) {
						/* Remove all scientists from the technology. */
						while (tech->scientists > 0)
							RS_RemoveScientist(tech, NULL);

						RS_MarkResearched(tech, base);
						researchListLength = 0;
						researchListPos = 0;
						newResearch++;
						/* Add this base to the list that needs an update call.
						 * tech->base was set to NULL with the last call of RS_RemoveScientist. */
						checkBases[base->idx] = base;
						tech->time = 0;
					}
				}
			}
		}
	}

	/* now update the data in all affected bases -- we only need to update tech list and not research menu */
	/** @todo CHECK THAT: very very strange */
	for (i = 0; i < MAX_BASES; i++) {
		if (checkBases[i])
			RS_InitGUIData(checkBases[i]);
	}

/*	if (newResearch)
		CL_GameTimeStop();
		*/
}

#ifdef DEBUG
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
	assert(req->link);
	switch(req->type) {
	case RS_LINK_TECH:
		return ((technology_t*)req->link)->id;
	case RS_LINK_TECH_NOT:
		return va("not %s",((technology_t*)req->link)->id);
#if 0
	case RS_LINK_TECH_BEFORE:
		return va("%s before %s",((technology_t*)req->link)->id, ((technology_t*)req->link2)->id);
	case RS_LINK_TECH_XOR:
		return va("%s xor %s",((technology_t*)req->link)->id, ((technology_t*)req->link2)->id);
#endif
	case RS_LINK_ITEM:
		return ((objDef_t*)req->link)->id;
	case RS_LINK_EVENT:
		return "not implemented yet";
	case RS_LINK_ALIEN:
		return ((teamDef_t*)req->link)->id;
	case RS_LINK_ALIEN_DEAD:
		return ((teamDef_t*)req->link)->id;
	case RS_LINK_ALIEN_GLOBAL:
		return "global alien count";
	default:
		return "unknown";
	}
}

static const char *RS_TechLinkTypeToName (requirementType_t type)
{
	switch(type) {
	case RS_LINK_TECH:
		return "tech";
	case RS_LINK_TECH_NOT:
		return "tech (not)";
#if 0
	case RS_LINK_TECH_BEFORE:
		return "tech (before)";
	case RS_LINK_TECH_XOR:
		return "tech (xor)";
#endif
	case RS_LINK_ITEM:
		return "item";
	case RS_LINK_EVENT:
		return "event";
	case RS_LINK_ALIEN:
		return "alien";
	case RS_LINK_ALIEN_DEAD:
		return "alien_dead";
	case RS_LINK_ALIEN_GLOBAL:
		return "alienglobal";
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

	Com_Printf("#techs: %i\n", gd.numTechnologies);
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = RS_GetTechByIDX(i);
		Com_Printf("Tech: %s\n", tech->id);
		Com_Printf("... time      -> %.2f\n", tech->time);
		Com_Printf("... name      -> %s\n", tech->name);
		reqs = &tech->require_AND;
		Com_Printf("... requires ALL  ->");
		for (j = 0; j < reqs->numLinks; j++)
			Com_Printf(" %s (%s) %s", reqs->links[j].id, RS_TechLinkTypeToName(reqs->links[j].type), RS_TechReqToName(&reqs->links[j]));
		reqs = &tech->require_OR;
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
			CL_DateConvertLong(&tech->preResearchedDate, &date);
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
			CL_DateConvertLong(&tech->researchedDate, &date);
			Com_Printf("... research date: %02i %02i %i\n", date.day, date.month, date.year);
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
	assert(baseCurrent);
	CL_ResearchType_f();
	RS_InitGUI(baseCurrent, qfalse);
}

/**
 * @brief Mark everything as researched
 * @sa CL_Connect_f
 * @sa MN_StartServer
 */
void RS_MarkResearchedAll (void)
{
	int i;
	technology_t *tech;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = RS_GetTechByIDX(i);
		Com_DPrintf(DEBUG_CLIENT, "...mark %s as researched\n", tech->id);
		RS_MarkOneResearchable(tech);
		RS_ResearchFinish(tech);
		/** @todo Set all "collected" entries in the requirements to the "amount" value. */
	}
}

#ifdef DEBUG
/**
 * @brief Set all items to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchAll (void)
{
	technology_t *tech;

	if (Cmd_Argc() != 2) {
		RS_MarkResearchedAll();
	} else {
		tech= RS_GetTechByID(Cmd_Argv(1));
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
static void RS_DebugResearchableAll (void)
{
	int i;
	technology_t *tech;

	if (Cmd_Argc() != 2) {
		for (i = 0; i < gd.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
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
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
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
 * Called from MN_InitStartup resp. CL_InitLocal
 */
void RS_InitStartup (void)
{
	/* add commands and cvars */
	Cmd_AddCommand("research_init", MN_ResearchInit_f, "Research menu init function binding");
	Cmd_AddCommand("research_select", CL_ResearchSelect_f, "Update current selection with the one that has been clicked");
	Cmd_AddCommand("research_type", CL_ResearchType_f, "Switch between different research types");
	Cmd_AddCommand("mn_start_research", RS_ResearchStart_f, "Start the research of the selected entry");
	Cmd_AddCommand("mn_stop_research", RS_ResearchStop_f, "Pause the research of the selected entry");
	Cmd_AddCommand("mn_show_ufopedia", RS_ShowPedia_f, "Show the entry in the UFOpaedia for the selected research topic");
	Cmd_AddCommand("mn_rs_add", RS_AssignScientist_f, "Assign one scientist to this entry");
	Cmd_AddCommand("mn_rs_remove", RS_RemoveScientist_f, "Remove one scientist from this entry");
	Cmd_AddCommand("mn_rs_change", RS_ChangeScientist_f, "Assign or remove scientist from this entry");
	Cmd_AddCommand("research_update", RS_UpdateData_f, NULL);
	Cmd_AddCommand("research_dependencies_click", RS_DependenciesClick_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("debug_listtech", RS_TechnologyList_f, "Print the current parsed technologies to the game console");
	Cmd_AddCommand("debug_researchall", RS_DebugResearchAll, "Mark all techs as researched");
	Cmd_AddCommand("debug_researchableall", RS_DebugResearchableAll, "Mark all techs as researchable");
#endif
}

/**
 * @brief This is called everytime RS_ParseTechnologies is called - to prevent cyclic hash tables
 */
void RS_ResetTechs (void)
{
	/* they are static - but i'm paranoid - this is called before the techs were parsed */
	memset(techHash, 0, sizeof(techHash));
	memset(techHashProvided, 0, sizeof(techHashProvided));

	/* delete redirectedTechs, will be filled during parse */
	LIST_Delete(&redirectedTechs);
}

/**
 * @brief The valid definition names in the research.ufo file.
 * @note Handled in parser below.
 * description, pre_description, require_AND, require_OR, up_chapter
 */
static const value_t valid_tech_vars[] = {
	{"name", V_TRANSLATION_STRING, offsetof(technology_t, name), 0},
	{"provides", V_CLIENT_HUNK_STRING, offsetof(technology_t, provides), 0},
	{"event", V_CLIENT_HUNK_STRING, offsetof(technology_t, finishedResearchEvent), 0},
	{"delay", V_INT, offsetof(technology_t, delay), MEMBER_SIZEOF(technology_t, delay)},
	{"producetime", V_INT, offsetof(technology_t, produceTime), MEMBER_SIZEOF(technology_t, produceTime)},
	{"time", V_FLOAT, offsetof(technology_t, time), MEMBER_SIZEOF(technology_t, time)},
	{"pushnews", V_BOOL, offsetof(technology_t, pushnews), MEMBER_SIZEOF(technology_t, pushnews)},
	{"image", V_CLIENT_HUNK_STRING, offsetof(technology_t, image), 0},
	{"mdl", V_CLIENT_HUNK_STRING, offsetof(technology_t, mdl), 0},

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
	{"icon", V_CLIENT_HUNK_STRING, offsetof(techMail_t, icon), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses one "tech" entry in the research.ufo file and writes it into the next free entry in technologies (technology_t).
 * @param[in] name Unique id of a technology_t. This is parsed from "tech xxx" -> id=xxx
 * @param[in] text the whole following text that is part of the "tech" item definition in research.ufo.
 * @sa CL_ParseScriptFirst
 * @sa GAME_SetMode
 * @note write into cl_localPool - free on every game restart and reparse
 */
void RS_ParseTechnologies (const char *name, const char **text)
{
	const value_t *vp;
	technology_t *tech;
	unsigned hash;
	const char *errhead = "RS_ParseTechnologies: unexpected end of file.";
	const char *token;
	requirements_t *requiredTemp;
	descriptions_t *descTemp;
	int i;

	for (i = 0; i < gd.numTechnologies; i++) {
		if (!Q_strcmp(gd.technologies[i].id, name)) {
			Com_Printf("RS_ParseTechnologies: Second tech with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	if (gd.numTechnologies >= MAX_TECHNOLOGIES) {
		Com_Printf("RS_ParseTechnologies: too many technology entries. limit is %i.\n", MAX_TECHNOLOGIES);
		return;
	}

	/* get body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("RS_ParseTechnologies: \"%s\" technology def without body ignored.\n", name);
		return;
	}

	/* New technology (next free entry in global tech-list) */
	tech = &gd.technologies[gd.numTechnologies];
	gd.numTechnologies++;

	memset(tech, 0, sizeof(*tech));

	/*
	 * Set standard values
	 */
	tech->idx = gd.numTechnologies - 1;
	tech->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
	hash = Com_HashKey(tech->id, TECH_HASH_SIZE);

	/* Set the default string for descriptions (available even if numDescriptions is 0) */
	tech->description.text[0] = _("No description available.");
	tech->pre_description.text[0] = _("No research proposal available.");
	/* Set desc-indices to undef. */
	tech->description.usedDescription = -1;
	tech->pre_description.usedDescription = -1;

	/* link the variable in */
	/* tech_hash should be null on the first run */
	tech->hashNext = techHash[hash];
	/* set the techHash pointer to the current tech */
	/* if there were already others in techHash at position hash, they are now
	 * accessable via tech->next - loop until tech->next is null (the first tech
	 * at that position)
	 */
	techHash[hash] = tech;

	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;

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
			else if (!Q_strncmp(token, "armour", MAX_VAR))
				tech->type = RS_ARMOUR;
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
			if ((!Q_strncmp(token, "description", MAX_VAR))
			|| (!Q_strncmp(token, "pre_description", MAX_VAR))) {
				/* Parse the available descriptions for this tech */

				/* Link to correct list. */
				if (!Q_strncmp(token, "pre_description", MAX_VAR)) {
					descTemp = &tech->pre_description;
				} else {
					descTemp = &tech->description;
				}

				token = COM_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;
				if (*token == '}')
					break;

				do {	/* Loop through all descriptions in the list.*/
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token == '}')
						break;

					if (descTemp->numDescriptions < MAX_DESCRIPTIONS) {
						/* Copy tech string into entry. */
						descTemp->tech[descTemp->numDescriptions] = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

						/* Copy description text into the entry. */
						token = COM_EParse(text, errhead, name);
						if (*token == '_')
							token++;	/**< Remove first char (i.e. "_") */
						else
							Sys_Error("RS_ParseTechnologies: '%s' No gettext string for description '%s'. Abort.\n", name, descTemp->tech[descTemp->numDescriptions]);

						descTemp->text[descTemp->numDescriptions] = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
						descTemp->numDescriptions++;
					}
				} while (*text);

			} else if (!Q_strncmp(token, "redirect", MAX_VAR)) {
				token = COM_EParse(text, errhead, name);
				/* Store this tech and the parsed tech-id of the target of the redirection for later linking. */
				LIST_AddPointer(&redirectedTechs, tech);
				LIST_AddString(&redirectedTechs, token);
			} else if ((!Q_strncmp(token, "require_AND", MAX_VAR))
				|| (!Q_strncmp(token, "require_OR", MAX_VAR))
				|| (!Q_strncmp(token, "require_for_production", MAX_VAR))) {
				/* Link to correct list. */
				if (!Q_strncmp(token, "require_AND", MAX_VAR)) {
					requiredTemp = &tech->require_AND;
				} else if (!Q_strncmp(token, "require_OR", MAX_VAR)) {
					requiredTemp = &tech->require_OR;
				} else { /* It's "require_for_production" */
					requiredTemp = &tech->require_for_production;
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


					if (!Q_strncmp(token, "tech", MAX_VAR) || !Q_strncmp(token, "tech_not", MAX_VAR)) {
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (!Q_strncmp(token, "tech_not", MAX_VAR))
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_TECH_NOT;
							else
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_TECH;

							/* Set requirement-name (id). */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-tech ('tech' or 'tech_not')- %s\n", requiredTemp->links[requiredTemp->numLinks].id);

							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
#if 0
					} else if (!Q_strncmp(token, "tech_before", MAX_VAR) || !Q_strncmp(token, "tech_xor", MAX_VAR)) {
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (!Q_strncmp(token, "tech_before", MAX_VAR))
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_TECH_BEFORE;
							else
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_TECH_XOR;

							/* Set requirement-name (id). */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id2 = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-tech ('tech_before' or 'tech_xor')- %s/%s\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].id2);

							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
#endif
					} else if (!Q_strncmp(token, "item", MAX_VAR)) {
						/* Defines what items need to be collected for this item to be researchable. */
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ITEM;
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-item - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if (!Q_strncmp(token, "event", MAX_VAR)) {
						token = COM_Parse(text);
						Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: require-event - %s\n", token);
						requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_EVENT;
						/* Get name/id & amount of required item. */
						/** @todo Implement final event system, so this can work 100% */
					} else if (!Q_strncmp(token, "alienglobal", MAX_VAR)) {
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ALIEN_GLOBAL;
							Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies:  require-alienglobal - %i\n", requiredTemp->links[requiredTemp->numLinks].amount);

							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else if ((!Q_strncmp(token, "alien_dead", MAX_VAR)) || (!Q_strncmp(token, "alien", MAX_VAR))) { /* Does this only check the beginning of the string? */
						/* Defines what live or dead aliens need to be collected for this item to be researchable. */
						if (requiredTemp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (!Q_strncmp(token, "alien_dead", MAX_VAR)) {
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ALIEN_DEAD;
								Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies:  require-alien dead - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							} else {
								requiredTemp->links[requiredTemp->numLinks].type = RS_LINK_ALIEN;
								Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies:  require-alien alive - %s - %i\n", requiredTemp->links[requiredTemp->numLinks].id, requiredTemp->links[requiredTemp->numLinks].amount);
							}
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].id = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							requiredTemp->links[requiredTemp->numLinks].amount = atoi(token);
							requiredTemp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", name, MAX_TECHLINKS);
						}
					} else {
						Com_Printf("RS_ParseTechnologies: \"%s\" unknown requirement-type: \"%s\" - ignored.\n", name, token);
					}
				} while (*text);
			} else if (!Q_strncmp(token, "up_chapter", MAX_VAR)) {
				/* UFOpaedia chapter */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				if (*token) {
					/* find chapter */
					for (i = 0; i < gd.numChapters; i++) {
						if (!Q_strncmp(token, gd.upChapters[i].id, MAX_VAR)) {
							/* add entry to chapter */
							tech->upChapter = &gd.upChapters[i];
							if (!gd.upChapters[i].first) {
								gd.upChapters[i].first = tech;
								gd.upChapters[i].last = tech;
								tech->upPrev = NULL;
								tech->upNext = NULL;
							} else {
								/* get "last entry" in chapter */
								technology_t *techOld = gd.upChapters[i].last;
								gd.upChapters[i].last = tech;
								techOld->upNext = tech;
								gd.upChapters[i].last->upPrev = techOld;
								gd.upChapters[i].last->upNext = NULL;
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
				 * used in UFOpaedia to check which article to display */
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
					for (vp = valid_techmail_vars; vp->string; vp++)
						if (!Q_strncmp(token, vp->string, sizeof(vp->string))) {
							/* found a definition */
							token = COM_EParse(text, errhead, name);
							if (!*text)
								return;

							switch (vp->type) {
							case V_TRANSLATION_STRING:
								token++;	/**< Remove first char (i.e. we assume it's the "_") */
							case V_CLIENT_HUNK_STRING:
								Mem_PoolStrDupTo(token, (char**) ((char*)mail + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
								break;
							case V_NULL:
								Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed (token: '%s') entry: '%s'\n", token, name);
								break;
							default:
								Com_EParseValue(mail, token, vp->type, vp->ofs, vp->size);
							}
							break;
						}
					/* grab the next entry */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
				} while (*text && *token != '}');
			} else {
				for (vp = valid_tech_vars; vp->string; vp++)
					if (!Q_strncmp(token, vp->string, sizeof(vp->string))) {
						/* found a definition */
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;

						if (!vp->ofs)
							break;
						switch (vp->type) {
						case V_TRANSLATION_STRING:
							token++;
						case V_CLIENT_HUNK_STRING:
							Mem_PoolStrDupTo(token, (char**) ((char*)tech + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
							break;
						case V_NULL:
							Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed (token: '%s') entry: '%s'\n", token, name);
							break;
						default:
							Com_EParseValue(tech, token, vp->type, vp->ofs, vp->size);
						}
						break;
					}
				/*@todo escape "type weapon/tech/etc.." here */
				if (!vp->string)
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
	tech->overalltime = tech->time;
}

/**
 * @brief Checks if the technology (tech-index) has been researched.
 * @param[in] techIdx index of the technology.
 * @return qboolean Returns qtrue if the technology has been researched, otherwise (or on error) qfalse;
 * @sa RS_IsResearched_ptr
 */
qboolean RS_IsResearched_idx (int techIdx)
{
	if (!GAME_IsCampaign())
		return qtrue;

	if (techIdx == TECH_INVALID)
		return qfalse;

	if ((gd.technologies[techIdx].statusResearch == RS_FINISH)
	&& (techIdx != TECH_INVALID)
	&& (techIdx >= 0))
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
	if (!GAME_IsCampaign())
		return qtrue;
	if (tech && tech->statusResearch == RS_FINISH)
		return qtrue;
	return qfalse;
}

#if 0
/* Currently not used, but may be useful later on. */
/**
 * @brief Checks if the item (as listed in "provides") has been researched.
 * @note This is mostly of use if the linked provided struct does not yet have a technology_t backlink. i.e. objDef_t can use RS_IsResearched_ptr(objDef_t->tech) directly.
 * @param[in] idProvided Unique id of an item/building/etc.. that is provided by a technology_t entry.
 * @return qboolean
 * @sa RS_IsResearched_ptr
 */
qboolean RS_ItemIsResearched (const char *idProvided)
{
	technology_t *tech;

	/* in multiplayer everyting is researched */
	if (GAME_IsMultiplayer())
		return qtrue;

	tech = RS_GetTechByProvided(idProvided);
	if (!tech)
		return qtrue;

	return RS_IsResearched_ptr(tech);
}
#endif

/**
 * @sa RS_ItemCollected
 * Call this function if you already hold a tech pointer.
 */
int RS_Collected_ (const technology_t * tech)
{
	if (tech)
		return tech->statusCollected;

	Com_DPrintf(DEBUG_CLIENT, "RS_Collected_: NULL technology given.\n");
	return -1;
}

/**
 * @brief Returns the technology pointer fo a tech index.
 * You can use this instead of "&gd.technologies[techIdx]" to avoid having to check valid indices.
 * @param[in] techIdx Index in the global gd.technologies[] array.
 * @return technology_t pointer or NULL if an error occured.
 */
technology_t* RS_GetTechByIDX (int techIdx)
{
	if (techIdx >= MAX_TECHNOLOGIES) {
		Com_DPrintf(DEBUG_CLIENT, "RS_ParseTechnologies: Index (%i) is bigger than MAX_TECHNOLOGIES (%i).\n", techIdx, MAX_TECHNOLOGIES);
		return NULL;
	}

	if (techIdx == TECH_INVALID || techIdx >= gd.numTechnologies)
		return NULL;
	else
		return &gd.technologies[techIdx];
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

	if (!id || !*id)
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

	assert(idProvided);
	/* catch empty strings */
	if (!*idProvided)
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

#if 0
/* Not really used anywhere, but I'll just leave it in here if it is needed again*/
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
			techList[j] = RS_GetTechByIDX(i);
			j++;
			/* j+1 because last item have to be NULL */
			if (j + 1 >= MAX_TECHNOLOGIES) {
				Com_Printf("RS_GetTechsByType: MAX_TECHNOLOGIES limit hit\n");
				break;
			}
		}
	}
	techList[j] = NULL;
	Com_DPrintf(DEBUG_CLIENT, "techlist with %i entries\n", j);
	return techList;
}
#endif


/**
 * @brief Searches for the technology that has the most scientists assigned in a given base.
 * @param[in] base In what base the tech should be researched.
 * @sa E_RemoveEmployeeFromBuildingOrAircraft
 */
technology_t *RS_GetTechWithMostScientists (const struct base_s *base)
{
	technology_t *tech;
	technology_t *tech_temp;
	int i;
	int max;

	if (!base)
		return NULL;

	tech = NULL;
	max = 0;
	for (i = 0; i < gd.numTechnologies; i++) {
		tech_temp = RS_GetTechByIDX(i);
		if (tech_temp->statusResearch == RS_RUNNING
		 && tech_temp->base == base) {
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
 * @todo this method is extremely inefficient... it could be dramatically improved
 */
int RS_GetTechIdxByName (const char *name)
{
	technology_t *tech;
	unsigned hash;

	hash = Com_HashKey(name, TECH_HASH_SIZE);
	for (tech = techHash[hash]; tech; tech = tech->hashNext)
		if (!Q_strcasecmp(name, tech->id))
			return tech->idx;

	Com_Printf("RS_GetTechIdxByName: Could not find tech '%s'\n", name);
	/* return TECH_INVALID if not found */
	return TECH_INVALID;
}

/**
 * @brief Returns the number of employees searching in labs in given base.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 * @note must not return 0 if hasBuilding[B_LAB] is qfalse: used to update capacity
 */
int RS_CountInBase (const base_t *base)
{
	int i, counter = 0;
	technology_t *tech;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = gd.technologies + i;
		assert(tech);
		if (tech->base == base) {
			/* Get a free lab from the base. */
			counter += tech->scientists;
		}
	}

	return counter;
}

/**
 * @brief Remove all exceeding scientist.
 * @param[in] base Pointer to base where a scientist should be removed.
 * @note function called
 */
void RS_RemoveScientistsExceedingCapacity (base_t *base)
{
	assert(base);

	/* Make sure base->capacities[CAP_LABSPACE].cur is set to proper value */
	base->capacities[CAP_LABSPACE].cur = RS_CountInBase(base);

	while (base->capacities[CAP_LABSPACE].cur > base->capacities[CAP_LABSPACE].max) {
		technology_t *tech = RS_GetTechWithMostScientists(base);
		RS_RemoveScientist(tech, NULL);
	}
}

qboolean RS_Save (sizebuf_t* sb, void* data)
{
	int i, j;

	for (i = 0; i < presaveArray[PRE_NMTECH]; i++) {
		const technology_t *t = RS_GetTechByIDX(i);
		MSG_WriteString(sb, t->id);
		MSG_WriteByte(sb, t->statusCollected);
		MSG_WriteFloat(sb, t->time);
		MSG_WriteByte(sb, t->statusResearch);
		if (t->base)
			MSG_WriteShort(sb, t->base->idx);
		else
			MSG_WriteShort(sb, -1);
		MSG_WriteByte(sb, t->scientists);
		MSG_WriteByte(sb, t->statusResearchable);
		MSG_WriteLong(sb, t->preResearchedDate.day);
		MSG_WriteLong(sb, t->preResearchedDate.sec);
		MSG_WriteLong(sb, t->researchedDate.day);
		MSG_WriteLong(sb, t->researchedDate.sec);
		MSG_WriteByte(sb, t->mailSent);
		for (j = 0; j < presaveArray[PRE_TECHMA]; j++) {
			/* only save the already read mails */
			MSG_WriteByte(sb, j);
			MSG_WriteByte(sb, t->mail[j].read);
		}
	}

	return qtrue;
}

static linkedList_t *loadTechBases;

qboolean RS_Load (sizebuf_t* sb, void* data)
{
	int i, j;
	int tempBaseIdx;

	/* Clear linked list. */
	LIST_Delete(&loadTechBases);

	for (i = 0; i < presaveArray[PRE_NMTECH]; i++) {
		const char *techString = MSG_ReadString(sb);
		technology_t *t = RS_GetTechByID(techString);
		if (!t) {
			Com_Printf("......your game doesn't know anything about tech '%s'\n", techString);
			/* We now read dummy data to skip the unknown tech. */
			MSG_ReadByte(sb);	/* statusCollected */
			MSG_ReadFloat(sb);	/* time */
			MSG_ReadByte(sb);	/* statusResearch */
			MSG_ReadShort(sb);	/* tech->base->idx */
			MSG_ReadByte(sb);	/* scientists */
			MSG_ReadByte(sb);	/* statusResearchable */
			MSG_ReadShort(sb);	/* preResearchedDateDay */
			MSG_ReadShort(sb);	/* preResearchedDateMonth */
			MSG_ReadShort(sb);	/* preResearchedDateYear */
			MSG_ReadShort(sb);	/* researchedDateDay */
			MSG_ReadShort(sb);	/* researchedDateMonth */
			MSG_ReadShort(sb);	/* researchedDateYear */
			MSG_ReadByte(sb);	/* mailSent */
			for (j = 0; j < presaveArray[PRE_TECHMA]; j++) {
				MSG_ReadByte(sb);	/* mailType */
				MSG_ReadByte(sb);	/* t->mail[mailType].read */
			}
			continue;
		}
		t->statusCollected = MSG_ReadByte(sb);
		t->time = MSG_ReadFloat(sb);
		t->statusResearch = MSG_ReadByte(sb);

		/* Prepare base-index for later pointer-restoration in RS_PostLoadInit. */
		tempBaseIdx = MSG_ReadShort(sb);
		LIST_AddPointer(&loadTechBases, t);
		LIST_Add(&loadTechBases, (byte *)&tempBaseIdx, sizeof(int));

		t->scientists = MSG_ReadByte(sb);
		t->statusResearchable = MSG_ReadByte(sb);

		t->preResearchedDate.day = MSG_ReadLong(sb);
		t->preResearchedDate.sec = MSG_ReadLong(sb);
		t->researchedDate.day = MSG_ReadLong(sb);
		t->researchedDate.sec = MSG_ReadLong(sb);

		t->mailSent = MSG_ReadByte(sb);
		for (j = 0; j < presaveArray[PRE_TECHMA]; j++) {
			const techMailType_t mailType = MSG_ReadByte(sb);
			t->mail[mailType].read = MSG_ReadByte(sb);
		}
	}

	return qtrue;
}


/**
 * @brief Called after savegame-load (all subsystems) is complete. Restores the base-pointers for each tech.
 * The base-pointer is searched with the base-index that was parsed in RS_Load.
 * @sa RS_Load
 * @sa SAV_GameActionsAfterLoad
 */
void RS_PostLoadInit (void)
{
	int baseIdx;

	/* this list has an entry for the tech and for the base index */
	linkedList_t *techBases = loadTechBases;

	while (techBases) {
		technology_t *t = (technology_t *) techBases->data;
		const int baseIndex = *(int *)techBases->next->data;
		/* Com_Printf("RS_PostLoadInit: DEBUG %s %i\n", t->id, baseIndex); */
		if (baseIndex >= 0)
			t->base = B_GetBaseByIDX(baseIndex);
		else
			t->base = NULL;
#ifdef DEBUG
		if (t->statusResearch == RS_RUNNING && t->scientists > 0) {
			if (!t->base)
				Sys_Error("No base but research is running and scientists are assigned");
		}
#endif
		techBases = techBases->next->next;
	}

	/* Clear linked list. */
	LIST_Delete(&loadTechBases);

	/* Udate research so that it can start (otherwise research does not go on until you entered Laboratory) */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		/** @todo CHECK THAT: very very strange */
		RS_InitGUIData(base);
	}
}

/**
 * @brief Returns true if the current base is able to handle research
 * @sa B_BaseInit_f
 */
qboolean RS_ResearchAllowed (const base_t* base)
{
	assert(base);
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && B_GetBuildingStatus(base, B_LAB)
	 && E_CountHired(base, EMPL_SCIENTIST) > 0) {
		MN_ExecuteConfunc("set_research_enabled");
		return qtrue;
	} else {
		MN_ExecuteConfunc("set_research_disabled");
		return qfalse;
	}
}

/**
 * @brief Checks the parsed tech data for errors
 * @return false if there are errors - true otherwise
 */
qboolean RS_ScriptSanityCheck (void)
{
	int i, error = 0;
	technology_t *t;

	for (i = 0, t = gd.technologies; i < gd.numTechnologies; i++, t++) {
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
				/* error++; TODO: Crafts still give errors - are there any definitions missing? */
				Com_Printf("...... technology '%s' has zero (0) produceTime, is this on purpose?\n", t->id);
			}
		}

		if ((t->type != RS_LOGIC)
		&&  ((!t->description.text[0]) || (t->description.text[0][0] == '_'))) {
			if (!t->description.text[0])
				Com_Printf("...... technology '%s' has a strange 'description' value '%s'.\n", t->id, t->description.text[0]);
			else
				Com_Printf("...... technology '%s' has no 'description' value.\n", t->id);
		}
	}

	if (!error)
		return qtrue;
	else
		return qfalse;
}
