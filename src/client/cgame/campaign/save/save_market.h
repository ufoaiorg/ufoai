/**
 * @file save_market.h
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

#ifndef SAVE_MARKET_H
#define SAVE_MARKET_H

#define SAVE_MARKET_MARKET "market"
#define SAVE_MARKET_ITEM "item"
#define SAVE_MARKET_AIRCRAFT "aircraft"
#define SAVE_MARKET_ID "id"
#define SAVE_MARKET_NUM "num"
#define SAVE_MARKET_BID "bid"
#define SAVE_MARKET_ASK "ask"
#define SAVE_MARKET_EVO "evo"
#define SAVE_MARKET_AUTOSELL "autoSell"

#endif

/*
DTD:

<!ELEMENT market item* aircraft*>
<!ELEMENT item EMPTY>
<!ATTLIST item
	id			CDATA	#REQUIRED
	num			CDATA	'0'
	bid			CDATA	'0'
	ask			CDATA	'0'
	evo			CDATA	'0.0'
	autoSell	(false,
				true)	false
>
<!ELEMENT aircraft EMPTY>
<!ATTLIST aircraft
	id			CDATA	#REQUIRED
	num			CDATA	'0'
	bid			CDATA	'0'
	ask			CDATA	'0'
	evo			CDATA	'0.0'
>

*/
