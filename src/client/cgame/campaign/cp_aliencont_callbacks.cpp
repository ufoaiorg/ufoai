/**
 * @file
 * @brief Menu related callback functions used alien containment
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

/*
#include "../../cl_shared.h"
#include "../../ui/ui_dataids.h"
#include "cp_aliencont.h"
*/

#include "cp_aliencont_callbacks.h"
#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "aliencontainment.h"

/**
 * @brief Alien containment menu init function.
 * @note Command to call this: ui_aliencont_init
 * @note Should be called whenever the alien containment menu gets active.
 */
static void AC_Init_f (void)
{
	base_t* base;
	if (cgi->Cmd_Argc() < 2)
		base = B_GetCurrentSelectedBase();
	else
		base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!base) {
		Com_Printf("No base selected\n");
		return;
	}

	cgi->UI_ExecuteConfunc("ui_aliencont_cap %d %d", CAP_GetCurrent(base, CAP_ALIENS), CAP_GetMax(base, CAP_ALIENS));
	cgi->UI_ExecuteConfunc("ui_aliencont_clear");
	if (!base->alienContainment)
		return;
	linkedList_t* list = base->alienContainment->list();
	LIST_Foreach(list, alienCargo_t, item) {
		const technology_t* tech = RS_GetTechForTeam(item->teamDef);
		cgi->UI_ExecuteConfunc("ui_aliencont_add \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" %f %d %d",
			item->teamDef->id, _(item->teamDef->name), tech->id, tech->image,
			(RS_IsResearched_ptr(tech)) ? _("Researched") : _("Awaiting autopsy"),
			(1.0f - tech->time / tech->overallTime) * 100, item->alive, item->dead);
	}
	cgi->LIST_Delete(&list);
}

/**
 * @brief Console command to kill all aliens on a base.
 * @note if the first argrument is a base index that, otherwise the current base will remove it's aliens
 * @sa AC_KillAll
 */
static void AC_KillAll_f (void)
{
	base_t* base;

	if (cgi->Cmd_Argc() < 2) {
		base = B_GetCurrentSelectedBase();
	} else {
		base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	}

	if (!base)
		return;
	if (!base->alienContainment)
		return;

	linkedList_t* list = base->alienContainment->list();
	LIST_Foreach(list, alienCargo_t, item) {
		base->alienContainment->add(item->teamDef, -item->alive, item->alive);
	}
	cgi->LIST_Delete(&list);

	cgi->Cmd_ExecuteString("ui_aliencont_init");
}

/**
 * @brief Kill single alien of a given type.
 */
static void AC_KillOne_f (void)
{
	base_t* base;
	const int argc = cgi->Cmd_Argc();

	if (argc < 3) {
		base = B_GetCurrentSelectedBase();
	} else {
		base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	}
	if (!base)
		return;
	if (!base->alienContainment)
		return;

	if (argc < 2) {
		Com_Printf("Usage: %s [baseIdx] <techId>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* this function should work by teamDef ID (or raceID), but currently multple teams defined per race
	 * that makes the thing more complicated */
	const char* techId = cgi->Cmd_Argv(argc - 1);
	linkedList_t* list = base->alienContainment->list();
	LIST_Foreach(list, alienCargo_t, item) {
		const technology_t* tech = RS_GetTechForTeam(item->teamDef);
		if (!Q_streq(tech->id, techId))
			continue;
		base->alienContainment->add(item->teamDef, -1, 1);
	}
	cgi->LIST_Delete(&list);

	cgi->Cmd_ExecuteString("ui_aliencont_init");
}

static const cmdList_t alienContCallbacks[] = {
	{"ui_aliencont_init", AC_Init_f, "Init function for alien containment menu"},
	{"ui_aliencont_killall", AC_KillAll_f, "Kills all aliens in current base"},
	{"ui_aliencont_killone", AC_KillOne_f, "Kills one alien of a given type"},
	{nullptr, nullptr, nullptr}
};
void AC_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(alienContCallbacks);
}

void AC_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(alienContCallbacks);
}
