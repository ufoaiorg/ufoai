/**
 * @file cl_aliencont_callbacks.c
 * @brief Menu related callback functions used alien containment
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_main.c
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "../cl_game.h" /* for CP_GameIsRunning */
#include "cl_aliencont.h"
#include "cl_aliencont_callbacks.h"

/**
 * @brief Maximal entries in aliencont menu.
 * @sa MAX_TEAMDEFS
 */
#define AC_MENU_MAX_ENTRIES 12

/** @brief Number of entries in a line of the aliencont menu. */
#define AC_MENU_LINE_ENTRIES 2

/** Statics for menu. */
static const aliensCont_t* aliencontCurrent;		/**< Current selected Alien Containment. */
static int numAliensOnList = 0;			/**< Number of aliens on AC menu list. */
/** @brief First line in aliencont menu. */
static int alienContFirstEntry;

/**
 * @brief Counts killed or captured aliens of given type in all bases.
 * @param[in] alienidx
 * @param[in] alive boolean whether the alien is alive or already dead
 * @return amount of killed aliens of given type
 */
static int AL_CountForMenu (int alienidx, qboolean alive)
{
	int i;
	int amount = 0;

	assert(alienidx >= 0);
	assert(alienidx < MAX_ALIENCONT_CAP);

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			continue;
		if (base->alienscont[alienidx].teamDef) {
			if (!alive)
				amount += base->alienscont[alienidx].amount_dead;
			else
				amount += base->alienscont[alienidx].amount_alive;
		}
	}
	return amount;
}

/**
 * @brief Open menu for next Alien Containment.
 * @sa AC_PrevAC_f
 */
static void AC_NextAC_f (void)
{
	base_t *base;
	qboolean found = qfalse;

	/* Can be called from everywhere. */
	if (!baseCurrent || !aliencontCurrent)
		return;

	/* Get next base */
	if (baseCurrent >= ccs.bases + MAX_BASES - 1)
		base = &ccs.bases[0];
	else
		base = baseCurrent + 1;

	/* Loop until we hit the original base. */
	while (base != baseCurrent) {
		if (base->founded && base->hasBuilding[B_ALIEN_CONTAINMENT] &&
			B_CheckBuildingTypeStatus(base, B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			found = qtrue;
			break;
		}

		/* Get next base */
		if (base >= ccs.bases + MAX_BASES - 1)
			base = &ccs.bases[0];	/* Wrap around from last to first base. */
		else
			base++;
	}

	if (!found)
		return;

	if (!base->founded)
		return;
	else
		Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push aliencont\n", base->idx));
}

/**
 * @brief Open menu for previous Alien Containment.
 * @sa AC_NextAC_f
 */
static void AC_PrevAC_f (void)
{
	qboolean found = qfalse;
	const base_t *base;

	/* Can be called from everywhere. */
	if (!baseCurrent || !GAME_CP_IsRunning() || !aliencontCurrent)
		return;

	/* Get previous base */
	if (baseCurrent <= ccs.bases)
		base = &ccs.bases[MAX_BASES - 1];
	else
		base = baseCurrent - 1;

	/* Loop until we hit the original base. */
	while (base != baseCurrent) {
		if (base->founded && base->hasBuilding[B_ALIEN_CONTAINMENT] &&
			B_CheckBuildingTypeStatus(base, B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			found = qtrue;
			break;
		}

		/* Get next base */
		if (base <= ccs.bases)
			base = &ccs.bases[MAX_BASES - 1];	/* Wrap around from first to last base. */
		else
			base--;
	}

	if (!found)
		return;

	if (!base->founded)
		return;
	else
		Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push aliencont\n", base->idx));
}

/**
 * @brief Call UFOpedia for selected alien.
 */
static void AC_OpenUFOpedia_f (void)
{
	const technology_t *tech;

	/* Can be called from everywhere. */
	if (!baseCurrent || !GAME_CP_IsRunning() || !aliencontCurrent)
		return;

	tech = aliencontCurrent->tech;

	/* Should never happen. */
	if (!tech) {
		Com_Printf("AC_OpenUFOpedia_f: No tech pointer set!\n");
		return;
	}

	if (RS_IsResearched_ptr(tech))
		UP_OpenWith(tech->id);
}


/**
 * @brief Open research menu.
 */
static void AC_ResearchAlien_f (void)
{
	const technology_t *tech;

	/* Can be called from everywhere. */
	if (!baseCurrent || !GAME_CP_IsRunning() || !aliencontCurrent)
		return;

	tech = aliencontCurrent->tech;
	if (!tech)
		Sys_Error("aliencontCurrent without tech pointer");

	if (!RS_IsResearched_ptr(tech))
		MN_PushMenu("research", NULL);
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

		for (i = 0, step = 0; i < gd.numAliensTD; i++) {
			if (!containment[i].amount_alive && !containment[i].amount_dead)
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
		Cvar_SetValue("mn_al_alive", AL_CountForMenu(aliencontCurrent->teamDef->idx, qtrue));
		Cvar_SetValue("mn_al_dead", AL_CountForMenu(aliencontCurrent->teamDef->idx, qfalse));
	}
}

/**
 * @brief Click function for aliencont menu list.
 * @sa AC_InitStartup
 */
static void AC_AlienClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2 || !baseCurrent) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf(DEBUG_CLIENT, "AC_AlienClick_f: listnumber %i\n", num);
	AC_AlienClick(baseCurrent, num);
}

/**
 * @brief Updates the alienscont menu.
 */
