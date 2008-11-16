/**
 * @file m_input.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_MENU_M_INPUT_H
#define CLIENT_MENU_M_INPUT_H

/* mouse input */
void MN_LeftClick(int x, int y);
void MN_RightClick(int x, int y);
void MN_MiddleClick(int x, int y);
void MN_MouseWheel(qboolean down, int x, int y);
void MN_MouseMove(int x, int y);
void MN_MouseDown(int x, int y, int button);
void MN_MouseUp(int x, int y, int button);

/* mouse capture */
menuNode_t* MN_GetMouseCapture(void);
void MN_SetMouseCapture(menuNode_t* node);
void MN_MouseRelease(void);

/* timer input */
struct menuNode_s;
struct menuTimer_s;
typedef void (*timerCallback_t)(struct menuNode_s *node, struct menuTimer_s *timer);
typedef struct menuTimer_s {
	struct menuNode_s *node;
	timerCallback_t callback;
	int calledTime;
	int nextTime;
	int delay;
	qboolean isRunning;
	void *userData;
} menuTimer_t;
menuTimer_t* MN_AllocTimer(struct menuNode_s *node, int firstDelay, timerCallback_t callback);
void MN_TimerStart(menuTimer_t *timer);
void MN_TimerStop(menuTimer_t *timer);
void MN_TimerRelease(menuTimer_t *timer);
void MN_HandleTimers(void);


/** @todo move it somewhere */
void MN_SetCvar(const char *name, const char *str, float value);

extern menuNode_t *mouseOverTest; /**< mouse over active node, for preview */

/**/

#endif
