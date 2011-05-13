/**
 * @file ui_timer.c
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

#include "../cl_shared.h"
#include "ui_nodes.h"
#include "ui_timer.h"

/**
 * @brief Number max of timer slot.
 */
#define UI_TIMER_SLOT_NUMBER 10

/**
 * @brief Timer slot. Only one.
 */
static uiTimer_t ui_timerSlots[UI_TIMER_SLOT_NUMBER];

/**
 * @brief First timer from the timer list.
 * This list is ordered from smaller to bigger nextTime value
 */
static uiTimer_t *ui_firstTimer;

/**
 * @brief Remove a timer from the active linked list
 * @note The function doesn't set to null next and previous attributes of the timer
 */
static inline void UI_RemoveTimerFromActiveList (uiTimer_t *timer)
{
	assert(timer >= ui_timerSlots && timer < ui_timerSlots + UI_TIMER_SLOT_NUMBER);
	if (timer->prev) {
		timer->prev->next = timer->next;
	} else {
		ui_firstTimer = timer->next;
	}
	if (timer->next) {
		timer->next->prev = timer->prev;
	}
}

/**
 * @brief Insert a timer in a sorted linked list of timers.
 * List are ordered from smaller to bigger nextTime value
 */
static void UI_InsertTimerInActiveList (uiTimer_t* first, uiTimer_t* newTimer)
{
	uiTimer_t* current = first;
	uiTimer_t* prev = NULL;

	/* find insert position */
	if (current != NULL) {
		prev = current->prev;
	}
	while (current) {
		if (newTimer->nextTime < current->nextTime)
			break;
		prev = current;
		current = current->next;
	}

	/* insert between previous and current */
	newTimer->prev = prev;
	newTimer->next = current;
	if (current != NULL) {
		current->prev = newTimer;
	}
	if (prev != NULL) {
		prev->next = newTimer;
	} else {
		ui_firstTimer = newTimer;
	}
}

/**
 * @brief Internal function to handle timers
 */
void UI_HandleTimers (void)
{
	/* is first element is out of date? */
	while (ui_firstTimer && ui_firstTimer->nextTime <= CL_Milliseconds()) {
		uiTimer_t *timer = ui_firstTimer;

		/* throw event */
		timer->calledTime++;
		timer->callback(timer->owner, timer);

		/* update the sorted list */
		if (timer->isRunning) {
			UI_RemoveTimerFromActiveList(timer);
			timer->nextTime += timer->delay;
			UI_InsertTimerInActiveList(timer->next, timer);
		}
	}
}

/**
 * @brief Allocate a new time for a node
 * @param[in] node node parent of the timer
 * @param[in] firstDelay millisecond delay to wait the callback
 * @param[in] callback callback function to call every delay
 */
uiTimer_t* UI_AllocTimer (uiNode_t *node, int firstDelay, timerCallback_t callback)
{
	uiTimer_t *timer = NULL;
	int i;

	/* search empty slot */
	for (i = 0; i < UI_TIMER_SLOT_NUMBER; i++) {
		if (ui_timerSlots[i].callback != NULL)
			continue;
		timer = ui_timerSlots + i;
		break;
	}
	if (timer == NULL)
		Com_Error(ERR_FATAL, "UI_AllocTimer: No more timer slot");

	timer->owner = node;
	timer->delay = firstDelay;
	timer->callback = callback;
	timer->calledTime = 0;
	timer->prev = NULL;
	timer->next = NULL;
	timer->isRunning = qfalse;
	return timer;
}

/**
 * @brief Restart a timer
 */
void UI_TimerStart (uiTimer_t *timer)
{
	if (timer->isRunning)
		return;
	assert(ui_firstTimer != timer && timer->prev == NULL && timer->next == NULL);
	timer->nextTime = CL_Milliseconds() + timer->delay;
	timer->isRunning = qtrue;
	UI_InsertTimerInActiveList(ui_firstTimer, timer);
}

/**
 * @brief Stop a timer
 */
void UI_TimerStop (uiTimer_t *timer)
{
	if (!timer->isRunning)
		return;
	UI_RemoveTimerFromActiveList(timer);
	timer->prev = NULL;
	timer->next = NULL;
	timer->isRunning = qfalse;
}

/**
 * @brief Release the timer. It no more exists
 */
void UI_TimerRelease (uiTimer_t *timer)
{
	UI_RemoveTimerFromActiveList(timer);
	timer->prev = NULL;
	timer->next = NULL;
	timer->owner = NULL;
	timer->callback = NULL;
}

#ifdef COMPILE_UNITTESTS

/**
 * @brief Return the first timer.
 * Only used for white box unittests
 */
const uiTimer_t *UI_PrivateGetFirstTimer (void)
{
	return ui_firstTimer;
}

void UI_PrivateInsertTimerInActiveList (uiTimer_t* first, uiTimer_t* newTimer)
{
	UI_InsertTimerInActiveList(first, newTimer);
}

#endif
