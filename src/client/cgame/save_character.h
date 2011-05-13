/**
 * @file save_character.h
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

#ifndef SAVE_CHARACTER_H
#define SAVE_CHARACTER_H

#define SAVE_CHARACTER_NAME "name"
#define SAVE_CHARACTER_BODY "body"
#define SAVE_CHARACTER_PATH "path"
#define SAVE_CHARACTER_HEAD "head"
#define SAVE_CHARACTER_SKIN "skin"
#define SAVE_CHARACTER_TEAMDEF "teamdef"
#define SAVE_CHARACTER_GENDER "gender"
#define SAVE_CHARACTER_UCN "ucn"
#define SAVE_CHARACTER_MAXHP "maxHp"
#define SAVE_CHARACTER_HP "hp"
#define SAVE_CHARACTER_STUN "stun"
#define SAVE_CHARACTER_MORALE "morale"
#define SAVE_CHARACTER_FIELDSIZE "fieldSize"
#define SAVE_CHARACTER_STATE "state"

#define SAVE_CHARACTER_SCORES "scores"

#define SAVE_CHARACTER_SKILLS "skill"
#define SAVE_CHARACTER_SKILLTYPE "type"
#define SAVE_CHARACTER_INITSKILL "initial"
#define SAVE_CHARACTER_EXPERIENCE "experience"
#define SAVE_CHARACTER_SKILLIMPROVE "improve"

#define SAVE_CHARACTER_KILLS "kill"
#define SAVE_CHARACTER_KILLTYPE "type"
#define SAVE_CHARACTER_KILLED "killed"
#define SAVE_CHARACTER_STUNNED "stunned"

#define SAVE_CHARACTER_SCORE_ASSIGNEDMISSIONS "missions"
#define SAVE_CHARACTER_SCORE_RANK "rank"

#define SAVE_CHARACTER_SKILLTYPE_NAMESPACE "saveCharacterSkillType"
#define SAVE_CHARACTER_KILLTYPE_NAMESPACE "saveCharacterKillType"
static const constListEntry_t saveCharacterConstants[] = {
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::power", ABILITY_POWER},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::speed", ABILITY_SPEED},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::accuracy", ABILITY_ACCURACY},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::mind", ABILITY_MIND},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::close", SKILL_CLOSE},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::heavy", SKILL_HEAVY},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::assault", SKILL_ASSAULT},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::sniper", SKILL_SNIPER},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::explosive", SKILL_EXPLOSIVE},
	{SAVE_CHARACTER_SKILLTYPE_NAMESPACE"::hp", SKILL_NUM_TYPES},

	{SAVE_CHARACTER_KILLTYPE_NAMESPACE"::enemy", KILLED_ENEMIES},
	{SAVE_CHARACTER_KILLTYPE_NAMESPACE"::civilian", KILLED_CIVILIANS},
	{SAVE_CHARACTER_KILLTYPE_NAMESPACE"::team", KILLED_TEAM},

	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT character (scores, inventory)>
<!ATTLIST character
	name			CDATA		#IMPLIED
	body			CDATA		#IMPLIED
	path			CDATA		#IMPLIED
	head			CDATA		#IMPLIED
	skin			CDATA		'0'
	teamdef			CDATA		#REQUIRED
	gender			CDATA		'0'
	ucn				CDATA		'0'
	maxHp			CDATA		'0'
	hp				CDATA		'0'
	stun			CDATA		'0'
	morale			CDATA		'0'
	fieldSize		CDATA		'1'
>

<!ELEMENT scores (skill*, kill*)>
<!ATTLIST scores
	missions		CDATA		'0'
	rank			CDATA		'-1'
>

<!ELEMENT skill EMPTY>
<!ATTLIST skill
	type			CDATA		#REQUIRED
	initial			CDATA		'0'
	experience		CDATA		'0'
	improve			CDATA		'0'
>

<!ELEMENT kill EMPTY>
<!ATTLIST kill
	type			CDATA		#REQUIRED
	killed			CDATA		'0'
	stunned			CDATA		'0'
>

*/
