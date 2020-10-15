/**
 * @file
 * @brief Header file for Date/Time management
 */

/*
Copyright (C) 2002-2020 UFO: Alien Invasion.

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

#pragma once

/* Time information. */
/**
 * @brief Engine-side time information in the game.
 * @note Use this in your custom structs that need to get saved or sent over the network.
 * @sa dateLong_t	For runtime use (human readable).
 * @sa CP_DateConvertLong
 */
typedef struct date_s {
	int day;	/**< Number of elapsed days since 1st january of year 0 */
	int sec;	/**< Number of elapsed seconds since the beginning of current day */
} date_t;

/* Time Constants */
#define DAYS_PER_YEAR		365
#define DAYS_PER_YEAR_AVG	365.25
/** DAYS_PER_MONTH -> @sa monthLength[] array in campaign.c */
#define MONTHS_PER_YEAR		12
#define SEASONS_PER_YEAR	4
#define SECONDS_PER_DAY		86400	/**< (24 * 60 * 60) */
#define SECONDS_PER_HOUR	3600	/**< (60 * 60) */
#define SECONDS_PER_MINUTE	60		/**< (60) */
#define MINUTES_PER_HOUR	60		/**< (60) */
#define HOURS_PER_DAY		24
