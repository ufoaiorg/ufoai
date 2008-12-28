/**
 * @file cl_game.h
 * @brief Shared game type headers
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

#ifndef CL_GAME_H
#define CL_GAME_H

#define GAME_NONE			0
#define GAME_SINGLEPLAYER	(1 << 0)
#define GAME_MULTIPLAYER	(1 << 1)
#define GAME_CAMPAIGN		(GAME_SINGLEPLAYER | (1 << 2))
#define GAME_SKIRMISH		(GAME_SINGLEPLAYER | (1 << 3))

#define GAME_MAX			GAME_SKIRMISH

#define GAME_IsSingleplayer()	(ccs.gametype & GAME_SINGLEPLAYER)
#define GAME_IsMultiplayer()	(ccs.gametype == GAME_MULTIPLAYER)
#define GAME_IsSkirmish()		(ccs.gametype == GAME_SKIRMISH)
#define GAME_IsCampaign()		(ccs.gametype == GAME_CAMPAIGN)

void GAME_InitStartup(void);
void GAME_SetMode(int gametype);

#endif
