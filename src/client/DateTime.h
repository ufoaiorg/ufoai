/**
 * @file
 * @brief DateTime class definition
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

#pragma once

/**
 * @brief Class describing a point of time
 */
class DateTime
{
	public:
		/* basic time constants */
		static const short SECONDS_PER_MINUTE = 60;
		static const short MINUTES_PER_HOUR = 60;
		static const short HOURS_PER_DAY = 24;
		static const int DAYS_PER_YEAR = 365;
		static constexpr float DAYS_PER_YEAR_AVG = 365.25f;
		static const short MONTHS_PER_YEAR = 12;
		static const short SEASONS_PER_YEAR = 4;
		/* computed time constants*/
		static const short SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
		static const int SECONDS_PER_DAY = HOURS_PER_DAY * SECONDS_PER_HOUR;

		/* constructors */
		DateTime(int day = 0, int second = 0);

		/* public methods */
		int getDateAsDays() const;
		int getTimeAsSeconds() const;

		/* operators */
		DateTime& operator+=(const DateTime& other);
		DateTime operator+(const DateTime& other) const;
		DateTime& operator-=(const DateTime& other);
		DateTime operator-(const DateTime& other) const;

		bool operator==(const DateTime& other) const;
		bool operator!=(const DateTime& other) const;
		bool operator<(const DateTime& other) const;
		bool operator<=(const DateTime& other) const;
		bool operator>(const DateTime& other) const;
		bool operator>=(const DateTime& other) const;

	protected:
		/* internal data */
		int day;	/**< Number of day (starting with 0) */
		int second;	/**< Second of the minute */

		/* private methods */
		void reCalculate(void);
#if 0
		/* public methods */
		void getDateParts(int* year, short* month, short* day, short* hour, short* minute, short* second) const;
		int getYear(void) const;
		int getMonth(void) const;
		int getDay(void) const;
		int getHour(void) const;
		int getMinute(void) const;
		int getSecond(void) const;

		const char* asString(const char* format) const;
#endif
};
