/**
 * @file cp_radar.c
 * @brief Radars / sensor stuff, to detect and track ufos
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

#include "../../cl_shared.h"
#include "../../cl_renderer.h" /* R_Color */
#include "../../renderer/r_image.h"
#include "../../renderer/r_draw.h" /* R_DrawLineStrip */
#include "../../ui/ui_nodes.h"
#include "cp_campaign.h"
#include "cp_overlay.h"
#include "cp_map.h"
#include "cp_ufo.h"

/**
 * used to store the previous configuration of overlay before radar
 * is automatically turned on (e.g when creating base or when UFO appears)
 */
qboolean radarOverlayWasSet;

/* Define base radar range (can be modified by level of the radar) */
const float RADAR_BASERANGE = 24.0f;
const float RADAR_BASETRACKINGRANGE = 34.0f;
const float RADAR_AIRCRAFTRANGE = 10.0f;
const float RADAR_AIRCRAFTTRACKINGRANGE = 14.0f;
const float RADAR_INSTALLATIONLEVEL = 1.0f;
/** @brief this is the multiplier applied to the radar range when the radar levels up */
static const float RADAR_UPGRADE_MULTIPLIER = 0.4f;

/**
 * @brief Update every static radar drawing (radar that don't move: base and installation radar).
 * @note This is only called when radar range of bases change.
 */
void RADAR_UpdateStaticRadarCoverage (void)
{
	base_t *base;

	/* Initialise radar range (will be filled below) */
	CP_InitializeRadarOverlay(qtrue);

	/* Add base radar coverage */
	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		if (base->radar.range) {
			CP_AddRadarCoverage(base->pos, base->radar.range, base->radar.trackingRange, qtrue);
		}
	}

	/* Add installation coverage */
	INS_Foreach(installation) {
		if (installation->installationStatus == INSTALLATION_WORKING && installation->radar.range)
			CP_AddRadarCoverage(installation->pos, installation->radar.range, installation->radar.trackingRange, qtrue);
	}

	/* Smooth and bind radar overlay without aircraft (in case no aircraft is on geoscape:
	 * RADAR_UpdateWholeRadarOverlay won't be called) */
	CP_InitializeRadarOverlay(qfalse);
	CP_UploadRadarCoverage();
}

/**
 * @brief Update map radar coverage with moving radar
 * @sa RADAR_UpdateWholeRadarOverlay
 */
static inline void RADAR_DrawCoverage (const radar_t* radar, const vec2_t pos)
{
	CP_AddRadarCoverage(pos, radar->range, radar->trackingRange, qfalse);
}

/**
 * @brief Update radar overlay of base, installation and aircraft range.
 */
void RADAR_UpdateWholeRadarOverlay (void)
{
	/* Copy Base and installation radar overlay*/
	CP_InitializeRadarOverlay(qfalse);

	/* Add aircraft radar coverage */
	AIR_Foreach(aircraft) {
		if (AIR_IsAircraftOnGeoscape(aircraft))
			RADAR_DrawCoverage(&aircraft->radar, aircraft->pos);
	}

	CP_UploadRadarCoverage();
}

/**
 * @brief Draw only the "wire" Radar coverage.
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 * @sa MAP_MapDrawEquidistantPoints
 */
static void RADAR_DrawLineCoverage (const uiNode_t* node, const radar_t* radar, const vec2_t pos)
{
	const vec4_t color = {1., 1., 1., .4};
	MAP_MapDrawEquidistantPoints(node, pos, radar->range, color);
	MAP_MapDrawEquidistantPoints(node, pos, radar->trackingRange, color);
}

