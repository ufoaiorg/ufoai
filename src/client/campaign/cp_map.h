/**
 * @file cp_map.h
 * @brief Header for Geoscape/Map management
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

nation_t* MAP_GetNation(const vec2_t pos);
qboolean MAP_AllMapToScreen(const struct uiNode_s* node, const vec2_t pos, int *x, int *y, int *z);
qboolean MAP_MapToScreen(const struct uiNode_s* node, const vec2_t pos, int *x, int *y);
void MAP_Draw3DMarkerIfVisible(const struct uiNode_s* node, const vec2_t pos, float angle, const char *model, int skin);
void MAP_MapDrawEquidistantPoints(const struct uiNode_s* node, const vec2_t center, const float angle, const vec4_t color);
float MAP_AngleOfPath(const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector);
void MAP_MapCalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
void MAP_DrawMap(const struct uiNode_s* node, campaign_t *campaign);
void MAP_CenterOnPoint_f(void);
void MAP_StopSmoothMovement(void);
base_t* MAP_PositionCloseToBase(const vec2_t pos);

void MAP_Scroll_f(void);
void MAP_Zoom_f(void);
void MAP_MapClick(struct uiNode_s * node, int x, int y);
void MAP_ResetAction(void);
void MAP_SelectAircraft(aircraft_t* aircraft);
void MAP_SelectUFO(aircraft_t* ufo);
void MAP_SelectMission(mission_t* mission);
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
byte *MAP_GetColor(const vec2_t pos, mapType_t type);
void MAP_Init(campaign_t *campaign);
qboolean MAP_PositionFitsTCPNTypes(const vec2_t posT, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations);
void MAP_SetOverlay(const char *overlayID);
void MAP_DeactivateOverlay(const char *overlayID);
void MAP_UpdateGeoscapeDock(void);
qboolean MAP_IsRadarOverlayActivated(void);
qboolean MAP_IsXVIOverlayActivated(void);
qboolean MAP_IsNationOverlayActivated(void);

#endif /* CLIENT_CL_MAP_H */
