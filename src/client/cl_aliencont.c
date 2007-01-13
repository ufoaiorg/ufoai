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
aliensTmp_t cargo[MAX_CARGO];

/**
 *Collecting aliens functions
 */

/**
 * @brief Prepares Alien Containment - names, states, and zeroed amount.
 * @sa B_BuildBase
 */
void AL_FillInContainment(void)
{
	int i, j;
	char name[MAX_VAR];
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

	for (i = 0, j = 0; i < (AL_UNKNOWN * 2); i+=2, j++) {
		Q_strncpyz(name, AL_AlienTypeToName(j), MAX_VAR);
		/* dead */
		containment[i].idx = i;
		Q_strncpyz(containment[i].alientype, name, MAX_VAR);
		containment[i].state = 0;
		containment[i].amount = 0;
		/* alive */
		containment[i+1].idx = i+1;
		Q_strncpyz(containment[i+1].alientype, name, MAX_VAR);
		containment[i+1].state = 1;
		containment[i+1].amount = 0;
	}
}

/**
 * @brief Type of alien to its name.
 * @param[in] type
 * @return name
 * @note Those names does not need to be translateable - used only
 * for string compares.
 */
char *AL_AlienTypeToName(alienType_t type)
{
	switch (type) {
	case AL_ORTNOK:
		return "Ortnok";
	case AL_TAMAN:
		return "Taman";
	case AL_SHEVAAR:
		return "Shevaar";
	case AL_FLYER:
		return "Flyer";
	case AL_UNKNOWN:
	default:
		return "Unknown";
    }
}

/**
 * @brief Collecting stunned aliens and alien bodies after the mission.
 * @sa CL_ParseResults
 */
void CL_CollectingAliens(void)
{
	int i, j;
	int alstate;
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
				if (le->HP <= 0)
					/* alien body */
					alstate = 0;
				else
					/* alive alien */
					alstate = 1;

				j = 0;
				while (j < aircraft->alientypes) {
					/* search alien type (dead/alive are two types) and increase amount */
					if ((Q_strncmp(cargo[j].alientype, teamDesc[teamDescID].name, MAX_VAR) == 0) && (cargo[j].state == alstate)) {
						cargo[j].amount++;
						Com_DPrintf("Counting: %s state: %i count: %i\n", cargo[j].alientype, cargo[j].state, cargo[j].amount);
						break;
					}
					j++;
				}
				if (j == aircraft->alientypes) {
					/* otherwise add new alien type */
					Q_strncpyz(cargo[j].alientype, teamDesc[teamDescID].name, MAX_VAR);
					cargo[j].state = alstate;
					aircraft->alientypes++;
					Com_DPrintf("Adding: %s state: %i count: %i\n", cargo[j].alientype, cargo[j].state, cargo[j].amount);
				}
			}
		}
	}

	/* print all of them */
	j = 0;
	while (j < aircraft->alientypes) {
		if (cargo[j].state == 0)
			Com_DPrintf("Collecting alien bodies... type: %s amount: %i\n", cargo[j].alientype, cargo[j].amount+1);
		if (cargo[j].state == 1)
			Com_DPrintf("Alive aliens captured... type: %s amount: %i\n", cargo[j].alientype, cargo[j].amount+1);
		j++;
	}
}

/**
 * @brief Puts alien cargo into Alien Containment.
 * @sa CL_DropshipReturned
 */
