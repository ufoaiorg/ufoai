/**
 * @file cp_ufo.c
 * @brief UFOs on geoscape
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

#include "../client.h"
#include "../renderer/r_draw.h"
#include "cp_campaign.h"
#include "cp_popup.h"
#include "cp_map.h"
#include "cp_ufo.h"
#include "cp_aircraft.h"
#include "cp_mapfightequip.h"

static const float MAX_DETECTING_RANGE = 25.0f; /**< range to detect and fire at phalanx aircraft */

typedef struct ufoTypeList_s {
	const char *id;		/**< script id string */
	const char *crashedId;		/**< script id string */
	int ufoType;		/**< ufoType_t values */
} ufoTypeList_t;

/**
 * @brief Valid ufo types
 * @note Use the same values for the names as we are already using in the scriptfiles
 * here, otherwise they are not translatable because they don't appear in the po files
 * @note Every ufotype (id) that doesn't have nogeoscape set to true must have an assembly
 * in the ufocrash[dn].ump files
 */
static const ufoTypeList_t ufoTypeList[] = {
	{"craft_ufo_bomber", "craft_crash_bomber", UFO_BOMBER},
	{"craft_ufo_carrier", "craft_crash_carrier", UFO_CARRIER},
	{"craft_ufo_corrupter", "craft_crash_corrupter", UFO_CORRUPTER},
	{"craft_ufo_fighter", "craft_crash_fighter", UFO_FIGHTER},
	{"craft_ufo_harvester", "craft_crash_harvester", UFO_HARVESTER},
	{"craft_ufo_scout", "craft_crash_scout", UFO_SCOUT},
	{"craft_ufo_supply", "craft_crash_supply", UFO_SUPPLY},
	{"craft_ufo_gunboat", "craft_crash_gunboat", UFO_GUNBOAT},
	{"craft_ufo_ripper", "craft_crash_ripper", UFO_RIPPER},
	{"craft_ufo_mothership", "craft_crash_mothership", UFO_MOTHERSHIP},

	{NULL, 0}
};

/**
 * @brief Translate UFO type to short name.
 * @sa UFO_TypeToName
 * @sa UFO_TypeToShortName
 */
ufoType_t UFO_ShortNameToID (const char *token)
{
	const ufoTypeList_t *list = ufoTypeList;

	while (list->id) {
		if (!strcmp(list->id, token))
			return list->ufoType;
		list++;
	}
	Com_Printf("UFO_ShortNameToID: Unknown ufo-type: %s\n", token);
	return UFO_MAX;
}

/**
 * @brief Translate UFO type to short name.
 * @sa UFO_TypeToName
 * @sa UFO_ShortNameToID
 */
const char* UFO_TypeToShortName (ufoType_t type)
{
	const ufoTypeList_t *list = ufoTypeList;

	while (list->id) {
		if (list->ufoType == type)
			return list->id;
		list++;
	}
	Com_Error(ERR_DROP, "UFO_TypeToShortName(): Unknown UFO type %i", type);
}

/**
 * @brief Translate UFO type to short name when UFO is crashed.
 * @sa UFO_TypeToShortName
 */
const char* UFO_CrashedTypeToShortName (ufoType_t type)
{
	const ufoTypeList_t *list = ufoTypeList;

	while (list->id) {
		if (list->ufoType == type)
			return list->crashedId;
		list++;
	}
	Com_Error(ERR_DROP, "UFO_TypeToShortName(): Unknown UFO type %i", type);
}

/**
 * @brief Translate UFO type to name.
 * @param[in] type UFO type in ufoType_t.
 * @return Translated UFO name.
 * @sa UFO_TypeToShortName
 * @sa UFO_ShortNameToID
 */
const char* UFO_TypeToName (ufoType_t type)
{
	const char *id = UFO_TypeToShortName(type);
	const technology_t *tech = RS_GetTechByProvided(id);
	if (tech)
		return _(tech->name);
	Com_Error(ERR_DROP, "UFO_TypeToName(): Unknown UFO type %i - no tech for '%s'\n", type, id);
}
/**
 * @brief Returns names of the UFO is UFO has been reseached.
 * @param[in] ufocraft Pointer to the UFO.
 */
