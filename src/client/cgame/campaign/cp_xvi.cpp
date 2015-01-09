/**
 * @file
 * @brief Campaign XVI code
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_overlay.h"
#include "cp_geoscape.h"
#include "cp_xvi.h"
#include "save/save_xvi.h"

/** @brief technology for XVI event */
static technology_t* rsAlienXVI;

/** @brief boolean used to know if each nation XVI level should be updated this day. */
static bool xviNationInfectionNeedsUpdate = false;

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
	if (!CP_IsXVIStarted())
		return;

	CP_ChangeXVILevel(pos, XVI_FACTOR);

	xviNationInfectionNeedsUpdate = true;
}

/**
 * @brief Reduce XVI everywhere.
 * @note This is called daily.
 */
void CP_ReduceXVIEverywhere (void)
{
	if (!CP_IsXVIStarted())
		return;

	/* Only decrease XVI if given days has passed */
	if (ccs.date.day % XVI_DECREASE_DAYS)
		return;

	CP_DecreaseXVILevelEverywhere();

	xviNationInfectionNeedsUpdate = true;
}

/**
 * @brief Update xviInfection value for each nation, using the XVI overlay.
 * @note should be executed after all daily event that could change XVI overlay
 */
void CP_UpdateNationXVIInfection (void)
{
	/* No need to update XVI levels if the overlay didn't change */
	if (!xviNationInfectionNeedsUpdate)
		return;

	/* width in pixel of the XVI overlay */
	int width;
	/* height in pixel of the XVI overlay */
	int height;
	CP_GetXVIMapDimensions(&width, &height);

	const float heightPerDegree = height / 180.0f;
	const float widthPerDegree = width / 360.0f;
	/* parameter used to normalize nation XVI level.
	 * decrease this factor to increase XVI level per nation */
	const float AREA_FACTOR = 650.0f;
	/* area used to normalized XVI infection level for each nation.
	 * depend on overlay size so that if we change resolution of
	 * overlay it doesn't impact nation XIInfection */
	const float normalizingArea = width * height / AREA_FACTOR;

	/* temporary array to store the XVI levels */
	float xviInfection[MAX_NATIONS];
	/* Initialize array */
	OBJZERO(xviInfection);

	for (int y = 0; y < height; y++) {
		int sum[MAX_NATIONS];
		const byte* previousNationColor;
		const nation_t* nation;
		/* current position (in latitude / longitude) */
		vec2_t currentPos;

		OBJZERO(sum);

		Vector2Set(currentPos, 180.0f, 90.0f - y / heightPerDegree);
		previousNationColor = GEO_GetColor(currentPos, MAPTYPE_NATIONS, nullptr);
		nation = GEO_GetNation(currentPos);

		for (int x = 0; x < width; x++) {
			const byte* nationColor;
			currentPos[0] = 180.0f - x / widthPerDegree;
			nationColor = GEO_GetColor(currentPos, MAPTYPE_NATIONS, nullptr);
			if (!VectorCompare(nationColor, previousNationColor)) {
				previousNationColor = nationColor;
				nation = GEO_GetNation(currentPos);
			}
			if (nation) {
				const int xviLevel = CP_GetXVILevel(x, y);
				if (xviLevel > 0)
					sum[nation->idx] += xviLevel;
			}
		}
		/* divide the total XVI infection by the area of a pixel
		 * because pixel are smaller as you go closer from the pole */
		for (int nationIdx = 0; nationIdx < ccs.numNations; nationIdx++)
			xviInfection[nationIdx] += ((float) sum[nationIdx]) / (cos(torad * currentPos[1]) * normalizingArea);
	}

	/* copy the new values of XVI infection level into nation array */
	for (int nationIdx = 0; nationIdx < ccs.numNations; nationIdx++) {
		nation_t* nation = NAT_GetNationByIDX(nationIdx);
		nation->stats[0].xviInfection = ceil(xviInfection[nation->idx]);
	}

	xviNationInfectionNeedsUpdate = false;
}

/**
 * @brief Return the average XVI rate
 * @note XVI = eXtraterrestial Viral Infection
 */
int CP_GetAverageXVIRate (void)
{
	assert(ccs.numNations);

	/* check for XVI infection rate */
	int xviRate = 0;
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		xviRate += stats->xviInfection;
	}
	xviRate /= ccs.numNations;
	return xviRate;
}

