/**
 * @file cl_joystick.c
 */

/*
Copyright (C) 2002-2007 ioQuake3 team.
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

#include "cl_joystick.h"
#include "../client.h"
#include "cl_input.h"
#include "../ui/ui_main.h"
#include "../ui/ui_nodes.h"
#include "../ui/node/ui_node_abstractoption.h"

static SDL_Joystick *stick = NULL;
static cvar_t *in_joystick;
static cvar_t *in_joystickNo;
static cvar_t *in_joystickThreshold;
static cvar_t *in_joystickSpeed;

static struct {
	qboolean buttons[16];
	unsigned int oldaxes;
	unsigned int oldhats;
} stick_state;

/* We translate axes movement into keypresses */
static const int joy_keys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY16, K_JOY17,
	K_JOY18, K_JOY19,
	K_JOY20, K_JOY21,
	K_JOY22, K_JOY23,

	K_JOY24, K_JOY25,
	K_JOY26, K_JOY27
};

/* translate hat events into keypresses
 * the 4 highest buttons are used for the first hat ... */
static const int hat_keys[16] = {
	K_JOY29, K_JOY30,
	K_JOY31, K_JOY32,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20
};

void IN_JoystickMove (void)
{
	qboolean joy_pressed[lengthof(joy_keys)];
	unsigned int axes = 0;
	unsigned int hats = 0;
	int total = 0;
	int i = 0;

	/* check whether a user has changed the joystick number */
	if (in_joystickNo->modified)
		IN_StartupJoystick();
	/* check whether joysticks are disabled */
	if (!in_joystick->integer)
		return;

	if (!stick)
		return;

	SDL_JoystickUpdate();

	OBJZERO(joy_pressed);

	/* update the ball state */
	total = SDL_JoystickNumBalls(stick);
	if (total > 0) {
		int balldx = 0;
		int balldy = 0;
		for (i = 0; i < total; i++) {
			int dx = 0;
			int dy = 0;
			SDL_JoystickGetBall(stick, i, &dx, &dy);
			balldx += dx;
			balldy += dy;
		}
		if (balldx || balldy) {
			mousePosX = balldx / viddef.rx;
			mousePosY = balldy / viddef.ry;
		}
	}

	/* now query the stick buttons... */
	total = SDL_JoystickNumButtons(stick);
	if (total > 0) {
		if (total > lengthof(stick_state.buttons))
			total = lengthof(stick_state.buttons);
		for (i = 0; i < total; i++) {
			const qboolean pressed = (SDL_JoystickGetButton(stick, i) != 0);
			if (pressed != stick_state.buttons[i]) {
				IN_EventEnqueue(K_JOY1 + i, 0, pressed);
				stick_state.buttons[i] = pressed;
			}
		}
	}

	/* look at the hats... */
	total = SDL_JoystickNumHats(stick);
	if (total > 0) {
		if (total > 4)
			total = 4;
		for (i = 0; i < total; i++)
			((Uint8 *)&hats)[i] = SDL_JoystickGetHat(stick, i);
	}

	/* update hat state */
	if (hats != stick_state.oldhats) {
		for (i = 0; i < 4; i++) {
			if (((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i]) {
				/* release event */
				switch (((Uint8 *)&stick_state.oldhats)[i]) {
				case SDL_HAT_UP:
					IN_EventEnqueue(hat_keys[4 * i + 0], 0, qfalse);
					break;
				case SDL_HAT_RIGHT:
					IN_EventEnqueue(hat_keys[4 * i + 1], 0, qfalse);
					break;
				case SDL_HAT_DOWN:
					IN_EventEnqueue(hat_keys[4 * i + 2], 0, qfalse);
					break;
				case SDL_HAT_LEFT:
					IN_EventEnqueue(hat_keys[4 * i + 3], 0, qfalse);
					break;
				case SDL_HAT_RIGHTUP:
					IN_EventEnqueue(hat_keys[4 * i + 0], 0, qfalse);
					IN_EventEnqueue(hat_keys[4 * i + 1], 0, qfalse);
					break;
				case SDL_HAT_RIGHTDOWN:
					IN_EventEnqueue(hat_keys[4 * i + 2], 0, qfalse);
					IN_EventEnqueue(hat_keys[4 * i + 1], 0, qfalse);
					break;
				case SDL_HAT_LEFTUP:
					IN_EventEnqueue(hat_keys[4 * i + 0], 0, qfalse);
					IN_EventEnqueue(hat_keys[4 * i + 3], 0, qfalse);
					break;
				case SDL_HAT_LEFTDOWN:
					IN_EventEnqueue(hat_keys[4 * i + 2], 0, qfalse);
					IN_EventEnqueue(hat_keys[4 * i + 3], 0, qfalse);
					break;
				default:
					break;
				}
				/* press event */
				switch (((Uint8 *)&hats)[i]) {
				case SDL_HAT_UP:
					IN_EventEnqueue(hat_keys[4 * i + 0], 0, qtrue);
					break;
				case SDL_HAT_RIGHT:
					IN_EventEnqueue(hat_keys[4 * i + 1], 0, qtrue);
					break;
				case SDL_HAT_DOWN:
					IN_EventEnqueue(hat_keys[4 * i + 2], 0, qtrue);
					break;
				case SDL_HAT_LEFT:
					IN_EventEnqueue(hat_keys[4 * i + 3], 0, qtrue);
					break;
				case SDL_HAT_RIGHTUP:
					IN_EventEnqueue(hat_keys[4 * i + 0], 0, qtrue);
					IN_EventEnqueue(hat_keys[4 * i + 1], 0, qtrue);
					break;
				case SDL_HAT_RIGHTDOWN:
					IN_EventEnqueue(hat_keys[4 * i + 2], 0, qtrue);
					IN_EventEnqueue(hat_keys[4 * i + 1], 0, qtrue);
					break;
				case SDL_HAT_LEFTUP:
					IN_EventEnqueue(hat_keys[4 * i + 0], 0, qtrue);
					IN_EventEnqueue(hat_keys[4 * i + 3], 0, qtrue);
					break;
				case SDL_HAT_LEFTDOWN:
					IN_EventEnqueue(hat_keys[4 * i + 2], 0, qtrue);
					IN_EventEnqueue(hat_keys[4 * i + 3], 0, qtrue);
					break;
				default:
					break;
				}
			}
		}
	}

	/* save hat state */
	stick_state.oldhats = hats;

	/* finally, look at the axes... */
	total = SDL_JoystickNumAxes(stick);
	if (total >= 2) {
		/* the first two axes are used for the cursor movement */
		for (i = 0; i < 2; i++) {
			const Sint16 axis = SDL_JoystickGetAxis(stick, i);
			const float velocity = ((float) axis) / 32767.0f;
			if (velocity > -in_joystickThreshold->value && velocity < in_joystickThreshold->value)
				continue;

			if (i & 1) {
				mousePosY += in_joystickSpeed->value * velocity;
				if (mousePosY > (int)viddef.context.height)
					mousePosY = (int)viddef.context.height;
				else if (mousePosY < 0)
					mousePosY = 0;
			} else {
				mousePosX += in_joystickSpeed->value * velocity;
				if (mousePosX > (int)viddef.context.width)
					mousePosX = (int)viddef.context.width;
				else if (mousePosX < 0)
					mousePosX = 0;
			}
		}
	}


	if (total > 2) {
		if (total > 16)
			total = 16;
		/* every axis except the first two can be normally bound to an action */
		for (i = 2; i < total; i++) {
			const Sint16 axis = SDL_JoystickGetAxis(stick, i);
			const float f = ((float) axis) / 32767.0f;
			if (f < -in_joystickThreshold->value) {
				axes |= (1 << (i * 2));
			} else if (f > in_joystickThreshold->value) {
				axes |= (1 << ((i * 2) + 1));
			}
		}
	}


	/* Time to update axes state based on old vs. new. */
	if (axes != stick_state.oldaxes) {
		for (i = 2; i < 16; i++) {
			if ((axes & (1 << i)) && !(stick_state.oldaxes & (1 << i)))
				IN_EventEnqueue(joy_keys[i], 0, qtrue);

			if (!(axes & (1 << i)) && (stick_state.oldaxes & (1 << i)))
				IN_EventEnqueue(joy_keys[i], 0, qfalse);
		}
	}

	/* Save for future generations. */
	stick_state.oldaxes = axes;
}

/**
 * @brief Adds joysticks to the options menu
 */
void IN_JoystickInitMenu (void)
{
	uiNode_t* joystickOptions = NULL;
	const int total = SDL_NumJoysticks();

	if (total == 0) {
		UI_AddOption(&joystickOptions, "", _("None"), "0");
	} else {
		int i;
		for (i = 0; i < total; i++)
			UI_AddOption(&joystickOptions, "", SDL_JoystickName(i), va("%i", i));
	}
	UI_RegisterOption(OPTION_JOYSTICKS, joystickOptions);
}

/**
 * @brief Init available joysticks
 */
void IN_StartupJoystick (void)
{
	int i = 0;
	int total = 0;

	in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE, "Activate or deactivate the use of a joystick");
	in_joystickNo = Cvar_Get("in_joystickNo", "0", CVAR_ARCHIVE, "Joystick to use - 0 is the first - 1 is the second ...");
	in_joystickThreshold = Cvar_Get("in_joystickThreshold", "0.05", CVAR_ARCHIVE, "The threshold for the joystick axes");
	in_joystickSpeed = Cvar_Get("in_joystickSpeed", "20", CVAR_ARCHIVE, "The joystick speed for the cursor");

	if (stick != NULL) {
		Com_Printf("... closing already initialized joystick\n");
		SDL_JoystickClose(stick);
	}

	stick = NULL;
	OBJZERO(stick_state);

	if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
		Com_DPrintf(DEBUG_CLIENT, "Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
		if (SDL_Init(SDL_INIT_JOYSTICK) == -1) {
			Com_DPrintf(DEBUG_CLIENT, "SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf(DEBUG_CLIENT, "SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
	}

	total = SDL_NumJoysticks();
	Com_Printf("%d possible joysticks\n", total);
	for (i = 0; i < total; i++)
		Com_DPrintf(DEBUG_CLIENT, "[%d] %s\n", i, SDL_JoystickName(i));

	if (in_joystickNo->integer < 0 || in_joystickNo->integer >= total)
		Cvar_Set("in_joystickNo", "0");
	in_joystickNo->modified = qfalse;

	stick = SDL_JoystickOpen(in_joystickNo->integer);

	if (stick == NULL) {
		Com_Printf("no joystick found.\n");
		return;
	}

	Com_Printf("joystick %d opened - set cvar in_joystickNo to change this\n", in_joystickNo->integer);
	Com_Printf("... name: %s\n", SDL_JoystickName(in_joystickNo->integer));
	Com_Printf("... axes: %d\n", SDL_JoystickNumAxes(stick));
	Com_Printf("... hats: %d\n", SDL_JoystickNumHats(stick));
	Com_Printf("... buttons: %d\n", SDL_JoystickNumButtons(stick));
	Com_Printf("... balls: %d\n", SDL_JoystickNumBalls(stick));

	SDL_JoystickEventState(SDL_QUERY);
}
