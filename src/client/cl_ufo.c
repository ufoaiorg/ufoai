/**
 * @file cl_ufo.c
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

#include "client.h"
#include "cl_global.h"

static void UFO_SetUfoRandomDest(aircraft_t* ufo);
extern void UFO_CampaignRunUfos(int dt);
static void UFO_RemoveUfoFromGeoscape_f(void);
extern void UFO_CampaignCheckEvents(void);
extern void UFO_Reset(void);

static const float max_detecting_range = 60.0f; /**< range to detect and fire at phalanx aircraft */

typedef struct ufoTypeList_s {
	const char *id;		/**< script id string */
	const char *name;	/**< these values are already taken from script file
						 * so we have to use gettext markers here */
	int ufoType;		/**< ufoType_t values */
} ufoTypeList_t;

/**
 * @brief Valid ufo types
 * @note Use the same values for the names as we are already using in the scriptfiles
 * here, otherwise they are not translateable because they don't appear in the po files
 * @note Every ufotype (id) that doesn't have nogeoscape set to true must have an assembly
 * in the ufocrash[dn].ump files
 */
static const ufoTypeList_t ufoTypeList[] = {
	{"ufo_scout", "UFO - Scout", UFO_SCOUT},
	{"ufo_fighter", "UFO - Fighter", UFO_FIGHTER},
	{"ufo_harvester", "UFO - Harvester", UFO_HARVESTER},
	{"ufo_condor", "UFO - Condor", UFO_CONDOR},
	{"ufo_carrier", "UFO - Carrier", UFO_CARRIER},
	{"ufo_supply", "UFO - Supply", UFO_SUPPLY},

	{NULL, NULL, 0}
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
		if (!Q_strcmp(list->id, token))
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
	Sys_Error("UFO_TypeToShortName(): Unknown UFO type %i\n", type);
	return NULL; /* never reached */
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
	const ufoTypeList_t *list = ufoTypeList;

	while (list->id) {
		if (list->ufoType == type)
			return _(list->name);
		list++;
	}
	Sys_Error("UFO_TypeToName(): Unknown UFO type %i\n", type);
	return NULL; /* never reached */
}

/**
 * @brief Give a random destination to the given UFO, and make him to move there
 * @todo Sometimes the ufos aren't changing the routes - CP_GetRandomPosForAircraft
 * returns correct values, but it seams, that MAP_MapCalcLine is not doing the correct
 * things - set debug_showufos to 1
 */
static void UFO_SetUfoRandomDest (aircraft_t* ufo)
{
	vec2_t pos;

	ufo->status = AIR_TRANSIT;
	ufo->time = 0;
	ufo->point = 0;
	CP_GetRandomPosForAircraft(pos);
	MAP_MapCalcLine(ufo->pos, pos, &(ufo->route));
}

/**
 * @brief Set new route to UFO so that it can flee
 * @todo Make aircraft flee in the direction opposite of v rather than choosing a random direction
 */
void UFO_FleePhalanxAircraft (aircraft_t *ufo, const vec2_t v)
{
	vec2_t pos;
	vec3_t initialVector, rotationAxis, dest;

	PolarToVec(ufo->pos, initialVector);
	PolarToVec(v, dest);

	CrossProduct(initialVector, dest, rotationAxis);
	VectorNormalize(rotationAxis);
	RotatePointAroundVector(dest, rotationAxis, initialVector, -15.0f);

	VecToPolar(dest, pos);

	ufo->time = 0;
	ufo->point = 0;
	MAP_MapCalcLine(ufo->pos, pos, &(ufo->route));
	ufo->aircraftTarget = NULL;
	ufo->baseTarget = NULL;
	ufo->status = AIR_FLEEING;
}

#ifdef UFO_ATTACK_BASES
/**
 * @brief Check if a UFO is the target of a base
 * @param[in] ufoIdx idx of the ufo in gd.ufos[]
 * @param[in] base Pointer to the base
 * @return 0 if ufo is not a target, 1 if target of a missile, 2 if target of a laser
 */
