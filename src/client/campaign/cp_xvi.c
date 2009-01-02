/**
 * @file cp_xvi.c
 * @brief Campaign XVI code
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

#include "../client.h"
#include "../cl_global.h"
#include "cl_map.h"
#include "cp_xvi.h"

static technology_t *rsAlienXVI;

/* headers from renderer */
void R_IncreaseXVILevel(const vec2_t pos);
void R_InitializeXVIOverlay(const char *mapname, byte *data, int width, int height);
byte* R_XVIMapCopy(int *width, int *height);

/**
 * @brief Sread XVI at a given position.
 * @param[in] pos Position where XVI should be spread.
 */
void CP_SpreadXVIAtPos (const vec2_t pos)
{
	nation_t *nation;

#ifdef DEBUG
	if (!ccs.XVISpreadActivated) {
		Com_Printf("CP_SpreadXVIAtPos: Try to spread XVI but XVISpread is not activated\n");
		return;
	}
#endif

	nation = MAP_GetNation(pos);
	if (nation)
		nation->stats[0].xviInfection++;

	R_IncreaseXVILevel(pos);
}

/**
 * @brief Return the average XVI rate
 * @note XVI = eXtraterrestial Viral Infection
 */
int CP_GetAverageXVIRate (void)
{
	int XVIRate = 0;
	int i;
	nation_t* nation;

	assert(gd.numNations);

	/* check for XVI infection rate */
	for (i = 0, nation = gd.nations; i < gd.numNations; i++, nation++) {
		XVIRate += nation->stats[0].xviInfection;
	}
	XVIRate /= gd.numNations;
	return (int) XVIRate;
}

/**
 * @brief Spread XVI for each mission that needs to be daily spread.
 * @note Daily called
 */
void CP_SpreadXVI (void)
{
	const linkedList_t *list = ccs.missions;

	/* don't check if XVI spreading didn't start yet */
	if (!ccs.XVISpreadActivated)
		return;

	for (;list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		if (mission->stage == STAGE_SPREAD_XVI)
			CP_SpreadXVIAtPos(mission->pos);
	}
}

/**
 * @brief Returns the alien XVI tech if the tech was already researched
 */
technology_t *CP_IsXVIResearched (void)
{
	return RS_IsResearched_ptr(rsAlienXVI) ? rsAlienXVI : NULL;
}

void CP_XVIInit (void)
{
	rsAlienXVI = RS_GetTechByID(XVI_EVENT_NAME);
	if (!rsAlienXVI)
		Sys_Error("CP_XVIInit: Could not find tech definition for " XVI_EVENT_NAME);
}

/**
 * @brief XVI map saving callback
 * @note Only save transparency
 * @sa Savegame callback
 * @sa SAV_Init
 * @sa XVI_Load
 */
qboolean XVI_Save (sizebuf_t *sb, void *data)
{
	byte *out;
	int x, y;
	int width;
	int height;

	out = R_XVIMapCopy(&width, &height);
	if (!out) {
		MSG_WriteShort(sb, 0);
		MSG_WriteShort(sb, 0);
		return qtrue;
	}

	MSG_WriteShort(sb, width);
	MSG_WriteShort(sb, height);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			MSG_WriteByte(sb, out[y * width + x]);
	}
	Mem_Free(out);
	return qtrue;
}

/**
 * @brief Load the XVI map from the savegame.
 * @sa Savegame callback
 * @sa SAV_Init
 * @sa XVI_Save
 */
qboolean XVI_Load (sizebuf_t *sb, void *data)
{
	byte *out;
	int i, j;
	int width, height;

	width = MSG_ReadShort(sb);
	height = MSG_ReadShort(sb);

	out = (byte *)Mem_PoolAlloc(width * height, vid_imagePool, 0);
	if (!out)
		Sys_Error("TagMalloc: failed on allocation of %i bytes for XVI_Load", width * height);

	for (i = 0; i < width; i++) {
		for (j = 0; j < height; j++) {
			out[j * width + i] = MSG_ReadByte(sb);
		}
	}

	R_InitializeXVIOverlay(curCampaign->map, out, width, height);

	Mem_Free(out);

	return qtrue;
}

/**
 * @brief Start XVI spreading in campaign.
 * @note This is called when 'a new twist' technology is discovered
 */
void CP_StartXVISpreading_f (void)
{
	int i, numAlienBases;

	/** @todo ccs.XVIShowMap should not be enabled at the same time than
	 * ccs.XVISpreadActivated: ccs.XVIShowMap means that PHALANX has a map of
	 * XVI, whereas ccs.XVISpreadActivated  means that aliens started
	 * spreading XVI */
	ccs.XVISpreadActivated = qtrue;
	ccs.XVIShowMap = qtrue;

	/* Spawn a few alien bases depending on difficulty level */
	if (curCampaign->difficulty > 0)
		numAlienBases = 3;
	else if (curCampaign->difficulty < 0)
		numAlienBases = 1;
	else
		numAlienBases = 2;

	for (i = 0; i < numAlienBases; i++)
		CP_CreateNewMission(INTERESTCATEGORY_BUILDING, qfalse);
}

/**
 * @brief This will hide or show the geoscape button for handling the xvi overlay map
 */
void CP_UpdateXVIMapButton (void)
{
	Cvar_SetValue("mn_xvimap", ccs.XVIShowMap);
}
