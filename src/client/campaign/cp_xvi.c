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

/** @brief technology for XVI event */
static technology_t *rsAlienXVI;

/** @brief boolean used to know if each nation XVI level should be updated this day. */
static qboolean xviNationInfectionNeedsUpdate = qfalse;

/**
 * @brief The factor that is used to determine XVI radius spreading.
 * The higher this factor, the faster XVI will spread.
 */
static const float XVI_FACTOR = 15.0f;

/** @brief Number of days between 2 XVI decrease (everywhere around the world). */
static const int XVI_DECREASE_DAYS = 7;

/** @brief Name of the technology for the XVI event */
#define XVI_EVENT_NAME "rs_alien_xvi_event"

/**
 * @brief Spread XVI at a given position.
 * @param[in] pos Position where XVI should be spread.
 */
void CP_SpreadXVIAtPos (const vec2_t pos)
{
	if (!CP_IsXVIResearched())
		return;

	R_ChangeXVILevel(pos, XVI_FACTOR);

	xviNationInfectionNeedsUpdate = qtrue;
}

/**
 * @brief Reduce XVI everywhere.
 * @note This is called daily.
 */
void CP_ReduceXVIEverywhere (void)
{
	if (!CP_IsXVIResearched())
		return;

	/* Only decrease XVI if given days has passed */
	if (ccs.date.day % XVI_DECREASE_DAYS)
		return;

	R_DecreaseXVILevelEverywhere();

	xviNationInfectionNeedsUpdate = qtrue;
}

/**
 * @brief Update xviInfection value for each nation, using the XVI overlay.
 * @note should be executed after all daily event that could change XVI overlay
 */
void CP_UpdateNationXVIInfection (void)
{
	/* temporary array to store the XVI levels */
	float xviInfection[MAX_NATIONS];
	int y;
	int nationIdx;
	/* width in pixel of the XVI overlay */
	int width;
	/* height in pixel of the XVI overlay */
	int height;
	/* pointer to XVI overlay */
	const byte *out = R_GetXVIMap(&width, &height);
	const float heightPerDegree = height / 180.0f;
	const float widthPerDegree = width / 360.0f;
	/* current position (in latitude / longitude) */
	vec2_t currentPos;
	/* parameter used to normalize nation XVI level.
	 * decrease this factor to increase XVI level per nation */
	const float AREA_FACTOR = 650.0f;
	/* area used to normalized XVI infection level for each nation.
	 * depend on overlay size so that if we change resolution of
	 * overlay it doesn't impact nation XIInfection */
	const float normalizingArea = width * height / AREA_FACTOR;

	/* No need to update XVI levels if the overlay didn't change */
	if (!xviNationInfectionNeedsUpdate)
		return;

	/* Initialize array */
	for (nationIdx = 0; nationIdx < ccs.numNations; nationIdx++)
		xviInfection[nationIdx] = 0;

	if (!out)
		return;

	for (y = 0; y < height; y++) {
		int x;
		int sum[MAX_NATIONS];
		const byte* previousNationColor;
		const nation_t* nation;

		memset(sum, 0, sizeof(sum));

		Vector2Set(currentPos, 180.0f, 90.0f - y / heightPerDegree);
		previousNationColor = MAP_GetColor(currentPos, MAPTYPE_NATIONS);
		nation = MAP_GetNation(currentPos);

		for (x = 0; x < width; x++) {
			const byte* nationColor;
			currentPos[0] = 180.0f - x / widthPerDegree;
			nationColor = MAP_GetColor(currentPos, MAPTYPE_NATIONS);
			if (nationColor != previousNationColor) {
				previousNationColor = nationColor;
				nation = MAP_GetNation(currentPos);
			}
			if (nation) {
				const int xviLevel = R_GetXVILevel(x, y);
				if (xviLevel > 0)
					sum[nation->idx] += xviLevel;
			}
		}
		/* divide the total XVI infection by the area of a pixel
		 * because pixel are smaller as you go closer from the pole */
		for (nationIdx = 0; nationIdx < ccs.numNations; nationIdx++)
			xviInfection[nationIdx] += ((float) sum[nationIdx]) / (cos(torad * currentPos[1]) * normalizingArea);
	}

	/* copy the new values of XVI infection level into nation array */
	for (nationIdx = 0; nationIdx < ccs.numNations; nationIdx++) {
		nation_t* nation = &ccs.nations[nationIdx];
		nation->stats[0].xviInfection = ceil(xviInfection[nation->idx]);
	}

	xviNationInfectionNeedsUpdate = qfalse;
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
 * @brief Returns @c true if the tech for the xvi event is already researched.
 * If this tech is already researched the aliens will spread their XVI - but it
 * does not mean that the Phalanx can also do anything against it or is even
 * able to the map of XVI
 * @see css.XVIShowMap
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
