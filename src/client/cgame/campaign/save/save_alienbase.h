/**
 * @file save_alienbase.h
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

#ifndef SAVE_ALIENBASE_H
#define SAVE_ALIENBASE_H

#define SAVE_ALIENBASE_ALIENBASES "alienBases"
#define SAVE_ALIENBASE_BASE "base"
#define SAVE_ALIENBASE_IDX "idx"
#define SAVE_ALIENBASE_POS "pos"
#define SAVE_ALIENBASE_SUPPLY "supply"
#define SAVE_ALIENBASE_STEALTH "stealth"

#endif

/*
DTD:

<!ELEMENT alienBases base*>

<!ELEMENT base pos>
<!ATTLIST base
	idx			CDATA		#REQUIRED
	supply		CDATA		'0'
	stealth		CDATA		'0.0'
>

<!ELEMENT pos EMPTY>
<!ATTLIST pos
	x			CDATA		'0.0'
	y			CDATA		'0.0'
>
*/
