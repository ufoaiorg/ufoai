/**
 * @file save_airfight.h
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

#ifndef SAVE_AIRFIGHT_H
#define SAVE_AIRFIGHT_H

#define SAVE_AIRFIGHT_PROJECTILE "projectile"

#define SAVE_AIRFIGHT_ITEMID "itemid"
#define SAVE_AIRFIGHT_POS "pos"
#define SAVE_AIRFIGHT_IDLETARGET "idleTarget"
#define SAVE_AIRFIGHT_TIME "time"
#define SAVE_AIRFIGHT_ANGLE "angle"
#define SAVE_AIRFIGHT_BULLET "bullet"
#define SAVE_AIRFIGHT_BEAM "beam"

#define SAVE_AIRFIGHT_ATTACKINGAIRCRAFT "attackingAircraft"
#define SAVE_AIRFIGHT_AIMEDAIRCRAFT "aimedAircraft"

#define SAVE_AIRFIGHT_AIRCRAFTIDX "idx"
#define SAVE_AIRFIGHT_ISUFO "isUFO"

#endif

/*
DTD:

<!ELEMENT projectile (pos* idleTarget attackingAircraft? aimedAircraft?)>
<!ATTLIST projectile
	itemid			CDATA	#REQUIRED
	time			CDATA	'0'
	angle			CDATA	'0.0'
	bullet			CDATA	'false'
	beam			CDATA	'false'
>

<!ELEMENT attackingAircraft EMPTY>
<!ATTLIST attackingAircraft
	idx				CDATA	#REQUIRED
	isUFO			CDATA	'false'
>
<!ELEMENT aimedAircraft EMPTY>
<!ATTLIST aimedAircraft
	idx				CDATA	#REQUIRED
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
