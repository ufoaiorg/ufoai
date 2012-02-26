/**
 * @file e_main.h
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

#ifndef E_MAIN_H_
#define E_MAIN_H_

/** @brief CL_ParseEvent timers and vars */
typedef struct eventTiming_s {
	int nextTime;	/**< time when the next event should be executed */
	int shootTime;	/**< time when the shoot was fired */
	int impactTime;	/**< time when the shoot hits the target. This is used to delay some events in case the
					 * projectile needs some time to reach its target. */
	qboolean parsedDeath;	/**< extra delay caused by death - @sa @c impactTime */
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
	const char *name;
	/**
	 * @brief The format string that is used to write and parse this event
	 */
	const char *formatString;
	/**
	 * @brief Callback that is executing the event
	 * @param self A pointer to this struct
	 * @param msg The buffer with the event data
	 */
	void (*eventCallback)(const struct eventRegister_s *self, struct dbuffer *msg);
	/**
	 * @brief Callback that is returning the time that is needed to execute this event
	 * @param self A pointer to this struct
	 * @param msg The buffer with the event data
	 * @param eventTiming The delta time value
	 */
	int (*timeCallback)(const struct eventRegister_s *self, struct dbuffer *msg, eventTiming_t *eventTiming);

	/**
	 * @brief Called to determine if this event is ok to run at this point. Should check any
	 * conflicts with other ongoing events (see @c le_t->locked ).
	 * @return @c true if OK to run, @c false if not.
	 */
	qboolean (*eventCheck)(const struct eventRegister_s *self, const struct dbuffer *msg);
} eventRegister_t;

const eventRegister_t *CL_GetEvent(const event_t eType);
int CL_GetNextTime(const eventRegister_t *event, eventTiming_t *eventTiming, int nextTime);

#endif
