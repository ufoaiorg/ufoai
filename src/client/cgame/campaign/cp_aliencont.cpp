/**
 * @file
 * @brief Deals with the Alien Containment stuff.
 * @note Collecting and managing aliens functions prefix: AL_
 * @note Alien Containment menu functions prefix: AC_
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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
#include "cp_capacity.h"
#include "cp_aliencont_callbacks.h"
#include "save/save_aliencont.h"
#include "aliencargo.h"

/**
 * Collecting aliens functions for aircraft
 */

/**
 * @brief Adds an alientype to an aircraft cargo
 * @param[in] aircraft The aircraft that owns the alien cargo to add the alien race to
 * @param[in] teamDef The team definition of the alien race to add to the alien cargo container of the given aircraft
 * @param[in] amount The amount of aliens of the given race (@c teamDef ) that should be added to the alien cargo
 * @param[in] dead true for cases where the aliens should be added as dead to the alien cargo - false for living aliens
 */
bool AL_AddAlienTypeToAircraftCargo (aircraft_t *aircraft, const teamDef_t *teamDef, int amount, bool dead)
{
	if (aircraft->alienCargo == NULL)
		aircraft->alienCargo = new AlienCargo();
	if (aircraft->alienCargo == NULL)
		return false;

	if (dead)
		return aircraft->alienCargo->add(teamDef, 0, amount);
	else
		return aircraft->alienCargo->add(teamDef, amount, 0);
}

/**
 * General Collecting aliens functions
 */

/**
 * @brief Prepares Alien Containment - names, states, and zeroed amount.
 * @param[in] base Pointer to the base with AC.
 * @sa B_BuildBase
 * @sa AL_AddAliens
 */
void AL_FillInContainment (base_t *base)
{
	int i, counter = 0;
	aliensCont_t *containment;

	assert(base);
	containment = base->alienscont;

	for (i = 0; i < cgi->csi->numTeamDefs; i++) {
		const teamDef_t *td = &cgi->csi->teamDef[i];
		if (!CHRSH_IsTeamDefAlien(td))
			continue;
		if (counter >= MAX_ALIENCONT_CAP)
			cgi->Com_Error(ERR_DROP, "Overflow in AL_FillInContainment");
		containment->teamDef = td;	/* Link to global race index. */
		containment->amountAlive = 0;
		containment->amountDead = 0;
		/* for sanity checking */
		containment->tech = ccs.teamDefTechs[td->idx];
		if (!containment->tech)
			cgi->Com_Error(ERR_DROP, "Could not find a valid tech for '%s'\n", td->name);
		Com_DPrintf(DEBUG_CLIENT, "AL_FillInContainment: type: %s tech-index: %i\n", td->name, containment->tech->idx);
		containment++;
		counter++;
	}
	CAP_SetCurrent(base, CAP_ALIENS, 0);
}

/**
 * @brief Puts alien cargo into Alien Containment.
 * @param[in] aircraft Aircraft transporting cargo to homebase.
 * @sa B_AircraftReturnedToHomeBase
 * @sa AL_FillInContainment
 * @note an event mail about missing breathing tech will be triggered if necessary.
 */
