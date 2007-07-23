/**
 * @file cl_aliencont.c
 * @brief Deals with the Alien Containment stuff.
 * @note Collecting and managing aliens functions prefix: AL_
 * @note Alien Containment menu functions prefix: AC_
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

/** Statics for menu. */
static char aliencontText[1024];		/**< Used in menu lists. */
static int numAliensOnList = 0;			/**< Number of aliens on AC menu list. */
static aliensCont_t* aliencontCurrent;		/**< Current selected Alien Containment. */

/**
 * Collecting aliens functions
 */

/**
 * @brief Prepares Alien Containment - names, states, and zeroed amount.
 * @param[in] Index of base with AC.
 * @sa B_BuildBase
 * @sa AL_AddAliens
 */
void AL_FillInContainment (int baseidx)
{
	int i, j;
	base_t *base = NULL;
	aliensCont_t *containment = NULL;

	base = &gd.bases[baseidx];
	assert (base);

	containment = base->alienscont;

	for (i = 0; i < numTeamDesc; i++) {
		if (!teamDesc[i].alien)
			continue;
		containment[i].idx = i;
		Q_strncpyz(containment[i].alientype, AL_AlienTypeToName(i), MAX_VAR);
		containment[i].amount_alive = 0;
		containment[i].amount_dead = 0;
		/* for sanity checking */
		containment[i].techIdx = -1;
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if ((Q_strncmp(containment[i].alientype, teamDesc[j].name, MAX_VAR)) == 0) {
				containment[i].techIdx = RS_GetTechIdxByName(teamDesc[j].tech);
				break;
			}
		}
		if (containment[i].techIdx == -1)
			Sys_Error("Could not find a valid tech for '%s'\n", containment[i].alientype);
		Com_DPrintf("AL_FillInContainment()... type: %s techIdx: %i\n", containment[i].alientype, containment[i].techIdx);
	}
}

/**
 * @brief Type of alien to its name.
 * @param[in] type
 * @return name (untranslated)
 */
const char *AL_AlienTypeToName (int teamDescIdx)
{
	if (teamDescIdx < 0 || teamDescIdx >= numTeamDesc)
		return "";
	return teamDesc[teamDescIdx].name;
}

/**
 * @brief Collecting stunned aliens and alien bodies after the mission.
 * @sa CL_ParseResults
 */
void AL_CollectingAliens (void)
{
	int i, j;
	int teamDescID = -1;

	le_t *le = NULL;
	aliensTmp_t *cargo = NULL;
	aircraft_t *aircraft = NULL;

	if (!baseCurrent) {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("AL_CollectingAliens()... No base selected!\n");
#endif
		return;
	}

	if ((baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	} else {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("AL_CollectingAliens()... No aircraft selected!\n");
#endif
		return;
	}

	/* Make sure dropship aliencargo is empty. */
	memset(aircraft->aliencargo,0,sizeof(aircraft->aliencargo));

	cargo = aircraft->aliencargo;
	aircraft->alientypes = 0;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;
		if ((le->type == ET_ACTOR || le->type == ET_ACTOR2x2) && le->team == TEAM_ALIEN) {
			if (le->teamDesc)
				/* -1: see Com_GetModelAndName */
				teamDescID = le->teamDesc - 1;
			else {
				Com_Printf("AL_CollectingAliens: Can't collect alien with no teamDescID\n");
				continue;
			}

			/* (le->state & STATE_DEAD) includes STATE_STUN */
			if (le->state & STATE_DEAD) {
				for (j = 0; j < aircraft->alientypes; j++) {
					/* Search alien type and increase amount */
					if (Q_strncmp(cargo[j].alientype, teamDesc[teamDescID].name, MAX_VAR) == 0) {
						/* Search stunned first. */
						if ((le->state & STATE_STUN) & ~STATE_DEAD) {
							/* alive alien */
							cargo[j].amount_alive++;
							Com_DPrintf("Counting: alive %s count: %i\n", cargo[j].alientype, cargo[j].amount_alive);
						} else {
							/* alien body */
							cargo[j].amount_dead++;
							Com_DPrintf("Counting: dead %s count: %i\n", cargo[j].alientype, cargo[j].amount_dead);
						}
						break;
					}
				}
				if (j == aircraft->alientypes) {
					/* otherwise add new alien type */
					Q_strncpyz(cargo[j].alientype, teamDesc[teamDescID].name, MAX_VAR);
					/* Search stunned first. */
					if ((le->state & STATE_STUN) & ~STATE_DEAD) {
						/* alive alien */
						cargo[j].amount_alive++;
						Com_DPrintf("Adding: alive %s count: %i\n", cargo[j].alientype, cargo[j].amount_alive);
					} else {
						/* alien body */
						cargo[j].amount_dead++;
						Com_DPrintf("Adding: dead %s count: %i\n", cargo[j].alientype, cargo[j].amount_dead);
					}
					aircraft->alientypes++;
				}
			}
		}
	}

	/* print all of them */
	for (i = 0; i < aircraft->alientypes; i++) {
		if (cargo[i].amount_dead > 0)
			Com_DPrintf("Collecting alien bodies... type: %s amount: %i\n", cargo[i].alientype, cargo[i].amount_dead);
		if (cargo[i].amount_alive > 0)
			Com_DPrintf("Alive aliens captured... type: %s amount: %i\n", cargo[i].alientype, cargo[i].amount_alive);
	}
}

