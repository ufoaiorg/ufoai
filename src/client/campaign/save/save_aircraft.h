/**
 * @file save_aircraft.h
 * @brief XML tag constants for savegame.
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


#define SAVE_AIRCRAFT_IDX "idx"
#define SAVE_AIRCRAFT_UFOS "UFOs"
#define SAVE_AIRCRAFT_AIRCRAFT "aircraft"
#define SAVE_AIRCRAFT_ID "id"
#define SAVE_AIRCRAFT_NAME "name"
#define SAVE_AIRCRAFT_STATUS "status"
#define SAVE_AIRCRAFT_FUEL "fuel"
#define SAVE_AIRCRAFT_DAMAGE "damage"
#define SAVE_AIRCRAFT_POS "pos"
#define SAVE_AIRCRAFT_DIRECTION "direction"
#define SAVE_AIRCRAFT_POINT "point"
#define SAVE_AIRCRAFT_TIME "time"
#define SAVE_AIRCRAFT_SLOT "slot"
#define SAVE_AIRCRAFT_ITEMID "itemid"
#define SAVE_AIRCRAFT_NEXTITEMID "nextItemid"
#define SAVE_AIRCRAFT_INSTALLATIONTIME "installationTime"
#define SAVE_AIRCRAFT_AMMOID "ammoid"
#define SAVE_AIRCRAFT_NEXTAMMOID "nextAmmoid"
#define SAVE_AIRCRAFT_AMMOLEFT "ammoLeft"
#define SAVE_AIRCRAFT_DELAYNEXTSHOT "delayNextShot"
#define SAVE_AIRCRAFT_WEAPONS "weapons"
#define SAVE_AIRCRAFT_SHIELDS "shields"
#define SAVE_AIRCRAFT_ELECTRONICS "electronics"
#define SAVE_AIRCRAFT_ROUTE "route"
#define SAVE_AIRCRAFT_DISTANCE "distance"
#define SAVE_AIRCRAFT_POINT "point"
#define SAVE_AIRCRAFT_MISSIONID "missionid"
#define SAVE_AIRCRAFT_DETECTIONIDX "detectionIDX"
#define SAVE_AIRCRAFT_LASTSPOTTED_DAY "lastSpottedDay"
#define SAVE_AIRCRAFT_LASTSPOTTED_SEC "lastSpottedSec"
#define SAVE_AIRCRAFT_MISSIONID "missionid"
#define SAVE_AIRCRAFT_AIRCRAFTTARGET "aircraftTarget"
#define SAVE_AIRCRAFT_AIRCRAFTTARGET "aircraftTarget"
#define SAVE_AIRCRAFT_AIRSTATS "airstats"
#define SAVE_AIRCRAFT_VAL "val"
#define SAVE_AIRCRAFT_DETECTED "detected"
#define SAVE_AIRCRAFT_LANDED "landed"
#define SAVE_AIRCRAFT_HANGAR "hangar"
#define SAVE_AIRCRAFT_AIRCRAFTTEAM "aircraftTeam"
#define SAVE_AIRCRAFT_MEMBER "member"
#define SAVE_AIRCRAFT_TEAM_IDX "idx"
#define SAVE_AIRCRAFT_TYPE "type"
#define SAVE_AIRCRAFT_PILOTIDX "pilotIDX"
#define SAVE_AIRCRAFT_CARGO "cargo"
#define SAVE_AIRCRAFT_TYPES "types"
#define SAVE_AIRCRAFT_ITEM "item"
#define SAVE_AIRCRAFT_AMOUNT "amount"
#define SAVE_AIRCRAFT_RADAR_RANGE "radarRange"
#define SAVE_AIRCRAFT_RADAR_TRACKINGRANGE "radarTrackingrange"
#define SAVE_AIRCRAFT_ALIENCARGO "alienCargo"
#define SAVE_AIRCRAFT_TYPES "types"
#define SAVE_AIRCRAFT_CARGO "cargo"
#define SAVE_AIRCRAFT_TEAMDEFID "teamdefid"
#define SAVE_AIRCRAFT_ALIVE "alive"
#define SAVE_AIRCRAFT_DEAD "dead"

#define SAVE_AIRCRAFT_PROJECTILES "projectiles"

#define SAVE_AIRCRAFT_PROJECTILE "projectile"
#define SAVE_AIRCRAFT_ITEMID "itemid"
#define SAVE_AIRCRAFT_POS "pos"
#define SAVE_AIRCRAFT_IDLETARGET "idleTarget"
#define SAVE_AIRCRAFT_TIME "time"
#define SAVE_AIRCRAFT_ANGLE "angle"
#define SAVE_AIRCRAFT_BULLET "bullet"
#define SAVE_AIRCRAFT_BEAM "beam"

#define SAVE_AIRCRAFT_ATTACKINGAIRCRAFT "attackingAircraft"
#define SAVE_AIRCRAFT_ISUFO "isUFO"

#define SAVE_AIRCRAFT_AIMEDAIRCRAFT "aimedAircraft"


/*
DTD: (incomplete)

<!ELEMENT projectiles projectile*>
<!ELEMENT projectile (pos* attackingAircraft? idleTarget)>
<!ATTLIST projectile
	itemid			CDATA	#REQUIRED		
	time			CDATA	'0'
	angle			CDATA	'0.0'
	bullet			CDATA	false
	beam			CDATA	false
>

<!ELEMENT attackingAircraft EMPTY>
<!ATTLIST attackingAircraft
	idx				CDATA	
	isUFO			CDATA	'false'
>
<!ELEMENT aimedAircraft EMPTY>
<!ATTLIST aimedAircraft
	idx				CDATA	
	isUFO			CDATA	'false'
>

<!ELEMENT pos EMPTY>
<!ATTLIST pos
	x				CDATA	'0'
	y				CDATA	'0'
>

<!ELEMENT idleTarget EMPTY>
<!ATTLIST idleTarget
	x				CDATA	'0'
	y				CDATA	'0'
	z				CDATA	'0'
>

*/
