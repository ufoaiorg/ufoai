/**
 * @file cp_mission_recon.h
 * @brief Campaign mission headers
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CP_MISSION_RECON_H
#define CP_MISSION_RECON_H

void CP_ReconMissionNextStage(mission_t *mission);
int CP_ReconMissionAvailableUFOs(const mission_t const *mission, ufoType_t *ufoTypes);
void CP_ReconMissionLeave(mission_t *mission);
void CP_ReconMissionGroundGo(mission_t *mission);
void CP_ReconMissionIsFailure(mission_t *mission);
void CP_ReconMissionAerial(mission_t *mission);

#endif
