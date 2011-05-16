/**
 * @file save_team.h
 * @brief XML tag constants for team savegame.
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

#define SAVE_TEAM_ROOTNODE "multiplayer"

#define SAVE_TEAM_NODE "team"
#define SAVE_TEAM_CHARACTER "character"

#define SAVE_TEAM_EQUIPMENT "equipment"
#define SAVE_TEAM_ITEM "item"
#define SAVE_TEAM_ID "id"
#define SAVE_TEAM_NUM "num"
#define SAVE_TEAM_NUMLOOSE "numLoose"

/*
DTD:

<!ELEMENT multiplayer team equipment>

<!ELEMENT team character*>
** for <character> check save_character.h (campaign/save)

<!ELEMENT equipment item*>
<!ELEMENT item EMPTY>
<!ATTLIST item
	id			CDATA	#REQUIRED
	num			CDATA	0
	numLoose	CDATA	0
>

*/