void AL_AddAliens (aircraft_t *aircraft)
{
	base_t *toBase;
	int i;
	int j;
	bool limit = false;
	bool messageAlreadySet = false;
	bool alienBreathing = false;

	assert(aircraft);
	toBase = aircraft->homebase;
	assert(toBase);

	if (aircraft->alienCargo == NULL)
		return;

	if (!B_GetBuildingStatus(toBase, B_ALIEN_CONTAINMENT)) {
		MS_AddNewMessage(_("Notice"), _("You cannot process aliens yet. Alien Containment not ready in this base."));
		return;
	}

	technology_t *breathingTech = RS_GetTechByID(BREATHINGAPPARATUS_TECH);
	if (!breathingTech)
		cgi->Com_Error(ERR_DROP, "AL_AddAliens: Could not get breathing apparatus tech definition");
	alienBreathing = RS_IsResearched_ptr(breathingTech);

	linkedList_t *cargo = aircraft->alienCargo->list();
	LIST_Foreach(cargo, alienCargo_t, item) {
		for (j = 0; j < ccs.numAliensTD; j++) {
			aliensCont_t *ac = &toBase->alienscont[j];
			assert(ac->teamDef);

			if (ac->teamDef == item->teamDef) {
				const bool isRobot = CHRSH_IsTeamDefRobot(item->teamDef);
				ac->amountDead += item->dead;
				aircraft->alienCargo->add(item->teamDef, 0, -item->dead);
				ccs.campaignStats.killedAliens += item->dead;

				if (item->alive <= 0)
					break;

				if (!alienBreathing && !isRobot) {
					/* We can not store living (i.e. no robots or dead bodies) aliens without rs_alien_breathing tech */
					ac->amountDead += item->alive;
					aircraft->alienCargo->add(item->teamDef, -item->alive, 0);
					ccs.campaignStats.killedAliens += item->alive;
					CP_TriggerEvent(CAPTURED_ALIENS_DIED, NULL);
					/* only once */
					if (!messageAlreadySet) {
						MS_AddNewMessage(_("Notice"), _("You can't hold live aliens yet. Aliens died."), MSG_DEATH);
						messageAlreadySet = true;
					}
				} else {
					for (int k = 0; k < item->alive; k++) {
						/* Check base capacity. */
						if (AL_CheckAliveFreeSpace(toBase, NULL, 1)) {
							AL_ChangeAliveAlienNumber(toBase, ac, 1);
							aircraft->alienCargo->add(item->teamDef, -1, 0);
							ccs.campaignStats.capturedAliens++;
						} else {
							/* Every exceeding alien is killed
							 * Display a message only when first one is killed */
							if (!limit) {
								CAP_SetCurrent(toBase, CAP_ALIENS, CAP_GetMax(toBase, CAP_ALIENS));
								MS_AddNewMessage(_("Notice"), _("You don't have enough space in Alien Containment. Some aliens got killed."));
								limit = true;
							}
							/* Just kill aliens which don't fit the limit. */
							ac->amountDead++;
							aircraft->alienCargo->add(item->teamDef, -1, 0);
							ccs.campaignStats.killedAliens++;
						}
					}
					CP_TriggerEvent(CAPTURED_ALIENS, NULL);
					/* only once */
					if (!messageAlreadySet) {
						MS_AddNewMessage(_("Notice"), _("You've captured new aliens."));
						messageAlreadySet = true;
					}
				}
				break;
			}
		}
	}
	LIST_Delete(&cargo);

	for (i = 0; i < ccs.numAliensTD; i++) {
		aliensCont_t *ac = &toBase->alienscont[i];
		technology_t *tech = ac->tech;
#ifdef DEBUG
		if (!tech)
			Sys_Error("AL_AddAliens: Failed to initialize the tech for '%s'", ac->teamDef->name);
#endif
		/* we need this to let RS_Collected_ return true */
		if (ac->amountAlive + ac->amountDead > 0)
			RS_MarkCollected(tech);
#ifdef DEBUG
		/* print all of them */
		if (ac->amountAlive > 0)
			Com_DPrintf(DEBUG_CLIENT, "AL_AddAliens alive: %s amount: %i\n", ac->teamDef->name, ac->amountAlive);
		if (ac->amountDead > 0)
			Com_DPrintf(DEBUG_CLIENT, "AL_AddAliens bodies: %s amount: %i\n", ac->teamDef->name, ac->amountDead);
#endif
	}
}

/**
 * @brief Removes alien(s) from Alien Containment.
 * @param[in,out] base Pointer to the base where we will perform action (remove, add, ... aliens).
 * @param[in] alienType Type of the alien (a teamDef_t pointer)
 * @param[in] amount Amount of aliens to be removed.
 * @param[in] action Type of action (see alienCalcType_t).
 * @sa AC_KillAll_f
 * @sa AC_KillOne_f
 * @note Call with NULL name when no matters what type to remove.
 * @todo integrate this with research system
 */
