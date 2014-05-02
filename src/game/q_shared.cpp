/**
 * @file
 * @brief Common functions.
 */

/*
All original material Copyright (C) 2002-2014 UFO: Alien Invasion.

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
const char* pa_format[] =
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

/*
 * Thoughts about (pilot) survival rates:
 * Crash over water: chances for survival are very bad
 * Crash over arctic or wasted terrain; fare a little better
 * Crash over mountain, desert or cold terrain: survive a little longer
 * Crash over tropical or fertile area: This is the good life :)
 */
static const terrainDef_s terrainDefTable[] = {
	{255, 128, 0,	"desert",	0.33f, 0.0f, 0.0f},
	{128, 255, 255, "arctic",	0.12f, 0.0f, 0.4f},
	{0,		0, 64,	"water",	0.10f, 0.0f, 0.0f},
	{255,	0, 0,	"mountain",	0.33f, 0.1f, 0.3f},
	{128, 128, 255,	"tropical",	2.50f, 0.3f, 0.0f},
	{0,		0, 255,	"cold",		0.33f, 0.0f, 0.3f},
	{128,	0, 128,	"wasted",	0.12f, 0.1f, 0.0f},
	{128, 255, 0,	"grass",	2.50f, 0.2f, 0.1f},
	{0,		0, 0,	nullptr,	0.00f, 0.0f, 0.0f}
};

TerrainDefs::TerrainDefs() {
	for (int i = 0; i < 2; i++)
		add(&terrainDefTable[i]);
}

bool TerrainDefs::add(const terrainDef_s* tdef) {
	if (findByColor(tdef->rgbRed, tdef->rgbGreen, tdef->rgbBlue))
		return false;

	if (findByName(tdef->terrainName))
		return false;

	for (int i = 0; i < MAX_TERRAINDEFS - 1; i++) {
		if (!terrainDefTable2[i]) {
			terrainDefTable2[i] = tdef;
			terrainDefTable2[i + 1] = nullptr;
			return true;
		}
	}
	return false;
}

TerrainDefs terrainDefs;