static void AC_UpdateMenu (const base_t *base)
{
	int i, j;

	Cvar_Set("mn_al_alientype", "");
	Cvar_Set("mn_al_alienimage", "");
	Cvar_SetValue("mn_al_dead", 0);
	Cvar_SetValue("mn_al_alive", 0);
	Cvar_SetValue("mn_al_capacity", base->capacities[CAP_ALIENS].cur);
	Cvar_SetValue("mn_al_capacity_max", base->capacities[CAP_ALIENS].max);

	/* Reset list. */
	MN_ExecuteConfunc("aliencont_clear");
	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		const aliensCont_t *containment = base->alienscont;
		for (i = 0, j = 0; i < gd.numAliensTD; i++) {
			if ((j >= alienContFirstEntry) && (j < AC_MENU_MAX_ENTRIES)) {
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
					if (containment[i].amount_alive > 0 || containment[i].amount_dead > 0) {
						/* Generate a list entry. */
						if (RS_IsResearched_ptr(tech)) {
							Cvar_Set(va("mn_ac_statusstr%i", j), _("Already researched"));
						} else {
							Cvar_Set(va("mn_ac_statusstr%i", j), _("Needs autopsy!"));
							if (!containment[i].amount_dead) {
								MN_ExecuteConfunc("aliencontkill%i", j);
							} else {
								MN_ExecuteConfunc("aliencontneedautopsy%i", j);
							}
						}
						Cvar_SetValue(va("mn_ac_progress%i", j), (1 - tech->time / tech->overalltime) * 100);
						/* Display name in the correct list-entry. */
						Cvar_Set(va("mn_ac_name%i", j), _(containment[i].teamDef->name));
						/* Display amount of dead aliens in the correct list-entry. */
						Cvar_SetValue(va("mn_ac_dead%i", j), containment[i].amount_dead);
						/* Display number of live aliens in the correct list-entry. */
						Cvar_SetValue(va("mn_ac_alive%i", j), containment[i].amount_alive);
						j++;
					}
				}
			}
		}

		numAliensOnList = j;

		for (; j < AC_MENU_MAX_ENTRIES; j++) {
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
	/* Reset the aliencont list. */
	numAliensOnList = 0;

	if (!baseCurrent) {
		Com_Printf("No base selected\n");
		return;
	}

	AC_UpdateMenu(baseCurrent);
}

/**
 * @brief Kill all aliens in current base.
 * @sa AC_KillAll
 */
static void AC_KillAll_f (void)
{
	/* Can be called from everywhere. */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	AC_KillAll(baseCurrent);
	/* Reinit menu to display proper values. */
	AC_UpdateMenu(baseCurrent);
}

/**
 * @brief Kill single alien of a given type.
 */
static void AC_KillOne_f (void)
{
	int num, i, step;

	/* Can be called from everywhere. */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));
	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "AC_KillOne_f: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (B_GetBuildingStatus(baseCurrent, B_ALIEN_CONTAINMENT)) {
		aliensCont_t *containment = baseCurrent->alienscont;
		for (i = 0, step = 0; i < gd.numAliensTD; i++) {
			if (!containment[i].amount_alive && !containment[i].amount_dead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}
		AL_RemoveAliens(baseCurrent, containment[num].teamDef, 1, AL_KILLONE);
		/* Reinit menu to display proper values. */
		AC_UpdateMenu(baseCurrent);
	}
}

/**
 * @brief Click function for scrolling up the aliencont list.
 */
static void AC_ListUp_f (void)
{
	if (!baseCurrent)
		return;

	if (alienContFirstEntry >= AC_MENU_LINE_ENTRIES)
		alienContFirstEntry -= AC_MENU_LINE_ENTRIES;

	AC_UpdateMenu(baseCurrent);
}

/**
 * @brief Click function for scrolling down the aliencont list.
 */
static void AC_ListDown_f (void)
{
	if (!baseCurrent)
		return;

	if (alienContFirstEntry + AC_MENU_MAX_ENTRIES < numAliensOnList)
		alienContFirstEntry += AC_MENU_LINE_ENTRIES;

	AC_UpdateMenu(baseCurrent);
}

void AC_InitCallbacks (void)
{
	Cmd_AddCommand("aliencont_init", AC_Init_f, "Init function for alien containment menu");
	Cmd_AddCommand("aliencont_killall", AC_KillAll_f, "Kills all aliens in current base");
	Cmd_AddCommand("aliencont_killone", AC_KillOne_f, "Kills one alien of a given type");
	Cmd_AddCommand("aliencont_research", AC_ResearchAlien_f, "Opens research menu");
	Cmd_AddCommand("aliencont_pedia", AC_OpenUFOpedia_f, "Opens UFOpedia entry for selected alien");
	Cmd_AddCommand("aliencont_nextbase", AC_NextAC_f, "Opens Alien Containment menu in next base");
	Cmd_AddCommand("aliencont_prevbase", AC_PrevAC_f, "Opens Alien Containment menu in previous base");
	Cmd_AddCommand("aliencont_list_up", AC_ListUp_f, "Scroll up function for aliencont list");
	Cmd_AddCommand("aliencont_list_down", AC_ListDown_f, "Scroll down function for aliencont list");
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
	Cmd_RemoveCommand("aliencont_nextbase");
	Cmd_RemoveCommand("aliencont_prevbase");
	Cmd_RemoveCommand("aliencont_list_up");
	Cmd_RemoveCommand("aliencont_list_down");
	Cmd_RemoveCommand("aliencont_click");

}
