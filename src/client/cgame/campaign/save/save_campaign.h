/**
 * @file save_campaign.h
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

#ifndef SAVE_CAMPAIGN_H
#define SAVE_CAMPAIGN_H

#define SAVE_CAMPAIGN_CAMPAIGN "campaign"

#define SAVE_CAMPAIGN_ID "id"
#define SAVE_CAMPAIGN_CREDITS "credits"
#define SAVE_CAMPAIGN_PAID "paid"
#define SAVE_CAMPAIGN_NEXTUNIQUECHARACTERNUMBER "nextUniqueCharacterNumber"
#define SAVE_CAMPAIGN_DATE "date"
#define SAVE_CAMPAIGN_CIVILIANSKILLED "civiliansKilled"
#define SAVE_CAMPAIGN_ALIENSKILLED "aliensKilled"

#define SAVE_CAMPAIGN_MAP "map"
#define SAVE_CAMPAIGN_CENTER0 "center0"
#define SAVE_CAMPAIGN_CENTER1 "center1"
#define SAVE_CAMPAIGN_ANGLES0 "angles0"
#define SAVE_CAMPAIGN_ANGLES1 "angles1"
#define SAVE_CAMPAIGN_ZOOM "zoom"
#define SAVE_CAMPAIGN_CL_GEOSCAPE_OVERLAY "r_geoscape_overlay"
#define SAVE_CAMPAIGN_RADAROVERLAYWASSET "radarOverlayWasSet"
#define SAVE_CAMPAIGN_XVISHOWMAP "XVIShowmap"

#define SAVE_CAMPAIGN_MAPDEFSTAT "mapDefStat"
#define SAVE_CAMPAIGN_MAPDEF "mapDef"
#define SAVE_CAMPAIGN_MAPDEF_ID "id"
#define SAVE_CAMPAIGN_MAPDEF_COUNT "count"

#endif

/*
DTD:

<!ELEMENT campaign (map date mapDefStat)>
<!ATTLIST campaign
	id							CDATA	#REQUIRED
	credits						CDATA	'0'
	paid						CDATA	'false'
	nextUniqueCharacterNumber	CDATA	'0'
	civiliansKilled				CDATA	'0'
	aliensKilled				CDATA	'0'
>

<!ELEMENT map EMPTY>
<!ATTLIST map
	center0						CDATA	'0'
	center1						CDATA	'0'
	angles0						CDATA	'0'
	angles1						CDATA	'0'
	zoom						CDATA	'0'
	r_geoscape_overlay			CDATA	'0'
	radarOverlayWasSet			CDATA	'false'
	XVIShowmap					CDATA	'false'
>

<!ELEMENT date EMPTY>
<!ATTLIST date
	day							CDATA	'0'
	sec							CDATA	'0'
>

<!ELEMENT mapDefStat mapDef*>

<!ELEMENT mapDef EMPTY>
<!ATTLIST mapDef
	id							CDATA	#RQEUIRED
	count						CDATA	'0'
>

*/
