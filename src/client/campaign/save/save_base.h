/**
 * @file save_base.h
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

#ifndef SAVE_BASES_H
#define SAVE_BASES_H

#define SAVE_BASES_BASES "bases"
#define SAVE_BASES_NUMAIRCRAFT "numAircraft"
#define SAVE_BASES_BASE "base"
#define SAVE_BASES_NAME "name"
#define SAVE_BASES_POS "pos"
#define SAVE_BASES_BUILDINGSPACE "buildingSpace"
#define SAVE_BASES_BUILDING "building"
#define SAVE_BASES_K "k"
#define SAVE_BASES_L "l"
#define SAVE_BASES_BUILDINGINDEX "buildingIDX"
#define SAVE_BASES_BLOCKED "blocked"
#define SAVE_BASES_BUILDINGS "buildings"
#define SAVE_BASES_BUILDINGTYPE "buildingType"
#define SAVE_BASES_BUILDING_PLACE "buildingPlace"
#define SAVE_BASES_BUILDINGSTATUS "buildingStatus"
#define SAVE_BASES_BUILDINGTIMESTART "buildingTimeStart"
#define SAVE_BASES_BUILDINGBUILDTIME "buildingBuildTime"
#define SAVE_BASES_BUILDINGLEVEL "buildingLevel"
#define SAVE_BASES_POS "pos"
#define SAVE_BASES_NUMBUILDINGS "numBuildings"
#define SAVE_BASES_BASESTATUS "baseStatus"
#define SAVE_BASES_ALIENINTEREST "alienInterest"
#define SAVE_BASES_BATTERIES "batteries"
#define SAVE_BASES_LASERS "lasers"
#define SAVE_BASES_CURRENTAIRCRAFTIDX "currentAircraftIDX"
#define SAVE_BASES_AIRCRAFTS "aircraft"
#define SAVE_BASES_AIRCRAFT "craft"
#define SAVE_BASES_STORAGE "storage"
#define SAVE_BASES_RADARRANGE "radarRange"
#define SAVE_BASES_TRACKINGRANGE "trackingRange"
#define SAVE_BASES_ALIENCONT "aliensCont"
#define SAVE_BASES_ALIEN "alien"
#define SAVE_BASES_ID "id"
#define SAVE_BASES_AMOUNTALIVE "amountAlive"
#define SAVE_BASES_AMOUNTDEAD "amountDead"
#define SAVE_BASES_ITEM "item"
#define SAVE_BASES_ODS_ID "id"
#define SAVE_BASES_NUM "num"
#define SAVE_BASES_NUMLOOSE "numLoose"
#define SAVE_BASES_WEAPON "weapon"
#define SAVE_BASES_AUTOFIRE "autoFire"
#define SAVE_BASES_TARGET "target"
#define SAVE_BASES_ITEMID "itemid"
#define SAVE_BASES_AMMOID "ammoid"
#define SAVE_BASES_NEXTITEMID "nextItemid"
#define SAVE_BASES_NEXTAMMOID "nextAmmoid"
#define SAVE_BASES_INSTALLATIONTIME "installationTime"
#define SAVE_BASES_AMMOLEFT "ammoLeft"
#define SAVE_BASES_DELAYNEXTSHOT "delayNextShot"

#endif

/*
DTD: (incomplete)

<!ELEMENT storage item*>

<!ELEMENT item EMPTY>
<!ATTLIST item
	id			CDATA		#REQUIRED
	num			CDATA		'0'
	numLoose	CDATA		'0'
>
*/

