/**
 * @file cp_auto_mission.h
 * @brief Header file for single player automatic (quick, simulated) missions,
 * without going to the battlescape.
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

#ifndef CP_AUTO_MISSION_H
#define CP_AUTO_MISSION_H

struct aircraft_s;
struct alienTeamGroup_s;
struct campaign_s;
struct battleParam_s;
struct missionResults_s;
struct mission_s;

void AM_Go(struct mission_s *mission, struct aircraft_s *aircraft, const struct campaign_s *campaign, const struct battleParam_s *battleParameters, struct missionResults_s *results);

void AM_InitStartup(void);
void AM_Shutdown(void);

#endif