void AL_RemoveAliens (base_t *base, const teamDef_t *alienType, int amount, const alienCalcType_t action)
{
	int j, toremove;
	aliensCont_t *containment;

	assert(base);
	containment = base->alienscont;

	switch (action) {
	case AL_RESEARCH:
		if (!alienType) {
			int maxidx = 0;
			int maxamount = 0; /* amount (of alien type), which is max in Containment) */
			/* Search for the type of alien, which has max amount
			 * in Alien Containment, then remove (amount). */
			while (amount > 0) {
				/* Find the type with maxamount. */
				for (j = 0; j < ccs.numAliensTD; j++) {
					const aliensCont_t *ac = &containment[j];
					if (maxamount < ac->amountAlive) {
						maxamount = ac->amountAlive;
						maxidx = j;
					}
				}
				if (maxamount == 0) {
					/* That should never happen. */
					Com_Printf("AL_RemoveAliens: unable to find alive aliens\n");
					return;
				}
				if (maxamount == 1) {
					/* If only one here, just remove. */
					AL_ChangeAliveAlienNumber(base, &containment[maxidx], -1);
					containment[maxidx].amountDead++;
					--amount;
				} else {
					/* If more than one, remove the amount. */
					toremove = maxamount - 1;
					if (toremove > amount)
						toremove = amount;
					AL_ChangeAliveAlienNumber(base, &containment[maxidx], -toremove);
					containment[maxidx].amountDead += toremove;
					amount -= toremove;
				}
			}
		}
		break;
	case AL_KILL:
		/* We ignore 2nd and 3rd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < ccs.numAliensTD; j++) {
			aliensCont_t *ac = &containment[j];
			if (ac->amountAlive > 0) {
				ac->amountDead += ac->amountAlive;
				AL_ChangeAliveAlienNumber(base, ac, -ac->amountAlive);
			}
		}
		break;
	case AL_KILLONE:
		/* We ignore 3rd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < ccs.numAliensTD; j++) {
			aliensCont_t *ac = &containment[j];
			assert(ac->teamDef);
			if (ac->teamDef == alienType) {
				if (ac->amountAlive == 0)
					return;
				/* We are killing only one here, so we
				 * don't care about amount param. */
				AL_ChangeAliveAlienNumber(base, ac, -1);
				ac->amountDead++;
				break;
			}
		}
		break;
	default:
		Sys_Error("AL_RemoveAliens: Use AL_AddAliens for action %i", action);
	}
}

/**
 * @brief adds one alien to a container
 * @param[out] base Pointer to the base of the alien containment
 * @param[in] alienType Pointer to the team definition (alien race)
 * @param[in] dead If the alien to add is dead
 */
void AL_AddAlienToContainer (base_t *base, const teamDef_t *alienType, const bool dead)
{
	int j;
	aliensCont_t *containment;

	assert(base);
	containment = base->alienscont;

	if (dead) {
		for (j = 0; j < ccs.numAliensTD; j++) {
			aliensCont_t *ac = &containment[j];
			assert(ac->teamDef);
			if (ac->teamDef == alienType) {
				ac->amountDead++;
				break;
			}
		}
	} else {
		/* We ignore 3rd parameter of AL_RemoveAliens() here: add only 1 alien */
		if (!AL_CheckAliveFreeSpace(base, NULL, 1)) {
			return; /* stop because we will else exceed the max of aliens */
		}
		for (j = 0; j < ccs.numAliensTD; j++) {
			aliensCont_t *ac = &containment[j];
			assert(ac->teamDef);
			if (ac->teamDef == alienType) {
				AL_ChangeAliveAlienNumber(base, ac, 1);
				break;
			}
		}
	}
}

/**
 * @brief Get index of alien.
 * @param[in] alienType Pointer to alien type.
 * @return Index of alien in alien containment (so less than @c ccs.numAliensTD)
 * @note It does NOT return the global team index from @c cgi->csi->teamDef array.
 * That would be @c alienType->idx
 * @sa RS_AssignTechLinks
 * @sa AL_GetAlienGlobalIDX
 */
static int AL_GetAlienIDX (const teamDef_t *alienType)
{
	int i, index;

	index = 0;
	for (i = 0; i < cgi->csi->numTeamDefs; i++) {
		const teamDef_t *td = &cgi->csi->teamDef[i];
		if (alienType == td)
			return index;
		if (CHRSH_IsTeamDefAlien(td))
			index++;
	}

	Com_Printf("AL_GetAlienIDX: Alien \"%s\" not found!\n", alienType->id);
	return -1;
}

/**
 * @brief Returns teamdef for global alien idx.
 * @param[in] alienTeamDefIdx Alien index
 * @sa AL_GetAlienIDX
 */
