/**
 * @file cp_geoscape_actions.c
 * @brief Geoscape actions like creating new bases and so on
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

#include "../client.h"
#include "../cl_global.h"
#include "cl_map.h"
#include "cp_geoscape_actions.h"

/**
 * @brief Check conditions for new installation and build it.
 * @param[in] pos Position on the geoscape.
 * @return True if the base has been build.
 * @sa INS_BuildInstallation
 */
qboolean CL_NewInstallation (installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos)
{
	byte *colorTerrain;

	assert(installation);

	if (installation->founded) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewInstallation: installation already founded: %i\n", installation->idx);
		return qfalse;
	} else if (gd.numInstallations == MAX_INSTALLATIONS) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewInstallation: max installation limit hit\n");
		return qfalse;
	}

	colorTerrain = MAP_GetColor(pos, MAPTYPE_TERRAIN);

	if (MapIsWater(colorTerrain)) {
		/* This should already have been catched in MAP_MapClick (cl_menu.c), but just in case. */
		MS_AddNewMessage(_("Notice"), _("Could not set up your installation at this location"), qfalse, MSG_INFO, NULL);
		return qfalse;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewInstallation: zoneType: '%s'\n", MAP_GetTerrainType(colorTerrain));
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for installation terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build installation */
	Vector2Copy(pos, installation->pos);

	gd.numInstallations++;

	return qtrue;
}