/**
 * @brief Spread XVI for each mission that needs to be daily spread.
 * @note Daily called
 */
void CP_SpreadXVI (void)
{
	/* don't check if XVI spreading didn't start yet */
	if (!CP_IsXVIStarted())
		return;

	MIS_Foreach(mission) {
		if (mission->stage == STAGE_SPREAD_XVI)
			CP_SpreadXVIAtPos(mission->pos);
	}
}

/**
 * @brief Returns @c true if the XVI effect should be visible to the player
 */
bool CP_IsXVIVisible (void)
{
	return RS_IsResearched_ptr(rsAlienXVI);
}

void CP_XVIInit (void)
{
	rsAlienXVI = RS_GetTechByID(XVI_EVENT_NAME);
	if (!rsAlienXVI)
		cgi->Com_Error(ERR_DROP, "CP_XVIInit: Could not find tech definition for " XVI_EVENT_NAME);
}

/**
 * @brief XVI map saving callback
 * @note Only save transparency
 * @sa Savegame callback
 * @sa SAV_InitXML
 * @sa XVI_LoadXML
 */
bool XVI_SaveXML (xmlNode_t* p)
{
	int width;
	int height;

	CP_GetXVIMapDimensions(&width, &height);

	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_XVI_XVI);
	cgi->XML_AddInt(n, SAVE_XVI_WIDTH, width);
	cgi->XML_AddInt(n, SAVE_XVI_HEIGHT, height);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const int xviLevel = CP_GetXVILevel(x, y);
			/* That saves many bytes in the savegame */
			if (xviLevel > 0) {
				xmlNode_t* s = cgi->XML_AddNode(n, SAVE_XVI_ENTRY);
				cgi->XML_AddInt(s, SAVE_XVI_X, x);
				cgi->XML_AddInt(s, SAVE_XVI_Y, y);
				cgi->XML_AddInt(s, SAVE_XVI_LEVEL, xviLevel);
			}
		}
	}
	return true;
}

/**
 * @brief Load the XVI map from the savegame.
 * @sa Savegame callback
 * @sa SAV_InitXML
 * @sa XVI_SaveXML
 */
bool XVI_LoadXML (xmlNode_t* p)
{
	int width, height;
	xmlNode_t* s;
	xmlNode_t* n = cgi->XML_GetNode(p, SAVE_XVI_XVI);
	/* If there is no XVI, it will not be loaded */
	if (!n) {
		CP_InitializeXVIOverlay();
		return true;
	}

	width = cgi->XML_GetInt(n, SAVE_XVI_WIDTH, 0);
	height = cgi->XML_GetInt(n, SAVE_XVI_HEIGHT, 0);

	for (s = cgi->XML_GetNode(n, SAVE_XVI_ENTRY); s; s = cgi->XML_GetNextNode(s, n, SAVE_XVI_ENTRY)) {
		const int x = cgi->XML_GetInt(s, SAVE_XVI_X, 0);
		const int y = cgi->XML_GetInt(s, SAVE_XVI_Y, 0);
		const int level = cgi->XML_GetInt(s, SAVE_XVI_LEVEL, 0);

		if (x >= 0 && x < width && y >= 0 && y <= height)
			CP_SetXVILevel(x, y, level);
	}
	return true;
}

/**
 * @brief Start XVI spreading in campaign.
 * @note This is called when 'a new twist' technology is discovered.
 */
void CP_StartXVISpreading_f (void)
{
	int numAlienBases;
	const campaign_t* campaign = ccs.curCampaign;

	ccs.startXVI = true;

	/* Spawn a few alien bases depending on difficulty level */
	if (campaign->difficulty > 0)
		numAlienBases = 3;
	else if (campaign->difficulty < 0)
		numAlienBases = 1;
	else
		numAlienBases = 2;

	for (int i = 0; i < numAlienBases; i++)
		CP_CreateNewMission(INTERESTCATEGORY_BUILDING, false);
}

/**
 * @brief This will hide or show the geoscape button for handling the xvi overlay map
 */
void CP_UpdateXVIMapButton (void)
{
	cgi->Cvar_SetValue("mn_xvimap", CP_IsXVIVisible());
}
