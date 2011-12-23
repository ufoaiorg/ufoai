/**
 * @file cp_time.c
 * @brief Campaign geoscape time code
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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_time.h"

static const int monthLength[MONTHS_PER_YEAR] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

typedef struct gameLapse_s {
	const char *name;
	int scale;
} gameLapse_t;

#define NUM_TIMELAPSE 8

/** @brief The possible geoscape time intervalls */
static const gameLapse_t lapse[NUM_TIMELAPSE] = {
	{N_("stopped"), 0},
	{N_("5 sec"), 5},
	{N_("5 mins"), 5 * 60},
	{N_("20 mins"), SECONDS_PER_HOUR / 3},
	{N_("1 hour"), SECONDS_PER_HOUR},
	{N_("12 hours"), 12 * SECONDS_PER_HOUR},
	{N_("1 day"), 24 * SECONDS_PER_HOUR},
	{N_("5 days"), 5 * SECONDS_PER_DAY}
};
CASSERT(lengthof(lapse) == NUM_TIMELAPSE);

/**
 * @brief Converts a number of second into a char to display.
 * @param[in] second Number of second.
 * @todo Abstract the code into an extra function (DateConvertSeconds?) see also CP_DateConvertLong
 */
const char* CP_SecondConvert (int second)
{
	static char buffer[6];
	const int hour = second / SECONDS_PER_HOUR;
	const int min = (second - hour * SECONDS_PER_HOUR) / 60;
	Com_sprintf(buffer, sizeof(buffer), "%2i:%02i", hour, min);
	return buffer;
}

/**
 * @brief Converts a date from the engine in a (longer) human-readable format.
 * @note The seconds from "date" are ignored here.
 * @note The function always starts calculation from Jan. and also catches new years.
 * @param[in] date Contains the date to be converted.
 * @param[out] dateLong The converted date.
  */
void CP_DateConvertLong (const date_t * date, dateLong_t * dateLong)
{
	byte i;
	int d;
	size_t length = lengthof(monthLength);

	/* Get the year */
	dateLong->year = date->day / DAYS_PER_YEAR;

	/* Get the days in the year. */
	d = date->day % DAYS_PER_YEAR;

	/* Subtract days until no full month is left. */
	for (i = 0; i < length; i++) {
		if (d < monthLength[i])
			break;
		d -= monthLength[i];
	}

	dateLong->day = d + 1;
	dateLong->month = i + 1;
	dateLong->hour = date->sec / SECONDS_PER_HOUR;
	dateLong->min = (date->sec - dateLong->hour * SECONDS_PER_HOUR) / 60;
	dateLong->sec = date->sec - dateLong->hour * SECONDS_PER_HOUR - dateLong->min * 60;

	assert(dateLong->month >= 1 && dateLong->month <= MONTHS_PER_YEAR);
	assert(dateLong->day >= 1 && dateLong->day <= monthLength[i]);
}

/**
 * @brief Updates date/time and timescale (=timelapse) on the geoscape menu
 * @sa SAV_GameLoad
 * @sa CP_CampaignRun
 */
void CP_UpdateTime (void)
{
	dateLong_t date;
	CP_DateConvertLong(&ccs.date, &date);

	/* Update the timelapse text */
	if (ccs.gameLapse >= 0 && ccs.gameLapse < NUM_TIMELAPSE) {
		Cvar_Set("mn_timelapse", _(lapse[ccs.gameLapse].name));
		ccs.gameTimeScale = lapse[ccs.gameLapse].scale;
		Cvar_SetValue("mn_timelapse_id", ccs.gameLapse);
	}

	/* Update the date */
	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%i %s %02i"), date.year, Date_GetMonthName(date.month - 1), date.day);
	Cvar_Set("mn_mapdate", cp_messageBuffer);

	/* Update the time. */
	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%02i:%02i"), date.hour, date.min);
	Cvar_Set("mn_maptime", cp_messageBuffer);
}

/**
 * @brief Stop game time speed
 */
void CP_GameTimeStop (void)
{
	/* don't allow time scale in tactical mode - only on the geoscape */
	if (!cp_missiontest->integer && CP_OnGeoscape())
		ccs.gameLapse = 0;

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CP_UpdateTime();
}

/**
 * @brief Check if time is stopped
 */
qboolean CP_IsTimeStopped (void)
{
	return !ccs.gameLapse;
}

/**
 * Time scaling is only allowed when you are on the geoscape and when you had at least one base built.
 */
static qboolean CP_AllowTimeScale (void)
{
	/* check the stats value - already build bases might have been destroyed
	 * so the B_GetCount() values is pointless here */
	if (!ccs.campaignStats.basesBuilt)
		return qfalse;

	return CP_OnGeoscape();
}

/**
 * @brief Decrease game time speed
 */
void CP_GameTimeSlow (void)
{
	/* don't allow time scale in tactical mode - only on the geoscape */
	if (CP_AllowTimeScale()) {
		if (ccs.gameLapse > 0)
			ccs.gameLapse--;
		/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
		CP_UpdateTime();
	}
}

/**
 * @brief Increase game time speed
 */
void CP_GameTimeFast (void)
{
	/* don't allow time scale in tactical mode - only on the geoscape */
	if (CP_AllowTimeScale()) {
		if (ccs.gameLapse < NUM_TIMELAPSE)
			ccs.gameLapse++;
		/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
		CP_UpdateTime();
	}
}

/**
 * @brief Set game time speed
 * @param[in] gameLapseValue The value to set the game time to.
 */
static void CP_SetGameTime (int gameLapseValue)
{
	if (gameLapseValue == ccs.gameLapse)
		return;

	/* check the stats value - already build bases might have been destroyed
	 * so the B_GetCount() values is pointless here */
	if (!ccs.campaignStats.basesBuilt)
		return;

	if (gameLapseValue < 0 || gameLapseValue >= NUM_TIMELAPSE)
		return;

	ccs.gameLapse = gameLapseValue;

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CP_UpdateTime();
}


/**
 * @brief Set a new time game from id
 * @sa CL_SetGameTime
 * @sa lapse
 */
void CP_SetGameTime_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <timeid>\n", Cmd_Argv(0));
		return;
	}
	CP_SetGameTime(atoi(Cmd_Argv(1)));
}

/**
 * @brief Convert a date_t date to seconds
 * @param[in] date The date in date_t format
 * @return the date in seconds
 */
int Date_DateToSeconds (const date_t *date)
{
	return date->day * 86400 + date->sec;
}

/**
 * @brief Check whether the given date and time is later than current date.
 * @param[in] now Current date.
 * @param[in] compare Date to compare.
 * @return True if current date is later than given one.
 */
qboolean Date_LaterThan (const date_t *now, const date_t *compare)
{
	if (now->day > compare->day)
		return qtrue;
	if (now->day < compare->day)
		return qfalse;
	if (now->sec > compare->sec)
		return qtrue;
	return qfalse;
}

/**
 * @brief Checks whether a given date is equal or earlier than the current campaign date
 * @param date The date to check
 * @return @c true if the given date is equal or earlier than the current campaign date, @c false otherwise
 */
qboolean Date_IsDue (const date_t *date)
{
	if (date->day < ccs.date.day)
		return qtrue;

	else if (date->day == ccs.date.day && date->sec <= ccs.date.sec)
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
 * @brief Substract the second date from the first and return the result.
 * @param[in] a First date.
 * @param[in] b Second date.
 */
date_t Date_Substract (date_t a, date_t b)
{
	a.day -= b.day;
	a.sec -= b.sec;
	if (a.sec < 0) {
		a.day--;
		a.sec += SECONDS_PER_DAY;
	}
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