const char* UFO_AircraftToIDOnGeoscape (aircraft_t *ufocraft)
{
	const technology_t *tech = ufocraft->tech;

	assert(tech);

	if (ufocraft->detectionIdx)
		return va("%s #%i", (RS_IsResearched_ptr(tech)) ? _(ufocraft->name) : _("UFO"), ufocraft->detectionIdx);
	return (RS_IsResearched_ptr(tech)) ? _(ufocraft->name) : _("UFO");
}

/**
 * @brief Returns a status string for recovered ufo used to be displayed in uforecovery and mission result overview.
 * @note this actually relies on valid mission results.
 */
const char* UFO_MissionResultToString (void)
{
	if (ccs.missionResults.crashsite)
		return va(_("\nSecured crashed %s\n"), UFO_TypeToName(ccs.missionResults.ufotype));
	else
		return va(_("\nSecured landed %s\n"), UFO_TypeToName(ccs.missionResults.ufotype));
}

/**
 * @brief Give a random destination to the given UFO, and make him to move there.
 * @param[in] ufocraft Pointer to the UFO which destination will be changed.
 * @sa UFO_SetRandomPos
 */
void UFO_SetRandomDest (aircraft_t* ufocraft)
{
	vec2_t pos;

	CP_GetRandomPosOnGeoscape(pos, qfalse);

	UFO_SendToDestination(ufocraft, pos);
}

/**
 * @brief Give a random destination to the given UFO close to a position, and make him to move there.
 * @param[in] ufocraft Pointer to the UFO which destination will be changed.
 * @param[in] pos The position the UFO should around.
 * @sa UFO_SetRandomPos
 */
void UFO_SetRandomDestAround (aircraft_t* ufocraft, vec2_t pos)
{
	vec2_t dest;
	const float spread = 2.0f;
	float rand1, rand2;

	gaussrand(&rand1, &rand2);
	rand1 *= spread;
	rand2 *= spread;

	Vector2Set(dest, pos[0] + rand1, pos[1] + rand2);

	UFO_SendToDestination(ufocraft, dest);
}

/**
 * @brief Give a random position to the given UFO
 * @param[in] ufocraft Pointer to the UFO which position will be changed.
 * @sa UFO_SetRandomDest
 */
static void UFO_SetRandomPos (aircraft_t* ufocraft)
{
	vec2_t pos;

	CP_GetRandomPosOnGeoscape(pos, qfalse);

	Vector2Copy(pos, ufocraft->pos);
}

#ifdef UFO_ATTACK_BASES
/**
 * @brief Make the specified UFO attack a base.
 * @param[in] ufo Pointer to the UFO.
 * @param[in] base Pointer to the target base.
 * @sa AIR_SendAircraftPursuingUFO
 */
static qboolean UFO_SendAttackBase (aircraft_t* ufo, base_t* base)
{
	int slotIdx;

	assert(ufo);
	assert(base);

	/* check whether the ufo can shoot the base - if not, don't try it even */
	slotIdx = AIRFIGHT_ChooseWeapon(ufo->weapons, ufo->maxWeapons, ufo->pos, base->pos);
	if (slotIdx != AIRFIGHT_WEAPON_CAN_SHOOT)
		return qfalse;

	MAP_MapCalcLine(ufo->pos, base->pos, &ufo->route);
	ufo->baseTarget = base;
	ufo->aircraftTarget = NULL;
	ufo->status = AIR_UFO; /** @todo Might crash in cl_map.c MAP_DrawMapMarkers */
	ufo->time = 0;
	ufo->point = 0;
	return qtrue;
}

/**
 * @brief Check if the ufo can shoot at a Base
 */
static void UFO_SearchBaseTarget (aircraft_t *ufo)
{
	float distance = 999999., dist;
	int baseIdx;

	/* check if the ufo is already attacking an aircraft */
	if (ufo->aircraftTarget)
		return;

	/* check if the ufo is already attacking a base */
	if (ufo->baseTarget) {
		AIRFIGHT_ExecuteActions(ufo, NULL);
		return;
	}

	/* No target, try to find a new base to attack */
	ufo->status = AIR_TRANSIT;
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		/* check if the ufo can attack a base (if it's not too far) */
		if (base->isDiscovered && (MAP_GetDistance(ufo->pos, base->pos) < MAX_DETECTING_RANGE)) {
			if (UFO_SendAttackBase(ufo, base))
				/* don't check for aircraft if we can destroy a base */
				continue;
		}
	}
}
#endif

