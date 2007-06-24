/**
 * @file scripts.h
 * @brief Header for script parsing functions
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

/*==============================================================
SCRIPT PARSING
==============================================================*/

#define MAX_TEAMDEFS	128

#define LASTNAME	3
typedef enum {
	NAME_NEUTRAL,
	NAME_FEMALE,
	NAME_MALE,

	NAME_LAST,
	NAME_FEMALE_LAST,
	NAME_MALE_LAST,

	NAME_NUM_TYPES
} nametypes_t;

typedef struct teamDesc_s {
	char id[MAX_VAR];
	qboolean alien;			/**< is this an alien teamdesc definition */
	qboolean armor, weapons; /**< able to use weapons/armor */
	char name[MAX_VAR];
	char tech[MAX_VAR]; /**< tech id from research.ufo */
} teamDesc_t;

extern teamDesc_t teamDesc[MAX_TEAMDEFS];
extern int numTeamDesc;

extern const char *name_strings[NAME_NUM_TYPES];

char *Com_GiveName(int gender, const char *category);
char *Com_GiveModel(int type, int gender, const char *category);
int Com_GetModelAndName(const char *team, character_t * chr);
const char* Com_GetActorSound(int category, int gender, actorSound_t soundType);

void Com_AddObjectLinks(void);
void Com_ParseScripts(void);
