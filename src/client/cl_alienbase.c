/**
 * @file cl_alienbase.c
 * @brief
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
#include "cl_alienbase.h"
#include "cl_map.h"

static alienBase_t alienBases[MAX_ALIEN_BASES];		/**< Alien bases spawned in game */
static int numAlienBases;							/**< Number of alien bases in game */

/**
 * @brief Reset global array and number of base
 * @note Use me when stating a new game
 */
void AB_ResetAlienBases (void)
{
	memset(alienBases, 0, sizeof(alienBases));
	numAlienBases = 0;
}

/**
 * @brief Set new base position
 * @param[out] position Position of the new base.
 * @note This function generates @c maxLoopPosition random positions, and select among those the one that
 * is the farthest from every other alien bases. This is intended to get a rather uniform distribution
 * of alien bases, while still keeping a random base localisation.
 */
void AB_SetAlienBasePosition (vec2_t position)
{
	int counter;
	vec2_t pos;
	alienBase_t* base;
	float minDistance = 0.0f;			/**< distance between current selected alien base */
	const int maxLoopPosition = 6;		/**< Number of random position among which the final one will be selected */

	counter = 0;
	while (counter < maxLoopPosition) {
		float distance = 0.0f;

		/* Get a random position */
		CP_GetRandomPosOnGeoscape(pos, qtrue);

		/* Alien base must not be too close from phalanx base */
		if (MAP_PositionCloseToBase(pos))
			continue;

		/* If this is the first alien base, there's no further condition: select this pos and quit */
		if (!numAlienBases) {
			Vector2Copy(pos, position);
			return;
		}

		/* Calculate minimim distance between THIS position (pos) and all alien bases */
		for (base = alienBases; base < alienBases + numAlienBases; base++) {
			const float currentDistance = MAP_GetDistance(base->pos, pos);
			if (distance < currentDistance) {
				distance = currentDistance;
			}
		}

		/* If this position is farther than previous ones, select it */
		if (minDistance < distance) {
			Vector2Copy(pos, position);
			minDistance = distance;
		}

		counter++;
	}
}

/**
 * @brief Build a new alien base
 * @param[in] pos Position of the new base.
 * @return Pointer to the base that has been built.
 */
alienBase_t* AB_BuildBase (vec2_t pos)
{
	alienBase_t *base;
	const float initialStealthValue = 50.0f;				/**< How hard PHALANX will find the base */

	if (numAlienBases >= MAX_ALIEN_BASES) {
		Com_Printf("AB_BuildBase: Too many alien bases build\n");
		return NULL;
	}

	base = &alienBases[numAlienBases];
	memset(base, 0, sizeof(*base));

	Vector2Copy(pos, base->pos);
	base->stealth = initialStealthValue;
	base->idx = numAlienBases++;

	return base;
}

/**
 * @brief Destroy an alien base.
 * @param[in] base Pointer to the alien base.
 */
void AB_DestroyBase (alienBase_t *base)
{
	assert(base);

	REMOVE_ELEM_ADJUST_IDX(alienBases, base->idx, numAlienBases);

	/* Alien loose all their interest in supply if there's no base to send the supply */
	if (numAlienBases == 0)
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

	if (checkIdx && baseIDX >= numAlienBases)
		return NULL;

	return &alienBases[baseIDX];
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
	distance = MAP_GetDistance(aircraft->pos, base->pos);
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
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		int aircraftIdx;
		if (!base)
			continue;

		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			const aircraft_t const *aircraft = &base->aircraft[aircraftIdx];
			alienBase_t* alienBase;

			/* Only aircraft on geoscape can detect alien bases */
			if (!AIR_IsAircraftOnGeoscape(aircraft))
				continue;

			for (alienBase = alienBases; alienBase < alienBases + numAlienBases; alienBase++) {
				AB_UpdateStealthForOneBase(aircraft, alienBase);
			}
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

	for (base = alienBases; base < alienBases + numAlienBases; base++) {
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
	return numAlienBases;
}

/**
 * @brief Choose Alien Base that should be supplied.
 * @param[out] pos Position of the base.
 * @return Pointer to the base.
 */
alienBase_t* AB_ChooseBaseToSupply (vec2_t pos)
{
	const int baseIDX = rand() % numAlienBases;

	Vector2Copy(alienBases[baseIDX].pos, pos);

	return &alienBases[baseIDX];
}

/**
 * @brief Supply a base.
 * @param[in] base Pointer to the supplied base.
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
	return numAlienBases;
}

#ifdef DEBUG
/**
 * @brief Print Alien Bases information to game console
 */
static void AB_AlienBaseDiscovered_f (void)
{
	int i;

	for (i = 0; i < numAlienBases; i++) {
		alienBases[i].stealth = -10.0f;
		CP_SpawnAlienBaseMission(&alienBases[i]);
	}
}

/**
 * @brief Print Alien Bases information to game console
 * @note called with debug_listalienbase
 */
static void AB_AlienBaseList_f (void)
{
	int i;

	if (numAlienBases == 0) {
		Com_Printf("No alien base founded\n");
		return;
	}

	for (i = 0; i < numAlienBases; i++) {
		Com_Printf("Alien Base: %i\n", i);
		if (i != alienBases[i].idx)
			Com_Printf("Warning: bad idx (%i instead of %i)\n", alienBases[i].idx, i);
		Com_Printf("...pos: (%f, %f)\n", alienBases[i].pos[0], alienBases[i].pos[1]);
		Com_Printf("...supply: %i\n", alienBases[i].supply);
		if (alienBases[i].stealth < 0)
			Com_Printf("...base discovered\n");
		else
			Com_Printf("...stealth: %f\n", alienBases[i].stealth);
	}
}
#endif

/**
 * @sa MN_InitStartup
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
 * @sa AB_Save
 */
qboolean AB_Load (sizebuf_t *sb, void *data)
{
	int i;

	numAlienBases = MSG_ReadShort(sb);
	for (i = 0; i < presaveArray[PRE_MAXALB]; i++) {
		alienBase_t *base = &alienBases[i];
		base->idx = MSG_ReadShort(sb);
		MSG_Read2Pos(sb, base->pos);
		base->supply = MSG_ReadShort(sb);
		base->stealth = MSG_ReadFloat(sb);
	}
	return qtrue;
}

/**
 * @brief Save callback for alien base data
 * @sa AB_Load
 */
qboolean AB_Save (sizebuf_t *sb, void *data)
{
	int i;

	MSG_WriteShort(sb, numAlienBases);
	for (i = 0; i < presaveArray[PRE_MAXALB]; i++) {
		const alienBase_t *base = &alienBases[i];
		MSG_WriteShort(sb, base->idx);
		MSG_Write2Pos(sb, base->pos);
		MSG_WriteShort(sb, base->supply);
		MSG_WriteFloat(sb, base->stealth);
	}

	return qtrue;
}