/**
 * @brief Check if a UFO is the target of a base
 * @param[in] ufo The UFO to check
 * @param[in] base Pointer to the base
 * @return 0 if ufo is not a target, 1 if target of a missile, 2 if target of a laser
 */
static int UFO_IsTargetOfBase (const aircraft_t const *ufo, const base_t const *base)
{
	int i;

	for (i = 0; i < base->numBatteries; i++) {
		if (base->batteries[i].target == ufo)
			return UFO_IS_TARGET_OF_MISSILE;
	}

	for (i = 0; i < base->numLasers; i++) {
		if (base->lasers[i].target == ufo)
			return UFO_IS_TARGET_OF_LASER;
	}

	return UFO_IS_NO_TARGET;
}

/**
 * @brief Check if a UFO is the target of an installation
 * @param[in] ufo The UFO to check
 * @param[in] installation Pointer to the installation
 * @return UFO_IS_NO_TARGET if ufo is not a target, UFO_IS_TARGET_OF_MISSILE if target of a missile
 */
static int UFO_IsTargetOfInstallation (const aircraft_t const *ufo, const installation_t const *installation)
{
	int i;

	for (i = 0; i < installation->numBatteries; i++) {
		if (installation->batteries[i].target == ufo)
			return UFO_IS_TARGET_OF_MISSILE;
	}

	return UFO_IS_NO_TARGET;
}

/**
 * @brief Update alien interest for one PHALANX base
 * @param[in] ufo Pointer to the aircraft_t
 * @param[in] base Pointer to the base
 * @note This function will be called quite often (every @c DETECTION_INTERVAL), so it must stay simple.
 * it must not depend on @c dt , otherwise alien interest will depend on time scale.
 * @note this function will only be used to determine WHICH base will be attacked,
 * and not IF a base will be attacked.
 * @sa UFO_UpdateAlienInterestForAllBases
 * @sa AB_UpdateStealthForOneBase
 */
static void UFO_UpdateAlienInterestForOneBase (const aircraft_t const *ufo, base_t *base)
{
	float probability;
	float distance;
	const float decreasingDistance = 10.0f;	/**< above this distance, probability to detect base will
												decrease by @c decreasingFactor */
	const float decreasingFactor = 5.0f;

	/* ufo can't find base if it's too far */
	distance = MAP_GetDistance(ufo->pos, base->pos);
	if (distance > MAX_DETECTING_RANGE)
		return;

	/* UFO has an increased probability to find a base if it is firing at it */
	switch (UFO_IsTargetOfBase(ufo, base)) {
	case UFO_IS_TARGET_OF_MISSILE:
		probability = 0.01f;
		break;
	case UFO_IS_TARGET_OF_LASER:
		probability = 0.001f;
		break;
	default:
		probability = 0.0001f;
		break;
	}

	/* decrease probability if the ufo is far from base */
	if (distance > decreasingDistance)
		probability /= decreasingFactor;

	/* probability must depend on DETECTION_INTERVAL (in case we change the value) */
	probability *= DETECTION_INTERVAL;

	base->alienInterest += probability;
}

/**
 * @brief Update alien interest for one PHALANX installation (radar tower, SAM, ...)
 * @param[in] ufo Pointer to the aircraft_t
 * @param[in] installation Pointer to the installation
 * @sa UFO_UpdateAlienInterestForOneBase
 */
