/**
 * @file
 * @brief Menu related console command callbacks
 */

/*
Copyright (C) 2002-2020 UFO: Alien Invasion.

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
#include "cp_ufo_callbacks.h"
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_ufo.h"
#include "cp_popup.h"

/**
 * @brief Select ufo on Geoscape
 */
static void UFO_GeoSelectUFO_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		return;
	}

	const int index = atoi(cgi->Cmd_Argv(1));
	if (index < 0 || index >= ccs.numUFOs)
		return;
	aircraft_t* aircraft = UFO_GetByIDX(index);
	if (aircraft == nullptr)
		return;

	if (!GEO_IsUFOSelected(aircraft))
		GEO_SelectUFO(aircraft);
	/** @todo Move this popup from cp_popup and rebuild */
	CL_DisplayPopupInterceptUFO(aircraft);
}

static const cmdList_t ufoCallbacks[] = {
	{"geo_ufo_select", UFO_GeoSelectUFO_f, nullptr},
	{nullptr, nullptr, nullptr}
};

void UFO_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(ufoCallbacks);
}

void UFO_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(ufoCallbacks);
}
