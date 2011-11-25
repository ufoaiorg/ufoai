/**
 * @file save_installation.h
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

#ifndef SAVE_INSTALLATION_H
#define SAVE_INSTALLATION_H

#define SAVE_INSTALLATION_INSTALLATIONS "installations"
#define SAVE_INSTALLATION_INSTALLATION "installation"
#define SAVE_INSTALLATION_IDX "idx"
#define SAVE_INSTALLATION_TEMPLATEID "templateid"
#define SAVE_INSTALLATION_NAME "name"
#define SAVE_INSTALLATION_POS "pos"
#define SAVE_INSTALLATION_STATUS "status"
#define SAVE_INSTALLATION_DAMAGE "damage"
#define SAVE_INSTALLATION_ALIENINTEREST "alienInterest"
#define SAVE_INSTALLATION_BUILDSTART "buildStart"

#define SAVE_INSTALLATION_BATTERIES "batteries"
#define SAVE_INSTALLATION_NUM "num"

#define SAVE_INSTALLATIONSTATUS_NAMESPACE "saveInstallationStatus"
static const constListEntry_t saveInstallationConstants[] = {
	{SAVE_INSTALLATIONSTATUS_NAMESPACE"::construction", INSTALLATION_UNDER_CONSTRUCTION},
	{SAVE_INSTALLATIONSTATUS_NAMESPACE"::working", INSTALLATION_WORKING},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT installations installation*>
<!ELEMENT installation pos batteries?>
<!ATTLIST installation
	idx				CDATA		#REQUIRED
	templateid		CDATA		#REQUIRED
	name			CDATA		#IMPLIED
	status			(construction,
					working)	#REQUIRED
	damage			CDATA		'0'
	alienInterest	CDATA		'0.0'
	buildStart		CDATA		'0'
>
<!ELEMENT pos EMPTY>
<!ATTLIST pos
	x				CDATA		'0'
	y				CDATA		'0'
	z				CDATA		'0'
>
<!ELEMENT batteries weapon*>
<!ATTLIST batteries
	num				CDATA		'0'
>

**Note: weapon defined in save_bases.h

*/
