/**
 * @file
 * @brief Header for Geoscape/Map management
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#ifndef CP_GEOSCAPE_H
#define CP_GEOSCAPE_H

#define KILOMETER_PER_DEGREE	111.2		/* this is the conversion between distance in game (in degree) and km */

#define MAP_IsAircraftSelected(aircraft) ((aircraft) == ccs.geoscape.selectedAircraft)
#define MAP_IsInterceptorSelected(aircraft) ((aircraft) == ccs.geoscape.interceptAircraft)
#define MAP_IsUFOSelected(ufo) ((ufo) == ccs.geoscape.selectedUFO)
#define MAP_IsMissionSelected(mission) ((mission) == ccs.geoscape.selectedMission)

#define MAP_GetSelectedAircraft() (ccs.geoscape.selectedAircraft)
#define MAP_GetInterceptorAircraft() (ccs.geoscape.interceptAircraft)
#define MAP_GetSelectedUFO() (ccs.geoscape.selectedUFO)
#define MAP_GetSelectedMission() (ccs.geoscape.selectedMission)
#define MAP_GetMissionAircraft() (ccs.geoscape.missionAircraft)

#define MAP_SetSelectedAircraft(aircraft) (ccs.geoscape.selectedAircraft = (aircraft))
#define MAP_SetInterceptorAircraft(interceptor) (ccs.geoscape.interceptAircraft = (interceptor))
#define MAP_SetSelectedUFO(ufo) (ccs.geoscape.selectedUFO = (ufo))
#define MAP_SetSelectedMission(mission) (ccs.geoscape.selectedMission = (mission))
#define MAP_SetMissionAircraft(aircraft) (ccs.geoscape.missionAircraft = (aircraft))

/* prototype */
struct uiNode_t;

void MAP_DrawMapMarkers(const uiNode_t* node);
bool MAP_MapClick(const uiNode_t* node, int x, int y, const vec2_t pos);

nation_t* MAP_GetNation(const vec2_t pos);
float MAP_AngleOfPath(const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector);
void MAP_MapCalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
void MAP_DrawMap(geoscapeData_t* data);
void MAP_CenterOnPoint_f(void);
void MAP_CenterPosition(const vec2_t pos);
base_t* MAP_PositionCloseToBase(const vec2_t pos);

void MAP_ResetAction(void);
void MAP_SelectAircraft(aircraft_t* aircraft);
void MAP_SelectUFO(aircraft_t* ufo);
mission_t* MAP_SelectMission(mission_t* mission);
void MAP_NotifyMissionRemoved(const mission_t* mission);
void MAP_NotifyUFORemoved(const aircraft_t* ufo, bool destroyed);
void MAP_NotifyAircraftRemoved(const aircraft_t* aircraft);
void MAP_NotifyUFODisappear(const aircraft_t* ufo);
void MAP_InitStartup(void);
const char* MAP_GetTerrainType(const byte* color);
int MAP_GetCivilianNumberByPosition(const vec2_t pos);
void MAP_PrintParameterStringByPos(const vec2_t pos);
void MAP_CheckPositionBoundaries(float *pos);
bool MAP_IsNight(const vec2_t pos);
const byte *MAP_GetColor(const vec2_t pos, mapType_t type, bool *coast);
void MAP_Init(const char *map);
void MAP_Reset(const char *map);
void MAP_Shutdown(void);
bool MAP_PositionFitsTCPNTypes(const vec2_t posT, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations);
void MAP_SetOverlay(const char *overlayID);
void MAP_DeactivateOverlay(const char *overlayID);
void MAP_UpdateGeoscapeDock(void);
bool MAP_IsRadarOverlayActivated(void);

#endif
