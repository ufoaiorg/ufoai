/**
 * @file cp_ufo.h
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

#ifndef CLIENT_CL_UFO_H
#define CLIENT_CL_UFO_H

enum {
	UFO_IS_NO_TARGET,
	UFO_IS_TARGET_OF_MISSILE,
	UFO_IS_TARGET_OF_LASER
};

#define UFO_GetGeoscapeIDX(ufo) ((ufo) - ccs.ufos)

const char* UFO_TypeToName(ufoType_t type);
const technology_t* UFO_GetTechnologyFromType(const ufoType_t type);
const aircraft_t* UFO_GetByType(const ufoType_t type);
qboolean UFO_ShouldAppearOnGeoscape(const ufoType_t type);
const char* UFO_AircraftToIDOnGeoscape(const aircraft_t *ufocraft);
void UFO_SetRandomDest(aircraft_t* ufo);
void UFO_SetRandomDestAround(aircraft_t* ufocraft, const vec2_t pos);
void UFO_FleePhalanxAircraft(aircraft_t *ufo, const vec2_t v);
void UFO_CheckShootBack(const campaign_t* campaign, aircraft_t *ufo, aircraft_t* phalanxAircraft);
void UFO_CampaignRunUFOs(const campaign_t* campaign, int dt);
void UFO_UpdateAlienInterestForAllBasesAndInstallations(void);
void UFO_DetectNewUFO(aircraft_t *ufocraft);
qboolean UFO_CampaignCheckEvents(void);
void UFO_InitStartup(void);
aircraft_t *UFO_AddToGeoscape(ufoType_t ufotype, const vec2_t destination, mission_t *mission);
void UFO_RemoveFromGeoscape(aircraft_t* ufo);
void UFO_SendToDestination(aircraft_t* ufo, const vec2_t dest);
qboolean UFO_SendPursuingAircraft(aircraft_t* ufo, aircraft_t* aircraft);
void UFO_NotifyPhalanxAircraftRemoved(const aircraft_t *const aircraft);
qboolean UFO_IsUFOSeenOnGeoscape(const aircraft_t const *ufo);
aircraft_t* UFO_GetByIDX(const int idx);
aircraft_t* UFO_GetNext(aircraft_t *lastUFO);
aircraft_t *UFO_GetNextOnGeoscape(aircraft_t *lastUFO);

#endif
