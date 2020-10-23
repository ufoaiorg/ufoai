/**
 * @file
 * @brief DateTime class methods
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

#include "DateTime.h"

/* Constructors */

/**
 * @brief Construct for DateTime class from date as days and time as seconds
 * @param[in] day Absolute number of days
 * @param[in] second Absolute number of seconds
 */
DateTime::DateTime (int day, int second) :
	day(day),
	second(second)
{
	reCalculate();
}

/* public methods */

/**
 * @brief Return the date part of the DateTime as days
 */
int DateTime::getDateAsDays() const
{
	return day;
}

/**
 * @brief Return the time part of the DateTime as seconds
 */
int DateTime::getTimeAsSeconds() const
{
	return second;
}

/* Operators */

/**
 * @brief Adds a DateTime (duration) to the left DateTime object
 * @param[in] DateTime& Reference to the date/time duration to add
 * @return A reference to the modified DateTime object (lvalue)
 */
DateTime& DateTime::operator+=(const DateTime& other)
{
	this->day += other.getDateAsDays();
	this->second += other.getTimeAsSeconds();
	this->reCalculate();
	return *this;
}

/**
 * @brief Calcluate the summary of two DateTime objects in a new one
 * @param[in] DateTime& Reference to the date/time duration to add
 * @return A new DateTime object with the result
 */
DateTime DateTime::operator+ (const DateTime& other) const
{
	return DateTime(*this) += other;
}

/**
 * @brief Substracts a DateTime (duration) from the left DateTime object
 * @param[in] DateTime& Reference to the date/time duration to substract
 * @return A reference to the modified DateTime object (lvalue)
 */
DateTime& DateTime::operator-=(const DateTime& other)
{
	this->day -= other.getDateAsDays();
	this->second -= other.getTimeAsSeconds();
	this->reCalculate();
	return *this;
}

/**
 * @brief Calcluate the difference of two DateTime objects in a new one
 * @param[in] DateTime& Reference to the date/time duration to substract
 * @return A new DateTime object with the result
 */
DateTime DateTime::operator- (const DateTime& other) const
{
	return DateTime(*this) -= other;
}

/**
 * @brief Decides whether two DateTimes are equal
 */
bool DateTime::operator==(const DateTime& other) const
{
	return this->day == other.getDateAsDays() && this->second == other.getTimeAsSeconds();
}

/**
 * @brief Decides whether two DateTimes are different
 */
bool DateTime::operator!=(const DateTime& other) const
{
	return !(*this == other);
}

/**
 * @brief Decides if the left DateTime is earlier than the right one
 */
bool DateTime::operator<(const DateTime& other) const
{
	return (this->day < other.getDateAsDays()) || (this->day == other.getDateAsDays() && this->second < other.getTimeAsSeconds());
}

/**
 * @brief Decides if the left DateTime is earlier or equal to the right one
 */
bool DateTime::operator<=(const DateTime& other) const
{
	return (*this == other) || (*this < other);
}

/**
 * @brief Decides if the left DateTime is later than the right one
 */
bool DateTime::operator>(const DateTime& other) const
{
	return (this->day > other.getDateAsDays()) || (this->day == other.getDateAsDays() && this->second > other.getTimeAsSeconds());
}

/**
 * @brief Decides if the left DateTime is later or equal to the right one
 */
bool DateTime::operator>=(const DateTime& other) const
{
	return (*this == other) || (*this > other);
}

/* Internal methods */

/**
 * @brief Handle over- and underflowing date/time parts
 */
void DateTime::reCalculate (void)
{
	int day_difference = int(second / SECONDS_PER_DAY);
	if (day_difference != 0) {
		day += day_difference;
		second -= day_difference * SECONDS_PER_DAY;
	}
	if (second < 0) {
		day--;
		second += SECONDS_PER_DAY;
	}
}

#if 0

#include "../shared/shared.h"
#include <math.h>

