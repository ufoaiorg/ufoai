/**
 * @file cp_alienbase.c
 * @brief Alien base related functions
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

#include "../cl_shared.h"
#include "cp_campaign.h"
#include "cp_alienbase.h"
#include "cp_map.h"
#include "cp_missions.h"
#include "save/save_alienbase.h"

#define MAPDEF_ALIENBASE "alienbase"
#define ALIENBASE_DISCOVERED_TECH "rs_alien_base_discovered_event"

/**
 * @brief Set new base position
 * @param[out] pos Position of the new base.
 * @note This function generates @c maxLoopPosition random positions, and select among those the one that
 * is the farthest from every other alien bases. This is intended to get a rather uniform distribution
 * of alien bases, while still keeping a random base localisation.
 */
void AB_SetAlienBasePosition (vec2_t pos)
{
	int counter;
	vec2_t randomPos;
	alienBase_t* base;
	float minDistance = 0.0f;			/**< distance between current selected alien base */
	const int maxLoopPosition = 6;		/**< Number of random position among which the final one will be selected */

	counter = 0;
	while (counter < maxLoopPosition) {
		float distance = 0.0f;

		/* Get a random position */
		CP_GetRandomPosOnGeoscape(randomPos, qtrue);

		/* Alien base must not be too close from phalanx base */
		if (MAP_PositionCloseToBase(randomPos))
			continue;

		/* If this is the first alien base, there's no further condition: select this pos and quit */
		if (!ccs.numAlienBases) {
			Vector2Copy(randomPos, pos);
			return;
		}

		/* Calculate minimim distance between THIS position (pos) and all alien bases */
		for (base = ccs.alienBases; base < ccs.alienBases + ccs.numAlienBases; base++) {
			const float currentDistance = GetDistanceOnGlobe(base->pos, randomPos);
			if (distance < currentDistance) {
				distance = currentDistance;
			}
		}

		/* If this position is farther than previous ones, select it */
		if (minDistance < distance) {
			Vector2Copy(randomPos, pos);
			minDistance = distance;
		}

		counter++;
	}
	if (counter == maxLoopPosition)
		Vector2Copy(randomPos, pos);
}

/**
 * @brief Build a new alien base
 * @param[in] pos Position of the new base.
 * @return Pointer to the base that has been built.
 */
alienBase_t* AB_BuildBase (const vec2_t pos)
{
	alienBase_t *base;
	const float initialStealthValue = 50.0f;				/**< How hard PHALANX will find the base */

	if (ccs.numAlienBases >= MAX_ALIEN_BASES) {
		Com_Printf("AB_BuildBase: Too many alien bases build\n");
		return NULL;
	}

	base = &ccs.alienBases[ccs.numAlienBases];
	memset(base, 0, sizeof(*base));

	Vector2Copy(pos, base->pos);
	base->stealth = initialStealthValue;
	base->idx = (ptrdiff_t)(base - ccs.alienBases);
	ccs.numAlienBases++;

	return base;
}

/**
 * @brief Destroy an alien base.
 * @param[in] base Pointer to the alien base.
 */
void AB_DestroyBase (alienBase_t *base)
{
	assert(base);

	REMOVE_ELEM_ADJUST_IDX(ccs.alienBases, base->idx, ccs.numAlienBases);

	/* Alien loose all their interest in supply if there's no base to send the supply */
	if (ccs.numAlienBases == 0)
		ccs.interest[INTERESTCATEGORY_SUPPLY] = 0;
}

/**
 * @brief Get Alien Base per Idx.
 * @param[in] baseIDX IDX of the alien Base in alienBases[].
 * @param[in] checkIdx True if you want to check if baseIdx is lower than number of base.
 * @return Pointer to the base.
 */
alienBase_t* AB_GetBase (int baseIDX, qboolean checkIdx)
{
	if (baseIDX < 0 || baseIDX >= MAX_ALIEN_BASES)
		return NULL;

	if (checkIdx && baseIDX >= ccs.numAlienBases)
		return NULL;

	return &ccs.alienBases[baseIDX];
}

/**
 * @brief Spawn a new alien base mission after it has been discovered.
 */
