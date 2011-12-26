/**
 * @file cp_ufo.c
 * @brief UFOs on geoscape
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
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_ufo.h"
#include "cp_aircraft.h"
#include "cp_mapfightequip.h"

static const float MAX_DETECTING_RANGE = 25.0f; /**< range to detect and fire at phalanx aircraft */


/**
 * @brief Iterates through the UFOs
 * @param[in] lastUFO Pointer of the aircraft to iterate from. call with NULL to get the first one.
 */
aircraft_t* UFO_GetNext (aircraft_t *lastUFO)
{
	aircraft_t* endOfUFOs = &ccs.ufos[ccs.numUFOs];
	aircraft_t* ufo;

	if (!ccs.numUFOs)
		return NULL;

	if (!lastUFO)
		return ccs.ufos;
	assert(lastUFO >= ccs.ufos);
	assert(lastUFO < endOfUFOs);

	ufo = lastUFO;

	ufo++;
	if (ufo >= endOfUFOs)
		return NULL;
	else
		return ufo;
}

/**
 * @sa AIR_IsAircraftOnGeoscape
 */
aircraft_t *UFO_GetNextOnGeoscape (aircraft_t *lastUFO)
{
	aircraft_t* ufo = lastUFO;
	while ((ufo = UFO_GetNext(ufo)) != NULL) {
		if (UFO_IsUFOSeenOnGeoscape(ufo)
#ifdef DEBUG
		|| Cvar_GetInteger("debug_showufos")
#endif
		)
			return ufo;
	}

	return NULL;
}

/**
 * @brief returns the UFO on the geoscape with a certain index
 * @param[in] idx Index of the UFO
 */
aircraft_t* UFO_GetByIDX (const int idx)
{
	assert(idx >= 0 && idx < MAX_UFOONGEOSCAPE);
	return &ccs.ufos[idx];
}

/**
 * @brief Get the technology for a given UFO type
 * @param type UFO type to get the technology for
 * @return The technology for the given UFO. If no technology was found for the UFO
 * id this might return @c NULL.
 */
const technology_t* UFO_GetTechnologyFromType (const ufoType_t type)
{
	const char *id = Com_UFOTypeToShortName(type);
	const technology_t *tech = RS_GetTechByProvided(id);
	return tech;
}

/**
 * @brief Get the aircraft template for a given UFO type
 * @param type The UFO type to get the template for
 * @return The aircraft template of the UFO - always returns a value
 */
const aircraft_t* UFO_GetByType (const ufoType_t type)
{
	int i;

	for (i = 0; i < ccs.numAircraftTemplates; i++) {
		aircraft_t *ufo = &ccs.aircraftTemplates[i];
		if (ufo->ufotype == type)
			return ufo;
	}

	Com_Error(ERR_DROP, "No ufo with type %i found", type);
}

/**
 * @brief Some UFOs may only appear if after some interest level in the current running campaign is reached
 * @param type The UFO type to check the interest level for
 * @return @c true if the UFO may appear on geoscape, @c false otherwise
 */
qboolean UFO_ShouldAppearOnGeoscape (const ufoType_t type)
{
	const aircraft_t *ufo = UFO_GetByType(type);

	return ufo->ufoInterestOnGeoscape <= ccs.overallInterest;
}

/**
 * @brief Translate UFO type to name.
 * @param[in] type UFO type in ufoType_t.
 * @return Translated UFO name.
 * @sa Com_UFOTypeToShortName
 * @sa Com_UFOShortNameToID
 */
const char* UFO_TypeToName (ufoType_t type)
{
	const technology_t *tech = UFO_GetTechnologyFromType(type);
	if (tech)
		return _(tech->name);
	Com_Error(ERR_DROP, "UFO_TypeToName(): Unknown UFO type %i\n", type);
}
/**
 * @brief Returns names of the UFO is UFO has been researched.
 * @param[in] ufocraft Pointer to the UFO.
 */
