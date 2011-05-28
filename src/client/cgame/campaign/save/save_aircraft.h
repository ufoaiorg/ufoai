/**
 * @file save_aircraft.h
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

#ifndef SAVE_AIRCRAFT_H
#define SAVE_AIRCRAFT_H

#define SAVE_AIRCRAFT_UFOS "UFOs"
#define SAVE_AIRCRAFT_PHALANX "aircraft"
#define SAVE_AIRCRAFT_NUMAIRCRAFT "numAircraft"

#define SAVE_AIRCRAFT_AIRCRAFT "craft"
#define SAVE_AIRCRAFT_ID "id"
#define SAVE_AIRCRAFT_NAME "name"
#define SAVE_AIRCRAFT_IDX "idx"

#define SAVE_AIRCRAFT_STATUS "status"
#define SAVE_AIRCRAFT_HOMEBASE "homebase"

#define SAVE_AIRCRAFT_FUEL "fuel"
#define SAVE_AIRCRAFT_DAMAGE "damage"
#define SAVE_AIRCRAFT_POS "pos"
#define SAVE_AIRCRAFT_DIRECTION "direction"
#define SAVE_AIRCRAFT_POINT "point"
#define SAVE_AIRCRAFT_TIME "time"

#define SAVE_AIRCRAFT_MISSIONID "missionid"
#define SAVE_AIRCRAFT_DETECTIONIDX "detectionIDX"
#define SAVE_AIRCRAFT_LASTSPOTTED_DATE "lastSpottedDate"

#define SAVE_AIRCRAFT_AIRCRAFTTARGET "aircraftTarget"

#define SAVE_AIRCRAFT_AIRSTATS "airstats"
#define SAVE_AIRCRAFT_AIRSTAT "stat"
#define SAVE_AIRCRAFT_AIRSTATID "id"
#define SAVE_AIRCRAFT_VAL "val"

#define SAVE_AIRCRAFT_DETECTED "detected"
#define SAVE_AIRCRAFT_LANDED "landed"
#define SAVE_AIRCRAFT_HANGAR "hangar"

#define SAVE_AIRCRAFT_AIRCRAFTTEAM "aircraftTeam"
#define SAVE_AIRCRAFT_MEMBER "member"
#define SAVE_AIRCRAFT_TEAM_UCN "ucn"

#define SAVE_AIRCRAFT_PILOTUCN "pilotUCN"

#define SAVE_AIRCRAFT_CARGO "cargo"

#define SAVE_AIRCRAFT_ITEM "item"
#define SAVE_AIRCRAFT_ITEMID "itemid"
#define SAVE_AIRCRAFT_AMOUNT "amount"

#define SAVE_AIRCRAFT_ALIENCARGO "alienCargo"

#define SAVE_AIRCRAFT_TEAMDEFID "teamdefid"
#define SAVE_AIRCRAFT_ALIVE "alive"
#define SAVE_AIRCRAFT_DEAD "dead"

#define SAVE_AIRCRAFT_ROUTE "route"
#define SAVE_AIRCRAFT_ROUTE_DISTANCE "distance"
#define SAVE_AIRCRAFT_ROUTE_POINT "point"

#define SAVE_AIRCRAFT_WEAPONS "weapons"
#define SAVE_AIRCRAFT_SHIELDS "shields"
#define SAVE_AIRCRAFT_ELECTRONICS "electronics"
#define SAVE_AIRCRAFT_SLOT "slot"

#define SAVE_AIRCRAFT_PROJECTILES "projectiles"

#define SAVE_AIRCRAFTSTATUS_NAMESPACE "saveAircraftStatus"
#define SAVE_AIRCRAFTSTAT_NAMESPACE "saveAircraftStat"
static const constListEntry_t saveAircraftConstants[] = {
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::none", AIR_NONE},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::refuel", AIR_REFUEL},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::home", AIR_HOME},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::idle", AIR_IDLE},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::transit", AIR_TRANSIT},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::mission", AIR_MISSION},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::ufo", AIR_UFO},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::drop", AIR_DROP},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::intercept", AIR_INTERCEPT},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::transfer", AIR_TRANSFER},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::returning", AIR_RETURNING},
	{SAVE_AIRCRAFTSTATUS_NAMESPACE"::crashed", AIR_CRASHED},

	{SAVE_AIRCRAFTSTAT_NAMESPACE"::speed", AIR_STATS_SPEED},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::maxspeed", AIR_STATS_MAXSPEED},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::shield", AIR_STATS_SHIELD},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::ecm", AIR_STATS_ECM},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::damage", AIR_STATS_DAMAGE},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::accuracy", AIR_STATS_ACCURACY},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::fuelsize", AIR_STATS_FUELSIZE},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::weaponrange", AIR_STATS_WRANGE},
	{SAVE_AIRCRAFTSTAT_NAMESPACE"::antimatter", AIR_STATS_ANTIMATTER},

	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT aircraft craft*>
<!ELEMENT UFOs craft*>
<!ELEMENT craft >
<!ATTLIST craft (pos direction weapons shields electronics route lastSpottedDate? airstats aircraftTeam cargo alienCargo)
	id					CDATA	#REQUIRED
	name				CDATA	#IMPLIED
	status		(none,refuel,home,idle,
				transit,mission,ufo,
				drop,intercept,transfer,
				returning)		#REQUIRED
	homebase			CDATA	#IMPLIED
	fuel				CDATA	'0'
	damage				CDATA	'0'
	point				CDATA	'0'
	time				CDATA	'0'
	missionid			CDATA	#IMPLIED
	detectionIDX		CDATA	#IMPLIED
	detected			CDATA	'false'
	landed				CDATA	'false'
	aircraftTarget		CDATA	#IMPLIED

	idx					CDATA	#REQUIRED
	hangar				CDATA	'0'
	pilotUCN			CDATA	#IMPLIED

	radarRange			CDATA	'0'
	radarTrackingRange	CDATA	'0'
>
** Note: idx is required for Phalanx crafts only

<!ELEMENT pos EMPTY>
<!ATTLIST pos
	x					CDATA	'0.0'
	y					CDATA	'0.0'
	z					CDATA	'0.0'
>

<!ELEMENT direction EMPTY>
<!ATTLIST direction
	x					CDATA	'0.0'
	y					CDATA	'0.0'
	z					CDATA	'0.0'
>

<!ELEMENT route point*>
<!ATTLIST route
	distance			CDATA	'0.0'
>

<!ELEMENT point EMPTY>
<!ATTLIST point
	x					CDATA	'0.0'
	y					CDATA	'0.0'
>

<!ELEMENT lastSpottedDate EMPTY>
<!ATTLIST lastSpottedDate
	day					CDATA	'0'
	sec					CDATA	'0'
>

<!ELEMENT weapons slot*>
<!ELEMENT shields slot>
<!ELEMENT electronics slot*>
<!ELEMENT slot EMPTY>
**Note: ATTLIST of slot is defined in save_fightequip.h

<!ELEMENT airstats airstat*>
<!ELEMENT airstat EMPTY>
<!ATTLIST airstat
	id		(speed,maxspeed,shield,
			ecm,damage,accuracy,
			fuelsize,weaponrange,
			antimatter)			#REQUIRED
	val					CDATA	'0'
>

<!ELEMENT aircraftTeam member*>
<!ELEMENT member EMPTY>
<!ATTLIST member
	ucn					CDATA	#IMPLIED
>

<!ELEMENT cargo item*>
<!ELEMENT item EMPTY>
<!ATTLIST item
	itemid				CDATA	#REQUIRED
	amount				CDATA	'0'
>

<!ELEMENT alienCargo cargo*>
<!ELEMENT cargo EMPTY>
<!ATTLIST cargo
	teamdefid			CDATA	#REQUIRED
	alive				CDATA	'0'
	dead				CDATA	'0'
>

<!ELEMENT projectiles projectile*>
**Note: projectile is defined in save_airfight.h

*/