static void UFO_UpdateAlienInterestForOneInstallation (const aircraft_t const *ufo, installation_t *installation)
{
	float probability;
	float distance;
	const float decreasingDistance = 10.0f;	/**< above this distance, probability to detect base will
												decrease by @c decreasingFactor */
	const float decreasingFactor = 5.0f;

	/* ufo can't find base if it's too far */
	distance = MAP_GetDistance(ufo->pos, installation->pos);
	if (distance > MAX_DETECTING_RANGE)
		return;

	/* UFO has an increased probability to find a base if it is firing at it */
	switch (UFO_IsTargetOfInstallation(ufo, installation)) {
	case UFO_IS_TARGET_OF_MISSILE:
		probability = 0.01f;
		break;
	case UFO_IS_TARGET_OF_LASER:
		probability = 0.001f;
		break;
	default:
		probability = 0.0001f;
		break;
	}

	/* decrease probability if the ufo is far from base */
	if (distance > decreasingDistance)
		probability /= decreasingFactor;

	/* probability must depend on DETECTION_INTERVAL (in case we change the value) */
	probability *= DETECTION_INTERVAL;

	installation->alienInterest += probability;
}

/**
 * @brief Update alien interest for all PHALANX bases.
 * @note called every @c DETECTION_INTERVAL
 * @sa UFO_UpdateAlienInterestForOneBase
 * @sa CL_CampaignRun
 */
void UFO_UpdateAlienInterestForAllBasesAndInstallations (void)
{
	int ufoIdx;

	for (ufoIdx = 0; ufoIdx < ccs.numUFOs; ufoIdx++) {
		const aircraft_t const *ufo = &ccs.ufos[ufoIdx];
		int idx;

		/* landed UFO can't detect any phalanx base or installation */
		if (ufo->landed)
			continue;

		for (idx = 0; idx < MAX_BASES; idx++) {
			base_t *base = B_GetFoundedBaseByIDX(idx);
			if (!base)
				continue;
			UFO_UpdateAlienInterestForOneBase(ufo, base);
		}

		for (idx = 0; idx < MAX_INSTALLATIONS; idx++) {
			installation_t *installation = INS_GetFoundedInstallationByIDX(idx);
			if (!installation)
				continue;
			UFO_UpdateAlienInterestForOneInstallation(ufo, installation);
		}
	}
}

/**
 * @brief Check if the ufo can shoot at a PHALANX aircraft
 */
static void UFO_SearchAircraftTarget (aircraft_t *ufo)
{
	int baseIdx;
	aircraft_t* phalanxAircraft;
	float distance = 999999.;

	/* UFO never try to attack a PHALANX aircraft except if they came on earth in that aim */
	if (ufo->mission->stage != STAGE_INTERCEPT) {
		/* Check if UFO is defending itself */
		if (ufo->aircraftTarget)
			UFO_CheckShootBack(ufo, ufo->aircraftTarget);
		return;
	}

#ifdef UFO_ATTACK_BASES
	/* check if the ufo is already attacking a base */
	if (ufo->baseTarget)
		return;
#endif

	/* check if the ufo is already attacking an aircraft */
	if (ufo->aircraftTarget) {
		/* check if the target disappeared from geoscape (fled in a base) */
		if (AIR_IsAircraftOnGeoscape(ufo->aircraftTarget))
			AIRFIGHT_ExecuteActions(ufo, ufo->aircraftTarget);
		else
			ufo->aircraftTarget = NULL;
		return;
	}

	ufo->status = AIR_TRANSIT;
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		/* check if the ufo can attack an aircraft
		 * reverse order - because aircraft can be destroyed in here */
		for (phalanxAircraft = base->aircraft + base->numAircraftInBase - 1; phalanxAircraft >= base->aircraft; phalanxAircraft--) {
			/* check that aircraft is flying */
			if (AIR_IsAircraftOnGeoscape(phalanxAircraft)) {
				/* get the distance from ufo to aircraft */
				const float dist = MAP_GetDistance(ufo->pos, phalanxAircraft->pos);
				/* check out of reach */
				if (dist > MAX_DETECTING_RANGE)
					continue;
				/* choose the nearest target */
				if (dist < distance) {
					distance = dist;
					if (UFO_SendPursuingAircraft(ufo, phalanxAircraft) && UFO_IsUFOSeenOnGeoscape(ufo)) {
						/* stop time and notify */
						MSO_CheckAddNewMessage(NT_UFO_ATTACKING, _("Notice"), va(_("A UFO is flying toward %s"), _(phalanxAircraft->name)), qfalse, MSG_STANDARD, NULL);
						/** @todo present a popup with possible orders like: return to base, attack the ufo, try to flee the rockets */
					}
				}
			}
		}
	}
}

