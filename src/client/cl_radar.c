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

void R_AddRadarCoverage(const vec2_t pos, float innerRadius, float outerRadius, qboolean source);
void R_InitializeRadarOverlay(qboolean source);
void R_UploadRadarCoverage(qboolean smooth);

qboolean radarOverlayWasSet;	/**< used to store the previous configuration of overlay before radar
								 * is automatically turned on (e.g when creating base or when UFO appears) */

/* Define radar range */
const float RADAR_BASERANGE = 24.0f;
const float RADAR_AIRCRAFTRANGE = 10.0f;
/** @brief outer circle radar is bigger than inner circle radar by 100 * RADAR_OUTER_CIRCLE_RATIO percent */
static const float RADAR_OUTER_CIRCLE_RATIO = 0.41f;
/** @brief this is the multiplier applied to the radar range when the radar levels up */
static const float RADAR_UPGRADE_MULTIPLIER = 0.4f;

/**
 * @brief Update base map radar coverage.
 * @note This is only called when radar range of bases change.
 */
void RADAR_UpdateBaseRadarCoverage (void)
{
	int baseIdx;

	/* Initialise radar range (will be filled below) */
	R_InitializeRadarOverlay(qtrue);

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		if (base) {
			const float rangeTracking = (1.0f + RADAR_OUTER_CIRCLE_RATIO) * base->radar.range;
			R_AddRadarCoverage(base->pos, base->radar.range, rangeTracking, qtrue);
		}
	}

	/* Smooth and bind radar overlay without aircraft (in case no aircraft is on geoscape:
	 * RADAR_UpdateWholeRadarOverlay won't be called) */
	R_InitializeRadarOverlay(qfalse);
	R_UploadRadarCoverage(qtrue);
}

/**
 * @brief Update map radar coverage with moving radar
 * @sa RADAR_UpdateWholeRadarOverlay
 */
static inline void RADAR_DrawCoverage (const radar_t* radar, const vec2_t pos)
{
	const float rangeTracking = (1.0f + RADAR_OUTER_CIRCLE_RATIO) * radar->range;

	R_AddRadarCoverage(pos, radar->range, rangeTracking, qfalse);
}

/**
 * @brief Update radar overlay of both radar and aircraft range.
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 * @param[in] updateOverlay True if only radar "wire" coverage should be drawn (for bases).
 * False if both overlay and wire coverage should be drawn (for aircraft).
 */
void RADAR_UpdateWholeRadarOverlay (void)
{
	int baseIdx;

	/* Copy Base radar overlay*/
	R_InitializeRadarOverlay(qfalse);

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		int aircraftIdx;
		if (!base)
			continue;
 
		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			if (AIR_IsAircraftOnGeoscape(&base->aircraft[aircraftIdx]))
				RADAR_DrawCoverage(&base->aircraft[aircraftIdx].radar, base->aircraft[aircraftIdx].pos);
		}
	}

	/* Smooth Radar Coverage and bind it */
	R_UploadRadarCoverage(qtrue);
}

/**
 * @brief Draw only the "wire" Radar coverage.
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 * @sa MAP_MapDrawEquidistantPoints
 */
static void RADAR_DrawLineCoverage (const menuNode_t* node, const radar_t* radar, const vec2_t pos)
{
	const vec4_t color = {1., 1., 1., .4};
	const float rangeTracking = (1.0f + RADAR_OUTER_CIRCLE_RATIO) * radar->range;

	/* Set color */
	R_Color(color);

	MAP_MapDrawEquidistantPoints(node, pos, radar->range, color);
	MAP_MapDrawEquidistantPoints(node, pos, rangeTracking, color);

	R_Color(NULL);
}

/**
 * @brief Draw only the "wire" part of the radar coverage in geoscape
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 */
void RADAR_DrawInMap (const menuNode_t *node, const radar_t *radar, vec2_t pos)
{
	int x, y, z;
	int i;
	const vec4_t color = {1., 1., 1., .3};
	screenPoint_t pts[2];

	/* Show radar range zones */
	RADAR_DrawLineCoverage(node, radar, pos);

	/* Set color */
	R_ColorBlend(color);

	/* Draw lines from radar to ufos sensored */
	MAP_AllMapToScreen(node, pos, &x, &y, &z);
	pts[0].x = x;
	pts[0].y = y;
	for (i = radar->numUFOs - 1; i >= 0; i--)
		if (MAP_AllMapToScreen(node, (gd.ufos + radar->ufos[i])->pos, &x, &y, NULL) && z < 0) {
			pts[1].x = x;
			pts[1].y = y;
			R_DrawLineStrip(2, (int*)pts); /* FIXME */
		}

	R_ColorBlend(NULL);
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
 * @brief Deactivate Radar overlay if there is no more UFO on geoscape
 */
void RADAR_DeactivateRadarOverlay (void)
{
	int baseIdx;
	aircraft_t *aircraft;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		if (base->radar.numUFOs)
			return;

		for (aircraft = base->aircraft; aircraft < base->aircraft + base->numAircraftInBase; aircraft++) {
			if (aircraft->radar.numUFOs)
				return;
		}
	}

	if (r_geoscape_overlay->integer & OVERLAY_RADAR)
		MAP_SetOverlay("radar");
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

	RADAR_DeactivateRadarOverlay();
}

/**
 * @brief Notify that the specified ufo has been removed from geoscape to one radar.
 * @param[in] radar Pointer to the radar where ufo should be removed.
 * @param[in] ufo Pointer to UFO to remove.
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 **/
static void RADAR_NotifyUFORemovedFromOneRadar (radar_t* radar, const aircraft_t* ufo, qboolean destroyed)
{
	int i, numUFO = ufo - gd.ufos;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO) {
			radar->numUFOs--;
			radar->ufos[i] = radar->ufos[radar->numUFOs];
			i--;	/* Allow the moved value to be checked */
		} else if (destroyed && (radar->ufos[i] > numUFO))
			radar->ufos[i]--;

	RADAR_DeactivateRadarOverlay();
}

/**
 * @brief Notify that the specified ufo has been removed from geoscape
 * @param[in] ufo Pointer to UFO to remove.
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 **/
void RADAR_NotifyUFORemoved (const aircraft_t* ufo, qboolean destroyed)
{
	int baseIdx;
	aircraft_t *aircraft;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		RADAR_NotifyUFORemovedFromOneRadar(&base->radar, ufo, destroyed);

		for (aircraft = base->aircraft; aircraft < base->aircraft + base->numAircraftInBase; aircraft++)
			RADAR_NotifyUFORemovedFromOneRadar(&aircraft->radar, ufo, destroyed);
	}
}

/**
 * @brief Initialise radar
 * @param[in] radar The radar to update/initialize
 * @param[in] range New range of the radar
 * @param[in] level The tech level of the radar
 * @param[in] updateSourceRadarMap
 */
void RADAR_Initialise (radar_t* radar, float range, float level, qboolean updateSourceRadarMap)
{
	const int oldrange = radar->range;

	if (!level)
		radar->range = 0.0f;
	else
		radar->range = range * (1 + (level - 1) * RADAR_UPGRADE_MULTIPLIER);

	assert(radar->numUFOs >= 0);

	if (updateSourceRadarMap && (radar->range - oldrange > UFO_EPSILON))
		RADAR_UpdateBaseRadarCoverage();

	RADAR_DeactivateRadarOverlay();
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

	if (!ufo->notOnGeoscape && (radar->range + (wasUFOSensored ? radar->range * RADAR_OUTER_CIRCLE_RATIO : 0) > dist)) {
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