void AL_AddAliens()
{
	int i, j;
	int alienTypeNum = 0;
	base_t *tobase = NULL;
	aliensTmp_t *cargo = NULL;
	aircraft_t *aircraft = NULL;

	if (baseCurrent) {
		tobase = baseCurrent;
	} else {
		/* should never happen */
		Com_Printf("AL_AddAliens()... No base selected!\n");
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

	j = 0; i = 0;
	while (i < aircraft->alientypes) {
		if ((Q_strncmp(tobase->alienscont[i].alientype, cargo[j].alientype, MAX_VAR) == 0) && (tobase->alienscont[i].state == cargo[j].state)) {
			tobase->alienscont[i].amount += cargo[j].amount;
			i++; j++;
			continue;
		}

		if (i == alienTypeNum) {
			Q_strncpyz(tobase->alienscont[i].alientype, cargo[j].alientype, MAX_VAR);
			tobase->alienscont[i].amount = cargo[j].amount;
			tobase->alienscont[i].state = cargo[j].state;
			alienTypeNum++;
		}

		i++; j++;
	}    

	/* print all of them */
	j = 0;
	while (j < aircraft->alientypes) {
		if (tobase->alienscont[j].state == 1)
			Com_DPrintf("AL_AddAliens alive: %s amount: %i\n", tobase->alienscont[j].alientype, tobase->alienscont[j].amount+1);
		if (tobase->alienscont[j].state == 0)
				Com_DPrintf("AL_AddAliens bodies: %s amount: %i\n", tobase->alienscont[j].alientype, tobase->alienscont[j].amount+1);
		j++;
	}
}

/**
 * @brief Counts alive aliens in all bases.
 * @note This should be called whenever you add or remove
 * aliens from alien containment.
 * @sa CL_DropshipReturned
 */
void AL_CountAll(void)
{
	int i, j;
	int amount = 0;
	base_t *base;

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (!base->founded)
			continue;
		if (!base->hasAlienCont)
			continue;
		for (j = 0; j < (AL_UNKNOWN * 2); j++) {
			if ((base->alienscont[j].alientype) && (base->alienscont[j].state == 1))
				amount += base->alienscont[j].amount;
		}
	}

	Cvar_SetValue("al_globalamount", amount);
}

/**
 * @brief Removes alien(s) from Alien Containment.
 * @param[in] alientype
 * @param[in] amount of alientype to be removed
 * @param[in] type of action
 * @note Call it with alientype AL_UNKNOWN when it does not
 * matter what type.
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
			for (j = 0; j < (AL_UNKNOWN * 2); j++) {
				if (containment[j].state == 0)
					continue;
				if (maxamount < containment[j].amount) {
					maxamount = containment[j].amount;
					maxidx = j;
				}
			}

			containment[maxidx].amount -= amount;
			return;
		}

		for (j = 0; j < (AL_UNKNOWN * 2); j++) {
			if (containment[j].state == 0)
				continue;
			if (Q_strncmp(containment[j].alientype, name, MAX_VAR) == 0) {
				containment[j].amount -= amount;
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

	for (i = 0; i < AL_UNKNOWN; i++) {
		if (!Q_strncmp(id, AL_AlienTypeToName(i), MAX_VAR))
			return i;
	}

	Com_Printf("AL_GetAlien(): Alien \"%s\" not found!\n", id);
	return -1;
}

/**
 * @brief Get amount of live aliens of alien bodies stored in Containment.
 * @param[in] index of alien
 * @param[in] requirement type (RS_LINK_ALIEN/RS_LINK_ALIEN_DEAD
 * @return amount of desired alien/body
 * @sa RS_RequirementsMet
 * @sa RS_CheckCollected
 */
int AL_GetAlienAmount(int idx, requirementType_t reqtype)
{
	int i, state;
	base_t *base = NULL;
	aliensCont_t *containment = NULL;

	if (baseCurrent) {
		base = baseCurrent;
	} else {
		/* should never happen */
		Com_Printf("AL_GetAlienAmount()... No base selected!\n");
		return -1;
	}

	containment = base->alienscont;
	switch (reqtype) {
	case RS_LINK_ALIEN:
		state = 1;
	case RS_LINK_ALIEN_DEAD:
		state = 0;
	default:
		state = 0;
	}

	for (i = 0; i < (AL_UNKNOWN * 2); i++) {
		if ((Q_strncmp(containment[i].alientype, AL_AlienTypeToName(idx), MAX_VAR) == 0)
		&& (containment[i].state == state))
			return containment[i].amount;
	}

	return -1;
}

/**
 * Menu functions
 */

/**
 * @brief Alien containment menu init function
 * @note Command to call this: aliencont_init
 * @note Should be called whenever the research menu gets active
 */
void AC_Init (void)
{
	menuText[TEXT_STANDARD] = aliencontText;
}

/**
 * @brief Defines commands and cvars for the alien containment menu(s)
 * @sa MN_ResetMenus
 */
void AC_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("aliencont_init", AC_Init, "Init function for alien containment menu");

	/* add cvars */
	/* TODO */

	memset(aliencontText, 0, sizeof(aliencontText));
}