/**
 * @brief Puts alien cargo into Alien Containment.
 * @param[in] baseidx Index of base in gd.bases[].
 * @param[in] airidx Global aircraft index.
 * @sa CL_DropshipReturned
 * @sa AL_FillInContainment
 */
void AL_AddAliens (int baseidx, int airidx)
{
	int i, j, k, albridx;
	base_t *tobase;
	aliensTmp_t *cargo;
	aircraft_t *aircraft;
	qboolean messageAlreadySet = qfalse;
	qboolean alienBreathing = qfalse;
	technology_t *tech;
	qboolean limit = qfalse;

	tobase = &gd.bases[baseidx];
	assert(tobase);

	if (!tobase->hasAlienCont) {
		MN_AddNewMessage(_("Notice"), _("You cannot process aliens yet. Alien Containment not ready in this base."), qfalse, MSG_STANDARD, NULL);
		return;
	}

	aircraft = AIR_AircraftGetFromIdx(airidx);
	assert(aircraft);
	cargo = aircraft->aliencargo;

	alienBreathing = RS_IsResearched_idx(RS_GetTechIdxByName("rs_alien_breathing"));
	albridx = INVSH_GetItemByID("brapparatus");

	for (i = 0; i < aircraft->alientypes; i++) {
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if (Q_strncmp(tobase->alienscont[j].alientype, cargo[i].alientype, MAX_VAR) == 0) {
				tobase->alienscont[j].amount_dead += cargo[i].amount_dead;
				/* Add breathing apparatuses as well and update storage capacity. */
				B_UpdateStorageAndCapacity(tobase, albridx, cargo[i].amount_dead, qfalse);
				if (cargo[i].amount_alive <= 0)
					continue;
				if (!alienBreathing) {
					/* we cannot store alive aliens without rs_alien_breathing tech */
					tobase->alienscont[j].amount_dead += cargo[i].amount_alive;
					/* Add breathing apparatuses as well and update storage capacity. */
					B_UpdateStorageAndCapacity(tobase, albridx, cargo[i].amount_alive, qfalse);
					/* only once */
					if (!messageAlreadySet) {
						MN_AddNewMessage(_("Notice"), _("You cannot hold alive aliens yet. Aliens died."), qfalse, MSG_STANDARD, NULL);
						messageAlreadySet = qtrue;
					}
				} else {
					for (k = 0; k < cargo[i].amount_alive; k++) {
						/* Check base capacity. */
						if (tobase->capacities[CAP_ALIENS].max > AL_CountInBase()) {
							tobase->alienscont[j].amount_alive++;
						} else {
							if (!limit) {
								/* Limit is hit. Set the amount of currently stored in capacities. */
								/* FIXME: this is needed? */
								tobase->capacities[CAP_ALIENS].cur = tobase->capacities[CAP_ALIENS].max;
								MN_AddNewMessage(_("Notice"), _("You don't have enough space in Alien Containment. Some aliens got killed."), qfalse, MSG_STANDARD, NULL);
								limit = qtrue;
								tobase->alienscont[j].amount_dead++;
							} else {
								/* Just kill aliens which don't fit the limit. */
								tobase->alienscont[j].amount_dead++;
								/* Add breathing apparatus as well and update storage capacity. */
								B_UpdateStorageAndCapacity(tobase, albridx, 1, qfalse);
							}
						}
					}
					/* only once */
					if (!messageAlreadySet) {
						MN_AddNewMessage(_("Notice"), _("You've captured new aliens."), qfalse, MSG_STANDARD, NULL);
						messageAlreadySet = qtrue;
					}
				}
			}
		}
	}
	/* Set the amount of currently stored in capacities. */
	tobase->capacities[CAP_ALIENS].cur = AL_CountInBase();

	for (i = 0; i < numTeamDesc; i++ ) {
		if (!teamDesc[i].alien)
			continue;
#ifdef DEBUG
		if (tobase->alienscont[i].techIdx == -1)
			Sys_Error("AL_AddAliens: Failed to initialize the tech for '%s'\n", tobase->alienscont[i].alientype);
#endif
		tech = RS_GetTechByIDX(tobase->alienscont[i].techIdx);
		/* we need this to let RS_Collected_ return true */
		if (tobase->alienscont[i].amount_alive + tobase->alienscont[i].amount_dead > 0)
			RS_MarkCollected(tech);
#ifdef DEBUG
		/* print all of them */
		if (tobase->alienscont[i].amount_alive > 0)
			Com_DPrintf("AL_AddAliens alive: %s amount: %i\n", tobase->alienscont[i].alientype, tobase->alienscont[i].amount_alive);
		if (tobase->alienscont[i].amount_dead > 0)
			Com_DPrintf("AL_AddAliens bodies: %s amount: %i\n", tobase->alienscont[i].alientype, tobase->alienscont[i].amount_dead);
#endif
	}
}