/**
 * @brief Make the specified UFO pursue a phalanx aircraft.
 * @param[in] ufo Pointer to the UFO.
 * @param[in] aircraft Pointer to the target aircraft.
 * @sa UFO_SendAttackBase
 */
qboolean UFO_SendPursuingAircraft (aircraft_t* ufo, aircraft_t* aircraft)
{
	int slotIdx;
	vec2_t dest;

	assert(ufo);
	assert(aircraft);

	/* check whether the ufo can shoot the aircraft - if not, don't try it even */
	slotIdx = AIRFIGHT_ChooseWeapon(ufo->weapons, ufo->maxWeapons, ufo->pos, aircraft->pos);
	if (slotIdx == AIRFIGHT_WEAPON_CAN_NEVER_SHOOT) {
		/* no ammo left: stop attack */
		ufo->status = AIR_TRANSIT;
		return qfalse;
	}

	AIR_GetDestinationWhilePursuing(ufo, aircraft, &dest);
	MAP_MapCalcLine(ufo->pos, dest, &ufo->route);
	ufo->status = AIR_UFO;
	ufo->time = 0;
	ufo->point = 0;
	ufo->aircraftTarget = aircraft;
	ufo->baseTarget = NULL;

	return qtrue;
}

/**
 * @brief Make the specified UFO go to destination.
 * @param[in] ufo Pointer to the UFO.
 * @param[in] dest Destination.
 * @sa UFO_SendAttackBase
 */
void UFO_SendToDestination (aircraft_t* ufo, vec2_t dest)
{
	assert(ufo);

	MAP_MapCalcLine(ufo->pos, dest, &ufo->route);
	ufo->status = AIR_TRANSIT;
	ufo->time = 0;
	ufo->point = 0;
}

/**
 * @brief Check if the ufo can shoot back at phalanx aircraft
 */
void UFO_CheckShootBack (aircraft_t *ufo, aircraft_t* phalanxAircraft)
{
	/* check if the ufo is already attacking a base */
	if (ufo->baseTarget) {
		AIRFIGHT_ExecuteActions(ufo, NULL);
	/* check if the ufo is already attacking an aircraft */
	} else if (ufo->aircraftTarget) {
		/* check if the target flee in a base */
		if (AIR_IsAircraftOnGeoscape(ufo->aircraftTarget))
			AIRFIGHT_ExecuteActions(ufo, ufo->aircraftTarget);
		else {
			ufo->aircraftTarget = NULL;
			CP_UFOProceedMission(ufo);
		}
	} else {
		/* check that aircraft is flying */
		if (AIR_IsAircraftOnGeoscape(phalanxAircraft))
			UFO_SendPursuingAircraft(ufo, phalanxAircraft);
	}
}

/**
 * @brief Make the UFOs run
 * @param[in] dt time delta
 */
void UFO_CampaignRunUFOs (int dt)
{
	int ufoIdx, k;

	assert(dt >= 0);

	/* dt may be 0 if a UFO has been detection occured (see CL_CampaignRun) */
	if (!dt)
		return;

	/* now the ufos are flying around, too - cycle backward - ufo might be destroyed */
	for (ufoIdx = ccs.numUFOs - 1; ufoIdx >= 0; ufoIdx--) {
		aircraft_t *ufo = &ccs.ufos[ufoIdx];
		/* don't run a landed ufo */
		if (ufo->landed)
			continue;

		/* Every UFO on geoscape should have a mission assigned */
		assert(ufo->mission);

#ifdef UFO_ATTACK_BASES
		/* Check if the UFO found a new base */
		UFO_FoundNewBase(ufo, dt);
#endif

		/* reached target and not following a phalanx aircraft? then we need a new destination */
		if (AIR_AircraftMakeMove(dt, ufo) && ufo->status != AIR_UFO) {
			float *end;
			end = ufo->route.point[ufo->route.numPoints - 1];
			Vector2Copy(end, ufo->pos);
			MAP_CheckPositionBoundaries(ufo->pos);
			if (ufo->mission->stage == STAGE_INTERCEPT && ufo->mission->data) {
				/* Attacking an installation: fly over this installation */
				UFO_SetRandomDestAround(ufo, ufo->mission->pos);
			} else
				UFO_SetRandomDest(ufo);
			if (CP_CheckNextStageDestination(ufo))
				/* UFO has been removed from game */
				continue;
		}

		/* is there a PHALANX aircraft to shoot at ? */
		UFO_SearchAircraftTarget(ufo);

#ifdef UFO_ATTACK_BASES
		/* is there a PHALANX base to shoot at ? */
		UFO_SearchBaseTarget(ufo);
#endif

		/* antimatter tanks */
		if (ufo->fuel <= 0)
			ufo->fuel = ufo->stats[AIR_STATS_FUELSIZE];

		/* Update delay to launch next projectile */
		for (k = 0; k < ufo->maxWeapons; k++) {
			if (ufo->weapons[k].delayNextShot > 0)
				ufo->weapons[k].delayNextShot -= dt;
		}
	}
}

