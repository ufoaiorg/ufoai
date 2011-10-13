/**
 * @file cp_map.h
 * @brief Header for Geoscape/Map management
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

#ifndef CLIENT_CL_MAP_H
#define CLIENT_CL_MAP_H

#define GLOBE_ROTATE -90
#define ROTATE_SPEED	0.5
#define KILOMETER_PER_DEGREE	111.2		/* this is the conversion between distance in game (in degree) and km */

extern cvar_t *cl_geoscape_overlay;
extern cvar_t *cl_mapzoommax;
extern cvar_t *cl_mapzoommin;
extern cvar_t *cl_3dmap;

/* prototype */
struct uiNode_s;

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

nation_t* MAP_GetNation(const vec2_t pos);
qboolean MAP_AllMapToScreen(const struct uiNode_s* node, const vec2_t pos, int *x, int *y, int *z);
void MAP_MapDrawEquidistantPoints(const struct uiNode_s* node, const vec2_t center, const float angle, const vec4_t color);
float MAP_AngleOfPath(const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector);
void MAP_MapCalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
void MAP_DrawMap(const struct uiNode_s* node, const campaign_t *campaign);
void MAP_CenterOnPoint_f(void);
void MAP_StopSmoothMovement(void);
base_t* MAP_PositionCloseToBase(const vec2_t pos);

void MAP_Scroll_f(void);
void MAP_Zoom_f(void);
qboolean MAP_MapClick(struct uiNode_s * node, int x, int y);
void MAP_ResetAction(void);
void MAP_SelectAircraft(aircraft_t* aircraft);
void MAP_SelectUFO(aircraft_t* ufo);
mission_t* MAP_SelectMission(mission_t* mission);
void MAP_NotifyMissionRemoved(const mission_t* mission);
void MAP_NotifyUFORemoved(const aircraft_t* ufo, qboolean destroyed);
void MAP_NotifyAircraftRemoved(const aircraft_t* aircraft);
void MAP_NotifyUFODisappear(const aircraft_t* ufo);
void MAP_InitStartup(void);
const char* MAP_GetTerrainType(const byte* color);
int MAP_GetCivilianNumberByPosition(const vec2_t pos);
void MAP_PrintParameterStringByPos(const vec2_t pos);
void MAP_CheckPositionBoundaries(float *pos);
qboolean MAP_IsNight(const vec2_t pos);
const byte *MAP_GetColor(const vec2_t pos, mapType_t type, qboolean *coast);
void MAP_Init(const char *map);
void MAP_Reset(const char *map);
void MAP_Shutdown(void);
qboolean MAP_PositionFitsTCPNTypes(const vec2_t posT, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations);
void MAP_SetOverlay(const char *overlayID);
void MAP_DeactivateOverlay(const char *overlayID);
void MAP_UpdateGeoscapeDock(void);
qboolean MAP_IsRadarOverlayActivated(void);
qboolean MAP_IsXVIOverlayActivated(void);
qboolean MAP_IsNationOverlayActivated(void);

#endif /* CLIENT_CL_MAP_H */
