/**
 * @file cp_aliencont_callbacks.c
 * @brief Menu related callback functions used alien containment
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../../ui/ui_main.h"
#include "cp_campaign.h"
#include "cp_aliencont.h"
#include "cp_aliencont_callbacks.h"

/**
 * @brief Maximal entries in aliencont menu.
 * @sa MAX_TEAMDEFS
 */
#define MAX_AC_MENU_ENTRIES 10

/** Statics for menu. */
static const aliensCont_t* aliencontCurrent;		/**< Current selected Alien Containment. */
static int numAliensOnList = 0;			/**< Number of aliens on AC menu list. */

/**
 * @brief Counts killed or captured aliens of given type in all bases.
 * @param[in] alienidx
 * @param[in] alive boolean whether the alien is alive or already dead
 * @return amount of killed aliens of given type
 */
static int AL_CountForMenu (int alienidx, qboolean alive)
{
	int amount = 0;
	base_t *base;

	assert(alienidx >= 0);
	assert(alienidx < MAX_ALIENCONT_CAP);

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		aliensCont_t *ac = &base->alienscont[alienidx];
		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			continue;
		if (ac->teamDef) {
			if (!alive)
				amount += ac->amountDead;
			else
				amount += ac->amountAlive;
		}
	}
	return amount;
}

/**
 * @brief Call UFOpedia for selected alien.
 */
static void AC_OpenUFOpedia_f (void)
{
	const technology_t *tech;

	/* Can be called from everywhere. */
	if (!aliencontCurrent)
		return;

	tech = aliencontCurrent->tech;

	/* Should never happen. */
	if (!tech) {
		Com_Printf("AC_OpenUFOpedia_f: No tech pointer set!\n");
		return;
	}

	UP_OpenWith(tech->id);
}


/**
 * @brief Open research menu.
 */
static void AC_ResearchAlien_f (void)
{
	const technology_t *tech;

	/* Can be called from everywhere. */
	if (!aliencontCurrent)
		return;

	tech = aliencontCurrent->tech;
	if (!tech)
		Com_Error(ERR_DROP, "aliencontCurrent without tech pointer");

	if (!RS_IsResearched_ptr(tech))
		UI_PushWindow("research", NULL, NULL);
}

/**
 * @brief updates menu related cvars for given base and alien num
 * @param base
 * @param num
 */
static void AC_AlienClick (const base_t *base, int num)
{
	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "AC_AlienClick: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		const aliensCont_t *containment = base->alienscont;
		int i, step;

		for (i = 0, step = 0; i < ccs.numAliensTD; i++) {
			if (!containment[i].amountAlive && !containment[i].amountDead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}

		aliencontCurrent = &base->alienscont[num];
		assert(aliencontCurrent->tech);
		Cvar_Set("mn_al_alienimage", aliencontCurrent->tech->image);
		assert(aliencontCurrent->teamDef);
		Cvar_Set("mn_al_alientype", _(aliencontCurrent->teamDef->name));
		Cvar_Set("mn_al_alive", va("%i (%i)", aliencontCurrent->amountAlive, AL_CountForMenu(aliencontCurrent->teamDef->idx, qtrue)));
		Cvar_Set("mn_al_dead",  va("%i (%i)", aliencontCurrent->amountDead, AL_CountForMenu(aliencontCurrent->teamDef->idx, qfalse)));
	}
}

/**
 * @brief Click function for aliencont menu list.
 * @sa AC_InitStartup
 */
