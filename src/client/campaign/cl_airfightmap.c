/**
 * @file cl_airfightmap.c
 * @brief Handles airfights between aircraft / UFOs
 */

/*
Copyright (C) 2002-2008 UFO: Alien Invasion team.

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

#include "../client.h"
#include "../cl_screen.h"
#include "../renderer/r_draw.h"
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "../menu/m_nodes.h"
#include "cl_campaign.h"
#include "cl_mapfightequip.h"
#include "cl_popup.h"
#include "cl_airfightmap.h"
#include "cl_ufo.h"
#include "cl_map.h"
#include "cp_time.h"

static aircraft_t *aircraftList[MAX_AIRCRAFT];
static aircraft_t *ufoList[MAX_AIRCRAFT];
static int numAircraftList, numUFOList;
static vec2_t airFightMapCenter;
static vec2_t smoothFinalAirFightCenter = {0.5, 0.5};		/**< value of ccs.center for a smooth change of position (see MAP_CenterOnPoint) */
static float smoothFinalZoom = 0.0f;		/**< value of finale ccs.zoom for a smooth change of angle (see MAP_CenterOnPoint)*/
static qboolean airFightSmoothMovement = qfalse;

static const int BULLET_SIZE = 1;					/**< the pixel size of a bullet */
static const float AF_SMOOTHING_STEP_2D = 0.02f;	/**< @todo */

/**
 * @brief Fills the @c aircraftList array with aircraft that are in the given combat range.
 * @param[in] maxDistance maximum distance from UFO centered on combat zoom to have aircraft added to list.
 * @see AFM_GetUFOsInCombatRange
 */
static void AFM_GetAircraftInCombatRange (float maxDistance)
{
	int baseIdx, aircraftIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			aircraft_t *aircraft = &base->aircraft[aircraftIdx];
			if (AIR_IsAircraftOnGeoscape(aircraft)) {
				/* const float maxRange = AIR_GetMaxAircraftWeaponRange(aircraft->weapons, aircraft->maxWeapons);*/
				if (aircraft->aircraftTarget == ccs.combatZoomedUFO || aircraft == ccs.combatZoomedUFO->aircraftTarget) {
					const float distanceToTarget = MAP_GetDistance(aircraft->pos, ccs.combatZoomedUFO->pos);
					if (distanceToTarget < maxDistance) {
						assert(numAircraftList < MAX_AIRCRAFT);
						aircraftList[numAircraftList++] = aircraft;
					}
				}
			}
		}
	}
}

/**
 * @brief Fills the @c aircraftList array with UFOs that are in the given combat range.
 * @param[in] maxDistance maximum distance from UFO centered on combat zoom to have UFO added to list.
 * @see AFM_GetAircraftInCombatRange
 */
static void AFM_GetUFOsInCombatRange (float maxDistance)
{
	int ufoIdx;

	for (ufoIdx = 0; ufoIdx < ccs.numUFOs; ufoIdx++) {
		aircraft_t *ufo = &ccs.ufos[ufoIdx];
		const float distance = MAP_GetDistance(ufo->pos, ccs.combatZoomedUFO->pos);
		if (distance < maxDistance || ccs.combatZoomedUFO == ufo) {
			assert(numUFOList < MAX_AIRCRAFT);
			ufoList[numUFOList++] = ufo;
		}
	}
}

/**
 * @brief Inits the aircraft combat menu and some structs
 */
void AFM_Init_f (void)
{
	const float MAXIMUM_COMBAT_RANGE = 4.0f;
	Vector2Copy(ccs.combatZoomedUFO->pos, airFightMapCenter);

	ccs.zoom = 1.2f;
	Vector2Set(ccs.center, 0.5f, 0.5f);

	Vector2Set(smoothFinalAirFightCenter, 0.5f, 0.5f);
	smoothFinalZoom = 8.0f;
	airFightSmoothMovement = qtrue;

	memset(aircraftList, 0, sizeof(aircraftList));
	memset(ufoList, 0, sizeof(ufoList));
	numAircraftList = 0;
	numUFOList = 0;

	AFM_GetAircraftInCombatRange(MAXIMUM_COMBAT_RANGE);
	AFM_GetUFOsInCombatRange(MAXIMUM_COMBAT_RANGE);
}