/**
 * @brief Draw only the "wire" part of the radar coverage in geoscape
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 */
void RADAR_DrawInMap (const uiNode_t *node, const radar_t *radar, const vec2_t pos)
{
	int x, y;
	const vec4_t color = {1., 1., 1., .3};
	qboolean display;

	/* Show radar range zones */
	RADAR_DrawLineCoverage(node, radar, pos);

	/* everything below is drawn only if there is at least one detected UFO */
	if (!radar->numUFOs)
		return;

	/* Set color */
	R_Color(color);

	/* Draw lines from radar to ufos sensored */
	display = MAP_AllMapToScreen(node, pos, &x, &y, NULL);
	if (display) {
		int i;
		screenPoint_t pts[2];

		pts[0].x = x;
		pts[0].y = y;

		for (i = 0; i < radar->numUFOs; i++) {
			const aircraft_t *ufo = radar->ufos[i];
			if (UFO_IsUFOSeenOnGeoscape(ufo) && MAP_AllMapToScreen(node, ufo->pos, &x, &y, NULL)) {
				pts[1].x = x;
				pts[1].y = y;
				R_DrawLineStrip(2, (int*)pts);
			}
		}
	}

	R_Color(NULL);
}

/**
 * @brief Deactivate Radar overlay if there is no more UFO on geoscape
 */
void RADAR_DeactivateRadarOverlay (void)
{
	base_t *base;

	/* never deactivate radar if player wants it to be always turned on */
	if (radarOverlayWasSet)
		return;

	AIR_Foreach(aircraft) {
		/** @todo Is aircraft->radar cleared for crashed aircraft? */
		if (aircraft->radar.numUFOs)
			return;
	}

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		if (base->radar.numUFOs)
			return;
	}

	INS_Foreach(installation) {
		if (installation->radar.numUFOs)
			return;
	}

	if (MAP_IsRadarOverlayActivated())
		MAP_SetOverlay("radar");
}

/**
 * @brief Check if UFO is in the sensored list
 */
static qboolean RADAR_IsUFOSensored (const radar_t* radar, const aircraft_t* ufo)
{
	int i;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == ufo)
			return qtrue;

	return qfalse;
}

/**
 * @brief Add a UFO in the list of sensored UFOs
 */
static qboolean RADAR_AddUFO (radar_t* radar, const aircraft_t* ufo)
{
#ifdef DEBUG
	if (RADAR_IsUFOSensored(radar, ufo)) {
		Com_Printf("RADAR_AddUFO: Aircraft already in radar range\n");
		return qfalse;
	}
#endif

	if (radar->numUFOs >= MAX_UFOONGEOSCAPE)
		return qfalse;

	radar->ufos[radar->numUFOs] = ufo;
	radar->numUFOs++;

	return qtrue;
}

/**
 * @brief UFO will no more be referenced by radar
 */
static void RADAR_RemoveUFO (radar_t* radar, const aircraft_t* ufo)
{
	int i;

	assert(radar->numUFOs < MAX_UFOONGEOSCAPE && radar->numUFOs > 0);

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == ufo)
			break;

	if (i == radar->numUFOs)
		return;

	REMOVE_ELEM(radar->ufos, i, radar->numUFOs);

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
	int i;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == ufo) {
			radar->numUFOs--;
			radar->ufos[i] = radar->ufos[radar->numUFOs];
			i--;	/* Allow the moved value to be checked */
		} else if (destroyed && (radar->ufos[i] > ufo))
			radar->ufos[i]--;

	RADAR_DeactivateRadarOverlay();
}

/**
 * @brief Notify to every radar that the specified ufo has been removed from geoscape
 * @param[in] ufo Pointer to UFO to remove.
 * @param[in] destroyed True if the UFO has been destroyed, false if it's only landed.
 **/
void RADAR_NotifyUFORemoved (const aircraft_t* ufo, qboolean destroyed)
{
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		RADAR_NotifyUFORemovedFromOneRadar(&base->radar, ufo, destroyed);

		AIR_ForeachFromBase(aircraft, base)
			RADAR_NotifyUFORemovedFromOneRadar(&aircraft->radar, ufo, destroyed);
	}

	INS_Foreach(installation) {
		if (installation->installationStatus == INSTALLATION_WORKING)
			RADAR_NotifyUFORemovedFromOneRadar(&installation->radar, ufo, destroyed);
	}
}