static int UFO_IsTargetOfBase (int ufoIdx, base_t *base)
{
	int i;

	for (i = 0; i < base->maxBatteries; i++) {
		if (base->targetMissileIdx[i] == ufoIdx)
			return UFO_IS_TARGET_OF_MISSILE;
	}

	for (i = 0; i < base->maxLasers; i++) {
		if (base->targetLaserIdx[i] == ufoIdx)
			return UFO_IS_TARGET_OF_LASER;
	}

	return UFO_IS_NO_TARGET;
}

/**
 * @brief Check if a UFO found a new base
 * @param[in] ufo Pointer to the aircraft_t
 * @param[in] dt Elapsed time since last check
 */
static void UFO_FoundNewBase (aircraft_t *ufo, int dt)
{
	base_t* base;
	float baseProbability;
	float distance;

	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		/* if base has already been discovered, skip */
		if (base->isDiscovered == qtrue)
			continue;

		/* ufo can't find base if it's too far */
		distance = MAP_GetDistance(ufo->pos, base->pos);
		if (distance > max_detecting_range)
			continue;

		switch (UFO_IsTargetOfBase(ufo - gd.ufos, base)) {
		case UFO_IS_TARGET_OF_MISSILE:
			baseProbability = 0.001f;
			break;
		case UFO_IS_TARGET_OF_LASER:
			baseProbability = 0.0001f;
			break;
		default:
			baseProbability = 0.00001f;
			break;
		}

		/* decrease probability if the ufo is far from base */
		if (distance > 10.0f)
			baseProbability /= 5.0f;

		baseProbability *= dt;

		if (frand() < baseProbability) {
			base->isDiscovered = qtrue;
			Com_DPrintf(DEBUG_CLIENT, "Base '%s' has been discoverd by UFO\n", base->name);
		}
	}
}
#endif

/**
 * @brief Check if the ufo can shoot at something
 */
