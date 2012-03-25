/**
 * @file q_shared.c
 * @brief Common functions.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/q_shared.c
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "q_shared.h"

/** @brief Player action format strings for netchannel transfer
 * @note They all have a prefix
 * @sa MSG_Write_PA */
const char *pa_format[] =
{
	"",					/**< PA_NULL */
	"s",				/**< PA_TURN */
	"g",				/**< PA_MOVE */
	"s",				/**< PA_STATE - don't use a bitmask here - only one value
						 * @sa G_ClientStateChange */
	"gbbl",				/**< PA_SHOOT */
	"s",				/**< PA_USE */
	"bbbbbb",			/**< PA_INVMOVE */
	"sss",				/**< PA_REACT_SELECT */
	"ss"				/**< PA_RESERVE_STATE */
};
CASSERT(lengthof(pa_format) == PA_NUM_EVENTS);