/* Public methods */
static const short DAYS_PER_MONTH[DateTime::MONTHS_PER_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
/**
 * @brief Calculate actual human readable date and time
 * @param[out] year Year part of the date
 * @param[out] month Month of the year (starting from 1)
 * @param[out] day Day of the month (starting from 1)
 * @param[out] hour Hour of the day (24 hours clock)
 * @param[out] minute Minute of the hour
 * @param[out] second Second of the minute
*/
void DateTime::getDateParts (int* year, short* month, short* day, short* hour, short* minute, short* second) const
{
	*year = int(this->day / DAYS_PER_YEAR);
	*month = 1;
	*day = 1 + this->day % DAYS_PER_YEAR;
	while ((*day) >= DAYS_PER_MONTH[(*month) - 1]) {
		(*day) -= DAYS_PER_MONTH[(*month) - 1];
		(*month)++;
	}

	(*hour) = this->second / SECONDS_PER_HOUR;
	(*minute) = (this->second - *hour * SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
	(*second) = this->second % SECONDS_PER_MINUTE;
}

/**
 * @brief Return the year part of the date
 */
int DateTime::getYear (void) const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	return year;
}

/**
 * @brief Return the month part of the date in 1-12 range
 */
int DateTime::getMonth (void) const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	return month + 1;
}

/**
 * @brief Return the dear part of the date in 1-31 range
 */
int DateTime::getDay (void) const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	return day + 1;
}

/**
 * @brief Return the hour part of the date
 */
int DateTime::getHour (void) const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	return hour;
}

/**
 * @brief Return the minute part of the date
 */
int DateTime::getMinute (void) const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	return minute;
}

/**
 * @brief Return the second part of the date
 */
int DateTime::getSecond (void) const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	return second;
}


/**
 * @brief Return a formatted date/time/duration string
 * @param[in] format Optional format string for the date/time/duration string
 * 	The following markers are supported:
 * 		%Y - 4+ digit full year
 * 		%m - Zero padded 2 digit month number (01-12)
 * 		%d - Zero padded 2 digit day of the month (01-31)
 * 		%H - Zero padded 2 digit hours (00-23)
 * 		%M - Zero padded 2 digit minutes (00-59)
 * 		%S - Zero padded 2 digit seconds (00-59)
 * 		%% - Percent sign
 * @return Pointer to a formatted date/time/duration string
 */
const char* DateTime::asString (const char* format = "%Y-%m-%d %H:%M:%S") const
{
	int year;
	short month;
	short day;
	short hour;
	short minute;
	short second;

	getDateParts(&year, &month, &day, &hour, &minute, &second);

	/* calculate memory need */
	int formatLength = strlen(format);
	int dateLength = 0;
	for (int i = 0; i < formatLength; i++) {
		/* regular characters */
		if (format[i] != '%') {
			dateLength++;
			continue;
		}

		/* macros */
		i++;
		/* check for invalid macros */
		if (i >= formatLength) {
			dateLength = -1;
			break;
		}

		switch (format[i]) {
		case '%':
			dateLength++;
			break;
		case 'Y':
			dateLength += log10(year) + 1;
			break;
		case 'm':
			dateLength += 2;
			break;
		case 'd':
			dateLength += 2;
			break;
		case 'H':
			dateLength += 2;
			break;
		case 'M':
			dateLength += 2;
			break;
		case 'S':
			dateLength += 2;
			break;
		default:
			dateLength = -1;
		}

		if (dateLength == -1)
			break;
	}

	static char string[64] = "";
	int stringIdx = 0;
	for (int i = 0; i < formatLength; i++) {
		if (format[i] != '%') {
			string[stringIdx++] = format[i];
			continue;
		}

		i++;
		switch (format[i]) {
		case '%':
			string[stringIdx++] = '%';
			break;
		case 'Y':
			Com_sprintf(string + stringIdx, dateLength - stringIdx, "%d", year);
			stringIdx += log10(year) + 1;
			break;
		case 'm':
			Com_sprintf(string + stringIdx, dateLength - stringIdx, "%02d", month);
			stringIdx += 2;
			break;
		case 'd':
			Com_sprintf(string + stringIdx, dateLength - stringIdx, "%02d", day);
			stringIdx += 2;
			break;
		case 'H':
			Com_sprintf(string + stringIdx, dateLength - stringIdx, "%02d", hour);
			stringIdx += 2;
			break;
		case 'M':
			Com_sprintf(string + stringIdx, dateLength - stringIdx, "%02d", minute);
			stringIdx += 2;
			break;
		case 'S':
			Com_sprintf(string + stringIdx, dateLength - stringIdx, "%02d", second);
			stringIdx += 2;
			break;
		default:
			break;
		}

	}
	string[stringIdx++] = '\0';

	return string;
}
#endif