/**
 * @brief Exits the aircraft combat menu
 */
void AFM_Exit_f (void)
{
	if (ccs.combatZoomedUFO) {
		ccs.combatZoomOn = qfalse;
		ccs.combatZoomedUFO = NULL;
		CL_EnsureValidGameLapseForGeoscape();
	}
}

#define CIRCLE_DRAW_POINTS	60
/**
 * @brief Draw equidistant points from a given point on a menu node
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] center The latitude and longitude of center point
 * @param[in] angle The angle defining the distance of the equidistant points to center
 * @param[in] color The color for drawing
 * @sa RADAR_DrawCoverage
 */
static void AFM_MapDrawEquidistantPoints (const menuNode_t* node, const vec2_t center, const float angle, const vec4_t color)
{
	int i, xCircle, yCircle;
	screenPoint_t pts[CIRCLE_DRAW_POINTS + 1];
	vec2_t posCircle;
	qboolean oldDraw = qfalse;
	int numPoints = 0;
	vec3_t initialVector, rotationAxis, currentPoint, centerPos;

	R_ColorBlend(color);

	/* Set centerPos corresponding to cartesian coordinates of the center point */
	PolarToVec(center, centerPos);

	/* Find a perpendicular vector to centerPos, and rotate centerPos around him to obtain one point distant of angle from centerPos */
	PerpendicularVector(rotationAxis, centerPos);
	RotatePointAroundVector(initialVector, rotationAxis, centerPos, angle);

	/* Now, each equidistant point is given by a rotation around centerPos */
	for (i = 0; i <= CIRCLE_DRAW_POINTS; i++) {
		qboolean draw = qfalse;
		const float degrees = i * 360.0f / (float)CIRCLE_DRAW_POINTS;
		RotatePointAroundVector(currentPoint, centerPos, initialVector, degrees);
		VecToPolar(currentPoint, posCircle);
		if (AFM_MapToScreen(node, posCircle, &xCircle, &yCircle))
			draw = qtrue;

		/* if moving from a point of the screen to a distant one, draw the path we already calculated, and begin a new path
		 * (to avoid unwanted lines) */
		if (draw != oldDraw && i != 0) {
			R_DrawLineStrip(numPoints, (int*)(&pts));
			numPoints = 0;
		}

		/* if the current point is to be drawn, add it to the path */
		if (draw) {
			pts[numPoints].x = xCircle;
			pts[numPoints].y = yCircle;
			numPoints++;
		}

		/* update value of oldDraw */
		oldDraw = draw;
	}

	/* Draw the last path */
	R_DrawLineStrip(numPoints, (int*)(&pts));
	R_ColorBlend(NULL);
}

/**
 * @brief Draw a bullet onto the geoscape map
 * @param[in] node Pointer to the node in which you want to draw the bullets.
 * @param[in] pos The position on screen (x and y coordinates) to draw the bullet at
 * @sa MAP_DrawMap
 */
static void AFM_DrawBullet (const menuNode_t* node, const vec3_t pos)
{
	int x, y;
	const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};

	if (AFM_MapToScreen(node, pos, &x, &y))
		R_DrawFill(x, y, BULLET_SIZE, BULLET_SIZE, ALIGN_CC, yellow);
}

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x X coordinate on the screen
 * @param[out] y Y coordinate on the screen
 * @returns qtrue if the screen position is within the boundaries of the menu
 * node. Otherwise returns qfalse.
 * @sa MAP_3DMapToScreen
 */
