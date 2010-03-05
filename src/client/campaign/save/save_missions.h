/**
 * @file save_missions.h
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


#define SAVE_MISSIONS_MISSION "mission"
#define SAVE_MISSIONS_MISSION_IDX "IDX"
#define SAVE_MISSIONS_ID "id"
#define SAVE_MISSIONS_MAPDEF_ID "mapDefId"
#define SAVE_MISSIONS_MAPDEFTIMES "mapDefTimes"
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
#define SAVE_MISSIONS_STARTDATE_DAY "startDateDay"
#define SAVE_MISSIONS_STARTDATE_SEC "startDateSec"
#define SAVE_MISSIONS_FINALDATE_DAY "finalDateDay"
#define SAVE_MISSIONS_FINALDATE_SEC "finalDateSec"
#define SAVE_MISSIONS_POS "pos"
#define SAVE_MISSIONS_UFO "UFO"
#define SAVE_MISSIONS_ONGEOSCAPE "onGeoscape"

/*
DTD:

<!ELEMENT missions EMPTY>
<!ELEMENT mission pos>
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
	category					CDATA	'0'
	stage						CDATA	'0'
	baseIDX						CDATA	#IMPLIED
	installationIDX				CDATA	#IMPLIED
	alienBaseIDX				CDATA	#IMPLIED
	location					CDATA	#IMPLIED
	initialOverallInterest		CDATA	'0'
	initialIndividualInterest	CDATA	'0'
	startDateDay				CDATA	'0'
	startDateSec				CDATA	'0'
	finalDateDay				CDATA	'0'
	finalDateSec				CDATA	'0'
	UFO							CDATA	#IMPLIED
	onGeoscape					CDATA	'false'
>

*/