#ifdef DEBUG
/**
 * @brief Debug function to destroy all the UFOs that are currently on the geoscape
 */
static void UFO_DestroyUFOs_f (void)
{
	aircraft_t* ufo;

	for (ufo = ccs.ufos; ufo < ccs.ufos + ccs.numUFOs; ufo++) {
		AIRFIGHT_ActionsAfterAirfight(NULL, ufo, qtrue);
	}
}

/**
 * @brief Write the ufo list, for debugging
 * @note called with debug_listufo
 */
static void UFO_ListOnGeoscape_f (void)
{
	aircraft_t* ufo;
	int k;

	Com_Printf("There are %i UFOs in game\n", ccs.numUFOs);
	for (ufo = ccs.ufos; ufo < ccs.ufos + ccs.numUFOs; ufo++) {
		Com_Printf("..%s (%s) - status: %i - pos: %.0f:%0.f\n", ufo->name, ufo->id, ufo->status, ufo->pos[0], ufo->pos[1]);
		Com_Printf("...route length: %i (current: %i), time: %i, distance: %.2f, speed: %i\n",
			ufo->route.numPoints, ufo->point, ufo->time, ufo->route.distance, ufo->stats[AIR_STATS_SPEED]);
		Com_Printf("...linked to mission '%s'\n", ufo->mission ? ufo->mission->id : "no mission");
		Com_Printf("... UFO is %s and %s\n", ufo->landed ? "landed" : "flying", ufo->detected ? "detected" : "undetected");
		Com_Printf("... damage: %i\n", ufo->damage);
		Com_Printf("...%i weapon slots: ", ufo->maxWeapons);
		for (k = 0; k < ufo->maxWeapons; k++) {
			aircraftSlot_t const* const w = &ufo->weapons[k];
			if (w->item) {
				char const* const state = w->ammo && w->ammoLeft > 0 ?
					"(loaded)" : "(unloaded)";
				Com_Printf("%s %s / ", w->item->id, state);
			} else
				Com_Printf("empty / ");
		}
		Com_Printf("\n");
	}
}
#endif

/**
 * @brief Add a UFO to geoscape
 * @param[in] ufoType The type of ufo (fighter, scout, ...).
 * @param[in] destination Position where the ufo should go. NULL is randomly chosen
 * @param[in] mission Pointer to the mission the UFO is involved in
 * @todo UFOs are not assigned unique idx fields. Could be handy...
 * @sa UFO_RemoveFromGeoscape
 * @sa UFO_RemoveFromGeoscape_f
 */