/**
 * @brief Set radar range to new value
 * @param[in,out] radar The radar to update/initialize
 * @param[in] range New range of the radar
 * @param[in] trackingRange New tracking range of the radar
 * @param[in] level The tech level of the radar
 * @param[in] updateSourceRadarMap if the radar overlay should be updated.
 */
void RADAR_Initialise (radar_t *radar, float range, float trackingRange, float level, qboolean updateSourceRadarMap)
{
	const int oldrange = radar->range;

	if (equal(level, 0.0)) {
		radar->range = 0.0f;
		radar->trackingRange = 0.0f;
	} else {
		radar->range = range * (1 + (level - 1) * RADAR_UPGRADE_MULTIPLIER);
		radar->trackingRange = trackingRange * (1 + (level - 1) * RADAR_UPGRADE_MULTIPLIER);
	}

	radar->ufoDetectionProbability = 0.000125f * DETECTION_INTERVAL;

	assert(radar->numUFOs >= 0);

	if (updateSourceRadarMap && !equal(radar->range, oldrange)) {
		RADAR_UpdateStaticRadarCoverage();
		RADAR_UpdateWholeRadarOverlay();
	}
}

/**
 * @brief Reset UFO sensored on radar.
 * @param[out] radar The radar to initialize.
 */
void RADAR_InitialiseUFOs (radar_t *radar)
{
	radar->numUFOs = 0;
	OBJZERO(radar->ufos);
}

/**
 * @brief Update radar coverage when building/destroying new radar
 * @note This must be called on each radar build/destruction because radar facilities may have different level.
 * @note This must also be called when radar installation become inactive or active (due to dependencies)
 * @note called with update_base_radar_coverage
 */
void RADAR_UpdateBaseRadarCoverage_f (void)
{
	int baseIdx;
	base_t *base;
	float level;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx> <buildingType>\n", Cmd_Argv(0));
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));

	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("RADAR_UpdateBaseRadarCoverage_f: %i is outside bounds\n", baseIdx);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);

	if (!base)
		return;

	level = B_GetMaxBuildingLevel(base, B_RADAR);
	RADAR_Initialise(&base->radar, RADAR_BASERANGE, RADAR_BASETRACKINGRANGE, level, qtrue);
	CP_UpdateMissionVisibleOnGeoscape();
}

/**
 * @brief Update radar coverage when building/destroying new radar
 * @param[in,out] installation The radartower to update
 * @param[in] radarRange New range of the radar
 * @param[in] trackingRadarRange New tracking range of the radar
 */
void RADAR_UpdateInstallationRadarCoverage (installation_t *installation, const float radarRange, const float trackingRadarRange)
{
	/* Sanity check */
	if (!installation || !installation->installationTemplate)
		Com_Error(ERR_DROP, "RADAR_UpdateInstallationRadarCoverage: No installation or no template!\n");

	/* Do nothing if installation not finished */
	if (installation->installationStatus != INSTALLATION_WORKING)
		return;
	/* Do nothing if this isn't a RadarTower */
	if (installation->installationTemplate->radarRange <= 0
	 || installation->installationTemplate->trackingRange <= 0)
		return;

	RADAR_Initialise(&installation->radar, radarRange, trackingRadarRange, RADAR_INSTALLATIONLEVEL, qtrue);
	CP_UpdateMissionVisibleOnGeoscape();
}

/**
 * @brief Adds detected UFO to any radar in range (if not already detected).
 * @param[in] ufo Pointer to the UFO to check.
 */
