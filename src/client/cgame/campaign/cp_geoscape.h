/**
 * @file
 * @brief Header for Geoscape management
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion.

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

#pragma once

#define KILOMETER_PER_DEGREE	111.2		/* this is the conversion between distance in game (in degree) and km */

/* check for water */
/* blue value is 64 */
#define MapIsWater(color)        (color[0] ==   0 && color[1] ==   0 && color[2] ==  64)

/* culture types */
#define MapIsWestern(color)      (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsEastern(color)      (color[0] == 255 && color[1] == 128 && color[2] ==   0)
#define MapIsOriental(color)     (color[0] == 255 && color[1] ==   0 && color[2] ==   0)
#define MapIsAfrican(color)      (color[0] == 128 && color[1] == 128 && color[2] == 255)

/* population types */
#define MapIsUrban(color)        (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsSuburban(color)     (color[0] == 255 && color[1] == 128 && color[2] ==   0)
#define MapIsVillage(color)      (color[0] == 255 && color[1] ==   0 && color[2] ==   0)
#define MapIsRural(color)        (color[0] == 128 && color[1] == 128 && color[2] == 255)
#define MapIsNopopulation(color) (color[0] == 128 && color[1] == 255 && color[2] ==   0)

/* RASTER enables a better performance for CP_GetRandomPosOnGeoscapeWithParameters set it to 1-6
 * the higher the value the better the performance, but the smaller the coverage */
#define RASTER 2

#define GEO_IsAircraftSelected(aircraft) ((aircraft) == ccs.geoscape.selectedAircraft)
#define GEO_IsInterceptorSelected(aircraft) ((aircraft) == ccs.geoscape.interceptAircraft)
#define GEO_IsUFOSelected(ufo) ((ufo) == ccs.geoscape.selectedUFO)
#define GEO_IsMissionSelected(mission) ((mission) == ccs.geoscape.selectedMission)

#define GEO_GetSelectedAircraft() (ccs.geoscape.selectedAircraft)
#define GEO_GetInterceptorAircraft() (ccs.geoscape.interceptAircraft)
#define GEO_GetSelectedUFO() (ccs.geoscape.selectedUFO)
#define GEO_GetSelectedMission() (ccs.geoscape.selectedMission)
#define GEO_GetMissionAircraft() (ccs.geoscape.missionAircraft)

#define GEO_SetSelectedAircraft(aircraft) (ccs.geoscape.selectedAircraft = (aircraft))
#define GEO_SetInterceptorAircraft(interceptor) (ccs.geoscape.interceptAircraft = (interceptor))
#define GEO_SetSelectedUFO(ufo) (ccs.geoscape.selectedUFO = (ufo))
#define GEO_SetSelectedMission(mission) (ccs.geoscape.selectedMission = (mission))
#define GEO_SetMissionAircraft(aircraft) (ccs.geoscape.missionAircraft = (aircraft))

/* prototype */
struct uiNode_t;

void GEO_DrawMarkers(const uiNode_t* node);
bool GEO_Click(const uiNode_t* node, int x, int y, const vec2_t pos);

nation_t* GEO_GetNation(const vec2_t pos);
float GEO_AngleOfPath(const vec2_t start, const vec2_t end, vec3_t direction, vec3_t ortVector);
void GEO_CalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
void GEO_Draw(geoscapeData_t* data);
void GEO_CenterOnPoint_f(void);
void GEO_CenterPosition(const vec2_t pos);
base_t* GEO_PositionCloseToBase(const vec2_t pos);

void GEO_ResetAction(void);
void GEO_SelectAircraft(aircraft_t* aircraft);
void GEO_SelectUFO(aircraft_t* ufo);
struct mission_s* GEO_SelectMission(struct mission_s* mission);
void GEO_NotifyMissionRemoved(const struct mission_s* mission);
void GEO_NotifyUFORemoved(const aircraft_t* ufo, bool destroyed);
void GEO_NotifyAircraftRemoved(const aircraft_t* aircraft);
void GEO_NotifyUFODisappear(const aircraft_t* ufo);
void GEO_InitStartup(void);
int GEO_GetCivilianNumberByPosition(const vec2_t pos);
void GEO_PrintParameterStringByPos(const vec2_t pos);
void GEO_CheckPositionBoundaries(float* pos);
bool GEO_IsNight(const vec2_t pos);
const byte* GEO_GetColor(const vec2_t pos, mapType_t type, bool* coast);
void GEO_Init(const char* map);
void GEO_Reset(const char* map);
void GEO_Shutdown(void);
bool GEO_PositionFitsTCPNTypes(const vec2_t posT, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations);
void GEO_SetOverlay(const char* overlayID, int status);
void GEO_UpdateGeoscapeDock(void);
bool GEO_IsRadarOverlayActivated(void);

void CP_GetRandomPosOnGeoscape(vec2_t pos, bool noWater);
bool CP_GetRandomPosOnGeoscapeWithParameters(vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations);
