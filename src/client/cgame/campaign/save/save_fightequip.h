/**
 * @file save_fightequip.h
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

#ifndef SAVE_FIGHTEQUIP_H
#define SAVE_FIGHTEQUIP_H

#define SAVE_SLOT_ITEMID "itemid"
#define SAVE_SLOT_NEXTITEMID "nextItemid"
#define SAVE_SLOT_INSTALLATIONTIME "installationTime"
#define SAVE_SLOT_AMMOID "ammoid"
#define SAVE_SLOT_NEXTAMMOID "nextAmmoid"
#define SAVE_SLOT_AMMOLEFT "ammoLeft"
#define SAVE_SLOT_DELAYNEXTSHOT "delayNextShot"

#endif

/**
DTD:

<!ATTLIST slot
	itemid				CDATA	#IMPLIED
	nextItemid			CDATA	#IMPLIED
	installationTime	CDATA	'0'
	ammoid				CDATA	#IMPLIED
	nextAmmoid			CDATA	#IMPLIED
	ammoLeft			CDATA	'0'
	delayNextSlot		CDATA	'0'
>

**Note: slot element is defined in save_aircraft.h and save_base.h

*/