/**
 * @brief Removes alien(s) from Alien Containment.
 * @param[in] alientype
 * @param[in] amount of alientype to be removed
 * @param[in] type of action
 * @sa AC_KillAll_f
 * @sa AC_KillOne_f
 * @note Call with name AL_UNKNOWN when no matters what type to remove.
 * @todo integrate this with research system
 */
void AL_RemoveAliens (const char *name, int amount, alienCalcType_t action)
{
	int j, toremove;
	int maxamount = 0; /* amount (of alive type), which is max in Containment) */
	int maxidx = 0;
	aliensCont_t *containment = NULL;

	if (!baseCurrent)
		return;

	containment = baseCurrent->alienscont;

	switch (action) {
	case AL_RESEARCH:
		if (Q_strncmp("AL_UNKNOWN", name, MAX_VAR) == 0) {
			/* Search for the type of alien, which has max amount
			   in Alien Containment, then remove (amount). */
			while (amount > 0) {
				/* Find the type with maxamount. */
				for (j = 0; j < numTeamDesc; j++) {
					if (!teamDesc[j].alien)
						continue;
					if (maxamount < containment[j].amount_alive) {
						maxamount = containment[j].amount_alive;
						maxidx = j;
					}
				}
				if (maxamount == 0) {
					/* That should never happen. */
					Com_Printf("AL_RemoveAliens()... unable to find alive aliens\n");
					return;
				}
				if (maxamount == 1) {
					/* If only one here, just remove. */
					containment[maxidx].amount_alive = 0;
					containment[maxidx].amount_dead++;
					--amount;
				} else {
					/* If more than one, remove the amount. */
					toremove = maxamount - 1;
					if (toremove > amount)
						toremove = amount;
					containment[maxidx].amount_alive -= toremove;
					containment[maxidx].amount_dead += toremove;
					amount -= toremove;
				}
			}
		}
		break;
	case AL_KILL:
		/* We ignore 1st and 2nd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if (containment[j].amount_alive > 0) {
				containment[j].amount_dead += containment[j].amount_alive;
				containment[j].amount_alive = 0;
			}
		}
		break;
	case AL_KILLONE:
		/* We ignore 2nd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if (Q_strncmp(containment[j].alientype, name, MAX_VAR) == 0) {
				if (containment[j].amount_alive == 0)
					return;
				/* We are killing only one here, so we
				   don't care about amount param.   */
				containment[j].amount_alive--;
				containment[j].amount_dead++;
				return;
			}
		}
		break;
	default:
		break;
	}
	/* Set the amount of currently stored in capacities. */
	baseCurrent->capacities[CAP_ALIENS].cur = AL_CountInBase();
	return;
}