const teamDef_t* AL_GetAlienTeamDef (int alienTeamDefIdx)
{
	int i, counter = 0;

	for (i = 0; i < cgi->csi->numTeamDefs; i++) {
		const teamDef_t *td = &cgi->csi->teamDef[i];
		if (CHRSH_IsTeamDefAlien(td)) {
			if (counter == alienTeamDefIdx)
				return td;
			counter++;
		}
	}
	Com_Printf("AL_GetAlienGlobalIDX: Alien with AC index %i not found!\n", alienTeamDefIdx);
	return NULL;
}

/**
 * @brief Get amount of live aliens or alien bodies stored in Containment.
 * @param[in] alienType The alien type to check for
 * @param[in] base The base to count in
 * @param[in] reqtype Requirement type (RS_LINK_ALIEN/RS_LINK_ALIEN_DEAD).
 * @return Amount of desired alien/body.
 * @sa RS_RequirementsMet
 * @sa RS_CheckCollected
 */
int AL_GetAlienAmount (const teamDef_t *alienType, requirementType_t reqtype, const base_t *base)
{
	const aliensCont_t *containment;
	int alienTypeIndex;

	assert(alienType);
	assert(base);
	alienTypeIndex = AL_GetAlienIDX(alienType);
	assert(alienTypeIndex >= 0);
	containment = &base->alienscont[alienTypeIndex];

	switch (reqtype) {
	case RS_LINK_ALIEN:
		return containment->amountAlive;
	case RS_LINK_ALIEN_DEAD:
		return containment->amountDead;
	default:
		return containment->amountDead;
	}
}

/**
 * @brief Counts live aliens in base.
 * @param[in] base Pointer to the base
 * @return amount of all live aliens stored in containment
 * @note must not return 0 if hasBuilding[B_ALIEN_CONTAINMENT] is false: used to update capacity
 * @sa AL_ChangeAliveAlienNumber
 * @sa B_ResetAllStatusAndCapacities_f
 */
int AL_CountInBase (const base_t *base)
{
	int j;
	int amount = 0;

	assert(base);

	for (j = 0; j < ccs.numAliensTD; j++) {
		if (base->alienscont[j].teamDef)
			amount += base->alienscont[j].amountAlive;
	}

	return amount;
}

/**
 * @brief Add / Remove live aliens to Alien Containment.
 * @param[in] base Pointer to the base where Alien Cont. should be checked.
 * @param[in] containment Pointer to the containment
 * @param[in] num Number of alien to be added/removed
 * @pre free space has already been checked
 * @todo handle containment[j].amountDead++; in case the @c num is negative?
 */
void AL_ChangeAliveAlienNumber (base_t *base, aliensCont_t *containment, int num)
{
	assert(base);
	assert(containment);

	/* Just a sanity check -- should never be reached */
	if (!AL_CheckAliveFreeSpace(base, containment, num))
		cgi->Com_Error(ERR_DROP, "AL_ChangeAliveAlienNumber: Can't add/remove %i live aliens, (capacity: %i/%i, Alien Containment Status: %i)\n",
			num, CAP_GetCurrent(base, CAP_ALIENS), CAP_GetMax(base, CAP_ALIENS),
			B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT));

	containment->amountAlive += num;
	CAP_AddCurrent(base, CAP_ALIENS, num);

#ifdef DEBUG
	if (CAP_GetCurrent(base, CAP_ALIENS) != AL_CountInBase(base))
		Com_Printf("AL_ChangeAliveAlienNumber: Wrong capacity in Alien containment: %i instead of %i\n",
			CAP_GetCurrent(base, CAP_ALIENS), AL_CountInBase(base));
#endif
}

/**
 * @brief Remove aliens that exceed containment capacity
 * @note called on destroying an Alien Containment (from building_ondestroy)
 * @param[in, out] base Pointer to the base to check
 */
void AL_RemoveAliensExceedingCapacity (base_t *base)
{
	const int max = CAP_GetMax(base, CAP_ALIENS);
	int current = CAP_GetCurrent(base, CAP_ALIENS);
	int i;

	assert(base);
	assert(max >= 0);

	for (i = 0; i < ccs.numAliensTD; i++) {
		const int remove = std::min(base->alienscont[i].amountAlive, current - max);

		if (!base->alienscont[i].teamDef)
			continue;

		/* remove dead aliens if there is no alien containment */
		if (max == 0)
			base->alienscont[i].amountDead =  0;

		if (remove > 0) {
			base->alienscont[i].amountAlive -= remove;
			CAP_SetCurrent(base, CAP_ALIENS, current - remove);
			current = CAP_GetCurrent(base, CAP_ALIENS);
		}
	}

	assert(max >= current);
}

