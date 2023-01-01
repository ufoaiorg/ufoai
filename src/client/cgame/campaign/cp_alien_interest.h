/**
 * @file
 * @brief Alien interest header
 */

/*
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

#pragma once

/** possible campaign interest categories: type of missions that aliens can undertake */
typedef enum interestCategory_s {
	INTERESTCATEGORY_NONE,			/**< No mission */
	INTERESTCATEGORY_RECON,			/**< Aerial recon mission or ground mission (UFO may or not land) */
	INTERESTCATEGORY_TERROR_ATTACK,	/**< Terror attack */
	INTERESTCATEGORY_BASE_ATTACK,	/**< Alien attack a phalanx base */
	INTERESTCATEGORY_BUILDING,		/**< Alien build a new base or subverse governments */
	INTERESTCATEGORY_SUBVERT,		/**< pseudo mission type for subverse governments */
	INTERESTCATEGORY_SUPPLY,		/**< Alien supply one of their bases */
	INTERESTCATEGORY_XVI,			/**< Alien try to spread XVI */
	INTERESTCATEGORY_INTERCEPT,		/**< Alien try to intercept PHALANX aircraft */
	INTERESTCATEGORY_INTERCEPTBOMBING,	/**< Alien try to attack PHALANX installation. A subtype of INTERCEPT */
	INTERESTCATEGORY_HARVEST,		/**< Alien try to harvest */
	INTERESTCATEGORY_ALIENBASE,		/**< Alien base already built on earth
									 * @note This is not a mission alien can undertake, but the result of
									 * INTERESTCATEGORY_BUILDING */
	INTERESTCATEGORY_UFOCARRIER,	/**< UFO-Carrier is detected */
	INTERESTCATEGORY_RESCUE,

	INTERESTCATEGORY_MAX
} interestCategory_t;

void INT_ResetAlienInterest(void);
void INT_IncreaseAlienInterest(const struct campaign_s* campaign);
void INT_ChangeIndividualInterest(float percentage, interestCategory_t category);

#ifdef DEBUG
const char* INT_InterestCategoryToName(interestCategory_t category);
#endif

void INT_InitStartup(void);
void INT_Shutdown(void);