/**
 * @brief Get index of alien.
 * @param[in] *id Pointer to alien type.
 * @return index of alien
 * @sa RS_AssignTechIdxs
 */
int AL_GetAlienIdx (const char *id)
{
	int i;

	for (i = 0; i < numTeamDesc; i++) {
		if (!teamDesc[i].alien)
			continue;
		if (!Q_strncmp(id, AL_AlienTypeToName(i), MAX_VAR))
			return i;
	}

	Com_Printf("AL_GetAlien(): Alien \"%s\" not found!\n", id);
	return -1;
}

/**
 * @brief Get amount of live aliens or alien bodies stored in Containment.
 * @param[in] idx Index of alien.
 * @param[in] reqtype Requirement type (RS_LINK_ALIEN/RS_LINK_ALIEN_DEAD).
 * @return Amount of desired alien/body.
 * @sa RS_RequirementsMet
 * @sa RS_CheckCollected
 */
int AL_GetAlienAmount (int idx, requirementType_t reqtype)
{
	base_t *base = NULL;
	aliensCont_t *containment = NULL;

	if (baseCurrent) {
		base = baseCurrent;
	} else {
		/* Should never happen. */
		Com_Printf("AL_GetAlienAmount()... No base selected!\n");
		return -1;
	}

	containment = base->alienscont;
	switch (reqtype) {
	case RS_LINK_ALIEN:
		return containment[idx].amount_alive;
	case RS_LINK_ALIEN_DEAD:
		return containment[idx].amount_dead;
	default:
		return containment[idx].amount_dead;
	}
}

/**
 * Menu functions
 */

/**
 * @brief Counts alive aliens in all bases.
 * @note This should be called whenever you add or remove
 * @note aliens from alien containment.
 * @return amount of all alive aliens stored in containments
 * @sa CL_DropshipReturned
 * @sa AC_Init
 */
int AL_CountAll (void)
{
	int i, j;
	int amount = 0;
	base_t *base;

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (!base->founded)
			continue;
		if (!base->hasAlienCont)
			continue;
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if (base->alienscont[j].alientype)
				amount += base->alienscont[j].amount_alive;
		}
	}
	return amount;
}

/**
 * @brief Counts alive aliens in current base.
 * @return amount of all alive aliens stored in containment
 * @sa AC_Init
 */
int AL_CountInBase (void)
{
	int j;
	int amount = 0;
	base_t *base;

	if (baseCurrent) {
		base = baseCurrent;
	} else {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("AL_CountInBase()... No base selected!\n");
#endif
		return -1;
	}
	if (!base->hasAlienCont)
		return 0;
	for (j = 0; j < numTeamDesc; j++) {
		if (!teamDesc[j].alien)
			continue;
		if (base->alienscont[j].alientype)
			amount += base->alienscont[j].amount_alive;
	}

	return amount;
}

/**
 * @brief Counts killed or captured aliens of given type in all bases.
 * @param[in] alientype
 * @param[in] alive boolean whether the alien is alive or already dead
 * @return amount of killed aliens of given type
 * @sa AC_SelectAlien_f
 */
static int AL_CountForMenu (int alienidx, qboolean alive)
{
	int i, j;
	int amount = 0;
	base_t *base;

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (!base->founded)
			continue;
		if (!base->hasAlienCont)
			continue;
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if ((base->alienscont[j].alientype) &&
				(Q_strncmp(base->alienscont[j].alientype, AL_AlienTypeToName(alienidx), MAX_VAR)) == 0) {
				if (alive == qfalse)
					amount += base->alienscont[j].amount_dead;
				else
					amount += base->alienscont[j].amount_alive;
			}
		}
	}
	return amount;
}

/**
 * @brief Update menu cvars for selected alien.
 */
static void AC_SelectAlien_f (void)
{
	technology_t *tech = NULL;

	if (!aliencontCurrent || !baseCurrent)
		return;

	tech = RS_GetTechByIDX(aliencontCurrent->techIdx);

	if (!tech) {
		menuText[TEXT_UFOPEDIA] = NULL;
		return;
	}

	Cvar_Set("mn_al_alienmodel", tech->mdl_top);
	Cvar_Set("mn_al_alientype", _(aliencontCurrent->alientype));
	Cvar_SetValue("mn_al_techidx", tech->idx);
	Cvar_SetValue("mn_al_killed", AL_CountForMenu(aliencontCurrent->idx, qfalse));
	Cvar_SetValue("mn_al_alive", AL_CountForMenu(aliencontCurrent->idx, qtrue));

	UP_Article(tech);

	/* Set state of Research and UFOpedia buttons. */
	if (tech && RS_IsResearched_idx(tech->idx))
		Cbuf_AddText("aliencont_researchg;aliencont_ufopediab\n");
	else
		Cbuf_AddText("aliencont_researchb;aliencont_ufopediag\n");
}