static void AC_AlienClick_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2 || !base) {
		Com_Printf("Usage: %s <alien list index>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf(DEBUG_CLIENT, "AC_AlienClick_f: listnumber %i\n", num);
	AC_AlienClick(base, num);
}

/**
 * @brief Updates the alienscont menu.
 */
static void AC_UpdateMenu (const base_t *base)
{

	Cvar_Set("mn_al_alientype", "");
	Cvar_Set("mn_al_alienimage", "");
	Cvar_SetValue("mn_al_dead", 0);
	Cvar_SetValue("mn_al_alive", 0);
	Cvar_SetValue("mn_al_capacity", CAP_GetCurrent(base, CAP_ALIENS));
	Cvar_SetValue("mn_al_capacity_max", CAP_GetMax(base, CAP_ALIENS));

	/* Reset list. */
	UI_ExecuteConfunc("aliencont_clear");
	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		const aliensCont_t *containment = base->alienscont;
		int i, j;
		for (i = 0, j = 0; i < ccs.numAliensTD; i++) {
			if (j < MAX_AC_MENU_ENTRIES) {
				if (containment[i].teamDef) {
					const technology_t *tech = containment[i].tech;
					if (!tech) {
						Com_Printf("AC_UpdateMenu: Tech entry for containment %i not set!\n", i);
						/* to let the click function still work */
						continue;
					}
					if (!aliencontCurrent) {
						aliencontCurrent = &containment[i];
					}
					if (containment[i].amountAlive > 0 || containment[i].amountDead > 0) {
						/* Generate a list entry. */
						if (RS_IsResearched_ptr(tech)) {
							Cvar_Set(va("mn_ac_statusstr%i", j), _("Researched"));
						} else {
							Cvar_Set(va("mn_ac_statusstr%i", j), _("Awaiting autopsy"));
							if (!containment[i].amountDead) {
								UI_ExecuteConfunc("aliencontkill %i", j);
							} else {
								UI_ExecuteConfunc("aliencontneedautopsy %i", j);
							}
						}
						Cvar_SetValue(va("mn_ac_progress%i", j), (1 - tech->time / tech->overallTime) * 100);
						/* Display name in the correct list-entry. */
						Cvar_Set(va("mn_ac_name%i", j), _(containment[i].teamDef->name));
						/* Display amount of dead aliens in the correct list-entry. */
						Cvar_SetValue(va("mn_ac_dead%i", j), containment[i].amountDead);
						/* Display number of live aliens in the correct list-entry. */
						Cvar_SetValue(va("mn_ac_alive%i", j), containment[i].amountAlive);
						j++;
					}
				}
			}
		}

		numAliensOnList = j;

		for (; j < MAX_AC_MENU_ENTRIES; j++) {
			Cvar_Set(va("mn_ac_statusstr%i", j), _("Free slot"));
			Cvar_Set(va("mn_ac_name%i", j), _("None"));
			Cvar_Set(va("mn_ac_dead%i", j), "");
			Cvar_Set(va("mn_ac_alive%i", j), "");
			Cvar_SetValue(va("mn_ac_progress%i", j), 0);
		}
	}

	/** @todo Select the containment we (maybe) just clicked again */
	AC_AlienClick(base, 0);
}

/**
 * @brief Alien containment menu init function.
 * @note Command to call this: aliencont_init
 * @note Should be called whenever the alien containment menu gets active.
 */
static void AC_Init_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	/* Reset the aliencont list. */
	numAliensOnList = 0;

	if (!base) {
		Com_Printf("No base selected\n");
		return;
	}

	AC_UpdateMenu(base);
}

/**
 * @brief Console command to kill all aliens on a base.
 * @note if the first argrument is a base index that, otherwise the current base will remove it's aliens
 * @sa AC_KillAll
 */
static void AC_KillAll_f (void)
{
	base_t *base;

	if (Cmd_Argc() < 2) {
		base = B_GetCurrentSelectedBase();
	} else {
		base = B_GetFoundedBaseByIDX(atoi(Cmd_Argv(1)));
	}

	/* Can be called from everywhere. */
	if (!base)
		return;

	AC_KillAll(base);
	/* Reinit menu to display proper values. */
	AC_UpdateMenu(base);
}

/**
 * @brief Kill single alien of a given type.
 */
static void AC_KillOne_f (void)
{
	int num;
	base_t *base = B_GetCurrentSelectedBase();

	/* Can be called from everywhere. */
	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <alien list index>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));
	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "AC_KillOne_f: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		aliensCont_t *containment = base->alienscont;
		int i, step;
		for (i = 0, step = 0; i < ccs.numAliensTD; i++) {
			if (!containment[i].amountAlive && !containment[i].amountDead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}
		AL_RemoveAliens(base, containment[num].teamDef, 1, AL_KILLONE);
		/* Reinit menu to display proper values. */
		AC_UpdateMenu(base);
	}
}

void AC_InitCallbacks (void)
{
	Cmd_AddCommand("aliencont_init", AC_Init_f, "Init function for alien containment menu");
	Cmd_AddCommand("aliencont_killall", AC_KillAll_f, "Kills all aliens in current base");
	Cmd_AddCommand("aliencont_killone", AC_KillOne_f, "Kills one alien of a given type");
	Cmd_AddCommand("aliencont_research", AC_ResearchAlien_f, "Opens research menu");
	Cmd_AddCommand("aliencont_pedia", AC_OpenUFOpedia_f, "Opens UFOpedia entry for selected alien");
	Cmd_AddCommand("aliencont_click", AC_AlienClick_f, "Click function for aliencont list");
	aliencontCurrent = NULL;
}

void AC_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("aliencont_init");
	Cmd_RemoveCommand("aliencont_killall");
	Cmd_RemoveCommand("aliencont_killone");
	Cmd_RemoveCommand("aliencont_research");
	Cmd_RemoveCommand("aliencont_pedia");
	Cmd_RemoveCommand("aliencont_click");
}