qboolean AFM_MapToScreen (const menuNode_t* node, const vec2_t pos, int *x, int *y)
{
	float sx;

	/* get "raw" position */
	sx = ((pos[0] - airFightMapCenter[0]) / 360) * 12 + ccs.center[0] - 0.5;

	/* shift it on screen */
	/** @todo: shouldn't this be a while loop? just in case? */
	if (sx < -0.5)
		sx += 1.0;
	else if (sx > 0.5)
		sx -= 1.0;

	*x = node->pos[0] + 0.5 * node->size[0] - sx * node->size[0] * ccs.zoom; /* (ccs.zoom * 0.0379); */
	*y = node->pos[1] + 0.5 * node->size[1] -
		(((pos[1] - airFightMapCenter[1]) / 180) * 12 + ccs.center[1] - 0.5) * node->size[1] * ccs.zoom; /* (ccs.zoom * 0.0379); */

	if (*x < node->pos[0] && *y < node->pos[1]
	 && *x > node->pos[0] + node->size[0] && *y > node->pos[1] + node->size[1])
		return qfalse;
	return qtrue;
}


/**
 * @brief Draws a 3D marker on geoscape if the player can see it.
 * @param[in] node Menu node.
 * @param[in] pos Longitude and latitude of the marker to draw.
 * @param[in] theta Angle (degree) of the model to the horizontal.
 * @param[in] model The name of the model of the marker.
 */
