/**
 * @file cp_time.c
 * @brief Campaign geoscape time code
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
#include "../cl_game.h"
#include "cp_time.h"


typedef struct gameLapse_s {
	const char *name;
	int scale;
	int type;
} gameLapse_t;

#define LAPSETYPE_GEOSCAPE    1       /**< The gameLapse_t type flag for geoscape */
#define LAPSETYPE_COMBATZOOM  2       /**< The gameLapse_t type flag for combat zoom */
#define LAPSETYPE_ALL         3       /**< The gameLapse_t type flag for both geoscape and combat zoom */


#define NUM_TIMELAPSE 11

/** @brief The possible geoscape time intervalls */
static const gameLapse_t lapse[NUM_TIMELAPSE] = {
	{N_("stopped"), 0, LAPSETYPE_GEOSCAPE},
	{N_("5 sec"), 5, LAPSETYPE_ALL},
	{N_("10 sec"), 10, LAPSETYPE_COMBATZOOM},
	{N_("1 min"), 60, LAPSETYPE_COMBATZOOM},
	{N_("5 mins"), 5 * 60, LAPSETYPE_ALL},
	{N_("10 mins"), 10 * 60, LAPSETYPE_COMBATZOOM},
	{N_("20 mins"), SECONDS_PER_HOUR / 3, LAPSETYPE_GEOSCAPE},
	{N_("1 hour"), SECONDS_PER_HOUR, LAPSETYPE_GEOSCAPE},
	{N_("12 hours"), 12 * SECONDS_PER_HOUR, LAPSETYPE_GEOSCAPE},
	{N_("1 day"), 24 * SECONDS_PER_HOUR, LAPSETYPE_GEOSCAPE},
	{N_("5 days"), 5 * SECONDS_PER_DAY, LAPSETYPE_GEOSCAPE}
};

/** @todo move into ccs_t - and also save this? needed? */
static int gameLapse;

/**
 * @brief Gets the maximum game lapse array position that matches the given lapse type.
 * @param[in] type The game lapse type.
 * @return The Maximum game lapse array position.
 */
static int CP_MaxGameLapseForType (int type)
{
	int idx;

	for (idx = NUM_TIMELAPSE - 1; idx >= 0; idx--) {
		if ((lapse[idx].type & type) == type) {
			break;
		}
	}
	return idx;
}

/**
 * @brief Gets the minimum game lapse array position that matches the given lapse type.
 * @param[in] type The game lapse type.
 * @return The Minimum game lapse array position.
 */
static int CP_MinGameLapseForType (int type)
{
	int idx;

	for (idx = 0; idx < NUM_TIMELAPSE; idx++) {
		if ((lapse[idx].type & type) == type) {
			break;
		}
	}
	return idx;
}

/**
 * @brief Ensures that the current gameLapse setting is valid for the specified type.
 * If it isn't then it sets it to the minimum for that type.
 * @param[in] type The game lapse type.
 */
static void CP_EnsureValidGameLapseForType (int type)
{
	if ((lapse[gameLapse].type & type) != type) {
		gameLapse = CP_MinGameLapseForType(type);
		CL_UpdateTime();
	}
}

/**
 * @brief Ensures that the current gameLapse setting is valid for combat zoom.
 * If it isn't then it sets it to the minimum for combat zoom.
 */
void CL_EnsureValidGameLapseForCombatZoom (void)
{
	CP_EnsureValidGameLapseForType(LAPSETYPE_COMBATZOOM);
}

/**
 * @brief Ensures that the current gameLapse setting is valid for the geoscape.
 * If it isn't then it sets it to the minimum for the geoscape.
 */
void CL_EnsureValidGameLapseForGeoscape (void)
{
	CP_EnsureValidGameLapseForType(LAPSETYPE_GEOSCAPE);
}

/**
 * @brief Updates date/time and timescale (=timelapse) on the geoscape menu
 * @sa SAV_GameLoad
 * @sa CL_CampaignRun
 */
void CL_UpdateTime (void)
{
	dateLong_t date;
	CL_DateConvertLong(&ccs.date, &date);

	/* Update the timelapse text */
	if (gameLapse >= 0 && gameLapse < NUM_TIMELAPSE) {
		Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
		gd.gameTimeScale = lapse[gameLapse].scale;
		Cvar_SetValue("mn_timelapse_id", gameLapse);
	}

	/* Update the date */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%i %s %02i"), date.year, Date_GetMonthName(date.month - 1), date.day);
	Cvar_Set("mn_mapdate", mn.messageBuffer);

	/* Update the time. */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%02i:%02i"), date.hour, date.min);
	Cvar_Set("mn_maptime", mn.messageBuffer);
}

/**
 * @brief Stop game time speed
 */
