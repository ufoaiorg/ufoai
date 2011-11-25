/**
 * @file save_statistics.h
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

#ifndef SAVE_STATS_H
#define SAVE_STATS_H

#define SAVE_STATS_STATS "stats"
#define SAVE_STATS_MISSIONS "missions"
#define SAVE_STATS_MISSIONSWON "missionsWon"
#define SAVE_STATS_MISSIONSLOST "missionsLost"
#define SAVE_STATS_BASESBUILT "basesBuilt"
#define SAVE_STATS_BASESATTACKED "basesAttacked"
#define SAVE_STATS_INSTALLATIONSBUILT "installationsBuilt"
#define SAVE_STATS_INTERCEPTIONS "interceptions"
#define SAVE_STATS_SOLDIERSLOST "soldiersLost"
#define SAVE_STATS_SOLDIERSNEW "soldiersNew"
#define SAVE_STATS_KILLEDALIENS "killedAliens"
#define SAVE_STATS_RESCUEDCIVILIANS "rescuedCivilians"
#define SAVE_STATS_RESEARCHEDTECHNOLOGIES "researchedTechnologies"
#define SAVE_STATS_MONEYINTERCEPTIONS "moneyInterceptions"
#define SAVE_STATS_MONEYBASES "moneyBases"
#define SAVE_STATS_MONEYRESEARCH "moneyResearch"
#define SAVE_STATS_MONEYWEAPONS "moneyWeapons"
#define SAVE_STATS_UFOSDETECTED "UFOsDetected"
#define SAVE_STATS_ALIENBASESBUILT "alienBasesBuilt"
#define SAVE_STATS_UFOSSTORED "UFOsStored"
#define SAVE_STATS_AIRCRAFTHAD "aircraftHad"

#endif

/*
DTD:

<!ELEMENT stats EMPTY>
<!ATTLIST stats
	missions					CDATA	'0'
	missionsWon					CDATA	'0'
	missionsLost				CDATA	'0'
	basesBuilt					CDATA	'0'
	basesAttacked				CDATA	'0'
	installationsBuilt			CDATA	'0'
	interceptions				CDATA	'0'
	soldiersLost				CDATA	'0'
	soldiersNew					CDATA	'0'
	killedAliens				CDATA	'0'
	rescuedCivilians			CDATA	'0'
	researchedTechnologies		CDATA	'0'
	moneyInterceptions			CDATA	'0'
	monexBases					CDATA	'0'
	moneyWeapons				CDATA	'0'
	UFOsDetected				CDATA	'0'
	alienBasesBuilt				CDATA	'0'
	UFOsStored					CDATA	'0'
	aircraftHad					CDATA	'0'
>
*/