/**
 * @brief Check if there is working Alien Containment in next base.
 * @sa AC_PrevAC
 * @sa AC_PrevAC_f
 * @sa AC_NextAC_f
 * @return qtrue if there is working Alien Containment in next base.
 */
static qboolean AC_NextAC (void)
{
	int i, baseID;
	qboolean found = qfalse;

	if (!baseCurrent)
		return qfalse;

	baseID = baseCurrent->idx;

	/* not all maxbases must be founded already */
	i = (baseID + 1) & (MAX_BASES - 1);
	if (i >= gd.numBases)
		i = 0; /* i -= gd.numBases - but 0 should work, too */

	for (; i < gd.numBases; i++) {
		if (!gd.bases[i].hasAlienCont)
			continue;
		if (B_CheckBuildingTypeStatus(&gd.bases[i], B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			baseID = i;
			found = qtrue;
			break;
		}
	}

	/* might still be the case */
	if (baseID == baseCurrent->idx)
		return qfalse;
	else
		return found;
}

/**
 * @brief Open menu for next Alien Containment.
 * @sa AC_NextAC
 * @sa AC_PrevAC
 * @sa AC_PrevAC_f
 */
static void AC_NextAC_f (void)
{
	int i, baseID;
	qboolean found = qfalse;

	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign || !aliencontCurrent)
		return;

	baseID = baseCurrent->idx;

	/* not all maxbases must be founded already */
	i = (baseID + 1) & (MAX_BASES - 1);
	if (i >= gd.numBases)
		i = 0; /* i -= gd.numBases - but 0 should work, too */

	for (; i < gd.numBases; i++) {
		if (!gd.bases[i].founded || !gd.bases[i].hasAlienCont)
			continue;
		if (B_CheckBuildingTypeStatus(&gd.bases[i], B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			baseID = i;
			found = qtrue;
			break;
		}
	}

	if (!found)
		baseID = baseCurrent->idx;

	if (!gd.bases[baseID].founded)
		return;
	else
		Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push aliencont\n", baseID));
}

/**
 * @brief Check if there is working Alien Containment in previous base.
 * @sa AC_PrevAC_f
 * @sa AC_NextAC
 * @sa AC_NextAC_f
 * @return qtrue if there is working Alien Containment in previous base.
 */
static qboolean AC_PrevAC (void)
{
	int i, baseID;
	qboolean found = qfalse;

	if (!baseCurrent)
		return qfalse;

	baseID = baseCurrent->idx;

	for (i = (baseID - 1) & (MAX_BASES - 1); i >= 0; i--) {
		if (!gd.bases[i].founded || !gd.bases[i].hasAlienCont)
			continue;
		if (B_CheckBuildingTypeStatus(&gd.bases[i], B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			baseID = i;
			found = qtrue;
			break;
		}
	}

	if (baseID == baseCurrent->idx)
		return qfalse;
	else
		return found;
}

/**
 * @brief Open menu for previous Alien Containment.
 * @sa AC_PrevAC
 * @sa AC_NextAC
 * @sa AC_NextAC_f
 */
static void AC_PrevAC_f (void)
{
	int i, baseID;
	qboolean found = qfalse;

	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign || !aliencontCurrent)
		return;

	baseID = baseCurrent->idx;

	for (i = (baseID - 1) & (MAX_BASES - 1); i >= 0; i--) {
		if (!gd.bases[i].hasAlienCont)
			continue;
		if (B_CheckBuildingTypeStatus(&gd.bases[i], B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			baseID = i;
			found = qtrue;
			break;
		}
	}

	if (!found)
		baseID = baseCurrent->idx;

	if (!gd.bases[baseID].founded)
		return;
	else
		Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push aliencont\n", baseID));
}

/**
 * @brief Call UFOpedia for selected alien.
 */
static void AC_OpenUFOpedia_f (void)
{
	technology_t *tech = NULL;

	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign || !aliencontCurrent)
		return;

	tech = RS_GetTechByIDX(aliencontCurrent->techIdx);

	/* Should never happen. */
	if (!tech)
		return;

	if (RS_IsResearched_ptr(tech))
		UP_OpenWith(tech->id);
}

/**
 * @brief Kill all aliens in current base.
 */
static void AC_KillAll_f (void)
{
	int i;
	qboolean aliens = qfalse;

	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign || !aliencontCurrent)
		return;

	/* Are there aliens here at all? */
	for (i = 0; i < numTeamDesc; i++) {
		if (!teamDesc[i].alien)
			continue;
		if (aliencontCurrent[i].amount_alive > 0) {
			aliens = qtrue;
			break;
		}
	}

	/* No aliens, return. */
	if (!aliens)
		return;

	AL_RemoveAliens(NULL, 0, AL_KILL);

	/* Reinit menu to display proper values. */
	AC_SelectAlien_f();
	Cbuf_AddText("aliencont_init\n");
}