/**
 * @brief Check if live aliens can be added/removed to Alien Containment.
 * @param[in] base Pointer to the base where Alien Cont. should be checked.
 * @param[in] containment Pointer to the containment (may be @c NULL when adding
 * aliens or if you don't care about alien type of alien you're removing)
 * @param[in] num Number of alien to be added/removed
 * @return true if action may be performed in base
 */
bool AL_CheckAliveFreeSpace (const base_t *base, const aliensCont_t *containment, const int num)
{
	if (num > 0) {
		/* We add aliens */
		/* you need Alien Containment and it's dependencies to handle aliens */
		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			return false;
		if (CAP_GetFreeCapacity(base, CAP_ALIENS) < num)
			return false;
	} else {
		/* @note don't check building status here.
		 * dependencies may have been destroyed before alien container (B_Destroy) */
		if (CAP_GetCurrent(base, CAP_ALIENS) + num < 0)
			return false;
		if (containment && (containment->amountAlive + num < 0))
			return false;
	}

	return true;
}

/**
 * Menu functions
 */

/**
 * @brief Counts live aliens in all bases.
 * @note This should be called whenever you add or remove
 * @note aliens from alien containment.
 * @return amount of all live aliens stored in containments
 * @sa B_AircraftReturnedToHomeBase
 * @sa AC_Init_f
 */
int AL_CountAll (void)
{
	int amount = 0;
	base_t *base = NULL;

	while ((base = B_GetNext(base)) != NULL) {
		int j;
		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			continue;
		for (j = 0; j < ccs.numAliensTD; j++) {
			const aliensCont_t *ac = &base->alienscont[j];
			if (ac->teamDef)
				amount += ac->amountAlive;
		}
	}
	return amount;
}

/**
 * @brief Kill all aliens in given base.
 * @param[in] base The base in which you want to kill all aliens
 * @sa AC_KillAll_f
 */
void AC_KillAll (base_t *base)
{
	int i;
	bool aliens = false;

	assert(base);

	/* Are there aliens here at all? */
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (base->alienscont[i].amountAlive > 0) {
			aliens = true;
			break;
		}
	}

	/* No aliens, return. */
	if (!aliens)
		return;

	AL_RemoveAliens(base, NULL, 0, AL_KILL);
}

#ifdef DEBUG
/**
 * @brief Add a single alien of a given type.
 */
static void AC_AddOne_f (void)
{
	const char *alienName;
	const teamDef_t *alienType;
	aliensCont_t *containment;
	bool updateAlive = true;
	int j;
	base_t *base = B_GetCurrentSelectedBase();

	/* Can be called from everywhere. */
	if (!base)
		return;

	/* arg parsing */
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <alientype> [dead:true|false]\n", cgi->Cmd_Argv(0));
		return;
	}

	alienName = cgi->Cmd_Argv(1);
	alienType = cgi->Com_GetTeamDefinitionByID(alienName);

	if (!alienType) {
		Com_Printf("AC_AddOne_f: Team definition '%s' does not exist.\n", alienName);
		return;
	}

	/* Check that alientType exists */
	containment = base->alienscont;
	for (j = 0; j < ccs.numAliensTD; j++) {
		aliensCont_t *ac = &containment[j];
		assert(ac->teamDef);
		if (ac->teamDef == alienType)
			break;
	}
	if (j == ccs.numAliensTD) {
		Com_Printf("AC_AddOne_f: Alien Type '%s' does not exist. Available choices are:\n", alienName);
		for (j = 0; j < ccs.numAliensTD; j++)
			Com_Printf("\t* %s\n", containment[j].teamDef->name);
		return;
	}

	if (cgi->Cmd_Argc() == 3)
		updateAlive = Com_ParseBoolean(cgi->Cmd_Argv(2));

	if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		return;
	}

	/* call the function that actually changes the persistent datastructure */
	AL_AddAlienToContainer(base, alienType, !updateAlive);
}
#endif