aircraft_t *UFO_AddToGeoscape (ufoType_t ufoType, vec2_t destination, mission_t *mission)
{
	int newUFONum;
	aircraft_t *ufo = NULL;

	if (ufoType == UFO_MAX) {
		Com_Printf("UFO_AddToGeoscape: ufotype does not exist\n");
		return NULL;
	}

	/* check max amount */
	if (ccs.numUFOs >= MAX_UFOONGEOSCAPE) {
		Com_Printf("UFO_AddToGeoscape: Too many UFOs on geoscape\n");
		return NULL;
	}

	for (newUFONum = 0; newUFONum < ccs.numAircraftTemplates; newUFONum++)
		if (ccs.aircraftTemplates[newUFONum].type == AIRCRAFT_UFO
		 && ufoType == ccs.aircraftTemplates[newUFONum].ufotype
		 && !ccs.aircraftTemplates[newUFONum].notOnGeoscape)
			break;

	if (newUFONum == ccs.numAircraftTemplates) {
		Com_DPrintf(DEBUG_CLIENT, "Could not add ufo type %i to geoscape\n", ufoType);
		return NULL;
	}

	/* Create ufo */
	ufo = &ccs.ufos[ccs.numUFOs];
	*ufo = ccs.aircraftTemplates[newUFONum];
	Com_DPrintf(DEBUG_CLIENT, "New UFO on geoscape: '%s' (ccs.numUFOs: %i, newUFONum: %i)\n", ufo->id, ccs.numUFOs, newUFONum);
	ufo->idx = ccs.numUFOs++;

	/* Update Stats of UFO */
	AII_UpdateAircraftStats(ufo);
	/* Give it HP */
	ufo->damage = ufo->stats[AIR_STATS_DAMAGE];
	/* Check for 0 damage which cause invulerable UFOs */
	assert(ufo->damage);

	/* Every ufo on geoscape needs a mission assigned */
	assert(mission);

	/* Initialise ufo data */
	UFO_SetRandomPos(ufo);
	AII_ReloadWeapon(ufo);					/* Load its weapons */
	ufo->landed = qfalse;
	ufo->detected = qfalse;					/* Not visible in radars (just for now) */
	ufo->mission = mission;
	if (destination)
		UFO_SendToDestination(ufo, destination);
	else
		UFO_SetRandomDest(ufo);				/* Random destination */

	return ufo;
}

/**
 * @brief Remove the specified ufo from geoscape
 * @sa CP_MissionRemove
 * @note Keep in mind that you have to update the ufo pointers after you called
 * this function
 */
void UFO_RemoveFromGeoscape (aircraft_t* ufo)
{
	/* Remove ufo from ufos list */
	const ptrdiff_t num = (ptrdiff_t)(ufo - ccs.ufos);

	Com_DPrintf(DEBUG_CLIENT, "Remove ufo from geoscape: '%s'\n", ufo->id);

	REMOVE_ELEM_ADJUST_IDX(ccs.ufos, num, ccs.numUFOs);
}

#ifdef DEBUG
/**
 * @brief Remove a UFO from the geoscape
  */
static void UFO_RemoveFromGeoscape_f (void)
{
	if (ccs.numUFOs > 0)
		UFO_RemoveFromGeoscape(ccs.ufos);
}
#endif

/**
 * @brief Perform actions when a new UFO is detected.
 * @param[in] ufocraft Pointer to the UFO that has just been detected.
 */
void UFO_DetectNewUFO (aircraft_t *ufocraft)
{
	/* Make this UFO detected */
	ufocraft->detected = qtrue;
	if (!ufocraft->detectionIdx) {
		ufocraft->detectionIdx = ++ccs.campaignStats.ufosDetected;
	}
	ufocraft->lastSpotted = ccs.date;

	/* If this is the first UFO on geoscape, activate radar */
	if (!(r_geoscape_overlay->integer & OVERLAY_RADAR))
		MAP_SetOverlay("radar");
}

/**
 * @brief Check events for UFOs: Appears or disappears on radars
 * @return qtrue if any new ufo was detected during this iteration, qfalse otherwise
 */