void CP_SpawnAlienBaseMission (alienBase_t *alienBase)
{
	mission_t *mission;

	mission = CP_CreateNewMission(INTERESTCATEGORY_ALIENBASE, qtrue);
	if (!mission) {
		Com_Printf("CP_SpawnAlienBaseMission: Could not add mission, abort\n");
		return;
	}

	mission->stage = STAGE_BASE_DISCOVERED;
	mission->data = (void *) alienBase;

	mission->mapDef = Com_GetMapDefinitionByID(MAPDEF_ALIENBASE);
	if (!mission->mapDef)
		Com_Error(ERR_FATAL, "Could not find mapdef "MAPDEF_ALIENBASE);

	Vector2Copy(alienBase->pos, mission->pos);
	mission->posAssigned = qtrue;

	Com_sprintf(mission->location, sizeof(mission->location), _("Alien base"));

	/* Alien base stay until it's destroyed */
	CP_MissionDisableTimeLimit(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);

	if (!RS_MarkStoryLineEventResearched(ALIENBASE_DISCOVERED_TECH))
		Com_DPrintf(DEBUG_CLIENT, ALIENBASE_DISCOVERED_TECH" is not marked as researched\n");
	else
		Cmd_ExecuteString("addeventmail alien_base_discovered");
}

/**
 * @brief Update stealth value of one alien base due to one aircraft.
 * @param[in] aircraft Pointer to the aircraft_t.
 * @param[in] base Pointer to the alien base.
 * @note base stealth decreases if it is inside an aircraft radar range, and even more if it's
 * inside @c radarratio times radar range.
 * @sa UFO_UpdateAlienInterestForOneBase
 */
static void AB_UpdateStealthForOneBase (const aircraft_t const *aircraft, alienBase_t *base)
{
	float distance;
	float probability = 0.0001f;			/**< base probability, will be modified below */
	const float radarratio = 0.4f;			/**< stealth decreases faster if base is inside radarratio times radar range */
	const float decreasingFactor = 5.0f;	/**< factor applied when outside @c radarratio times radar range */

	/* base is already discovered */
	if (base->stealth < 0)
		return;

	/* aircraft can't find base if it's too far */
	distance = GetDistanceOnGlobe(aircraft->pos, base->pos);
	if (distance > aircraft->radar.range)
		return;

	/* the bigger the base, the higher the probability to find it */
	probability *= base->supply;

	/* decrease probability if the base is far from aircraft */
	if (distance > aircraft->radar.range * radarratio)
		probability /= decreasingFactor;

	/* probability must depend on DETECTION_INTERVAL (in case we change the value) */
	probability *= DETECTION_INTERVAL;

	base->stealth -= probability;

	/* base discovered ? */
	if (base->stealth < 0) {
		base->stealth = -10.0f;		/* just to avoid rounding errors */
		CP_SpawnAlienBaseMission(base);
	}
}

/**
 * @brief Update stealth value of every base for every aircraft.
 * @note Called every @c DETECTION_INTERVAL
 * @sa CL_CampaignRun
 * @sa UFO_UpdateAlienInterestForOneBase
 */
void AB_UpdateStealthForAllBase (void)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		aircraft_t *aircraft;

		aircraft = NULL;
		while ((aircraft = AIR_GetNextFromBase(base, aircraft)) != NULL) {
			alienBase_t* alienBase;

			/* Only aircraft on geoscape can detect alien bases */
			if (!AIR_IsAircraftOnGeoscape(aircraft))
				continue;

			for (alienBase = ccs.alienBases; alienBase < ccs.alienBases + ccs.numAlienBases; alienBase++)
				AB_UpdateStealthForOneBase(aircraft, alienBase);
		}
	}
}

/**
 * @brief Nations help in searching alien base.
 * @note called once per day, but will update stealth only every @c daysPerWeek day
 * @sa CL_CampaignRun
 */
void AB_BaseSearchedByNations (void)
{
	const int daysPerWeek = 7;				/**< delay (in days) between base stealth update */
	float probability = 1.0f;				/**< base probability, will be modified below */
	const float xviLevel = 20.0f;			/**< xviInfection value of nation that will divide probability to
											 * find alien base by 2*/
	alienBase_t* base;

	/* Stealth is updated only once a week */
	if (ccs.date.day % daysPerWeek)
		return;

	for (base = ccs.alienBases; base < ccs.alienBases + ccs.numAlienBases; base++) {
		const nation_t *nation = MAP_GetNation(base->pos);

		/* If nation is a lot infected, it won't help in finding base (government infected) */
		if (nation && nation->stats[0].xviInfection)
			probability /= 1.0f + nation->stats[0].xviInfection / xviLevel;

		/* the bigger the base, the higher the probability to find it */
		probability *= base->supply;

		base->stealth -= probability;
	}
}

/**
 * @brief Check if a supply mission is possible.
 * @return True if there is at least one base to supply.
 */
qboolean AB_CheckSupplyMissionPossible (void)
{
	return ccs.numAlienBases;
}

/**
 * @brief Choose Alien Base that should be supplied.
 * @return Pointer to the base.
 */
