/**
 * @file save_base.h
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

#ifndef SAVE_BASES_H
#define SAVE_BASES_H

#define SAVE_BASES_BASES "bases"

#define SAVE_BASES_BASE "base"
#define SAVE_BASES_IDX "idx"
#define SAVE_BASES_NAME "name"
#define SAVE_BASES_POS "pos"
#define SAVE_BASES_BASESTATUS "baseStatus"
#define SAVE_BASES_ALIENINTEREST "alienInterest"

#define SAVE_BASES_BUILDINGSPACE "buildingSpace"
#define SAVE_BASES_BUILDING "building"
#define SAVE_BASES_X "x"
#define SAVE_BASES_Y "y"
#define SAVE_BASES_BUILDINGINDEX "buildingIDX"
#define SAVE_BASES_BLOCKED "blocked"

#define SAVE_BASES_BUILDINGS "buildings"
#define SAVE_BASES_BUILDINGTYPE "buildingType"
#define SAVE_BASES_BUILDING_PLACE "buildingPlace"
#define SAVE_BASES_BUILDINGSTATUS "buildingStatus"
#define SAVE_BASES_BUILDINGTIMESTART "buildingTimeStart"
#define SAVE_BASES_BUILDINGBUILDTIME "buildingBuildTime"
#define SAVE_BASES_BUILDINGLEVEL "buildingLevel"

#define SAVE_BASES_BATTERIES "batteries"
#define SAVE_BASES_LASERS "lasers"
#define SAVE_BASES_WEAPON "weapon"
#define SAVE_BASES_AUTOFIRE "autoFire"
#define SAVE_BASES_TARGET "target"

#define SAVE_BASES_STORAGE "storage"
#define SAVE_BASES_ITEM "item"
#define SAVE_BASES_ODS_ID "id"
#define SAVE_BASES_NUM "num"
#define SAVE_BASES_NUMLOOSE "numLoose"

#define SAVE_BASES_RADARRANGE "radarRange"
#define SAVE_BASES_TRACKINGRANGE "trackingRange"

#define SAVE_BASESTATUS_NAMESPACE "savebaseStatus"
#define SAVE_BUILDINGSTATUS_NAMESPACE "savebuildingStatus"
static const constListEntry_t saveBaseConstants[] = {
	{SAVE_BASESTATUS_NAMESPACE"::underattack", BASE_UNDER_ATTACK},
	{SAVE_BASESTATUS_NAMESPACE"::working", BASE_WORKING},
	/* other base statuses (notused, destroyed) should not appear in savegames */
	{SAVE_BUILDINGSTATUS_NAMESPACE"::construction", B_STATUS_UNDER_CONSTRUCTION},
	{SAVE_BUILDINGSTATUS_NAMESPACE"::built", B_STATUS_CONSTRUCTION_FINISHED},
	{SAVE_BUILDINGSTATUS_NAMESPACE"::working", B_STATUS_WORKING},
	{SAVE_BUILDINGSTATUS_NAMESPACE"::down", B_STATUS_DOWN},

	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT bases base+>
<!ELEMENT base pos buildingSpace buildings batteries lasers>
<!ATTLIST base
	idx					CDATA		#REQUIRED
	name				CDATA		#IMPLIED
	baseStatus	(underattack,
					working)		#REQUIRED
	alienInterest		CDATA		'0'
	currentAircraftIDX	CDATA		#IMPLIED
	radarRange			CDATA		'0'
	trackingRange		CDATA		'0'
>

<!ELEMENT pos EMPTY>
<!ATTLIST pos
	x			CDATA		'0.0'
	y			CDATA		'0.0'
	z			CDATA		'0.0'
>

<!ELEMENT buildingSpace building+>
<!ELEMENT building EMPTY>
<!ATTLIST building
	x			CDATA		'0'
	y			CDATA		'0'
	buildingIDX	CDATA		#IMPLIED
	blocked		CDATA		'false'
>

<!ELEMENT buildings building*>
<!ELEMENT building pos>
<!ATTLIST building
	buildingType		CDATA	#REQUIRED
	buildingPlace		CDATA
	buildingStatus (


	)
	buildingTimeStart	CDATA	'0'
	buildingBuildTime	CDATA	'0'
	buildingLevel		CDATA	'0'
>

<!ELEMENT batteries weapon*>
<!ELEMENT lasers weapon*>
<!ELEMENT weapon EMPTY>
<!ATTLIST weapon
	autoFire			CDATA	'true'
	target				CDATA	#IMPLIED
	...
>
**Note: more ATTLIST of weapon is defined in save_fightequip.h

<!ELEMENT storage item*>

<!ELEMENT item EMPTY>
<!ATTLIST item
	id			CDATA		#REQUIRED
	num			CDATA		'0'
	numLoose	CDATA		'0'
>
*/