qboolean UFO_CampaignCheckEvents (void)
{
	qboolean detected, newDetection;
	int baseIdx, installationIdx;
	aircraft_t *ufo, *aircraft;

	newDetection = qfalse;

	/* For each ufo in geoscape */
	for (ufo = ccs.ufos + ccs.numUFOs - 1; ufo >= ccs.ufos; ufo--) {
		/* don't update UFO status id UFO is landed or crashed */
		if (ufo->landed)
			continue;

		/* detected tells us whether or not a UFO is detected NOW, whereas ufo->detected tells us whether or not the UFO was detected PREVIOUSLY. */
		detected = qfalse;

		for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
			base_t *base = B_GetFoundedBaseByIDX(baseIdx);
			if (!base)
				continue;
			if (!B_GetBuildingStatus(base, B_POWER))
				continue;

			/* maybe the ufo is already detected, don't reset it */
			detected |= RADAR_CheckUFOSensored(&base->radar, base->pos, ufo, detected | ufo->detected);

			/* Check if UFO is detected by an aircraft */
			for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--) {
				if (!AIR_IsAircraftOnGeoscape(aircraft))
					continue;
				/* maybe the ufo is already detected, don't reset it */
				detected |= RADAR_CheckUFOSensored(&aircraft->radar, aircraft->pos, ufo, detected | ufo->detected);
			}
		}

		for (installationIdx = 0; installationIdx < MAX_INSTALLATIONS; installationIdx++) {
			installation_t *installation = INS_GetFoundedInstallationByIDX(installationIdx);
			if (!installation)
				continue;

			/* maybe the ufo is already detected, don't reset it */
			detected |= RADAR_CheckUFOSensored(&installation->radar, installation->pos, ufo, detected | ufo->detected);
		}

		/* Check if ufo appears or disappears on radar */
		if (detected != ufo->detected) {
			if (detected) {
				/* if UFO is aiming a PHALANX aircraft, warn player */
				if (ufo->aircraftTarget) {
					/* stop time and notify */
					MSO_CheckAddNewMessage(NT_UFO_ATTACKING, _("Notice"), va(_("A UFO is flying toward %s"), _(ufo->aircraftTarget->name)), qfalse, MSG_STANDARD, NULL);
					/** @todo present a popup with possible orders like: return to base, attack the ufo, try to flee the rockets
					 * @sa UFO_SearchAircraftTarget */
				} else
					MSO_CheckAddNewMessage(NT_UFO_SPOTTED, _("Notice"), _("Our radar detected a new UFO"), qfalse, MSG_UFOSPOTTED, NULL);
				newDetection = qtrue;
				UFO_DetectNewUFO(ufo);
			} else if (!detected) {
				MSO_CheckAddNewMessage(NT_UFO_SIGNAL_LOST, _("Notice"), _("Our radar has lost the tracking on a UFO"), qfalse, MSG_UFOSPOTTED, NULL);
				/* Make this UFO undetected */
				ufo->detected = qfalse;
				/* Notify that ufo disappeared */
				AIR_AircraftsUFODisappear(ufo);
				MAP_NotifyUFODisappear(ufo);

				/* Deactivate Radar overlay */
				RADAR_DeactivateRadarOverlay();
			}
		}
	}
	return newDetection;
}

/**
 * @brief Notify to UFOs that a Phalanx aircraft has been destroyed.
 * @param[in] aircraft Pointer to the Phalanx aircraft that has been removed.
 */
void UFO_NotifyPhalanxAircraftRemoved (const aircraft_t *const aircraft)
{
	int ufoIdx;

	assert(aircraft);

	for (ufoIdx = 0; ufoIdx < ccs.numUFOs; ufoIdx++) {
		aircraft_t *ufo = &ccs.ufos[ufoIdx];

		if (ufo->aircraftTarget == aircraft)
			ufo->aircraftTarget = NULL;
	}
}

/**
 * @brief Check if an aircraft should be seen on geoscape.
 * @return true or false wether UFO should be seen or not on geoscape.
 */
qboolean UFO_IsUFOSeenOnGeoscape (const aircraft_t const *ufo)
{
#ifdef DEBUG
	if (ufo->notOnGeoscape)
		Com_Error(ERR_DROP, "UFO_IsUFOSeenOnGeoscape: ufo %s can't be used on geoscape", ufo->id);
#endif
	return (!ufo->landed && ufo->detected);
}

/**
 * @sa MN_InitStartup
 */
void UFO_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_destroyufos", UFO_DestroyUFOs_f, "Destroys all UFOs on the geoscape");
	Cmd_AddCommand("debug_listufo", UFO_ListOnGeoscape_f, "Print UFO information to game console");
	Cmd_AddCommand("removeufo", UFO_RemoveFromGeoscape_f, "Remove a UFO from geoscape");
	Cvar_Get("debug_showufos", "0", CVAR_DEVELOPER, "Show all UFOs on geoscape");
#endif
}
