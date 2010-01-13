/**
 * @file save_character.h
 * @brief XML tag constants for savegame.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#define SAVE_CHARACTER_NAME "name"
#define SAVE_CHARACTER_BODY "body"
#define SAVE_CHARACTER_PATH "path"
#define SAVE_CHARACTER_HEAD "head"
#define SAVE_CHARACTER_SKIN "skin"
#define SAVE_CHARACTER_TEAMDEFIDX "teamdefIDX"
#define SAVE_CHARACTER_GENDER "gender"
#define SAVE_CHARACTER_UCN "ucn"
#define SAVE_CHARACTER_MAXHP "maxHp"
#define SAVE_CHARACTER_HP "hp"
#define SAVE_CHARACTER_STUN "stun"
#define SAVE_CHARACTER_MORALE "morale"
#define SAVE_CHARACTER_FIELDSIZE "fieldSize"
#define SAVE_CHARACTER_SCORE "score"
#define SAVE_CHARACTER_SKILL "skill"
#define SAVE_CHARACTER_EXPERIENCE "experience"
#define SAVE_CHARACTER_INITSKILL "initSkill"
#define SAVE_CHARACTER_KILLS "kills"
#define SAVE_CHARACTER_STUNS "stuns"
#define SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS "assignedMissions"
#define SAVE_CHARACTER_SCORE_RANK "rank"

/*
DTD: (incomplete)

<!ELEMENT character (score*, inventory?)>
<!ATTLIST character
	name			#CDATA		#IMPLIED
	body			#CDATA		#IMPLIED
	path			#CDATA		#IMPLIED
	head			#CDATA		#IMPLIED
	skin			#NMTOKEN	0
	teamdefIDX		#NMTOKEN	#REQUIRED
	gender		
	ucn
	maxHp
	hp
	stun
	morale
	fieldSize
>

*/

