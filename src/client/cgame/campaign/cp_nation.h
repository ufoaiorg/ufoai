/**
 * @file cp_nation.c
 * @brief Nation code
 * @note Functions with NAT_*
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

#ifndef CP_NATION_H
#define CP_NATION_H

/**
 * @brief Detailed information about the nation relationship (currently per month, but could be used elsewhere).
 * @todo Maybe we should also move the "funding" stuff (see nation_t) in here? It is static right now though so i see no reason to do that yet.
 */
typedef struct nationInfo_s {
	qboolean	inuse;	/**< Is this entry used? */

	/* Relationship */
	float happiness;	/**< percentage (0.00 - 1.00) of how the nation appreciates PHALANX. 1.00 is the maximum happiness */
	int xviInfection;	/**< Increase by one each time a XVI spread is done in this nation. */
} nationInfo_t;

/**
 * @brief Nation definition
 */
typedef struct nation_s {
	const char *id;		/**< Unique ID of this nation. */
	const char *name;		/**< Full name of the nation. */
	int idx;		/**< position in the nations array */

	vec4_t color;		/**< The color this nation uses in the color-coded earth-map */
	vec2_t pos;		/**< Nation position on geoscape. */

	nationInfo_t stats[MONTHS_PER_YEAR];	/**< Detailed information about the history of this nations relationship toward PHALANX and the aliens.
									 * The first entry [0] is the current month - all following entries are stored older months.
									 * Combined with the funding info below we can generate an overview over time.
									 */

	/* Funding */
	int maxFunding;		/**< How many (monthly) credits. */
	int maxSoldiers;	/**< How many (monthly) soldiers. */
	int maxScientists;	/**< How many (monthly) scientists. */
	int maxWorkers;		/**< How many (monthly) workers. */
	int maxPilots;		/**< How many (monthly) pilots. */
} nation_t;

/**
 * @brief City definition
 */
typedef struct city_s {
	const char *id;			/**< Unique ID of this city. */
	const char *name;		/**< Full name of the city. */
	int idx;				/**< position in the cities array */

	vec2_t pos;				/**< City position on geoscape. */
} city_t;

nation_t *NAT_GetNationByIDX(const int index);
nation_t *NAT_GetNationByID(const char *nationID);
void NAT_UpdateHappinessForAllNations(const float minhappiness);
void NAT_SetHappiness(const float minhappiness, nation_t *nation, const float happiness);
int NAT_GetFunding(const nation_t* const nation, int month);
const nationInfo_t* NAT_GetCurrentMonthInfo(const nation_t* const nation);
const char* NAT_GetHappinessString(const nation_t* nation);

void CL_ParseNations(const char *name, const char **text);
void CL_ParseCities(const char *name, const char **text);
qboolean NAT_ScriptSanityCheck(void);

void NAT_HandleBudget(const struct campaign_s *campaign);
void NAT_BackupMonthlyData(void);

void NAT_InitStartup(void);
void NAT_Shutdown(void);

#define MAX_NATIONS 8

#endif