/**
 * @brief Kill single alien of a given type.
 */
static void AC_KillOne_f (void)
{
	int num, i, step;
	aliensCont_t *containment = NULL;

	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign || !aliencontCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf("AC_KillOne_f: listnumber %i\n", num);

	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf("AC_KillOne_f: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (baseCurrent->hasAlienCont) {
		containment = baseCurrent->alienscont;
		for (i = 0, step = 0; i < numTeamDesc; i++) {
			if (!teamDesc[i].alien)
				continue;
			if (!containment[i].amount_alive && !containment[i].amount_dead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}
	} else
		return;

	AL_RemoveAliens(containment[num].alientype, 1, AL_KILLONE);
	/* Reinit menu to display proper values. */
	AC_SelectAlien_f();
	Cbuf_AddText("aliencont_init\n");
}

/**
 * @brief Open research menu.
 */
static void AC_ResearchAlien_f (void)
{
	int techidx;

	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign || !aliencontCurrent)
		return;

	techidx = Cvar_VariableInteger("mn_al_techidx");

	if (!techidx)
		return;

	if (!RS_IsResearched_idx(techidx))
		Cbuf_AddText("mn_push research\n");
}

/**
 * @brief Alien containment menu init function.
 * @note Command to call this: aliencont_init
 * @note Should be called whenever the alien containment menu gets active.
 */
static void AC_Init (void)
{
	int i;
	/* Tmp buffer for string generating. */
	char tmp[128];
	technology_t* tech;
	aliensCont_t *containment = NULL;
	qboolean killall_state = qfalse;

	/* Reset the aliencont list. */
	aliencontText[0] = '\0';
	numAliensOnList = 0;

	if (!baseCurrent) {
		Com_Printf("No base selected\n");
		return;
	}

	if (baseCurrent->hasAlienCont) {
		containment = baseCurrent->alienscont;
		for (i = 0; i < numTeamDesc; i++) {
			if (!teamDesc[i].alien)
				continue;
			if (containment[i].alientype) {
				tech = RS_GetTechByIDX(containment[i].techIdx);
				if (tech == NULL) {
					Com_Printf("Could not find tech '%i'\n", containment[i].techIdx);
					/* to let the click function still work */
					Q_strcat(aliencontText, "missing tech\n", sizeof(aliencontText));
					continue;
				}
				if (!aliencontCurrent) {
					aliencontCurrent = &containment[i];
				}
				if ((containment[i].amount_alive > 0) || (containment[i].amount_dead > 0)) {
					/* Set state of KillOne button. */
					if (containment[i].amount_alive > 0)
						Cbuf_AddText(va("aliencont_alienb%i\n", numAliensOnList));
					else
						Cbuf_AddText(va("aliencont_alieng%i\n", numAliensOnList));
					/* Generate a list entry. */
					Com_sprintf(tmp, sizeof(tmp), "%s\t%s\t%i\n",
						_(containment[i].alientype),
						(RS_IsResearched_ptr(tech) ? _("Yes") : _("Needs autopsy!")),
						containment[i].amount_alive);
					Q_strcat(aliencontText, tmp, sizeof(aliencontText));
					numAliensOnList++;
				}
			}
		}

		/* Disable KillOne buttons for empty entries. */
		for (i = 0; i < 10; i++) {
			if (i < numAliensOnList)
				continue;
			Cbuf_AddText(va("aliencont_aliend%i\n", i));
		}

		/* Set state of KillAll button. */
		for (i = 0; i < numTeamDesc; i++) {
			if (!teamDesc[i].alien)
				continue;
			if (containment[i].amount_alive > 0) {
				killall_state = qtrue;
				break;
			}
		}
		if (killall_state)
			Cbuf_AddText("aliencont_killb\n");
		else
			Cbuf_AddText("aliencont_killg\n");

		/* By default Research and UFOpedia buttons should be disabled. */
		Cbuf_AddText("aliencont_researchg;aliencont_ufopediag\n");

		/* State of next and prev AC buttons. */
		if (AC_NextAC())
			Cbuf_AddText("aliencont_nextbb\n");
		else
			Cbuf_AddText("aliencont_nextbg\n");
		if (AC_PrevAC())
			Cbuf_AddText("aliencont_prevbb\n");
		else
			Cbuf_AddText("aliencont_prevbg\n");

		Cvar_SetValue("mn_al_globalamount", AL_CountAll());
		Cvar_SetValue("mn_al_localamount", baseCurrent->capacities[CAP_ALIENS].cur);
		Cvar_SetValue("mn_al_capacity", baseCurrent->capacities[CAP_ALIENS].max);
		Cvar_Set("mn_al_base", baseCurrent->name);

		menuText[TEXT_STANDARD] = aliencontText;
	} else
		menuText[TEXT_STANDARD] = NULL;
	menuText[TEXT_UFOPEDIA] = NULL;
}