static void UFO_SearchTarget (aircraft_t *ufo)
{
	base_t* base;
	aircraft_t* phalanxAircraft;
	float distance = 999999., dist;

	if (ufo->status != AIR_FLEEING) {
		/* check if the ufo is already attacking a base */
		if (ufo->baseTarget) {
			AIRFIGHT_ExecuteActions(ufo, NULL);
		/* check if the ufo is already attacking an aircraft */
		} else if (ufo->aircraftTarget) {
			/* check if the target flee in a base */
			if (ufo->aircraftTarget->status > AIR_HOME)
				AIRFIGHT_ExecuteActions(ufo, ufo->aircraftTarget);
			else
				ufo->aircraftTarget = NULL;
		} else {
			ufo->status = AIR_TRANSIT;
			/* reverse order - because bases can be destroyed in here */
			for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
#ifdef UFO_ATTACK_BASES
				/* check if the ufo can attack a base (if it's not too far) */
				if (base->isDiscovered && (MAP_GetDistance(ufo->pos, base->pos) < max_detecting_range)) {
					if (AIR_SendUfoPurchasingBase(ufo, base))
						/* don't check for aircraft if we can destroy a base */
						continue;
				}
#endif

				/* check if the ufo can attack an aircraft
				 * reverse order - because aircraft can be destroyed in here */
				for (phalanxAircraft = base->aircraft + base->numAircraftInBase - 1; phalanxAircraft >= base->aircraft; phalanxAircraft--) {
					/* check that aircraft is flying */
					if (phalanxAircraft->status > AIR_HOME) {
						/* get the distance from ufo to aircraft */
						dist = MAP_GetDistance(ufo->pos, phalanxAircraft->pos);
						/* check out of reach */
						if (dist > max_detecting_range)
							continue;
						/* choose the nearest target */
						if (dist < distance) {
							distance = dist;
							AIR_SendUfoPurchasingAircraft(ufo, phalanxAircraft);
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Make the UFOs run
 * @param[in] dt time delta
 */
void UFO_CampaignRunUfos (int dt)
{
	aircraft_t*	ufo;
	int k;

	/* now the ufos are flying around, too - cycle backward - ufo might be destroyed */
	for (ufo = gd.ufos + gd.numUfos - 1; ufo >= gd.ufos; ufo--) {
#ifdef UFO_ATTACK_BASES
		/* Check if the UFO found a new base */
		UFO_FoundNewBase(ufo, dt);
#endif

		/* reached target and not following a phalanx aircraft? then we need a new target */
		if (AIR_AircraftMakeMove(dt, ufo) && ufo->status != AIR_UFO) {
			float *end;
			end = ufo->route.point[ufo->route.numPoints - 1];
			Vector2Copy(end, ufo->pos);
			UFO_SetUfoRandomDest(ufo);
		}

		/* is there a PHALANX aircraft to shoot ? */
		UFO_SearchTarget(ufo);

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

/**
 * @brief Check if a UFO has weapons and ammo to shoot
 * @param[in] ufo Pointer to the UFO
 * @param[in] base Pointer to the base shooting at the UFO
 */
qboolean UFO_UFOCanShoot (const aircraft_t *ufo)
{
	int i;

	for (i = 0; i < ufo->maxWeapons; i++) {
		if (ufo->weapons[i].itemIdx != NONE && ufo->weapons[i].ammoIdx != NONE  && ufo->weapons[i].ammoLeft > 0)
			return qtrue;
	}

	return qfalse;
}

#ifdef DEBUG
/**
 * @brief Write the ufo list, for debugging
 *
 * listufo
 * only for debugging
 */
static void UFO_ListUfosOnGeoscape_f (void)
{
	aircraft_t* ufo;
	int k;

	Com_Printf("There are %i ufos on geoscape\n", gd.numUfos);
	for (ufo = gd.ufos + gd.numUfos - 1; ufo >= gd.ufos; ufo--) {
		Com_Printf("..%s (%s) - status: %i - pos: %.0f:%0.f\n", ufo->name, ufo->id, ufo->status, ufo->pos[0], ufo->pos[1]);
		Com_Printf("...route length: %i (current: %i), time: %i, distance: %.2f, speed: %i\n",
			ufo->route.numPoints, ufo->point, ufo->time, ufo->route.distance, ufo->stats[AIR_STATS_SPEED]);
		Com_Printf("...%i weapon slots: ", ufo->maxWeapons);
		for (k = 0; k < ufo->maxWeapons; k++) {
			if (ufo->weapons[k].itemIdx > -1) {
				Com_Printf("%s", csi.ods[ufo->weapons[k].itemIdx].id);
				if (ufo->weapons[k].ammoIdx > -1 && ufo->weapons[k].ammoLeft > 0)
					Com_Printf(" (loaded)");
				else
					Com_Printf(" (unloaded)");
			}
			else
				Com_Printf("empty");
			Com_Printf(" / ");
		}
		Com_Printf("\n");
	}
}
#endif

/**
 * @brief Add a UFO to geoscape
 * @todo: UFOs are not assigned unique idx fields. Could be handy...
 * @sa UFO_RemoveUfoFromGeoscape
 * @sa UFO_RemoveUfoFromGeoscape_f
 */
static void UFO_NewUfoOnGeoscape_f (void)
{
	static int newUfoNum = -1;
	aircraft_t *ufo = NULL;
	ufoType_t ufotype = UFO_MAX;

	/* check max amount */
	if (gd.numUfos >= MAX_UFOONGEOSCAPE)
		return;

	if (Cmd_Argc() == 2) {
		ufotype = atoi(Cmd_Argv(1));
		if (ufotype > UFO_MAX || ufotype < 0)
			ufotype = UFO_MAX;
	}

	if (newUfoNum >= numAircraft_samples)
		newUfoNum = -1;

	/* Get next type of ufo in aircraft list */
	while (++newUfoNum < numAircraft_samples)
		if (aircraft_samples[newUfoNum].type == AIRCRAFT_UFO
		 && !aircraft_samples[newUfoNum].notOnGeoscape)
			break;
	if (newUfoNum == numAircraft_samples)
		for (newUfoNum = 0; newUfoNum < numAircraft_samples; newUfoNum++)
			if (aircraft_samples[newUfoNum].type == AIRCRAFT_UFO
			 && (ufotype == UFO_MAX || ufotype == aircraft_samples[newUfoNum].ufotype)
			 && !aircraft_samples[newUfoNum].notOnGeoscape)
				break;
	if (newUfoNum == numAircraft_samples) {
		if (ufotype != UFO_MAX)
			Com_DPrintf(DEBUG_CLIENT, "Could not add ufo type %i to geoscape\n", ufotype);
		return;
	}

	/* Create ufo */
	ufo = gd.ufos + gd.numUfos;
	memcpy(ufo, aircraft_samples + newUfoNum, sizeof(aircraft_t));
	Com_DPrintf(DEBUG_CLIENT, "New ufo on geoscape: '%s' (gd.numUfos: %i, newUfoNum: %i)\n", ufo->id, gd.numUfos, newUfoNum);
	gd.numUfos++;

	/* Initialise ufo data */
	AII_ReloadWeapon(ufo);					/* Load its weapons */
	ufo->visible = qfalse;					/* Not visible in radars (just for now) */
	CP_GetRandomPosForAircraft(ufo->pos);	/* Random position */
	UFO_SetUfoRandomDest(ufo);				/* Random destination */
}

/**
 * @brief Remove the specified ufo from geoscape
 * @sa UFO_NewUfoOnGeoscape_f
 */
void UFO_RemoveUfoFromGeoscape (aircraft_t* ufo)
{
	int num;
	base_t* base;
	aircraft_t* aircraft;

	/* Remove ufo from ufos list */
	num = ufo - gd.ufos;
	if (num < 0 || num >= gd.numUfos) {
		Com_DPrintf(DEBUG_CLIENT, "Cannot remove ufo: '%s'\n", ufo->name);
		return;
	}
	Com_DPrintf(DEBUG_CLIENT, "Remove ufo from geoscape: '%s'\n", ufo->name);
	memcpy(gd.ufos + num, gd.ufos + num + 1, (gd.numUfos - num - 1) * sizeof(aircraft_t));
	gd.numUfos--;

	/* Remove ufo from bases and aircraft radars */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		Radar_NotifyUfoRemoved(&(base->radar), ufo);

		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
			Radar_NotifyUfoRemoved(&(aircraft->radar), ufo);
	}

	/* Notications */
	CL_PopupNotifyUfoRemoved(ufo);
	AIR_AircraftsNotifyUfoRemoved(ufo);
	MAP_NotifyUfoRemoved(ufo);
}

/**
 * @brief Remove a UFO from the geoscape
  */
static void UFO_RemoveUfoFromGeoscape_f (void)
{
	if (gd.numUfos > 0)
		UFO_RemoveUfoFromGeoscape(gd.ufos);
}

/**
 * @brief Check events for ufos : appears or disappears on radars
 */
void UFO_CampaignCheckEvents (void)
{
	qboolean visible;
	aircraft_t *ufo, *aircraft;
	base_t* base;

	/* For each ufo in geoscape */
	for (ufo = gd.ufos + gd.numUfos - 1; ufo >= gd.ufos; ufo--) {
		visible = ufo->visible;
		ufo->visible = qfalse;

		/* Check for ufo detection by bases */
		for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
			if (!base->founded || !base->hasBuilding[B_POWER])
				continue;
			/* maybe the ufo is already visible, don't reset it */
			ufo->visible |= RADAR_CheckUfoSensored(&(base->radar), base->pos, ufo, visible);
		}

		/* Check for ufo tracking by aircraft */
		if (visible || ufo->visible)
			for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
				for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
					/* maybe the ufo is already visible, don't reset it */
					ufo->visible |= RADAR_CheckUfoSensored(&(aircraft->radar), aircraft->pos, ufo, qtrue);

		/* Check if ufo appears or disappears on radar */
		if (visible != ufo->visible) {
			if (ufo->visible) {
				MN_AddNewMessage(_("Notice"), _("Our radar detected a new UFO"), qfalse, MSG_STANDARD, NULL);
				CL_GameTimeStop();
			} else {
				MN_AddNewMessage(_("Notice"), _("Our radar has lost the tracking on a UFO"), qfalse, MSG_STANDARD, NULL);

				/* Notify that ufo disappeared */
				AIR_AircraftsUfoDisappear(ufo);
				CL_PopupNotifyUfoDisappeared(ufo);
				MAP_NotifyUfoDisappear(ufo);
			}
		}
	}
}

/**
 * @brief Prepares UFO recovery in global recoveries array.
 * @param[in] base Pointer to the base, where the UFO recovery will be made.
 * @sa UFO_Recovery
 * @sa UFO_ConditionsForStoring
 */
void UFO_PrepareRecovery (base_t *base)
{
	int i;
	ufoRecoveries_t *recovery = NULL;
	aircraft_t *ufocraft = NULL;
	date_t event;

	assert(base);

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == Cvar_VariableInteger("mission_ufotype"))
			break;
	}
	assert(ufocraft);

	/* Find free uforecovery slot. */
	for (i = 0; i < MAX_RECOVERIES; i++) {
		if (!gd.recoveries[i].active) {
			/* Make sure it is empty here. */
			memset(&gd.recoveries[i], 0, sizeof(gd.recoveries[i]));
			recovery = &gd.recoveries[i];
			break;
		}
	}

	if (!recovery) {
		Com_Printf("UFO_PrepareRecovery()... free recovery slot not found.\n");
		return;
	}
	assert(recovery);

	/* Prepare date of the recovery event - always two days after mission. */
	event = ccs.date;
	/* if you change these 2 days to something other you
	 * have to review all translations, too */
	event.day += 2;
	/* Prepare recovery. */
	recovery->active = qtrue;
	recovery->baseID = base->idx;
	recovery->ufotype = ufocraft->idx_sample;
	recovery->event = event;

	/* Update base capacity. */
	if (ufocraft->weight == AIRCRAFT_LARGE) {
		/* Large UFOs can only fit in large UFO hangars */
		base->capacities[CAP_UFOHANGARS_LARGE].cur++;
	} else {
		/* Small UFOs can fit in small and large UFO hangars: try to fill small hangar first */
		if (base->capacities[CAP_UFOHANGARS_SMALL].max > base->capacities[CAP_UFOHANGARS_SMALL].cur)
			base->capacities[CAP_UFOHANGARS_SMALL].cur++;
		else if (base->capacities[CAP_UFOHANGARS_LARGE].max > base->capacities[CAP_UFOHANGARS_LARGE].cur)
			/* there's no more room in a small hangar: fill large hangar */
			base->capacities[CAP_UFOHANGARS_LARGE].cur++;
		else
			/* no more room -- this shouldn't happen as we've used UFO_ConditionsForStoring */
			Com_Printf("UFO_PrepareRecovery: No room in UFO hangars to store %s\n", ufocraft->name);
	}

	Com_DPrintf(DEBUG_CLIENT, "UFO_PrepareRecovery()... the recovery entry in global array is done; base: %s, ufotype: %i, date: %i\n",
		base->name, recovery->ufotype, recovery->event.day);

	/* Send an email */
	CP_UFOSendMail(ufocraft, base);
}

/**
 * @brief Function to process active recoveries.
 * @sa CL_CampaignRun
 * @sa UFO_PrepareRecovery
 */
void UFO_Recovery (void)
{
	int i, item;
	objDef_t *od;
	base_t *base;
	aircraft_t *ufocraft;
	ufoRecoveries_t *recovery;

	for (i = 0; i < MAX_RECOVERIES; i++) {
		recovery = &gd.recoveries[i];
		if (recovery->active && recovery->event.day == ccs.date.day) {
			base = &gd.bases[recovery->baseID];
			if (!base->founded) {
				/* Destination base was destroyed meanwhile. */
				/* UFO is lost, send proper message to the user.*/
				recovery->active = qfalse;
				/* @todo: prepare MN_AddNewMessage() here */
				return;
			}
			/* Get ufocraft. */
			ufocraft = &aircraft_samples[recovery->ufotype];
			assert(ufocraft);
			/* Get item. */
			/* We can do this because aircraft id is the same as dummy item id. */
			item = INVSH_GetItemByID(ufocraft->id);
			assert(item != NONE);
			od = &csi.ods[item];
			assert(od);
			/* Process UFO recovery. */
			/* don't call B_UpdateStorageAndCapacity here - it's a dummy item */
			base->storage.num[item]++;	/* Add dummy UFO item to enable research topic. */
			RS_MarkCollected(od->tech);	/* Enable research topic. */
			/* Reset this recovery. */
			memset(&gd.recoveries[i], 0, sizeof(gd.recoveries[i]));
		}
	}
}

/**
 * @brief Checks conditions for storing given ufo in given base.
 * @param[in] base Pointer to the base, where we are going to store UFO.
 * @param[in] ufocraft Pointer to ufocraft which we are going to store.
 * @return qtrue if given base can store given ufo.
 */
qboolean UFO_ConditionsForStoring (const base_t *base, const aircraft_t *ufocraft)
{
	assert(base && ufocraft);

	if (ufocraft->weight == AIRCRAFT_LARGE) {
		/* Large UFOs can only fit large UFO hangars */
		if (!base->hasBuilding[B_UFO_HANGAR])
			return qfalse;

		/* Check there is still enough room for this UFO */
		if (base->capacities[CAP_UFOHANGARS_LARGE].max <= base->capacities[CAP_UFOHANGARS_LARGE].cur)
			return qfalse;
	} else {
		/* This is a small UFO */

		/* There's no hangar functional */
		if (!base->hasBuilding[B_UFO_SMALL_HANGAR] && !base->hasBuilding[B_UFO_HANGAR])
			return qfalse;

		/* there's only a large hangar, but it's full */
		if (!base->hasBuilding[B_UFO_SMALL_HANGAR]
			&& base->capacities[CAP_UFOHANGARS_LARGE].max <= base->capacities[CAP_UFOHANGARS_LARGE].cur)
			return qfalse;

		/* there's only a small hangar, but it's full */
		if (!base->hasBuilding[B_UFO_HANGAR]
			&& base->capacities[CAP_UFOHANGARS_SMALL].max <= base->capacities[CAP_UFOHANGARS_SMALL].cur)
			return qfalse;

		/* there are both hangar, both full */
		if (base->capacities[CAP_UFOHANGARS_LARGE].max <= base->capacities[CAP_UFOHANGARS_LARGE].cur
			&& base->capacities[CAP_UFOHANGARS_SMALL].max <= base->capacities[CAP_UFOHANGARS_SMALL].cur)
			return qfalse;
	}

	return qtrue;
}

#ifdef DEBUG
/**
 * @brief This function will destroy all ufos on the geoscape and
 * spawn the crash site missions when the ufo was "shot" over land
 * @note Give a parameter (a number) to spawn new ufos and crash
 * them afterwards
 */
static void UFO_DestroyAllUFOsOnGeoscape_f (void)
{
	int i, cnt;

	/* add new ufos to destroy */
	if (Cmd_Argc() == 2) {
		cnt = atoi(Cmd_Argv(1));
		Cmd_BufClear();
		for (i = 0; i < cnt; i++)
			UFO_NewUfoOnGeoscape_f();
	}

	for (i = 0; i < gd.numUfos; i++)
		AIRFIGHT_ActionsAfterAirfight(NULL, &gd.ufos[i], qtrue);
}
#endif

/**
 * @sa MN_ResetMenus
 */
void UFO_Reset (void)
{
	Cmd_AddCommand("addufo", UFO_NewUfoOnGeoscape_f, "Add a new UFO to geoscape");
	Cmd_AddCommand("removeufo", UFO_RemoveUfoFromGeoscape_f, "Remove a UFO from geoscape");
#ifdef DEBUG
	Cmd_AddCommand("debug_destroyallufos", UFO_DestroyAllUFOsOnGeoscape_f, "Destroy all UFOs on geoscape and spawn the crashsite missions (if not over water)");
	Cmd_AddCommand("debug_listufo", UFO_ListUfosOnGeoscape_f, "Print UFO information to game console");
	Cvar_Get("debug_showufos", "0", 0, "Show all UFOs on geoscape");
#endif
}
