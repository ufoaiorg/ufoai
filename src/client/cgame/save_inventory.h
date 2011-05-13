/**
 * @file save_inventory.h
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

#ifndef SAVE_INVENTORY_H
#define SAVE_INVENTORY_H

#define SAVE_INVENTORY_INVENTORY "inventory"

#define SAVE_INVENTORY_ITEM "item"
#define SAVE_INVENTORY_AMMO "ammo"
#define SAVE_INVENTORY_ROTATED "rotated"
#define SAVE_INVENTORY_AMOUNT "amount"
#define SAVE_INVENTORY_X "x"
#define SAVE_INVENTORY_Y "y"
#define SAVE_INVENTORY_CONTAINER "container"
#define SAVE_INVENTORY_WEAPONID "weaponid"
#define SAVE_INVENTORY_MUNITIONID "munitionid"

#endif

/*
DTD:

<!ELEMENT inventory item*>
<!ELEMENT item EMPTY>
<!ATTLIST item
	container	CDATA	#REQUIRED
	weaponid	CDATA	#REQUIRED
	x			CDATA	'0'
	y			CDATA	'0'
	rotated		CDATA	'0'
	amount		CDATA	'1'
	ammoid		CDATA	#IMPLIED
	ammo		CDATA	'0'
>
*/
