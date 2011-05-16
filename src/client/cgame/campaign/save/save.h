/**
 * @file save.h
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

#ifndef SAVE_H
#define SAVE_H

#define SAVE_ROOTNODE "savegame"

/** @note this data is loaded from/saved to binary header, we save it into the XML for readability only */
#define SAVE_SAVEVERSION "saveVersion"
#define SAVE_UFOVERSION "gameVersion"
#define SAVE_COMMENT "comment"
#define SAVE_REALDATE "realDate"
#define SAVE_GAMEDATE "gameDate"

#endif

/*
DTD:

<!ELEMENT savegame (bases research campaign interests missions market employees* alienCont UFOs projectiles
	installations storedUFOs production messages stats nations transfers alienBases
	XVI messageOptions)>
<!ATTLIST savegame
		saveVersion		CDATA		#IMPLIED
		gameVersion		CDATA		#IMPLIED
		comment			CDATA		#IMPLIED
		realDate		CDATA		#IMPLIED
		gameDate		CDATA		#IMPLIED
>
*/