void CL_GameTimeStop (void)
{
	const int lapseType = gd.combatZoomedUFO ? LAPSETYPE_COMBATZOOM : LAPSETYPE_GEOSCAPE;

	if (!GAME_IsCampaign())
		return;

	if ((lapse[0].type & lapseType) != lapseType)
		return;

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (!CL_OnBattlescape()) {
		gameLapse = 0;
	}

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}


/**
 * @brief Decrease game time speed
 *
 * Decrease game time speed - only works when there is already a base available
 */
void CL_GameTimeSlow (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const int lapseType = gd.combatZoomedUFO ? LAPSETYPE_COMBATZOOM : LAPSETYPE_GEOSCAPE;
	const int minGameLapse = CP_MinGameLapseForType(lapseType);

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == minGameLapse)
		return;

	assert(gameLapse >= minGameLapse);

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && (!Q_strncmp(menu->name, "map", 3) || !Q_strncmp(menu->name, "airfight", 8))) {
		gameLapse--;
		while ((lapse[gameLapse].type & lapseType) != lapseType) {
			if (gameLapse <= minGameLapse) {
				gameLapse = minGameLapse;
				break;
			} else {
				gameLapse--;
			}
		}
	}

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}

/**
 * @brief Increase game time speed
 *
 * Increase game time speed - only works when there is already a base available
 */
void CL_GameTimeFast (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const int lapseType = gd.combatZoomedUFO ? LAPSETYPE_COMBATZOOM : LAPSETYPE_GEOSCAPE;
	const int maxGameLapse = CP_MaxGameLapseForType(lapseType);

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == maxGameLapse)
		return;

	assert(gameLapse <= maxGameLapse);

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && (!Q_strncmp(menu->name, "map", 3) || !Q_strncmp(menu->name, "airfight", 8))) {
		gameLapse++;
		while ((lapse[gameLapse].type & lapseType) != lapseType) {
			if (gameLapse >= maxGameLapse) {
				gameLapse = maxGameLapse;
				break;
			} else {
				gameLapse++;
			}
		}
	}

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}

/**
 * @brief Set game time speed
 * @param[in] gameLapseValue The value to set the game time to.
 * @note
 */
void CL_SetGameTime (int gameLapseValue)
{
	if (gameLapseValue == gameLapse)
		return;

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == NUM_TIMELAPSE - 1)
		return;

	assert(gameLapse < NUM_TIMELAPSE);

	gameLapse = gameLapseValue;

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}


/**
 * @brief Set a new time game from id
 * @sa CL_SetGameTime
 * @sa lapse
 */
void CL_SetGameTime_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <timeid>\n", Cmd_Argv(0));
		return;
	}
	CL_SetGameTime(atoi(Cmd_Argv(1)));
}

/**
 * @brief Check wheter given date and time is later than current date.
 * @param[in] now Current date.
 * @param[in] compare Date to compare.
 * @return True if current date is later than given one.
 */
qboolean Date_LaterThan (date_t now, date_t compare)
{
	if (now.day > compare.day)
		return qtrue;
	if (now.day < compare.day)
		return qfalse;
	if (now.sec > compare.sec)
		return qtrue;
	return qfalse;
}

/**
 * @brief Add two dates and return the result.
 * @param[in] a First date.
 * @param[in] b Second date.
 * @return The result of adding date_ b to date_t a.
 */
date_t Date_Add (date_t a, date_t b)
{
	a.sec += b.sec;
	a.day += (a.sec / SECONDS_PER_DAY) + b.day;
	a.sec %= SECONDS_PER_DAY;
	return a;
}

/**
 * @brief Return a random relative date which lies between a lower and upper limit.
 * @param[in] minFrame Minimal date.
 * @param[in] maxFrame Maximal date.
 * @return A date value between minFrame and maxFrame.
 */
date_t Date_Random (date_t minFrame, date_t maxFrame)
{
	maxFrame.sec = max(minFrame.day * SECONDS_PER_DAY + minFrame.sec,
		(maxFrame.day * SECONDS_PER_DAY + maxFrame.sec) * frand());

	maxFrame.day = maxFrame.sec / SECONDS_PER_DAY;
	maxFrame.sec = maxFrame.sec % SECONDS_PER_DAY;
	return maxFrame;
}

/**
 * @brief Returns the monatname to the given month index
 * @param[in] month The month index - [0-11]
 * @return month name as char*
 */
const char *Date_GetMonthName (int month)
{
	switch (month) {
	case 0:
		return _("Jan");
	case 1:
		return _("Feb");
	case 2:
		return _("Mar");
	case 3:
		return _("Apr");
	case 4:
		return _("May");
	case 5:
		return _("Jun");
	case 6:
		return _("Jul");
	case 7:
		return _("Aug");
	case 8:
		return _("Sep");
	case 9:
		return _("Oct");
	case 10:
		return _("Nov");
	case 11:
		return _("Dec");
	default:
		return "Error";
	}
}