const char* UFO_AircraftToIDOnGeoscape (const aircraft_t *ufocraft)
{
	const technology_t *tech = ufocraft->tech;

	assert(tech);

	if (ufocraft->detectionIdx)
		return va("%s #%i", (RS_IsResearched_ptr(tech)) ? _(ufocraft->name) : _("UFO"), ufocraft->detectionIdx);
	return (RS_IsResearched_ptr(tech)) ? _(ufocraft->name) : _("UFO");
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
void UFO_SetRandomDestAround (aircraft_t* ufocraft, const vec2_t pos)
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
	const float decreasingDistance = 10.0f; /**< above this distance, probability to detect base will
	 decrease by @c decreasingFactor */
	const float decreasingFactor = 5.0f;

	/* ufo can't find base if it's too far */
	distance = GetDistanceOnGlobe(ufo->pos, base->pos);
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
	const float decreasingDistance = 10.0f; /**< above this distance, probability to detect base will
	 decrease by @c decreasingFactor */
	const float decreasingFactor = 5.0f;

	/* ufo can't find base if it's too far */
	distance = GetDistanceOnGlobe(ufo->pos, installation->pos);
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
 * @sa CP_CampaignRun
 */
void UFO_UpdateAlienInterestForAllBasesAndInstallations (void)
{
	aircraft_t *ufo;

	ufo = NULL;
	while ((ufo = UFO_GetNext(ufo)) != NULL) {
		base_t *base;

		/* landed UFO can't detect any phalanx base or installation */
		if (ufo->landed)
			continue;

		base = NULL;
		while ((base = B_GetNext(base)) != NULL)
			UFO_UpdateAlienInterestForOneBase(ufo, base);

		INS_Foreach(installation)
			UFO_UpdateAlienInterestForOneInstallation(ufo, installation);
	}
}

/**
 * @brief Check if the ufo can shoot at a PHALANX aircraft
 */
static void UFO_SearchAircraftTarget (const campaign_t* campaign, aircraft_t *ufo)
{
	float distance = 999999.;

	/* UFO never try to attack a PHALANX aircraft except if they came on earth in that aim */
	if (ufo->mission->stage != STAGE_INTERCEPT) {
		/* Check if UFO is defending itself */
		if (ufo->aircraftTarget)
			UFO_CheckShootBack(campaign, ufo, ufo->aircraftTarget);
		return;
	}

	/* check if the ufo is already attacking an aircraft */
	if (ufo->aircraftTarget) {
		/* check if the target disappeared from geoscape (fled in a base) */
		if (AIR_IsAircraftOnGeoscape(ufo->aircraftTarget))
			AIRFIGHT_ExecuteActions(campaign, ufo, ufo->aircraftTarget);
		else
			ufo->aircraftTarget = NULL;
		return;
	}

	ufo->status = AIR_TRANSIT;
	AIR_Foreach(phalanxAircraft) {
		/* check that aircraft is flying */
		if (AIR_IsAircraftOnGeoscape(phalanxAircraft)) {
			/* get the distance from ufo to aircraft */
			const float dist = GetDistanceOnGlobe(ufo->pos, phalanxAircraft->pos);
			/* check out of reach */
			if (dist > MAX_DETECTING_RANGE)
				continue;
			/* choose the nearest target */
			if (dist < distance) {
				distance = dist;
				if (UFO_SendPursuingAircraft(ufo, phalanxAircraft) && UFO_IsUFOSeenOnGeoscape(ufo)) {
					/* stop time and notify */
					MSO_CheckAddNewMessage(NT_UFO_ATTACKING, _("Notice"), va(_("A UFO is flying toward %s"), phalanxAircraft->name), qfalse,
						MSG_STANDARD, NULL);
					/** @todo present a popup with possible orders like: return to base, attack the ufo, try to flee the rockets */
				}
			}
		}
	}
}

/**
 * @brief Make the specified UFO pursue a phalanx aircraft.
 * @param[in,out] ufo Pointer to the UFO.
 * @param[in,out] aircraft Pointer to the target aircraft.
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

	return qtrue;
}

/**
 * @brief Make the specified UFO go to destination.
 * @param[in,out] ufo Pointer to the UFO.
 * @param[in] dest Destination.
 * @sa UFO_SendAttackBase
 */
void UFO_SendToDestination (aircraft_t* ufo, const vec2_t dest)
{
	assert(ufo);

	MAP_MapCalcLine(ufo->pos, dest, &ufo->route);
	ufo->status = AIR_TRANSIT;
	ufo->time = 0;
	ufo->point = 0;
}

/**
 * @brief Check if the ufo can shoot back at phalanx aircraft
 * @param[in] campaign The campaign data structure
 * @param[in,out] ufo The ufo to check the shotting for
 * @param[in,out] phalanxAircraft The possible target
 */
void UFO_CheckShootBack (const campaign_t* campaign, aircraft_t *ufo, aircraft_t* phalanxAircraft)
{
	/* check if the ufo is already attacking an aircraft */
	if (ufo->aircraftTarget) {
		/* check if the target flee in a base */
		if (AIR_IsAircraftOnGeoscape(ufo->aircraftTarget))
			AIRFIGHT_ExecuteActions(campaign, ufo, ufo->aircraftTarget);
		else {
			ufo->aircraftTarget = NULL;
			CP_UFOProceedMission(campaign, ufo);
		}
	} else {
		/* check that aircraft is flying */
		if (AIR_IsAircraftOnGeoscape(phalanxAircraft))
			UFO_SendPursuingAircraft(ufo, phalanxAircraft);
	}
}

/**
 * @brief Make the UFOs run
 * @param[in] campaign The campaign data structure
 * @param[in] deltaTime The time passed since last call
 */
void UFO_CampaignRunUFOs (const campaign_t* campaign, int deltaTime)
{
	int ufoIdx, k;

	/* now the ufos are flying around, too - cycle backward - ufo might be destroyed */
	for (ufoIdx = ccs.numUFOs - 1; ufoIdx >= 0; ufoIdx--) {
		aircraft_t *ufo = UFO_GetByIDX(ufoIdx);
		/* don't run a landed ufo */
		if (ufo->landed)
			continue;

		/* Every UFO on geoscape should have a mission assigned */
		assert(ufo->mission);

		/* reached target and not following a phalanx aircraft? then we need a new destination */
		if (AIR_AircraftMakeMove(deltaTime, ufo) && ufo->status != AIR_UFO) {
			float *end;
			end = ufo->route.point[ufo->route.numPoints - 1];
			Vector2Copy(end, ufo->pos);
			MAP_CheckPositionBoundaries(ufo->pos);
			if (ufo->mission->stage == STAGE_INTERCEPT && ufo->mission->data.aircraft) {
				/* Attacking an installation: fly over this installation */
				UFO_SetRandomDestAround(ufo, ufo->mission->pos);
			} else
				UFO_SetRandomDest(ufo);
			if (CP_CheckNextStageDestination(campaign, ufo))
				/* UFO has been removed from game */
				continue;
			/* UFO was destroyed (maybe because the mission was removed) */
			if (ufoIdx == ccs.numUFOs)
				continue;
		}

		/* is there a PHALANX aircraft to shoot at ? */
		UFO_SearchAircraftTarget(campaign, ufo);

		/* antimatter tanks */
		if (ufo->fuel <= 0)
			ufo->fuel = ufo->stats[AIR_STATS_FUELSIZE];

		/* Update delay to launch next projectile */
		for (k = 0; k < ufo->maxWeapons; k++) {
			aircraftSlot_t *slot = &ufo->weapons[k];
			if (slot->delayNextShot > 0)
				slot->delayNextShot -= deltaTime;
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
	campaign_t* campaign = ccs.curCampaign;

	for (ufo = ccs.ufos; ufo < ccs.ufos + ccs.numUFOs; ufo++) {
		AIRFIGHT_ActionsAfterAirfight(campaign, NULL, ufo, qtrue);
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
 * @brief Get the template data for the given ufo type
 * @param ufoType The ufo type to search the template for.
 * @note This function will only return those templates that may appear on the geoscape!
 * @return @c NULL in case the ufoType wasn't found, or the pointer to the ufo template.
 */
static const aircraft_t* UFO_GetTemplateForGeoscape (ufoType_t ufoType)
{
	int newUFONum;

	for (newUFONum = 0; newUFONum < ccs.numAircraftTemplates; newUFONum++) {
		const aircraft_t *tpl = &ccs.aircraftTemplates[newUFONum];
		if (tpl->type == AIRCRAFT_UFO && ufoType == tpl->ufotype && !tpl->notOnGeoscape)
			break;
	}

	/* not found */
	if (newUFONum == ccs.numAircraftTemplates)
		return NULL;

	return &ccs.aircraftTemplates[newUFONum];
}

/**
 * @brief Creates a new ufo on the geoscape from the given aircraft template
 * @param ufoTemplate The aircraft template to create the ufo from.
 * @return @c NULL if the max allowed amount of ufos are already on the geoscape, otherwise
 * the newly created ufo pointer
 */
static aircraft_t* UFO_CreateFromTemplate (const aircraft_t *ufoTemplate)
{
	aircraft_t *ufo;

	/* check max amount */
	if (ccs.numUFOs >= MAX_UFOONGEOSCAPE)
		return NULL;

	/* must be an ufo */
	assert(ufoTemplate->type == AIRCRAFT_UFO);
	/* must be allowed to be on the geoscape */
	assert(!ufoTemplate->notOnGeoscape);

	/* get a new free slot */
	ufo = UFO_GetByIDX(ccs.numUFOs);
	/* copy the data */
	*ufo = *ufoTemplate;
	/* assign an unique index */
	ufo->idx = ccs.numUFOs++;

	return ufo;
}

/**
 * @brief Add a UFO to geoscape
 * @param[in] ufoType The type of ufo (fighter, scout, ...).
 * @param[in] destination Position where the ufo should go. NULL is randomly chosen
 * @param[in] mission Pointer to the mission the UFO is involved in
 * @sa UFO_RemoveFromGeoscape
 * @sa UFO_RemoveFromGeoscape_f
 */
aircraft_t *UFO_AddToGeoscape (ufoType_t ufoType, const vec2_t destination, mission_t *mission)
{
	aircraft_t *ufo;
	const aircraft_t *ufoTemplate;

	ufoTemplate = UFO_GetTemplateForGeoscape(ufoType);
	if (ufoTemplate == NULL)
		return NULL;

	/* Create ufo */
	ufo = UFO_CreateFromTemplate(ufoTemplate);
	if (ufo == NULL)
		return NULL;

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
	AII_ReloadAircraftWeapons(ufo); /* Load its weapons */
	ufo->landed = qfalse;
	ufo->detected = qfalse; /* Not visible in radars (just for now) */
	ufo->mission = mission;
	if (destination)
		UFO_SendToDestination(ufo, destination);
	else
		UFO_SetRandomDest(ufo); /* Random destination */

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
	const ptrdiff_t num = (ptrdiff_t) (ufo - ccs.ufos);

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
	if (ufocraft->detected)
		return;

	/* Make this UFO detected */
	ufocraft->detected = qtrue;
	if (!ufocraft->detectionIdx) {
		ufocraft->detectionIdx = ++ccs.campaignStats.ufosDetected;
	}
	ufocraft->lastSpotted = ccs.date;

	/* If this is the first UFO on geoscape, activate radar */
	if (!MAP_IsRadarOverlayActivated())
		MAP_SetOverlay("radar");

	MAP_UpdateGeoscapeDock();
}

/**
 * @brief Check events for UFOs: Appears or disappears on radars
 * @return qtrue if any new ufo was detected during this iteration, qfalse otherwise
 */
qboolean UFO_CampaignCheckEvents (void)
{
	qboolean newDetection;
	aircraft_t *ufo;

	newDetection = qfalse;

	/* For each ufo in geoscape */
	ufo = NULL;
	while ((ufo = UFO_GetNext(ufo)) != NULL) {
		char detectedBy[MAX_VAR] = "";
		float minDistance = -1;
		/* detected tells us whether or not a UFO is detected NOW, whereas ufo->detected tells
		 * us whether or not the UFO was detected PREVIOUSLY. */
		qboolean detected = qfalse;
		base_t *base;

		/* don't update UFO status id UFO is landed or crashed */
		if (ufo->landed)
			continue;

		/* note: We can't exit these loops as soon as we found the UFO detected
		 * RADAR_CheckUFOSensored registers the UFO in every radars' detection list
		 * which detect it */

		/* Check if UFO is detected by an aircraft */
		AIR_Foreach(aircraft) {
			if (!AIR_IsAircraftOnGeoscape(aircraft))
				continue;
			/* maybe the ufo is already detected, don't reset it */
			if (RADAR_CheckUFOSensored(&aircraft->radar, aircraft->pos, ufo, detected | ufo->detected)) {
				const int distance = GetDistanceOnGlobe(aircraft->pos, ufo->pos);
				detected = qtrue;
				if (minDistance < 0 || minDistance > distance) {
					minDistance = distance;
					Q_strncpyz(detectedBy, aircraft->name, sizeof(detectedBy));
				}
			}
		}

		/* Check if UFO is detected by a base */
		base = NULL;
		while ((base = B_GetNext(base)) != NULL) {
			if (!B_GetBuildingStatus(base, B_POWER))
				continue;

			/* maybe the ufo is already detected, don't reset it */
			if (RADAR_CheckUFOSensored(&base->radar, base->pos, ufo, detected | ufo->detected)) {
				const int distance = GetDistanceOnGlobe(base->pos, ufo->pos);
				detected = qtrue;
				if (minDistance < 0 || minDistance > distance) {
					minDistance = distance;
					Q_strncpyz(detectedBy, base->name, sizeof(detectedBy));
				}
			}

		}

		/* Check if UFO is detected by a radartower */
		INS_Foreach(installation) {
			/* maybe the ufo is already detected, don't reset it */
			if (RADAR_CheckUFOSensored(&installation->radar, installation->pos, ufo, detected | ufo->detected)) {
				const int distance = GetDistanceOnGlobe(installation->pos, ufo->pos);
				detected = qtrue;
				if (minDistance < 0 || minDistance > distance) {
					minDistance = distance;
					Q_strncpyz(detectedBy, installation->name, sizeof(detectedBy));
				}
			}
		}

		/* Check if ufo appears or disappears on radar */
		if (detected != ufo->detected) {
			if (detected) {
				/* if UFO is aiming a PHALANX aircraft, warn player */
				if (ufo->aircraftTarget) {
					/* stop time and notify */
					MSO_CheckAddNewMessage(NT_UFO_ATTACKING, _("Notice"), va(_("A UFO is flying toward %s"), ufo->aircraftTarget->name), qfalse,
							MSG_STANDARD, NULL);
					/** @todo present a popup with possible orders like: return to base, attack the ufo, try to flee the rockets
					 * @sa UFO_SearchAircraftTarget */
				} else {
					MSO_CheckAddNewMessage(NT_UFO_SPOTTED, _("Notice"), va(_("Our radar detected a new UFO near %s"), detectedBy), qfalse,
							MSG_UFOSPOTTED, NULL);
				}
				newDetection = qtrue;
				UFO_DetectNewUFO(ufo);
			} else if (!detected) {
				MSO_CheckAddNewMessage(NT_UFO_SIGNAL_LOST, _("Notice"), _("Our radar has lost the tracking on a UFO"), qfalse, MSG_UFOLOST, NULL);
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
void UFO_NotifyPhalanxAircraftRemoved (const aircraft_t * const aircraft)
{
	int ufoIdx;

	assert(aircraft);

	for (ufoIdx = 0; ufoIdx < ccs.numUFOs; ufoIdx++) {
		aircraft_t *ufo = UFO_GetByIDX(ufoIdx);

		if (ufo->aircraftTarget == aircraft)
			ufo->aircraftTarget = NULL;
	}
}

/**
 * @brief Check if an aircraft should be seen on geoscape.
 * @return true or false wether UFO should be seen or not on geoscape.
 * @sa AIR_IsAircraftOnGeoscape
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
 * @sa UI_InitStartup
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
