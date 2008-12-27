/**
 * @file cp_missions.h
 * @brief Campaign missions headers
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

#ifndef CP_MISSIONS_H
#define CP_MISSIONS_H

#include "cp_mission_baseattack.h"
#include "cp_mission_harvest.h"
#include "cp_mission_recon.h"
#include "cp_mission_terror.h"

void CP_SetMissionVars(const mission_t *mission);
void CP_StartMissionMap(mission_t* mission);
void CP_MissionRemove(mission_t *mission);
qboolean CP_MissionCreate(mission_t *mission);
qboolean CP_CheckNewMissionDetectedOnGeoscape(void);
qboolean CP_CheckMissionLimitedInTime(const mission_t *mission);
void CP_MissionDisableTimeLimit(mission_t *mission);
void CP_MissionRemoveFromGeoscape(mission_t *mission);
void CP_MissionAddToGeoscape(mission_t *mission, qboolean force);
void CP_UFORemoveFromGeoscape(mission_t *mission, qboolean destroyed);
mission_t* CP_GetLastMissionAdded(void);

#endif
