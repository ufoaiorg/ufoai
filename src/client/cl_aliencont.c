
/**
 * @file cl_aliencont.c
 * @brief Deals with the alien containment stuff
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
static char aliencontText[1024];

/* amount of aliens that are on the aliencont list - see AC_Init */
static int numAliensOnList = 0;

/* current selected aliencont */
static aliensCont_t* aliencontCurrent;

/**
 * Collecting aliens functions
 */

/**
 * @brief Prepares Alien Containment - names, states, and zeroed amount.
 * @sa B_BuildBase
 * @sa AL_AddAliens
 */
void AL_FillInContainment(void)
{
	int i, j;
	base_t *base = NULL;
	aliensCont_t *containment = NULL;

	if (baseCurrent) {
		base = baseCurrent;
	} else {
		/* should never happen */
		Com_Printf("AL_FillInContainment()... No base selected!\n");
		return;
	}

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
 * @return name
 */
char *AL_AlienTypeToName(int teamDescIdx)
{
	if (teamDescIdx < 0 || teamDescIdx >= numTeamDesc)
		return "";
	return teamDesc[teamDescIdx].name;
}

/**
 * @brief Collecting stunned aliens and alien bodies after the mission.
 * @sa CL_ParseResults
 */
void CL_CollectingAliens(void)
{
	int i, j;
	int teamDescID = -1;

	le_t *le = NULL;
	aliensTmp_t *cargo = NULL;
	aircraft_t *aircraft = NULL;

	if (!baseCurrent) {
		/* should never happen */
		Com_Printf("CL_CollectingAliens()... No base selected!\n");
		return;
	}

	if (baseCurrent->aircraftCurrent >= 0) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	} else {
		/* should never happen */
		Com_Printf("CL_CollectingAliens()... No aircraft selected!\n");
		return;
	}

	/* make sure dropship aliencargo is empty */
	memset(aircraft->aliencargo,0,sizeof(aircraft->aliencargo));

	cargo = aircraft->aliencargo;
	aircraft->alientypes = 0;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;
		if ((le->type == ET_ACTOR || le->type == ET_UGV) && le->team == TEAM_ALIEN) {
			if (le->teamDesc)
				/* -1: see Com_GetModelAndName */
				teamDescID = le->teamDesc - 1;
			else {
				Com_Printf("CL_CollectingAliens: Can't collect alien with no teamDescID\n");
				continue;
			}

			if (le->HP <= 0 || (le->state & STATE_STUN)) {
				for (j = 0; j < aircraft->alientypes; j++) {
					/* Search alien type and increase amount */
					if (Q_strncmp(cargo[j].alientype, teamDesc[teamDescID].name, MAX_VAR) == 0) {
						if (le->HP <= 0) {
							/* alien body */
							cargo[j].amount_dead++;
							Com_DPrintf("Counting: dead %s count: %i\n", cargo[j].alientype, cargo[j].amount_dead);
						} else {
							/* alive alien */
							cargo[j].amount_alive++;
							Com_DPrintf("Counting: alive %s count: %i\n", cargo[j].alientype, cargo[j].amount_alive);
						}
						break;
					}
				}
				if (j == aircraft->alientypes) {
					/* otherwise add new alien type */
					Q_strncpyz(cargo[j].alientype, teamDesc[teamDescID].name, MAX_VAR);
					if (le->HP <= 0) {
						/* alien body */
						cargo[j].amount_dead++;
						Com_DPrintf("Adding: dead %s count: %i\n", cargo[j].alientype, cargo[j].amount_dead);
					} else {
						/* alive alien */
						cargo[j].amount_alive++;
						Com_DPrintf("Adding: alive %s count: %i\n", cargo[j].alientype, cargo[j].amount_alive);
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
 * @sa CL_DropshipReturned
 * @sa AL_FillInContainment
 */
void AL_AddAliens(void)
{
	int i, j;
	base_t *tobase = NULL;
	aliensTmp_t *cargo = NULL;
	aircraft_t *aircraft = NULL;
	qboolean messageAlreadySet = qfalse;
	qboolean alienBreathing = qfalse;
	technology_t *tech;

	if (baseCurrent) {
		tobase = baseCurrent;
	} else {
		/* should never happen */
		Com_Printf("AL_AddAliens()... No base selected!\n");
		return;
	}

	if (!tobase->hasAlienCont) {
		MN_AddNewMessage(_("Notice"), _("You cannot process aliens yet. Alien Containment not ready in this base."), qfalse, MSG_STANDARD, NULL);
		return;
	}

	if (tobase->aircraftCurrent >= 0) {
		aircraft = &tobase->aircraft[tobase->aircraftCurrent];
	} else {
		/* should never happen */
		Com_Printf("AL_AddAliens()... No aircraft selected!\n");
		return;
	}

	cargo = aircraft->aliencargo;

	alienBreathing = RS_IsResearched_idx(RS_GetTechIdxByName("rs_alien_breathing"));

	for (i = 0; i < aircraft->alientypes; i++) {
		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if (Q_strncmp(tobase->alienscont[j].alientype, cargo[i].alientype, MAX_VAR) == 0) {
				tobase->alienscont[j].amount_dead += cargo[i].amount_dead;
				if (cargo[i].amount_alive <= 0)
					continue;
				if (!alienBreathing) {
					/* we cannot store alive aliens without rs_alien_breathing tech */
					tobase->alienscont[j].amount_dead += cargo[i].amount_alive;
					/* only once */
					if (!messageAlreadySet) {
						MN_AddNewMessage(_("Notice"), _("You cannot hold alive aliens yet. Aliens died."), qfalse, MSG_STANDARD, NULL);
						messageAlreadySet = qtrue;
					}
				} else {
					tobase->alienscont[j].amount_alive += cargo[i].amount_alive;
					/* only once */
					if (!messageAlreadySet) {
						MN_AddNewMessage(_("Notice"), _("You've captured new aliens."), qfalse, MSG_STANDARD, NULL);
						messageAlreadySet = qtrue;
					}
				}
			}
		}
	}

	for (i = 0; i < numTeamDesc; i++ ) {
		if (!teamDesc[i].alien)
			continue;
#ifdef DEBUG
		if (tobase->alienscont[i].techIdx == -1)
			Sys_Error("AL_AddAliens: Failed to initialize the tech for '%s'\n", tobase->alienscont[i].alientype);
#endif
		tech = RS_GetTechByIDX(tobase->alienscont[i].techIdx);
		/* we need this to let RS_Collected_ return true */
		tech->statusCollected = tobase->alienscont[i].amount_alive + tobase->alienscont[i].amount_dead;
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
 * @note Call it with alientype AL_UNKNOWN when it does not
 * matter what type.
 * @TODO integrate this with research system
 */
void AL_RemoveAliens(alienType_t alientype, int amount, alienCalcType_t action)
{
	int j;
	int maxamount = 0; /* amount (of alive type), which is max in Containment) */
	int maxidx = 0;
	char name[MAX_VAR];
	base_t *base = NULL;
	aliensCont_t *containment = NULL;

	if (baseCurrent) {
		base = baseCurrent;
	} else {
		/* should never happen */
		Com_Printf("AL_RemoveAliens()... No base selected!\n");
		return;
	}

	containment = base->alienscont;
	Q_strncpyz(name, AL_AlienTypeToName(alientype), MAX_VAR);

	switch (action) {
	case AL_RESEARCH:
		if (alientype == AL_UNKNOWN) {
			/* search for the type of alien, which has max amount
			   in Alien Containment; then remove (amount) */
			/* FIXME: do not let to remove to negative value */
			for (j = 0; j < numTeamDesc; j++) {
				if (!teamDesc[j].alien)
					continue;
				if (maxamount < containment[j].amount_alive) {
					maxamount = containment[j].amount_alive;
					maxidx = j;
				}
			}

			containment[maxidx].amount_alive -= amount;
			return;
		}

		for (j = 0; j < numTeamDesc; j++) {
			if (!teamDesc[j].alien)
				continue;
			if (Q_strncmp(containment[j].alientype, name, MAX_VAR) == 0) {
				containment[j].amount_alive -= amount;
				return;
			}
		}
	case AL_KILL:
		/* TODO: killing aliens during base defence */
	default:
		return;
	}
}

/**
 * @brief Get index of alien.
 * @param[in] alientype
 * @return index of alien
 * @sa RS_AssignTechIdxs
 */
int AL_GetAlienIdx(char *id)
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
 * @param[in] index of alien
 * @param[in] requirement type (RS_LINK_ALIEN/RS_LINK_ALIEN_DEAD)
 * @return amount of desired alien/body
 * @sa RS_RequirementsMet
 * @sa RS_CheckCollected
 */
int AL_GetAlienAmount(int idx, requirementType_t reqtype)
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

	return -1;
}

/**
 * Menu functions
 */

/**
 * @brief Counts alive aliens in all bases.
 * @note This should be called whenever you add or remove
 * aliens from alien containment.
 * @return amount of all alive aliens stored in containments
 * @sa CL_DropshipReturned
 * @sa AC_Init
 */
int AL_CountAll(void)
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
int AL_CountInBase(void)
{
	int j;
	int amount = 0;
	base_t *base;

	if (baseCurrent) {
		base = baseCurrent;
	} else {
		/* should never happen */
		Com_Printf("AL_CountInBase()... No base selected!\n");
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
 * @brief Counts killed aliens of given type in all bases.
 * @param[in] alientype
 * @param[in] alive boolean whether the alien is alive or already dead
 * @return amount of killed aliens of given type
 * @sa AC_SelectAlien_f
 */
static int AL_CountForMenu(int alienidx, qboolean alive)
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
 * @brief Update menu cvars for selected alien
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
	Cvar_SetValue("mn_al_killed", AL_CountForMenu(aliencontCurrent->idx, qfalse));
	Cvar_SetValue("mn_al_alive", AL_CountForMenu(aliencontCurrent->idx, qtrue));

	UP_Article(tech);
}

/**
 * @brief Alien containment menu init function
 * @note Command to call this: aliencont_init
 * @note Should be called whenever the research menu gets active
 */
static void AC_Init (void)
{
	int i;
	/* tmp buffer for string generating */
	char tmp[128];
	technology_t* tech;
	aliensCont_t *containment = NULL;

	/* reset the aliencont list */
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
					Com_sprintf(tmp, sizeof(tmp), "%s\t%s\t%i\n",
						_(containment[i].alientype),
						(RS_IsResearched_ptr(tech) ? _("Yes") : _("Needs autopsy!")),
						containment[i].amount_alive); /* FIXME: what about the dead ones */
					Q_strcat(aliencontText, tmp, sizeof(aliencontText));
					numAliensOnList++;
				}
			}
		}
	}

	Cvar_SetValue("mn_al_globalamount", AL_CountAll());
	Cvar_SetValue("mn_al_localamount", AL_CountInBase());

	menuText[TEXT_STANDARD] = aliencontText;
	menuText[TEXT_UFOPEDIA] = NULL;
}

/**
 * @brief Click function for aliencont menu list
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
 * @brief Defines commands and cvars for the alien containment menu(s)
 * @sa MN_ResetMenus
 */
void AC_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("aliencont_init", AC_Init, "Init function for alien containment menu");
	Cmd_AddCommand("aliencontlist_click", AC_AlienListClick_f, "Click function for aliencont list");
	Cmd_AddCommand("aliencont_select", AC_SelectAlien_f, "Updates the menu cvars for the current selected alien");

	/* add cvars */
	/* TODO */

	memset(aliencontText, 0, sizeof(aliencontText));
	aliencontCurrent = NULL;
}
