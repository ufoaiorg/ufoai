/**
 * @file save_interest.h
 * @brief XML tag constants for savegame.
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

#ifndef SAVE_INTEREST_H
#define SAVE_INTEREST_H

#define SAVE_INTERESTS "interests"

#define SAVE_INTERESTS_LASTINCREASEDELAY "lastIncreaseDelay"
#define SAVE_INTERESTS_LASTMISSIONSPAWNEDDELAY "lastMissionSpawnedDelay"
#define SAVE_INTERESTS_OVERALL "overall"

#define SAVE_INTERESTS_INTEREST "interest"
#define SAVE_INTERESTS_ID "id"
#define SAVE_INTERESTS_VAL "value"

#define SAVE_INTERESTCAT_NAMESPACE "saveInterestCat"
static const constListEntry_t saveInterestConstants[] = {
	{SAVE_INTERESTCAT_NAMESPACE"::none", INTERESTCATEGORY_NONE},
	{SAVE_INTERESTCAT_NAMESPACE"::recon", INTERESTCATEGORY_RECON},
	{SAVE_INTERESTCAT_NAMESPACE"::terror", INTERESTCATEGORY_TERROR_ATTACK},
	{SAVE_INTERESTCAT_NAMESPACE"::baseAttack", INTERESTCATEGORY_BASE_ATTACK},
	{SAVE_INTERESTCAT_NAMESPACE"::building", INTERESTCATEGORY_BUILDING},
	{SAVE_INTERESTCAT_NAMESPACE"::supply", INTERESTCATEGORY_SUPPLY},
	{SAVE_INTERESTCAT_NAMESPACE"::XVI", INTERESTCATEGORY_XVI},
	{SAVE_INTERESTCAT_NAMESPACE"::rescue", INTERESTCATEGORY_RESCUE},
	{SAVE_INTERESTCAT_NAMESPACE"::intercept", INTERESTCATEGORY_INTERCEPT},
	{SAVE_INTERESTCAT_NAMESPACE"::harvest", INTERESTCATEGORY_HARVEST},
	{SAVE_INTERESTCAT_NAMESPACE"::alienBase", INTERESTCATEGORY_ALIENBASE},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT interests interest*>
<!ATTLIST interests
	lastIncreaseDelay		CDATA		'0'
	lastMissionSpawnedDelay	CDATA		'0'
	overall					CDATA		'0'
>

<!ELEMENT interest EMPTY>
<!ATTLIST interest
	id			(none, recon, terror,
				baseAttack, building,
				supply, XVI, intercept,
				harvest, alienBase)		#REQUIRED
	value					CDATA		'0'
>

*/
