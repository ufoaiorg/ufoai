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

	if (radar->numUfos == 0)
		return;

	/* Show radar range zones */
	RADAR_DrawCoverage(node,radar,pos);

	/* Set color */
	R_Color(color);

	/* Draw lines from radar to ufos sensored */
	MAP_AllMapToScreen(node, pos, &x, &y, &z);
	pts[0].x = x;
	pts[0].y = y;
	for (i = radar->numUfos - 1; i >= 0; i--)
		if (MAP_AllMapToScreen(node, (gd.ufos + radar->ufos[i])->pos, &x, &y, NULL) && z > 0) {
			pts[1].x = x;
			pts[1].y = y;
			R_DrawLineStrip(2, (int*)pts); /* FIXME */
		}

	R_Color(NULL);
}

/**
 * @brief Add a UFO in the list of sensored ufo
 */
static qboolean RADAR_AddUfo (radar_t* radar, int numUfo)
{
	if (radar->numUfos >= MAX_UFOONGEOSCAPE)
		return qfalse;

	radar->ufos[radar->numUfos] = numUfo;
	radar->numUfos++;
	return qtrue;
}

/**
 * @brief Check if ufo is sensored list, and return its position in list (-1 if not)
 */
static int RADAR_IsUfoSensored (const radar_t* radar, int numUfo)
{
	int i;

	for (i = 0; i < radar->numUfos; i++)
		if (radar->ufos[i] == numUfo)
			return i;

	return -1;
}

/**
 * @brief Ufo will no more be referenced by radar
 */
void RADAR_RemoveUfo (radar_t* radar, const aircraft_t* ufo)
{
	int i, numUfo = ufo - gd.ufos;

	for (i = 0; i < radar->numUfos; i++)
		if (radar->ufos[i] == numUfo) {
			radar->numUfos--;
			radar->ufos[i] = radar->ufos[radar->numUfos];
			return;
		}
}

/**
 * @brief Notify that the specified ufo has been removed from geoscape
 **/
void Radar_NotifyUfoRemoved (radar_t* radar, const aircraft_t* ufo)
{
	int i, numUfo = ufo - gd.ufos;

	for (i = 0; i < radar->numUfos; i++)
		if (radar->ufos[i] == numUfo) {
			radar->numUfos--;
			radar->ufos[i] = radar->ufos[radar->numUfos];
			i--;	/* Allow the moved value to be checked */
		} else if (radar->ufos[i] > numUfo)
			radar->ufos[i]--;
}

/**
 * @brief Initialise radar
 */
void Radar_Initialise (radar_t* radar, float range, float level)
{
	if (!level)
		radar->range = 0.0f;
	else
		radar->range = range * (1 + (level - 1) * radarUpgradeMultiplier);
	if (!radar->numUfos)
		radar->numUfos = 0;
}

/**
 * @brief Check if the specified ufo is inside the sensor range of base
 * Return true if the aircraft is inside sensor
 * @sa UFO_CampaignCheckEvents
 */
qboolean RADAR_CheckUfoSensored (radar_t* radar, vec2_t posRadar,
	const aircraft_t* ufo, qboolean wasUfoSensored)
{
	int dist;
	int num;
	int numAircraftSensored;

	/* Get num of ufo in gd.ufos */
	num = ufo - gd.ufos;
	if (num < 0 || num >= gd.numUfos)
		return qfalse;

	numAircraftSensored = RADAR_IsUfoSensored(radar, num);	/* indice of ufo in radar list */
	dist = MAP_GetDistance(posRadar, ufo->pos);	/* Distance from radar to ufo */

	if (radar->range + (wasUfoSensored ? radar->range * outerCircleRatio : 0) > dist) {
		/* Ufo is inside the radar range */
		if (numAircraftSensored < 0) {
			/* Ufo was not sensored by the radar */
			RADAR_AddUfo(radar, num);
		}
		return qtrue;
	}

	/* Ufo is not in the sensor range */
	if (numAircraftSensored >= 0) {
		/* Ufo was sensored by the radar */
		RADAR_RemoveUfo(radar, ufo);
	}
	return qfalse;
}
