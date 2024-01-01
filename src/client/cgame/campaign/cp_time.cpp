/**
 * @file
 * @brief Campaign geoscape time code
 */

/*
Copyright (C) 2002-2024 UFO: Alien Invasion.

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

#include "cp_time.h"
#include "../../DateTime.h"
#include "../../cl_shared.h"
#include "cp_campaign.h"

typedef struct gameLapse_s {
	const char* name;
	int scale;
} gameLapse_t;

#define NUM_TIMELAPSE 8

const int DAYS_PER_MONTH[DateTime::MONTHS_PER_YEAR] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/** @brief The possible geoscape time intervalls */
static const gameLapse_t lapse[NUM_TIMELAPSE] = {
	{N_("stopped"), 0},
	{N_("5 sec"), 5},
	{N_("5 mins"), 5 * DateTime::SECONDS_PER_MINUTE},
	{N_("20 mins"), 20 * DateTime::SECONDS_PER_MINUTE},
	{N_("1 hour"), DateTime::SECONDS_PER_HOUR},
	{N_("12 hours"), 12 * DateTime::SECONDS_PER_HOUR},
	{N_("1 day"), DateTime::SECONDS_PER_DAY},
	{N_("5 days"), 5 * DateTime::SECONDS_PER_DAY}
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
	const int hour = second / DateTime::SECONDS_PER_HOUR;
	const int min = (second - hour * DateTime::SECONDS_PER_HOUR) / 60;
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
void CP_DateConvertLong (const DateTime& date, dateLong_t* dateLong)
{
	/* Get the year */
	dateLong->year = date.getDateAsDays() / DateTime::DAYS_PER_YEAR;

	/* Get the days in the year. */
	int d = date.getDateAsDays() % DateTime::DAYS_PER_YEAR;

	/* Subtract days until no full month is left. */
	byte i;
	for (i = 0; i < DateTime::MONTHS_PER_YEAR; i++) {
		if (d < DAYS_PER_MONTH[i])
			break;
		d -= DAYS_PER_MONTH[i];
	}

	dateLong->day = d + 1;
	dateLong->month = i + 1;
	dateLong->hour = date.getTimeAsSeconds() / DateTime::SECONDS_PER_HOUR;
	dateLong->min = (date.getTimeAsSeconds() - dateLong->hour * DateTime::SECONDS_PER_HOUR) / 60;
	dateLong->sec = date.getTimeAsSeconds() - dateLong->hour * DateTime::SECONDS_PER_HOUR - dateLong->min * 60;

	assert(dateLong->month >= 1 && dateLong->month <= DateTime::MONTHS_PER_YEAR);
	assert(dateLong->day >= 1 && dateLong->day <= DAYS_PER_MONTH[i]);
}

/**
 * @brief Updates date/time and timescale (=timelapse) on the geoscape menu
 * @sa SAV_GameLoad
 * @sa CP_CampaignRun
 */
void CP_UpdateTime (void)
{
	dateLong_t date;
	CP_DateConvertLong(ccs.date, &date);

	/* Update the timelapse text */
	if (ccs.gameLapse >= 0 && ccs.gameLapse < NUM_TIMELAPSE) {
		cgi->Cvar_Set("mn_timelapse", "%s", _(lapse[ccs.gameLapse].name));
		ccs.gameTimeScale = lapse[ccs.gameLapse].scale;
		cgi->Cvar_SetValue("mn_timelapse_id", ccs.gameLapse);
	}

	/* Update the date */
	cgi->Cvar_Set("mn_mapdate", _("%i %s %02i"), date.year, Date_GetMonthName(date.month - 1), date.day);

	/* Update the time. */
	cgi->Cvar_Set("mn_maptime", _("%02i:%02i"), date.hour, date.min);
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
bool CP_IsTimeStopped (void)
{
	return !ccs.gameLapse;
}

/**
 * Time scaling is only allowed when you are on the geoscape and when you had at least one base built.
 */
static bool CP_AllowTimeScale (void)
{
	/* check the stats value - already build bases might have been destroyed
	 * so the B_GetCount() values is pointless here */
	if (!ccs.campaignStats.basesBuilt)
		return false;

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
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <timeid>\n", cgi->Cmd_Argv(0));
		return;
	}
	CP_SetGameTime(atoi(cgi->Cmd_Argv(1)));
}

/**
 * @brief Convert a date to seconds
 * @param[in] date The date in DateTime format
 * @return the date in seconds
 */
int Date_DateToSeconds (const DateTime& date)
{
	return date.getDateAsDays() * DateTime::SECONDS_PER_DAY + date.getTimeAsSeconds();
}

/**
 * @brief Return a random relative date which lies between a lower and upper limit.
 * @param[in] minFrame Minimal date.
 * @param[in] maxFrame Maximal date.
 * @return A date value between minFrame and maxFrame.
 */
DateTime Date_Random (const DateTime& minFrame, const DateTime& maxFrame)
{
	const int random = (maxFrame.getDateAsDays() * DateTime::SECONDS_PER_DAY + maxFrame.getTimeAsSeconds()) * frand();
	return DateTime(0, std::max(minFrame.getDateAsDays() * DateTime::SECONDS_PER_DAY + minFrame.getTimeAsSeconds(), random));
}

/**
 * @brief Returns the short monthame to the given month index
 * @param[in] month The month index - [0-11]
 * @return month name as char*
 */
const char* Date_GetMonthName (int month)
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
