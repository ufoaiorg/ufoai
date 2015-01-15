/**
 * @file
 * @brief Deals with the Alien Containment stuff.
 * @note Collecting and managing aliens functions prefix: AL_
 * @note Alien Containment menu functions prefix: AC_
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "aliencontainment.h"

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
bool AL_AddAlienTypeToAircraftCargo (aircraft_t* aircraft, const teamDef_t* teamDef, int amount, bool dead)
{
	if (aircraft->alienCargo == nullptr)
		aircraft->alienCargo = new AlienCargo();
	if (aircraft->alienCargo == nullptr)
		return false;

	if (dead)
		return aircraft->alienCargo->add(teamDef, 0, amount);
	return aircraft->alienCargo->add(teamDef, amount, 0);
}

/**
 * General Collecting aliens functions
 */

/**
 * @brief Puts alien cargo into Alien Containment.
 * @param[in] aircraft Aircraft transporting cargo to homebase.
 * @sa B_AircraftReturnedToHomeBase
 * @sa AL_FillInContainment
 * @note an event mail about missing breathing tech will be triggered if necessary.
 */
void AL_AddAliens (aircraft_t* aircraft)
{
	if (!aircraft)
		return;
	if (!aircraft->alienCargo)
		return;
	if (!aircraft->homebase) {
		Com_Printf("AL_AddAliens: Aircraft %s (idx: %d) has no base, alienCargo destroyed\n", aircraft->name, aircraft->idx);
		delete aircraft->alienCargo;
		aircraft->alienCargo = nullptr;
		return;
	}

	if (aircraft->homebase->alienContainment == nullptr)
		aircraft->homebase->alienContainment = new AlienContainment(CAP_Get(aircraft->homebase, CAP_ALIENS), nullptr);

	AlienContainment* cont = aircraft->homebase->alienContainment;
	if (!cont)
		return;

	bool messageSent = false;
	linkedList_t* cargo = aircraft->alienCargo->list();
	LIST_Foreach(cargo, alienCargo_t, item) {
		const bool lifeSupported = AlienContainment::isLifeSupported(item->teamDef);

		if (!lifeSupported) {
			cont->add(item->teamDef, 0, item->alive + item->dead);
			aircraft->alienCargo->add(item->teamDef, -item->alive, -item->dead);

			ccs.campaignStats.killedAliens += item->dead + item->alive;
			if (item->alive > 0) {
				CP_TriggerEvent(CAPTURED_ALIENS_DIED, nullptr);
				/* only once */
				if (!messageSent) {
					MS_AddNewMessage(_("Notice"), _("You can't hold live aliens yet. Aliens died."), MSG_DEATH);
					messageSent = true;
				}
			}
		} else {
			cont->add(item->teamDef, item->alive, item->dead);
			aircraft->alienCargo->add(item->teamDef, -item->alive, -item->dead);

			ccs.campaignStats.killedAliens += item->dead;
			ccs.campaignStats.capturedAliens += item->alive;
			if (item->alive > 0) {
				CP_TriggerEvent(CAPTURED_ALIENS, nullptr);
				if (!messageSent) {
					MS_AddNewMessage(_("Notice"), _("You've captured new aliens."));
					messageSent = true;
				}
			}
		}
	}
	cgi->LIST_Delete(&cargo);
}

/**
 * @brief Counts live aliens in all bases.
 */
int AL_CountAll (void)
{
	int amount = 0;
	base_t* base = nullptr;

	while ((base = B_GetNext(base)) != nullptr) {
		if (base->alienContainment)
			amount += base->alienContainment->getAlive();
	}
	return amount;
}

#ifdef DEBUG
/**
 * @brief Add a single alien of a given type.
 */
static void AC_AddOne_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base) {
		Com_Printf("%s: No base selected\n", cgi->Cmd_Argv(0));
		return;
	}
	if (!base->alienContainment) {
		Com_Printf("%s: Current base has no alien containment\n", cgi->Cmd_Argv(0));
		return;
	}
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <alientype> [dead:true|false]\n", cgi->Cmd_Argv(0));
		return;
	}
	const char* alienName = cgi->Cmd_Argv(1);
	if (!alienName)
		return;

	bool updateAlive = true;
	if (cgi->Cmd_Argc() == 3)
		updateAlive = cgi->Com_ParseBoolean(cgi->Cmd_Argv(2));

	if (updateAlive)
		base->alienContainment->add(alienName, 1, 0);
	else
		base->alienContainment->add(alienName, 0, 1);
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
 * @brief Load callback for savin in XML Format
 * @param[in] parent Parent XML node in the savegame
 * @todo Remove: Fallback for compatibility
 */
bool AC_LoadXML (xmlNode_t* parent)
{
	xmlNode_t* aliencont = cgi->XML_GetNode(parent, SAVE_ALIENCONT_ALIENCONT);
	if (!aliencont)
		return true;
	FOREACH_XMLNODE(contNode, aliencont, SAVE_ALIENCONT_CONT) {
		const int baseIdx = cgi->XML_GetInt(contNode, SAVE_ALIENCONT_BASEIDX, MAX_BASES);
		base_t* base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base) {
			Com_Printf("AC_LoadXML: Invalid base idx '%i'\n", baseIdx);
			continue;
		}

		FOREACH_XMLNODE(alienNode, contNode, SAVE_ALIENCONT_ALIEN) {
			const char* teamId = cgi->XML_GetString(alienNode, SAVE_ALIENCONT_TEAMID);
			const int alive = cgi->XML_GetInt(alienNode, SAVE_ALIENCONT_AMOUNTALIVE, 0);
			const int dead = cgi->XML_GetInt(alienNode, SAVE_ALIENCONT_AMOUNTDEAD, 0);

			if (alive == 0 && dead == 0)
				continue;

			if (!base->alienContainment)
				base->alienContainment = new AlienContainment(CAP_Get(base, CAP_ALIENS), nullptr);

			base->alienContainment->add(teamId, alive, dead);
		}
	}

	return true;
}

/**
 * @brief Returns true if the current base is able to handle captured aliens
 * @param[in] base Pointer to the base to check alien containment at
 */
bool AC_ContainmentAllowed (const base_t* base)
{
	return base->alienContainment != nullptr;
}
