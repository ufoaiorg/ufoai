/**
 * @file save_produce.h
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

#ifndef SAVE_PRODUCE_H
#define SAVE_PRODUCE_H

#define SAVE_PRODUCE_PRODUCTION "production"

#define SAVE_PRODUCE_QUEUE "queue"
#define SAVE_PRODUCE_QUEUEIDX "IDX"

#define SAVE_PRODUCE_ITEM "item"
#define SAVE_PRODUCE_ITEMID "itemid"
#define SAVE_PRODUCE_AIRCRAFTID "aircraftid"
#define SAVE_PRODUCE_UFOIDX "UFOIDX"
#define SAVE_PRODUCE_AMOUNT "amount"
#define SAVE_PRODUCE_PROGRESS "progress"

#endif

/*
DTD:

<!ELEMENT production queue*>

<!ELEMENT queue item*>
<!ATTLIST queue
	IDX			CDATA	#REQUIRED
>

<!ELEMENT item EMPTY>
<!ATTLIST item
	itemid		CDATA	#IMPLIED
	aircraftid	CDATA	#IMPLIED
	UFOIDX		CDATA	#IMPLIED
	amount		CDATA	#REQUIRED
	frame		CDATA	'0'
>

*Note: One of itemid, aircraftid, UFOIDX is required
*Note: amount must be greater than zero
*/
