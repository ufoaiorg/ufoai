/**
 * @file save_xvi.h
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

#ifndef SAVE_XVI_H
#define SAVE_XVI_H

#define SAVE_XVI_XVI "XVI"
#define SAVE_XVI_WIDTH "width"
#define SAVE_XVI_HEIGHT "height"

#define SAVE_XVI_ENTRY "entry"
#define SAVE_XVI_X "x"
#define SAVE_XVI_Y "y"
#define SAVE_XVI_LEVEL "level"

#endif

/*
DTD:

<!ELEMENT XVI entry*>
<!ATTLIST XVI
	width	CDATA	'0'
	height	CDATA	'0'
<

<!ELEMENT entry EMPTY>
<!ATTLIST entry
	x		CDATA	'0'
	y		CDATA	'0'
	level	CDATA	'0'
>

*/
