/**
 * @file
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

#pragma once

class dbuffer;

/** @brief CL_ParseEvent timers and vars */
typedef struct eventTiming_s {
	int nextTime;	/**< time when the next event should be executed */
	int shootTime;	/**< time when the shoot was fired */
	int impactTime;	/**< time when the shoot hits the target. This is used to delay some events in case the
					 * projectile needs some time to reach its target. */
	bool parsedDeath;	/**< extra delay caused by death - @sa @c impactTime */
	bool parsedShot;	/** delay as caused by shot */
} eventTiming_t;

/**
 * @brief Struct that defines one particular event with all its callbacks and data.
 */
typedef struct eventRegister_s {
	/**
	 * @brief The type of this event
	 */
	const event_t type;
	/**
	 * @brief the name of this event (e.g. for logs)
	 */
	const char* name;
	/**
	 * @brief The format string that is used to write and parse this event
	 */
	const char* formatString;
	/**
	 * @brief Callback that is executing the event
	 * @param self A pointer to this struct
	 * @param msg The buffer with the event data
	 */
	void (*eventCallback)(const struct eventRegister_s* self, dbuffer* msg);
	/**
	 * @brief Callback that is returning the time that is needed to execute this event
	 * @param self A pointer to this struct
	 * @param msg The buffer with the event data
	 * @param eventTiming The delta time value
	 */
	int (*timeCallback)(const struct eventRegister_s* self, dbuffer* msg, eventTiming_t* eventTiming);

	/**
	 * @brief Called to determine if this event is ok to run at this point. Should check any
	 * conflicts with other ongoing events (see @c LE_LOCKED ).
	 * @return @c true if OK to run, @c false if not.
	 */
	bool (*eventCheck)(const struct eventRegister_s* self, const dbuffer* msg);
} eventRegister_t;

const eventRegister_t* CL_GetEvent(const event_t eType);
int CL_GetNextTime(const eventRegister_t* event, eventTiming_t* eventTiming, int nextTime);
int CL_GetStepTime(const eventTiming_t* eventTiming, const le_t* le, int step);
const char* CL_ConvertSoundFromEvent(char* sound, size_t size);
