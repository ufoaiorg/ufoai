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
 * @brief Type of alien to its name
 * @param[in] type
 * @return name
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
		default:
			return "Unknown";
    }
}

/**
 * @brief Collecting stunned aliens and alien bodies after the mission
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
 * @brief Puts alien cargo into Alien Containment
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
			Com_Printf("AL_AddAliens alive: %s amount: %i\n", tobase->alienscont[j].alientype, tobase->alienscont[j].amount+1);
		if (tobase->alienscont[j].state == 0)
				Com_Printf("AL_AddAliens bodies: %s amount: %i\n", tobase->alienscont[j].alientype, tobase->alienscont[j].amount+1);
		j++;
	}
}

/**
 * @brief Counts alive aliens in all bases
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
		for (j = 0; j < AL_UNKNOWN; j++) {
			if ((base->alienscont[j].alientype) && (base->alienscont[j].state == 1))
				amount += base->alienscont[j].amount;
		}
	}

	Cvar_SetValue("al_globalamount", amount);
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