alienBase_t* AB_ChooseBaseToSupply (void)
{
	const int baseIDX = rand() % ccs.numAlienBases;
	return AB_GetBase(baseIDX, qtrue);
}

/**
 * @brief Supply a base.
 * @param[in] base Pointer to the supplied base.
 * @param[in] decreaseStealth If the stealth level of the base should be decreased.
 */
void AB_SupplyBase (alienBase_t *base, qboolean decreaseStealth)
{
	const float decreasedStealthValue = 5.0f;				/**< How much stealth is reduced because Supply UFO was seen */

	assert(base);

	base->supply++;
	if (decreaseStealth && base->stealth >= 0.0f)
		base->stealth -= decreasedStealthValue;
}

/**
 * @brief Check number of alien bases.
 * @return number of alien bases.
 */
int AB_GetAlienBaseNumber (void)
{
	return ccs.numAlienBases;
}

#ifdef DEBUG
/**
 * @brief Print Alien Bases information to game console
 */
static void AB_AlienBaseDiscovered_f (void)
{
	int i;

	for (i = 0; i < ccs.numAlienBases; i++) {
		ccs.alienBases[i].stealth = -10.0f;
		CP_SpawnAlienBaseMission(&ccs.alienBases[i]);
	}
}

/**
 * @brief Print Alien Bases information to game console
 * @note called with debug_listalienbase
 */
static void AB_AlienBaseList_f (void)
{
	int i;

	if (ccs.numAlienBases == 0) {
		Com_Printf("No alien base founded\n");
		return;
	}

	for (i = 0; i < ccs.numAlienBases; i++) {
		Com_Printf("Alien Base: %i\n", i);
		if (i != ccs.alienBases[i].idx)
			Com_Printf("Warning: bad idx (%i instead of %i)\n", ccs.alienBases[i].idx, i);
		Com_Printf("...pos: (%f, %f)\n", ccs.alienBases[i].pos[0], ccs.alienBases[i].pos[1]);
		Com_Printf("...supply: %i\n", ccs.alienBases[i].supply);
		if (ccs.alienBases[i].stealth < 0)
			Com_Printf("...base discovered\n");
		else
			Com_Printf("...stealth: %f\n", ccs.alienBases[i].stealth);
	}
}
#endif

/**
 * @sa UI_InitStartup
 */
void AB_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listalienbase", AB_AlienBaseList_f, "Print Alien Bases information to game console");
	Cmd_AddCommand("debug_alienbasevisible", AB_AlienBaseDiscovered_f, "Set all alien bases to discovered");
#endif
}

/**
 * @brief Load callback for alien base data
 * @param[in] p XML Node structure, where we get the information from
 * @sa AB_SaveXML
 */
qboolean AB_LoadXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n, *s;

	n = mxml_GetNode(p, SAVE_ALIENBASE_ALIENBASES);
	if (!n)
		return qfalse;

	for (i = 0, s = mxml_GetNode(n, SAVE_ALIENBASE_BASE); i < MAX_ALIEN_BASES && s; i++, s = mxml_GetNextNode(s, n, SAVE_ALIENBASE_BASE)) {
		alienBase_t *base = AB_GetBase(i, qfalse);

		assert(base);
		base->idx = (ptrdiff_t)(base - ccs.alienBases);
		if (!mxml_GetPos2(s, SAVE_ALIENBASE_POS, base->pos)) {
			Com_Printf("Position is invalid for Alienbase %d (idx %d)\n", i, base->idx);
			return qfalse;
		}
		base->supply = mxml_GetInt(s, SAVE_ALIENBASE_SUPPLY, 0);
		base->stealth = mxml_GetFloat(s, SAVE_ALIENBASE_STEALTH, 0.0);
	}
	ccs.numAlienBases = i;

	return qtrue;
}

/**
 * @brief Save callback for alien base data
 * @param[out] p XML Node structure, where we write the information to
 * @sa AB_LoadXML
 */
qboolean AB_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n = mxml_AddNode(p, SAVE_ALIENBASE_ALIENBASES);

	for (i = 0; i < ccs.numAlienBases; i++) {
		const alienBase_t *base = AB_GetBase(i, qtrue);
		mxml_node_t *s = mxml_AddNode(n, SAVE_ALIENBASE_BASE);

		assert(base);
		mxml_AddPos2(s, SAVE_ALIENBASE_POS, base->pos);
		mxml_AddIntValue(s, SAVE_ALIENBASE_SUPPLY, base->supply);
		mxml_AddFloatValue(s, SAVE_ALIENBASE_STEALTH, base->stealth);
	}

	return qtrue;
}

