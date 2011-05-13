/**
 * @file ui_timer.h
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

#ifndef CLIENT_UI_UI_TIMER_H
#define CLIENT_UI_UI_TIMER_H

struct uiNode_s;
struct uiTimer_s;

typedef void (*timerCallback_t)(struct uiNode_s *node, struct uiTimer_s *timer);

/**
 * @todo We can use void* for the owner type, and allow to use it outside nodes
 */
typedef struct uiTimer_s {
	struct uiTimer_s *next;	/**< next timer in the ordered list of active timers */
	struct uiTimer_s *prev;	/**< previous timer in the ordered list of active timers */
	int nextTime;				/**< next time we must call the callback function. Must node be edited, it used to sort linkedlist of timers */

	struct uiNode_s *owner;	/**< owner node of the timer */
	timerCallback_t callback;	/**< function called every delay */
	int calledTime;				/**< time we call the function. For the first call the value is 1 */

	int delay;					/**< delay in millisecond between each call of */
	void *userData;				/**< free to use data, not used by the core functions */
	qboolean isRunning;			/**< true if the timer is running */
} uiTimer_t;

uiTimer_t* UI_AllocTimer(struct uiNode_s *node, int firstDelay, timerCallback_t callback) __attribute__ ((warn_unused_result));
void UI_TimerStart(uiTimer_t *timer);
void UI_TimerStop(uiTimer_t *timer);
void UI_TimerRelease(uiTimer_t *timer);

void UI_HandleTimers(void);

#ifdef COMPILE_UNITTESTS
const uiTimer_t *UI_PrivateGetFirstTimer(void);
void UI_PrivateInsertTimerInActiveList(uiTimer_t* first, uiTimer_t* newTimer);
#endif

#endif