/**
 * @brief Click function for aliencont menu list.
 * @sa AC_Reset
 */
static void AC_AlienListClick_f (void)
{
	int num, i, step;
	aliensCont_t *containment = NULL;

	if (Cmd_Argc() < 2 || !baseCurrent) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf("AC_AlienListClick_f: listnumber %i\n", num);

	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf("AC_AlienListClick_f: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (baseCurrent->hasAlienCont) {
		containment = baseCurrent->alienscont;
		for (i = 0, step = 0; i < numTeamDesc; i++) {
			if (!teamDesc[i].alien)
				continue;
			if (!containment[i].amount_alive && !containment[i].amount_dead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}
	} else
		return;

	aliencontCurrent = &baseCurrent->alienscont[num];
	AC_SelectAlien_f();
}

/**
 * @brief Defines commands and cvars for the alien containment menu(s).
 * @sa MN_ResetMenus
 */
void AC_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("aliencont_init", AC_Init, "Init function for alien containment menu");
	Cmd_AddCommand("aliencontlist_click", AC_AlienListClick_f, "Click function for aliencont list");
	Cmd_AddCommand("aliencont_select", AC_SelectAlien_f, "Updates the menu cvars for the current selected alien");
	Cmd_AddCommand("aliencont_pedia", AC_OpenUFOpedia_f, "Opens UFOpedia entry for selected alien");
	Cmd_AddCommand("aliencont_killall", AC_KillAll_f, "Kills all aliens in current base");
	Cmd_AddCommand("aliencont_research", AC_ResearchAlien_f, "Opens research menu");
	Cmd_AddCommand("aliencont_nextbase", AC_NextAC_f, "Opens Alien Containment menu in next base");
	Cmd_AddCommand("aliencont_prevbase", AC_PrevAC_f, "Opens Alien Containment menu in previous base");
	Cmd_AddCommand("aliencont_killone", AC_KillOne_f, "Kills one alien of a given type");

	memset(aliencontText, 0, sizeof(aliencontText));
	aliencontCurrent = NULL;
}

/**
 * @brief Save callback for savegames
 * @sa AC_Load
 * @sa B_Save
 * @sa SAV_GameSave
 */
qboolean AC_Save (sizebuf_t* sb, void* data)
{
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa AC_Save
 * @sa B_Load
 * @sa SAV_GameLoad
 */
qboolean AC_Load (sizebuf_t* sb, void* data)
{
	return qtrue;
}

/**
 * @brief Returns true if the current base is able to handle captured aliens
 * @sa B_BaseInit_f
 */
qboolean AC_ContainmentAllowed (void)
{
	if (baseCurrent->baseStatus != BASE_UNDER_ATTACK
	 && B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, B_ALIEN_CONTAINMENT) > 0) {
		return qtrue;
	} else {
		return qfalse;
	}
}