void RADAR_AddDetectedUFOToEveryRadar (const aircraft_t const *ufo)
{
	base_t *base = NULL;

	AIR_Foreach(aircraft) {
		if (!AIR_IsAircraftOnGeoscape(aircraft))
			continue;

		if (!RADAR_IsUFOSensored(&aircraft->radar, ufo)) {
			/* Distance from radar to UFO */
			const float dist = GetDistanceOnGlobe(ufo->pos, aircraft->pos);
			if (dist <= aircraft->radar.trackingRange)
				RADAR_AddUFO(&aircraft->radar, ufo);
		}
	}

	while ((base = B_GetNext(base)) != NULL) {
		if (!RADAR_IsUFOSensored(&base->radar, ufo)) {
			/* Distance from radar to UFO */
			const float dist = GetDistanceOnGlobe(ufo->pos, base->pos);
			if (dist <= base->radar.trackingRange)
				RADAR_AddUFO(&base->radar, ufo);
		}
	}

	INS_Foreach(installation) {
		/* No need to check installations without radar */
		if (!installation->radar.trackingRange)
			continue;

		if (!RADAR_IsUFOSensored(&installation->radar, ufo)) {
			/* Distance from radar to UFO */
			const float dist = GetDistanceOnGlobe(ufo->pos, installation->pos);
			if (dist <= ufo->radar.trackingRange)
				RADAR_AddUFO(&installation->radar, ufo);
		}
	}
}

/**
 * @brief Check if the specified position is within base radar range
 * @note aircraft radars are not checked (and this is intended)
 * @return true if the position is inside one of the base radar range
 */
qboolean RADAR_CheckRadarSensored (const vec2_t pos)
{
	base_t *base = NULL;

	while ((base = B_GetNext(base)) != NULL) {
		const float dist = GetDistanceOnGlobe(pos, base->pos);		/* Distance from base to position */
		if (dist <= base->radar.range)
			return qtrue;
	}

	INS_Foreach(installation) {
		float dist;

		dist = GetDistanceOnGlobe(pos, installation->pos);		/* Distance from base to position */
		if (dist <= installation->radar.range)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Check if the specified UFO is inside the sensor range of the given radar
 * @param[in,out] radar radar that may detect the UFO.
 * @param[in] posRadar Position of @c radar
 * @param[in,out] ufo aircraft that should be checked.
 * @param[in] detected Is the UFO already detected by another radar? (Beware: this is not the same as ufo->detected)
 * @return true if the aircraft is inside sensor and was sensored
 * @sa UFO_CampaignCheckEvents
 * @sa CP_CheckNewMissionDetectedOnGeoscape
 */
qboolean RADAR_CheckUFOSensored (radar_t *radar, const vec2_t posRadar,
	const aircraft_t *ufo, qboolean detected)
{
	int dist;
	qboolean ufoIsSensored;

	/* indice of ufo in radar list */
	ufoIsSensored = RADAR_IsUFOSensored(radar, ufo);
	/* Distance from radar to ufo */
	dist = GetDistanceOnGlobe(posRadar, ufo->pos);

	if ((detected ? radar->trackingRange : radar->range) > dist) {
		if (detected) {
			if (!ufoIsSensored) {
				/* UFO was not sensored by this radar, but by another one
				 * (it just entered this radar zone) */
				RADAR_AddUFO(radar, ufo);
			}
			return qtrue;
		} else {
			/* UFO is sensored by no radar, so it shouldn't be sensored
			 * by this radar */
			assert(ufoIsSensored == qfalse);
			/* Check if UFO is detected */
			if (frand() <= radar->ufoDetectionProbability) {
				RADAR_AddDetectedUFOToEveryRadar(ufo);
				return qtrue;
			}
			return qfalse;
		}
	}

	/* UFO is not in this sensor range any more (but maybe
	 * in the range of another radar) */
	if (ufoIsSensored)
		RADAR_RemoveUFO(radar, ufo);

	return qfalse;
}

/**
 * @brief Set radar to proper values after loading
 * @note numUFOs is not saved, so we must calculate it.
 * @note should be called after loading.
 */
void RADAR_SetRadarAfterLoading (void)
{
	aircraft_t *ufo;

	ufo = NULL;
	while ((ufo = UFO_GetNext(ufo)) != NULL) {
		if (!ufo->detected)
			continue;

		RADAR_AddDetectedUFOToEveryRadar(ufo);
	}

	MAP_UpdateGeoscapeDock();
}