/**
 * @brief Defines commands and cvars for the alien containment menu(s).
 * @sa UI_InitStartup
 */
void AC_InitStartup (void)
{
	/* add commands */
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_addalientocont", AC_AddOne_f, "Add one alien of a given type");
#endif
	AC_InitCallbacks();
}

/**
 * @brief Savecallback for saving in XML Format
 * @sa AC_LoadXML
 * @sa B_SaveXML
 * @sa SAV_GameSaveXML
 */
bool AC_SaveXML (xmlNode_t * parent)
{
	xmlNode_t *aliencont;
	base_t *base;

	aliencont = cgi->XML_AddNode(parent, SAVE_ALIENCONT_ALIENCONT);

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		xmlNode_t *node;
		int k;

		if (!AC_ContainmentAllowed(base))
			continue;

		node = cgi->XML_AddNode(aliencont, SAVE_ALIENCONT_CONT);
		cgi->XML_AddInt(node, SAVE_ALIENCONT_BASEIDX, base->idx);
		for (k = 0; k < MAX_ALIENCONT_CAP && k < ccs.numAliensTD; k++) {
			xmlNode_t * snode = cgi->XML_AddNode(node, SAVE_ALIENCONT_ALIEN);

			assert(base->alienscont);
			assert(base->alienscont[k].teamDef);
			assert(base->alienscont[k].teamDef->id);

			cgi->XML_AddString(snode, SAVE_ALIENCONT_TEAMID, base->alienscont[k].teamDef->id);
			cgi->XML_AddIntValue(snode, SAVE_ALIENCONT_AMOUNTALIVE, base->alienscont[k].amountAlive);
			cgi->XML_AddIntValue(snode, SAVE_ALIENCONT_AMOUNTDEAD, base->alienscont[k].amountDead);
		}
	}

	return true;
}

/**
 * @brief Load callback for savin in XML Format
 * @sa AC_LoadXML
 * @sa B_SaveXML
 * @sa SAV_GameLoadXML
 */
bool AC_LoadXML (xmlNode_t * parent)
{
	xmlNode_t *aliencont;
	xmlNode_t *contNode;
	int i;

	aliencont = cgi->XML_GetNode(parent, SAVE_ALIENCONT_ALIENCONT);

	/* Init alienContainers */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetBaseByIDX(i);

		AL_FillInContainment(base);
	}
	/* Load data */
	for (contNode = cgi->XML_GetNode(aliencont, SAVE_ALIENCONT_CONT); contNode;
			contNode = cgi->XML_GetNextNode(contNode, aliencont, SAVE_ALIENCONT_CONT)) {
		int j = cgi->XML_GetInt(contNode, SAVE_ALIENCONT_BASEIDX, MAX_BASES);
		base_t *base = B_GetFoundedBaseByIDX(j);
		int k;
		xmlNode_t *alienNode;

		if (!base) {
			Com_Printf("AC_LoadXML: Invalid base idx '%i'\n", j);
			continue;
		}

		for (k = 0, alienNode = cgi->XML_GetNode(contNode, SAVE_ALIENCONT_ALIEN); alienNode && k < MAX_ALIENCONT_CAP; alienNode = cgi->XML_GetNextNode(alienNode, contNode, SAVE_ALIENCONT_ALIEN), k++) {
			const char *const s = cgi->XML_GetString(alienNode, SAVE_ALIENCONT_TEAMID);
			/* Fill Alien Containment with default values like the tech pointer. */
			base->alienscont[k].teamDef = cgi->Com_GetTeamDefinitionByID(s);
			if (base->alienscont[k].teamDef) {
				base->alienscont[k].amountAlive = cgi->XML_GetInt(alienNode, SAVE_ALIENCONT_AMOUNTALIVE, 0);
				base->alienscont[k].amountDead = cgi->XML_GetInt(alienNode, SAVE_ALIENCONT_AMOUNTDEAD, 0);
			}
		}
	}

	return true;
}

/**
 * @brief Returns true if the current base is able to handle captured aliens
 * @sa B_BaseInit_f
 * @note Alien cont. must be accessible during base attack to kill aliens.
 */
bool AC_ContainmentAllowed (const base_t* base)
{
	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		return true;
	} else {
		return false;
	}
}
