/**
 * @file cp_xvi.c
 * @brief Campaign XVI code
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../renderer/r_overlay.h"
#include "../mxml/mxml_ufoai.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_xvi.h"

static technology_t *rsAlienXVI;

#define XVI_EVENT_NAME "rs_alien_xvi_event"

/**
 * @brief Spread XVI at a given position.
 * @param[in] pos Position where XVI should be spread.
 */
void CP_SpreadXVIAtPos (const vec2_t pos)
{
	nation_t *nation;

	if (!CP_IsXVIResearched())
		return;

	nation = MAP_GetNation(pos);
	if (nation)
		nation->stats[0].xviInfection++;

	R_IncreaseXVILevel(pos);
}

/**
 * @brief Reduce XVI at a given position.
 * @param[in] pos Position where XVI should be reduced.
 */
void CP_ReduceXVIAtPos (const vec2_t pos)
{
	nation_t *nation;

	if (!CP_IsXVIResearched())
		return;

	nation = MAP_GetNation(pos);
	if (nation)
		nation->stats[0].xviInfection--;

	R_DecreaseXVILevel(pos);
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

	assert(ccs.numNations);

	/* check for XVI infection rate */
	for (i = 0, nation = ccs.nations; i < ccs.numNations; i++, nation++) {
		XVIRate += nation->stats[0].xviInfection;
	}
	XVIRate /= ccs.numNations;
	return XVIRate;
}

/**
 * @brief Spread XVI for each mission that needs to be daily spread.
 * @note Daily called
 */
void CP_SpreadXVI (void)
{
	const linkedList_t *list = ccs.missions;

	/* don't check if XVI spreading didn't start yet */
	if (!CP_IsXVIResearched())
		return;

	for (;list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		if (mission->stage == STAGE_SPREAD_XVI)
			CP_SpreadXVIAtPos(mission->pos);
	}
}

/**
 * @brief Returns @c true if the tech for the xvi event is already researched
 */
qboolean CP_IsXVIResearched (void)
{
	return RS_IsResearched_ptr(rsAlienXVI);
}

void CP_XVIInit (void)
{
	rsAlienXVI = RS_GetTechByID(XVI_EVENT_NAME);
	if (!rsAlienXVI)
		Com_Error(ERR_DROP, "CP_XVIInit: Could not find tech definition for " XVI_EVENT_NAME);
}

/**
 * @brief XVI map saving callback
 * @note Only save transparency
 * @sa Savegame callback
 * @sa SAV_InitXML
 * @sa XVI_LoadXML
 */
qboolean XVI_SaveXML (mxml_node_t *p)
{
	int y;
	int width;
	int height;
	mxml_node_t *n;
	int defaultval = 0; /* that value should be the value, which is the most used one in the array */
	byte *out = R_GetXVIMap(&width, &height);
	if (!out)
		return qtrue;

	/* ok, do the saving... */
	n = mxml_AddNode(p, "xvi");
	mxml_AddInt(n, "width", width);
	mxml_AddInt(n, "height", height);
	mxml_AddInt(n, "default", defaultval);

	for (y = 0; y < height; y++) {
		int x;
		for (x = 0; x < width; x++) {
			/* That saves many bytes in the savegame */
			if (out[y * width + x] != defaultval) {
				mxml_node_t *s = mxml_AddNode(n, "entry");
				mxml_AddInt(s, "x", x);
				mxml_AddInt(s, "y", y);
				mxml_AddInt(s, "xv", out[y * width + x]);
			}
		}
	}
	return qtrue;
}

/**
 * @brief Load the XVI map from the savegame.
 * @sa Savegame callback
 * @sa SAV_InitXML
 * @sa XVI_SaveXML
 */
qboolean XVI_LoadXML (mxml_node_t *p)
{
	byte *out;
	int width, height;
	mxml_node_t *s;
	int defaultval;
	mxml_node_t *n = mxml_GetNode(p, "xvi");
	/* If there is no XVI, it will not be loaded */
	if (!n) {
		R_InitializeXVIOverlay(NULL);
		return qtrue;
	}

	width = mxml_GetInt(n, "width", 0);
	height = mxml_GetInt(n, "height", 0);
	defaultval = mxml_GetInt(n, "default", 0);

	out = (byte *)Mem_PoolAlloc(width * height, vid_imagePool, 0);
	if (!out)
		Com_Error(ERR_DROP, "TagMalloc: failed on allocation of %i bytes for XVI_Load", width * height);

	memset(out, defaultval, sizeof(out)); /*setting the whole array to the defaultval. That saves much memory in saving */

	for (s = mxml_GetNode(n, "entry"); s; s = mxml_GetNextNode(s, n, "entry")) {
		const int x = mxml_GetInt(s, "x", 0);
		const int y = mxml_GetInt(s, "y", 0);
		if (x >= 0 && x < width && y >= 0 && y <= height)
			out[y * width + x] = mxml_GetInt(s, "xv", 0);
	}

	R_InitializeXVIOverlay(out);
	Mem_Free(out);
	return qtrue;
}

/**
 * @brief Start XVI spreading in campaign.
 * @note This is called when 'a new twist' technology is discovered.
 */
void CP_StartXVISpreading_f (void)
{
	int i, numAlienBases;

	/** @todo ccs.XVIShowMap should not be enabled at the same time than
	 * CP_IsXVIResearched(): ccs.XVIShowMap means that PHALANX has a map of
	 * XVI, whereas CP_IsXVIResearched() means that aliens started
	 * spreading XVI */
	ccs.XVIShowMap = qtrue;

	/* Spawn a few alien bases depending on difficulty level */
	if (ccs.curCampaign->difficulty > 0)
		numAlienBases = 3;
	else if (ccs.curCampaign->difficulty < 0)
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