qboolean AFM_Draw3DMarkerIfVisible (const menuNode_t* node, const vec2_t pos, float theta, const char *model, int skin)
{
	int x, y;
	const qboolean test = AFM_MapToScreen(node, pos, &x, &y);

	if (test) {
		vec3_t screenPos, angles;
		const int z = -10;
		const float zoom = ccs.zoom / 37.0f;

		/* Set position of the model on the screen */
		VectorSet(screenPos, x, y, z);
		VectorSet(angles, theta, 180, 0);

		/* Draw */
		R_Draw3DMapMarkers(angles, zoom, screenPos, model, skin);
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief
 * @param[in] pos
 */
static void AFM_CenterMapPosition (const vec3_t pos)
{
	Vector2Set(ccs.center, 0.5f - ((pos[0] - airFightMapCenter[0]) / 360) * 12, 0.5f - ((pos[1] - airFightMapCenter[1]) / 180) * 12);
}

#if 0
/**
 * @brief
 * @param[in] maxInterpolationPoints
 * @param[in] pos
 * @param[in] projectedPos
 * @param[in] drawPos
 * @param[in] numInterpolationPoints
 */
static inline void AFM_GetNextProjectedStepPosition (const int maxInterpolationPoints, const vec3_t pos, const vec3_t projectedPos, vec3_t drawPos, int numInterpolationPoints)
{
	if (maxInterpolationPoints > 2 && numInterpolationPoints < maxInterpolationPoints) {
		/* If a new point hasn't been given and there is at least 3 points need to be filled in then
		 * use linear interpolation to draw the points until a new projectile point is provided.
		 * The reason you need at least 3 points is that acceptable results can be achieved with 2 or less
		 * gaps in points so dont add the overhead of interpolation. */
		const float xInterpolStep = (projectedPos[0] - pos[0]) / (float)maxInterpolationPoints;
		numInterpolationPoints += 1;
		drawPos[0] = pos[0] + (xInterpolStep * numInterpolationPoints);
		LinearInterpolation(pos, projectedPos, drawPos[0], drawPos[1]);
	} else {
		VectorCopy(pos, drawPos);
	}
}
#endif

/**
 * @brief Draws all ufos, aircraft, bases and so on to the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @sa MAP_DrawMap
 */
static void AFM_DrawMapMarkers (const menuNode_t* node)
{
	aircraftProjectile_t *projectile;
	int idx;
	int maxInterpolationPoints;
	const vec4_t darkBrown = {0.3608f, 0.251f, 0.2f, 1.0f};
	const vec2_t northPole = {0.0f, 90.0f};
	vec3_t centroid = {0,0,0};
	int combatAirIdx;
	vec3_t combatZoomAircraftInCombatPos[MAX_AIRCRAFT];
	int combatZoomNumCombatAircraft = 0;

	for (idx = 0; idx < numAircraftList; idx++) {
		const aircraft_t *aircraft = aircraftList[idx];
		VectorCopy(aircraft->pos, combatZoomAircraftInCombatPos[combatZoomNumCombatAircraft]);
		combatZoomNumCombatAircraft++;
	}
	for (idx = 0; idx < numUFOList; idx++) {
		const aircraft_t *aircraft = ufoList[idx];
		VectorCopy(aircraft->pos, combatZoomAircraftInCombatPos[combatZoomNumCombatAircraft]);
		combatZoomNumCombatAircraft++;
	}

	/* finds the centroid of all aircraft in combat range who are targeting the zoomed ufo
	 * and uses this as the point on which to center. */
	for (combatAirIdx = 0; combatAirIdx < combatZoomNumCombatAircraft; combatAirIdx++) {
		VectorAdd(combatZoomAircraftInCombatPos[combatAirIdx], centroid, centroid);
	}
	if (combatAirIdx > 1) {
		VectorScale(centroid, 1.0 / (float)combatAirIdx, centroid);
		AFM_CenterMapPosition(centroid);
	} else {
		AFM_CenterMapPosition(ccs.combatZoomedUFO->pos);
	}

	if (ccs.gameTimeScale > 0)
		maxInterpolationPoints = floor(1.0f / (cls.frametime * (float)ccs.gameTimeScale));
	else
		maxInterpolationPoints = 0;

	/** @todo the next two for loops are doing the same for aircraft and ufo - refactor this to some function
	 * that can be used for those aircraft and ufos */

	/* draw all aircraft in combat range */
	for (idx = 0; idx < numAircraftList; idx++) {
		vec3_t drawPos = {0, 0, 0};
		aircraft_t *aircraft = aircraftList[idx];
		float weaponRanges[MAX_AIRCRAFTSLOT];
		float angle;
		qboolean newDraw = qtrue;
		const int numWeaponRanges = AIR_GetAircraftWeaponRanges(aircraft->weapons, aircraft->maxWeapons, weaponRanges);
#if 0
		Com_DPrintf(DEBUG_CLIENT, "pos=%f,%f projPos=%f,%f \n", aircraft->pos[0], aircraft->pos[1], aircraft->projectedPos[0],
			aircraft->projectedPos[1]);
#endif
		if (maxInterpolationPoints > 2 && aircraft->numInterpolationPoints < maxInterpolationPoints) {
			/* If a new point hasn't been given and there is at least 3 points need to be filled in then
			 * use linear interpolation to draw the points until a new projectile point is provided.
			 * The reason you need at least 3 points is that acceptable results can be achieved with 2 or less
			 * gaps in points so dont add the overhead of interpolation. */
			const float xInterpolStep = (aircraft->projectedPos[0] - aircraft->pos[0]) / (float)maxInterpolationPoints;
			aircraft->numInterpolationPoints += 1;
			drawPos[0] = aircraft->pos[0] + (xInterpolStep * (float)aircraft->numInterpolationPoints);
			LinearInterpolation(aircraft->pos, aircraft->projectedPos, drawPos[0], drawPos[1]);
		} else if (maxInterpolationPoints <= 2) {
			VectorCopy(aircraft->pos, drawPos);
		} else {
			newDraw = qfalse;
		}

		if (newDraw) {
			VectorCopy(drawPos, aircraft->oldDrawPos);
		} else {
			VectorCopy(aircraft->oldDrawPos, drawPos);
		}

		/* Draw all weapon ranges for this aircraft if at least one UFO is visible */
		if (numWeaponRanges > 0) {
			int idxWeaponRanges;
			for (idxWeaponRanges = 0; idxWeaponRanges < numWeaponRanges; idxWeaponRanges++) {
				AFM_MapDrawEquidistantPoints(node, drawPos, weaponRanges[idxWeaponRanges], darkBrown);
			}
		}

		if (aircraft->status >= AIR_TRANSIT) {
			angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL);
		} else {
			angle = MAP_AngleOfPath(aircraft->pos, northPole, aircraft->direction, NULL);
		}

		AFM_Draw3DMarkerIfVisible(node, drawPos, angle, aircraft->model, 0);
	}

	/* draw all ufos in combat range */
	for (idx = 0; idx < numUFOList; idx++) {
		vec3_t drawPos = {0, 0, 0};
		aircraft_t *ufo = ufoList[idx];
		float weaponRanges[MAX_AIRCRAFTSLOT];
		float angle;
		qboolean newDraw = qtrue;
		int numWeaponRanges = AIR_GetAircraftWeaponRanges(ufo->weapons, ufo->maxWeapons, weaponRanges);

		if (maxInterpolationPoints > 2 && ufo->numInterpolationPoints < maxInterpolationPoints) {
			/* If a new point hasn't been given and there is at least 3 points need to be filled in then
			 * use linear interpolation to draw the points until a new projectile point is provided.
			 * The reason you need at least 3 points is that acceptable results can be achieved with 2 or less
			 * gaps in points so dont add the overhead of interpolation. */
			const float xInterpolStep = (ufo->projectedPos[0] - ufo->pos[0]) / (float)maxInterpolationPoints;
			ufo->numInterpolationPoints += 1;
			drawPos[0] = ufo->pos[0] + (xInterpolStep * (float)ufo->numInterpolationPoints);
			LinearInterpolation(ufo->pos, ufo->projectedPos, drawPos[0], drawPos[1]);
		} else if (maxInterpolationPoints <= 2) {
			VectorCopy(ufo->pos, drawPos);
		} else {
			newDraw = qfalse;
		}

		if (newDraw) {
			VectorCopy(drawPos, ufo->oldDrawPos);
		} else {
			VectorCopy(ufo->oldDrawPos, drawPos);
		}

		/* Draw all weapon ranges for this aircraft if at least one UFO is visible */
		if (numWeaponRanges > 0) {
			int idxWeaponRanges;
			for (idxWeaponRanges = 0; idxWeaponRanges < numWeaponRanges; idxWeaponRanges++) {
				AFM_MapDrawEquidistantPoints(node, drawPos, weaponRanges[idxWeaponRanges], darkBrown);
			}
		}

		if (ufo->status >= AIR_TRANSIT) {
			angle = MAP_AngleOfPath(ufo->pos, ufo->route.point[ufo->route.numPoints - 1], ufo->direction, NULL);
		} else {
			angle = MAP_AngleOfPath(ufo->pos, northPole, ufo->direction, NULL);
		}

		AFM_Draw3DMarkerIfVisible(node, drawPos, angle, ufo->model, 0);
	}


	/* draws projectiles */
	for (projectile = ccs.projectiles + ccs.numProjectiles - 1; projectile >= ccs.projectiles; projectile --) {
		vec3_t drawPos = {0, 0, 0};
		qboolean newDraw = qtrue;

		if (!projectile->aimedAircraft)
			continue;

		if (projectile->hasMoved) {
			projectile->hasMoved = qfalse;
			VectorCopy(projectile->pos[0], drawPos);
		} else {
#if 0
			AFM_GetNextProjectedStepPosition(projectile->numInterpolationPoints, maxInterpolationPoints, projectile->pos[0],
				projectile->projectedPos[0], drawPos);
#endif
			if (maxInterpolationPoints > 2 && projectile->numInterpolationPoints < maxInterpolationPoints) {
				/* If a new point hasn't been given and there is at least 3 points need to be filled in then
				 * use linear interpolation to draw the points until a new projectile point is provided.
				 * The reason you need at least 3 points is that acceptable results can be achieved with 2 or less
				 * gaps in points so dont add the overhead of interpolation. */
				const float xInterpolStep = (projectile->projectedPos[0][0] - projectile->pos[0][0]) / (float)maxInterpolationPoints;
				projectile->numInterpolationPoints += 1;
				drawPos[0] = projectile->pos[0][0] + (xInterpolStep * projectile->numInterpolationPoints);
				LinearInterpolation(projectile->pos[0], projectile->projectedPos[0], drawPos[0], drawPos[1]);
			} else if (maxInterpolationPoints <= 2) {
				VectorCopy(projectile->pos[0], drawPos);
			} else {
				newDraw = qfalse;
			}

			if (newDraw) {
				VectorCopy(drawPos, projectile->oldDrawPos[0]);
			} else {
				VectorCopy(projectile->oldDrawPos[0], drawPos);
			}
		}

		if (projectile->bullets)
			AFM_DrawBullet(node, drawPos);
#if 0
		else if (projectile->laser)
			/** @todo Implement rendering of laser shot */
			 MAP_DrawLaser(node, vec3_origin, vec3_origin);
#endif
		else
			AFM_Draw3DMarkerIfVisible(node, drawPos, projectile->angle, projectile->aircraftItem->model, 0);
	}

}

/**
 * @brief smooth translation of the 2D geoscape
 * @note updates slowly values of ccs.center so that its value goes to smoothFinal2DGeoscapeCenter
 * @note updates slowly values of ccs.zoom so that its value goes to ZOOM_LIMIT
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
static void AFM_SmoothTranslate (void)
{
#if 0
	const float dist1 = smoothFinalAirFightCenter[0] - ccs.center[0];
	const float dist2 = smoothFinalAirFightCenter[1] - ccs.center[1];
	const float length = sqrt(dist1 * dist1 + dist2 * dist2);
#endif
	const float diff_zoom = smoothFinalZoom - ccs.zoom;
	const float speed = 1.0f;

	if (/*length < AF_SMOOTHING_STEP_2D && */ fabs(diff_zoom) < AF_SMOOTHING_STEP_2D) {
#if 0
		ccs.center[0] = smoothFinalAirFightCenter[0];
		ccs.center[1] = smoothFinalAirFightCenter[1];
#endif
		ccs.zoom = smoothFinalZoom;
		airFightSmoothMovement = qfalse;
	} else {
#if 0
		if (length > 0)
			ccs.center[0] = ccs.center[0] + AF_SMOOTHING_STEP_2D * (dist1) / length;
			ccs.center[1] = ccs.center[1] + AF_SMOOTHING_STEP_2D * (dist2) / length;
			ccs.zoom = ccs.zoom + AF_SMOOTHING_STEP_2D * (diff_zoom * speed) / length;
		else
#endif
			ccs.zoom = ccs.zoom + AF_SMOOTHING_STEP_2D * (diff_zoom * speed);
	}
}

void AFM_StopSmoothMovement (void)
{
	airFightSmoothMovement = qfalse;
}

/**
 * @brief Draw the geoscape
 * @param[in] node The map menu node
 * @sa MAP_DrawMapMarkers
 */
void AFM_DrawMap (const menuNode_t* node)
{
	/* store these values in ccs struct to be able to handle this even in the input code */
	Vector2Copy(node->pos, ccs.mapPos);
	Vector2Copy(node->size, ccs.mapSize);

	if (airFightSmoothMovement)
		AFM_SmoothTranslate();
	R_DrawAirFightBackground(node->pos[0], node->pos[1], node->size[0], node->size[1], ccs.center[0], ccs.center[1], (0.5 / ccs.zoom));
	AFM_DrawMapMarkers (node);
}
