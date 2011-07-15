/**
 * @file cl_team.h
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

#ifndef CLIENT_CL_TEAM_H
#define CLIENT_CL_TEAM_H

#define MAX_WHOLETEAM			32
#define MAX_TEAMDATASIZE		32768

typedef struct actorSkin_s {
	/** */
	int idx;

	/** Unique string identifier */
	char *id;

	/** Name of the skin, displayed in user interface */
	char name[MAX_VAR];

	/** If true, this skin is used in singleplayer mode */
	qboolean singleplayer;
	/** If true, this skin is used in multiplayer mode */
	qboolean multiplayer;
} actorSkin_t;

void CL_GenerateCharacter(character_t *chr, const char *teamDefName);
void CL_UpdateCharacterValues(const character_t *chr, const char *cvarPrefix);

void TEAM_InitStartup(void);

actorSkin_t* CL_AllocateActorSkin(const char *name);
unsigned int CL_GetActorSkinCount(void);

extern chrList_t chrDisplayList;

#endif
