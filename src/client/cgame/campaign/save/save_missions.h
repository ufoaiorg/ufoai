/**
 * @file save_missions.h
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

#ifndef SAVE_MISSIONS_H
#define SAVE_MISSIONS_H

#define SAVE_MISSIONS "missions"
#define SAVE_MISSIONS_MISSION "mission"
#define SAVE_MISSIONS_MISSION_IDX "IDX"
#define SAVE_MISSIONS_ID "id"
#define SAVE_MISSIONS_MAPDEF_ID "mapDefId"
#define SAVE_MISSIONS_ACTIVE "active"
#define SAVE_MISSIONS_POSASSIGNED "posAssigned"
#define SAVE_MISSIONS_CRASHED "crashed"
#define SAVE_MISSIONS_ONWIN "onWin"
#define SAVE_MISSIONS_ONLOSE "onLose"
#define SAVE_MISSIONS_CATEGORY "category"
#define SAVE_MISSIONS_STAGE "stage"
#define SAVE_MISSIONS_BASEINDEX "baseIDX"
#define SAVE_MISSIONS_INSTALLATIONINDEX "installationIDX"
#define SAVE_MISSIONS_ALIENBASEINDEX "alienbaseIDX"
#define SAVE_MISSIONS_LOCATION "location"
#define SAVE_MISSIONS_INITIALOVERALLINTEREST "initialOverallInterest"
#define SAVE_MISSIONS_INITIALINDIVIDUALINTEREST "initialIndividualInterest"
#define SAVE_MISSIONS_STARTDATE "startDate"
#define SAVE_MISSIONS_FINALDATE "finalDate"
#define SAVE_MISSIONS_POS "pos"
#define SAVE_MISSIONS_CRASHED_AIRCRAFT "crashedAircraft"
#define SAVE_MISSIONS_ONGEOSCAPE "onGeoscape"

#define SAVE_MISSIONSTAGE_NAMESPACE "saveMissionStage"
static const constListEntry_t saveMissionConstants[] = {
	{SAVE_MISSIONSTAGE_NAMESPACE"::notActive", STAGE_NOT_ACTIVE},
	{SAVE_MISSIONSTAGE_NAMESPACE"::comeFromOrbit", STAGE_COME_FROM_ORBIT},
	{SAVE_MISSIONSTAGE_NAMESPACE"::reconAir", STAGE_RECON_AIR},
	{SAVE_MISSIONSTAGE_NAMESPACE"::missionGoTo", STAGE_MISSION_GOTO},
	{SAVE_MISSIONSTAGE_NAMESPACE"::reconGround", STAGE_RECON_GROUND},
	{SAVE_MISSIONSTAGE_NAMESPACE"::terrorMission", STAGE_TERROR_MISSION},
	{SAVE_MISSIONSTAGE_NAMESPACE"::buildBase", STAGE_BUILD_BASE},
	{SAVE_MISSIONSTAGE_NAMESPACE"::baseAttack", STAGE_BASE_ATTACK},
	{SAVE_MISSIONSTAGE_NAMESPACE"::subvertGov", STAGE_SUBVERT_GOV},
	{SAVE_MISSIONSTAGE_NAMESPACE"::supply", STAGE_SUPPLY},
	{SAVE_MISSIONSTAGE_NAMESPACE"::spreadXVI", STAGE_SPREAD_XVI},
	{SAVE_MISSIONSTAGE_NAMESPACE"::intercept", STAGE_INTERCEPT},
	{SAVE_MISSIONSTAGE_NAMESPACE"::baseDiscovered", STAGE_BASE_DISCOVERED},
	{SAVE_MISSIONSTAGE_NAMESPACE"::harvest", STAGE_HARVEST},
	{SAVE_MISSIONSTAGE_NAMESPACE"::returnToOrbit", STAGE_RETURN_TO_ORBIT},
	{SAVE_MISSIONSTAGE_NAMESPACE"::missionOver", STAGE_OVER},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT missions EMPTY>
<!ELEMENT mission pos startDate finalDate>
<!ATTLIST mission
	IDX							CDATA	#REQUIRED
	id							CDATA	#REQUIRED
	active						CDATA	'false'
	mapDefId					CDATA	#IMPLIED
	mapDefTimes					CDATA	'0'
	posAssigned					CDATA	#IMPLIED
	crashed						CDATA	'false'
	onWin						CDATA	#IMPLIED
	onLose						CDATA	#IMPLIED
	category	(none, recon, terror,
				baseAttack, building,
				supply, XVI, intercept,
				harvest, alienBase)		#REQUIRED
	stage		(notActive, comeFromOrbit,
				reconAir, missionGoTo,
				reconGround, terrorMission,
				buildBase, baseAttack,
				subvertGov, supply,
				spreadXVI, intercept,
				baseDiscovered, harvest,
				returnToOrbit,
				missionOver)			#REQUIRED

							CDATA	'0'
	baseIDX						CDATA	#IMPLIED
	installationIDX				CDATA	#IMPLIED
	alienBaseIDX				CDATA	#IMPLIED
	location					CDATA	#IMPLIED
	initialOverallInterest		CDATA	'0'
	initialIndividualInterest	CDATA	'0'
	UFO							CDATA	#IMPLIED
	onGeoscape					CDATA	'false'
>

<!ELEMENT startDate EMPTY>
<!ATTLIST
	day							CDATA	'0'
	sec							CDATA	'0'
>

<!ELEMENT finalDate EMPTY>
<!ATTLIST
	day							CDATA	'0'
	sec							CDATA	'0'
>

*/
