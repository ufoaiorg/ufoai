/**
 * @file cl_radar.c
 * @brief Radars / sensor stuff, to detect and track ufos
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

#include "client.h"
#include "cl_global.h"
#include "cl_map.h"
#include "../renderer/r_draw.h"

/* Define radar range */
const float baseRadarRange = 30.0f;
const float aircraftRadarRange = 7.0f;
/* outer circle radar is bigger than inner circle radar by 100 * outerCircleRatio percent */
static const float outerCircleRatio = 0.41f;
/* this is the multiplier applied to the radar range when the radar levels up */
static const float radarUpgradeMultiplier = 0.3f;

/**
 * @brief Show Radar coverage
 * @sa MAP_MapDrawEquidistantPoints
 */
void RADAR_DrawCoverage (const menuNode_t* node, const radar_t* radar, vec2_t pos)
{
	float rangeTracking;

	const vec4_t color = {0, 1, 0, 1};
	/* Set color */
	R_Color(color);

	rangeTracking = (1.0f + outerCircleRatio) * radar->range;

	MAP_MapDrawEquidistantPoints(node, pos, radar->range, color);
	MAP_MapDrawEquidistantPoints(node, pos, rangeTracking, color);

	R_Color(NULL);
}

/**
 * @brief Display radar in geoscape
 */
void RADAR_DrawInMap (const menuNode_t* node, const radar_t* radar, vec2_t pos)
{
	int x, y, z;
	int i;
	const vec4_t color = {0, 1, 0, 1};
	screenPoint_t pts[2];

	if (radar->numUFOs == 0)
		return;

	/* Show radar range zones */
	RADAR_DrawCoverage(node, radar, pos);

	/* Set color */
	R_Color(color);

	/* Draw lines from radar to ufos sensored */
	MAP_AllMapToScreen(node, pos, &x, &y, &z);
	pts[0].x = x;
	pts[0].y = y;
	for (i = radar->numUFOs - 1; i >= 0; i--)
		if (MAP_AllMapToScreen(node, (gd.ufos + radar->ufos[i])->pos, &x, &y, NULL) && z > 0) {
			pts[1].x = x;
			pts[1].y = y;
			R_DrawLineStrip(2, (int*)pts); /* FIXME */
		}

	R_Color(NULL);
}

/**
 * @brief Add a UFO in the list of sensored UFOs
 */
static qboolean RADAR_AddUFO (radar_t* radar, int numUFO)
{
	if (radar->numUFOs >= MAX_UFOONGEOSCAPE)
		return qfalse;

	radar->ufos[radar->numUFOs] = numUFO;
	radar->numUFOs++;
	return qtrue;
}

/**
 * @brief Check if UFO is sensored list, and return its position in list (-1 if not)
 */
static int RADAR_IsUFOSensored (const radar_t* radar, int numUFO)
{
	int i;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO)
			return i;

	return -1;
}

/**
 * @brief UFO will no more be referenced by radar
 */
static void RADAR_RemoveUFO (radar_t* radar, const aircraft_t* ufo)
{
	int i, numUFO = ufo - gd.ufos;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO) {
			radar->numUFOs--;
			radar->ufos[i] = radar->ufos[radar->numUFOs];
			return;
		}
}

/**
 * @brief Notify that the specified ufo has been removed from geoscape
 **/
void RADAR_NotifyUFORemoved (radar_t* radar, const aircraft_t* ufo)
{
	int i, numUFO = ufo - gd.ufos;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO) {
			radar->numUFOs--;
			radar->ufos[i] = radar->ufos[radar->numUFOs];
			i--;	/* Allow the moved value to be checked */
		} else if (radar->ufos[i] > numUFO)
			radar->ufos[i]--;
}

/**
 * @brief Initialise radar
 */
void RADAR_Initialise (radar_t* radar, float range, float level)
{
	if (!level)
		radar->range = 0.0f;
	else
		radar->range = range * (1 + (level - 1) * radarUpgradeMultiplier);
	if (!radar->numUFOs)
		radar->numUFOs = 0;
}

/**
 * @brief Check if the specified position is within base radar range
 * @note aircraft radars are not checked (and this is intended)
 * @return true if the position is inside one of the base radar range
 */
qboolean RADAR_CheckRadarSensored (const vec2_t pos)
{
	base_t *base;

	for (base = gd.bases; base < gd.bases + MAX_BASES; base++) {
		const float dist = MAP_GetDistance(pos, base->pos);		/* Distance from base to position */
		if (dist <= base->radar.range)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Check if the specified UFO is inside the sensor range of base
 * @return true if the aircraft is inside sensor
 * @sa UFO_CampaignCheckEvents
 */
qboolean RADAR_CheckUFOSensored (radar_t* radar, vec2_t posRadar,
	const aircraft_t* ufo, qboolean wasUFOSensored)
{
	int dist;
	int num;
	int numAircraftSensored;

	/* Get num of ufo in gd.ufos */
	num = ufo - gd.ufos;
	if (num < 0 || num >= gd.numUFOs)
		return qfalse;

	numAircraftSensored = RADAR_IsUFOSensored(radar, num);	/* indice of ufo in radar list */
	dist = MAP_GetDistance(posRadar, ufo->pos);	/* Distance from radar to ufo */

	if (!ufo->notOnGeoscape && (radar->range + (wasUFOSensored ? radar->range * outerCircleRatio : 0) > dist)) {
		/* UFO is inside the radar range */
		if (numAircraftSensored < 0) {
			/* UFO was not sensored by the radar */
			RADAR_AddUFO(radar, num);
		}
		return qtrue;
	}

	/* UFO is not in the sensor range */
	if (numAircraftSensored >= 0) {
		/* UFO was sensored by the radar */
		RADAR_RemoveUFO(radar, ufo);
	}
	return qfalse;
}
