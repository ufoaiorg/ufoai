/**
 * @file
 * @brief Client input handling - bindable commands.
 * @note Continuous button event tracking is complicated by the fact that two different
 * input sources (say, mouse button 1 and the control key) can both press the
 * same button, but the button should only be released when both of the
 * pressing key have been released.
 *
 * When a key event issues a button command (+forward, +attack, etc), it appends
 * its key number as a parameter to the command so it can be matched up with
 * the release.
 *
 * Key_Event(unsigned int key, unsigned short unicode, bool down, unsigned time);
 *
 *  +mlook src time
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_input.c
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../client.h"
#include "cl_input.h"
#include "cl_keys.h"
#include "cl_joystick.h"
#include "../battlescape/cl_localentity.h"
#include "../battlescape/cl_hud.h"
#include "../cl_console.h"
#include "../cl_screen.h"
#include "../battlescape/cl_actor.h"
#include "../battlescape/cl_view.h"
#include "../battlescape/cl_parse.h"
#include "../ui/ui_main.h"
#include "../ui/ui_input.h"
#include "../ui/node/ui_node_abstractnode.h"
#include "../../shared/utf8.h"

#include "../../common/tracing.h"
#include "../renderer/r_misc.h"

/* power of two please */
#define MAX_KEYQ 64

#if SDL_VERSION_ATLEAST(2,0,0)
#define SDL_keysym SDL_Keysym
#endif

static struct {
	unsigned int key;
	unsigned short unicode;
	int down;
} keyq[MAX_KEYQ];

static int keyq_head = 0;
static int keyq_tail = 0;

static cvar_t* in_debug;
cvar_t* cl_isometric;

mouseSpace_t mouseSpace;
int mousePosX, mousePosY;
static int oldMousePosX, oldMousePosY;

static int battlescapeMouseDraggingX;
static int battlescapeMouseDraggingY;
static bool battlescapeMouseDraggingPossible, battlescapeMouseDraggingActive;
enum {
	BATTLESCAPE_MOUSE_DRAGGING_TRIGGER_X = VID_NORM_WIDTH / 10,
	BATTLESCAPE_MOUSE_DRAGGING_TRIGGER_Y = VID_NORM_HEIGHT / 10
};

/*
===============================================================================
KEY BUTTONS
===============================================================================
*/

typedef struct {
	int down[2];				/**< key nums holding it down */
	unsigned downtime;			/**< msec timestamp when the key was pressed down */
	unsigned msec;				/**< downtime for this key in msec (delta between pressed and released) */
	int state;					/**< 1 if down, 0 if not down */
} kbutton_t;

static kbutton_t in_turnleft, in_turnright, in_shiftleft, in_shiftright;
static kbutton_t in_shiftup, in_shiftdown;
static kbutton_t in_zoomin, in_zoomout;
static kbutton_t in_turnup, in_turndown;
static kbutton_t in_pantilt;

/**
 * @brief Handles the catch of a @c kbutton_t state
 * @sa IN_KeyUp
 * @sa CL_GetKeyMouseState
 * @note Called from console callbacks with two parameters, the
 * key and the milliseconds when the key was released
 */
static void IN_KeyDown (kbutton_t* b)
{
	int k;
	const char* c = Cmd_Argv(1);

	if (c[0])
		k = atoi(c);
	else
		/* typed manually at the console for continuous down */
		k = -1;

	/* repeating key */
	if (k == b->down[0] || k == b->down[1])
		return;

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Printf("Three keys down for a button!\n");
		return;
	}

	/* still down */
	if (b->state)
		return;

	/* save timestamp */
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = CL_Milliseconds() - 100;

	/* down */
	b->state = 1;
}

/**
 * @brief Handles the release of a @c kbutton_t state
 * @sa IN_KeyDown
 * @sa CL_GetKeyMouseState
 * @note Called from console callbacks with two parameters, the
 * key and the milliseconds when the key was released
 * @param[in,out] b the button state to
 */
static void IN_KeyUp (kbutton_t* b)
{
	int k;
	unsigned uptime;
	const char* c = Cmd_Argv(1);

	if (c[0])
		k = atoi(c);
	/* typed manually at the console, assume for unsticking, so clear all */
	else {
		b->down[0] = b->down[1] = 0;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	/* key up without corresponding down (menu pass through) */
	else
		return;

	/* some other key is still holding it down */
	if (b->down[0] || b->down[1])
		return;

	/* still up (this should not happen) */
	if (!b->state)
		return;

	/* save timestamp */
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec = uptime - b->downtime;
	else
		b->msec = 10;

	/* now up */
	b->state = 0;
}

static void IN_TurnLeftDown_f (void)
{
	IN_KeyDown(&in_turnleft);
}
static void IN_TurnLeftUp_f (void)
{
	IN_KeyUp(&in_turnleft);
}
static void IN_TurnRightDown_f (void)
{
	IN_KeyDown(&in_turnright);
}
static void IN_TurnRightUp_f (void)
{
	IN_KeyUp(&in_turnright);
}
static void IN_TurnUpDown_f (void)
{
	IN_KeyDown(&in_turnup);
}
static void IN_TurnUpUp_f (void)
{
	IN_KeyUp(&in_turnup);
}
static void IN_TurnDownDown_f (void)
{
	IN_KeyDown(&in_turndown);
}
static void IN_TurnDownUp_f (void)
{
	IN_KeyUp(&in_turndown);
}
static void IN_PanTiltDown_f (void)
{
	if (IN_GetMouseSpace() != MS_WORLD)
		return;
	IN_KeyDown(&in_pantilt);
}
static void IN_PanTiltUp_f (void)
{
	IN_KeyUp(&in_pantilt);
}
static void IN_ShiftLeftDown_f (void)
{
	IN_KeyDown(&in_shiftleft);
}
static void IN_ShiftLeftUp_f (void)
{
	IN_KeyUp(&in_shiftleft);
}
static void IN_ShiftLeftUpDown_f (void)
{
	IN_KeyDown(&in_shiftleft);
	IN_KeyDown(&in_shiftup);
}
static void IN_ShiftLeftUpUp_f (void)
{
	IN_KeyUp(&in_shiftleft);
	IN_KeyUp(&in_shiftup);
}
static void IN_ShiftLeftDownDown_f (void)
{
	IN_KeyDown(&in_shiftleft);
	IN_KeyDown(&in_shiftdown);
}
static void IN_ShiftLeftDownUp_f (void)
{
	IN_KeyUp(&in_shiftleft);
	IN_KeyUp(&in_shiftdown);
}
static void IN_ShiftRightDown_f (void)
{
	IN_KeyDown(&in_shiftright);
}
static void IN_ShiftRightUp_f (void)
{
	IN_KeyUp(&in_shiftright);
}
static void IN_ShiftRightUpDown_f (void)
{
	IN_KeyDown(&in_shiftright);
	IN_KeyDown(&in_shiftup);
}
static void IN_ShiftRightUpUp_f (void)
{
	IN_KeyUp(&in_shiftright);
	IN_KeyUp(&in_shiftup);
}
static void IN_ShiftRightDownDown_f (void)
{
	IN_KeyDown(&in_shiftright);
	IN_KeyDown(&in_shiftdown);
}
static void IN_ShiftRightDownUp_f (void)
{
	IN_KeyUp(&in_shiftright);
	IN_KeyUp(&in_shiftdown);
}
static void IN_ShiftUpDown_f (void)
{
	IN_KeyDown(&in_shiftup);
}
static void IN_ShiftUpUp_f (void)
{
	IN_KeyUp(&in_shiftup);
}
static void IN_ShiftDownDown_f (void)
{
	IN_KeyDown(&in_shiftdown);
}
static void IN_ShiftDownUp_f (void)
{
	IN_KeyUp(&in_shiftdown);
}
static void IN_ZoomInDown_f (void)
{
	IN_KeyDown(&in_zoomin);
}
static void IN_ZoomInUp_f (void)
{
	IN_KeyUp(&in_zoomin);
}
static void IN_ZoomOutDown_f (void)
{
	IN_KeyDown(&in_zoomout);
}
static void IN_ZoomOutUp_f (void)
{
	IN_KeyUp(&in_zoomout);
}


/**
 * @brief Switch one worldlevel up
 */
static void CL_LevelUp_f (void)
{
	if (!CL_OnBattlescape())
		return;
	Cvar_SetValue("cl_worldlevel", (cl_worldlevel->integer < cl.mapMaxLevel - 1) ? cl_worldlevel->integer + 1 : cl.mapMaxLevel - 1);
}

/**
 * @brief Switch one worldlevel down
 */
static void CL_LevelDown_f (void)
{
	if (!CL_OnBattlescape())
		return;
	Cvar_SetValue("cl_worldlevel", (cl_worldlevel->integer > 0) ? cl_worldlevel->integer - 1 : 0);
}

static void CL_ZoomInQuant_f (void)
{
	CL_CameraZoomIn();
}

static void CL_ZoomOutQuant_f (void)
{
	CL_CameraZoomOut();
}

static void CL_WheelDown_f (void)
{
	UI_MouseScroll(0, 1);
}

static void CL_WheelUp_f (void)
{
	UI_MouseScroll(0, -1);
}

/**
 * @brief Left mouse click
 */
static void CL_SelectDown_f (void)
{
	battlescapeMouseDraggingPossible = (CL_BattlescapeRunning() && IN_GetMouseSpace() != MS_UI);
	if (!battlescapeMouseDraggingPossible)
		return;
	battlescapeMouseDraggingX = mousePosX;
	battlescapeMouseDraggingY = mousePosY;
	battlescapeMouseDraggingActive = false;
	CL_InitBattlescapeMouseDragging();
}

static void CL_SelectUp_f (void)
{
#ifdef ANDROID
	/* Android input quirk - when user tries to zoom/rotate, and touches the screen with a second finger,
	 * SDL will send left mouse button up event, and right mouse button down event immediately after that,
	 * so we need to cancel the mouse click action, and let the user zoom/rotate as she wants */
	if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) == 0)
#endif
	if (IN_GetMouseSpace() == MS_WORLD && !battlescapeMouseDraggingActive)
		CL_ActorSelectMouse();
	battlescapeMouseDraggingPossible = false;
	battlescapeMouseDraggingActive = false;
	if (IN_GetMouseSpace() == MS_UI)
		return;
	IN_SetMouseSpace(MS_NULL);
}

static void CL_ProcessMouseDragging (void)
{
	if (!battlescapeMouseDraggingPossible)
		return;

	if (!battlescapeMouseDraggingActive &&
		(abs(battlescapeMouseDraggingX - mousePosX) > BATTLESCAPE_MOUSE_DRAGGING_TRIGGER_X ||
		 abs(battlescapeMouseDraggingY - mousePosY) > BATTLESCAPE_MOUSE_DRAGGING_TRIGGER_Y))
		battlescapeMouseDraggingActive = true;

	if (battlescapeMouseDraggingActive)
		CL_BattlescapeMouseDragging();
}

/**
 * @brief Middle mouse click
 */
static void CL_ActionDown_f (void)
{
	if (!CL_OnBattlescape())
		return;
	IN_KeyDown(&in_pantilt);
}

static void CL_ActionUp_f (void)
{
	IN_KeyUp(&in_pantilt);
	if (IN_GetMouseSpace() == MS_UI)
		return;
	if (in_pantilt.msec < 250)
		CL_ActorActionMouse();
	IN_SetMouseSpace(MS_NULL);
}

/**
 * @brief Turn button is hit
 */
static void CL_TurnDown_f (void)
{
	if (IN_GetMouseSpace() == MS_UI)
		return;
	if (IN_GetMouseSpace() == MS_WORLD)
		CL_ActorTurnMouse();
}

static void CL_TurnUp_f (void)
{
	if (IN_GetMouseSpace() == MS_UI)
		return;
	IN_SetMouseSpace(MS_NULL);
}

/**
 * @todo only call/register it when we are on the battlescape
 */
static void CL_HudRadarDown_f (void)
{
	if (!CL_BattlescapeRunning())
		return;
	UI_PushWindow("radarmenu");
}

/**
 * @todo only call/register it when we are on the battlescape
 */
static void CL_HudRadarUp_f (void)
{
	if (!CL_BattlescapeRunning())
		return;
	UI_CloseWindow("radarmenu");
}

/**
 * @brief Right mouse button is hit in menu
 */
static void CL_RightClickDown_f (void)
{
	if (IN_GetMouseSpace() == MS_UI) {
		UI_MouseDown(mousePosX, mousePosY, K_MOUSE2);
	}
}

/**
 * @brief Right mouse button is freed in menu
 */
static void CL_RightClickUp_f (void)
{
	if (IN_GetMouseSpace() == MS_UI) {
		UI_MouseUp(mousePosX, mousePosY, K_MOUSE2);
	}
}

/**
 * @brief Middle mouse button is hit in menu
 */
static void CL_MiddleClickDown_f (void)
{
	if (IN_GetMouseSpace() == MS_UI) {
		UI_MouseDown(mousePosX, mousePosY, K_MOUSE3);
	}
}

/**
 * @brief Middle mouse button is freed in menu
 */
static void CL_MiddleClickUp_f (void)
{
	if (IN_GetMouseSpace() == MS_UI) {
		UI_MouseUp(mousePosX, mousePosY, K_MOUSE3);
	}
}

/**
 * @brief Left mouse button is hit in menu
 */
static void CL_LeftClickDown_f (void)
{
	if (IN_GetMouseSpace() == MS_UI) {
		UI_MouseDown(mousePosX, mousePosY, K_MOUSE1);
	}
}

/**
 * @brief Left mouse button is freed in menu
 */
static void CL_LeftClickUp_f (void)
{
	if (IN_GetMouseSpace() == MS_UI) {
		UI_MouseUp(mousePosX, mousePosY, K_MOUSE1);
	}
}

#define SCROLL_BORDER	4
#define MOUSE_YAW_SCALE		0.1
#define MOUSE_PITCH_SCALE	0.1

/**
 * @note see SCROLL_BORDER define
 */
float CL_GetKeyMouseState (int dir)
{
	float value;

	switch (dir) {
	case STATE_FORWARD:
		/* sum directions, 'true' is use as '1' */
		value = (in_shiftup.state & 1) + (mousePosY <= (viddef.y / viddef.ry) + SCROLL_BORDER) - (in_shiftdown.state & 1) - (mousePosY >= ((viddef.y + viddef.viewHeight) / viddef.ry) - SCROLL_BORDER);
		break;
	case STATE_RIGHT:
		/* sum directions, 'true' is use as '1' */
		value = (in_shiftright.state & 1) + (mousePosX >= ((viddef.x + viddef.viewWidth) / viddef.rx) - SCROLL_BORDER) - (in_shiftleft.state & 1) - (mousePosX <= (viddef.x / viddef.rx) + SCROLL_BORDER);
		break;
	case STATE_ZOOM:
		value = (in_zoomin.state & 1) - (in_zoomout.state & 1);
		break;
	case STATE_ROT:
		value = (in_turnleft.state & 1) - (in_turnright.state & 1);
		if (in_pantilt.state)
			value -= (float) (mousePosX - oldMousePosX) * MOUSE_YAW_SCALE;
		break;
	case STATE_TILT:
		value = (in_turnup.state & 1) - (in_turndown.state & 1);
		if (in_pantilt.state)
			value += (float) (mousePosY - oldMousePosY) * MOUSE_PITCH_SCALE;
		break;
	default:
		value = 0.0;
		break;
	}

	return value;
}

/**
 * @brief Called every frame to parse the input
 * @sa CL_Frame
 */
static void IN_Parse (void)
{
	IN_SetMouseSpace(MS_NULL);

	/* standard menu and world mouse handling */
	if (UI_IsMouseOnWindow()) {
		IN_SetMouseSpace(MS_UI);
		return;
	}

	if (cls.state != ca_active)
		return;

	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	if (CL_ActorMouseTrace()) {
		/* mouse is in the world */
		IN_SetMouseSpace(MS_WORLD);
	}
}

/**
 * @brief Debug function to print sdl key events
 */
static inline void IN_PrintKey (const SDL_Event* event, int down)
{
	if (in_debug->integer) {
		Com_Printf("key name: %s (down: %i)", SDL_GetKeyName(event->key.keysym.sym), down);
		int unicode;
#if SDL_VERSION_ATLEAST(2,0,0)
		unicode = event->key.keysym.sym;
#else
		unicode = event->key.keysym.unicode;
#endif
		if (unicode) {
			Com_Printf(" unicode: %x", unicode);
			if (unicode >= '0' && unicode <= '~')  /* printable? */
				Com_Printf(" (%c)", (unsigned char)(unicode));
		}
		Com_Printf("\n");
	}
}

/**
 * @brief Translate the keys to ufo keys
 */
static void IN_TranslateKey (const unsigned int keycode, unsigned int* ascii)
{
	switch (keycode) {
	case SDLK_PAGEUP:
		*ascii = K_PGUP;
		break;
#if SDL_VERSION_ATLEAST(2,0,0)
	case SDLK_KP_0:
		*ascii = K_KP_INS;
		break;
	case SDLK_KP_1:
		*ascii = K_KP_END;
		break;
	case SDLK_KP_2:
		*ascii = K_KP_DOWNARROW;
		break;
	case SDLK_KP_3:
		*ascii = K_KP_PGDN;
		break;
	case SDLK_KP_4:
		*ascii = K_KP_LEFTARROW;
		break;
	case SDLK_KP_5:
		*ascii = K_KP_5;
		break;
	case SDLK_KP_6:
		*ascii = K_KP_RIGHTARROW;
		break;
	case SDLK_KP_7:
		*ascii = K_KP_HOME;
		break;
	case SDLK_KP_8:
		*ascii = K_KP_UPARROW;
		break;
	case SDLK_KP_9:
		*ascii = K_KP_PGUP;
		break;
	case SDLK_PRINTSCREEN:
		*ascii = K_PRINT;
		break;
	case SDLK_SCROLLLOCK:
		*ascii = K_SCROLLOCK;
		break;
#else
	case SDLK_KP0:
		*ascii = K_KP_INS;
		break;
	case SDLK_KP1:
		*ascii = K_KP_END;
		break;
	case SDLK_KP2:
		*ascii = K_KP_DOWNARROW;
		break;
	case SDLK_KP3:
		*ascii = K_KP_PGDN;
		break;
	case SDLK_KP4:
		*ascii = K_KP_LEFTARROW;
		break;
	case SDLK_KP5:
		*ascii = K_KP_5;
		break;
	case SDLK_KP6:
		*ascii = K_KP_RIGHTARROW;
		break;
	case SDLK_KP7:
		*ascii = K_KP_HOME;
		break;
	case SDLK_KP8:
		*ascii = K_KP_UPARROW;
		break;
	case SDLK_KP9:
		*ascii = K_KP_PGUP;
		break;
	case SDLK_LSUPER:
	case SDLK_RSUPER:
		*ascii = K_SUPER;
		break;
	case SDLK_COMPOSE:
		*ascii = K_COMPOSE;
		break;
	case SDLK_PRINT:
		*ascii = K_PRINT;
		break;
	case SDLK_BREAK:
		*ascii = K_BREAK;
		break;
	case SDLK_EURO:
		*ascii = K_EURO;
		break;
	case SDLK_SCROLLOCK:
		*ascii = K_SCROLLOCK;
		break;
	case SDLK_NUMLOCK:
		*ascii = K_KP_NUMLOCK;
		break;
#endif
	case SDLK_PAGEDOWN:
		*ascii = K_PGDN;
		break;
	case SDLK_HOME:
		*ascii = K_HOME;
		break;
	case SDLK_END:
		*ascii = K_END;
		break;
	case SDLK_LEFT:
		*ascii = K_LEFTARROW;
		break;
	case SDLK_RIGHT:
		*ascii = K_RIGHTARROW;
		break;
	case SDLK_DOWN:
		*ascii = K_DOWNARROW;
		break;
	case SDLK_UP:
		*ascii = K_UPARROW;
		break;
	case SDLK_ESCAPE:
		*ascii = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		*ascii = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		*ascii = K_ENTER;
		break;
	case SDLK_TAB:
		*ascii = K_TAB;
		break;
	case SDLK_F1:
		*ascii = K_F1;
		break;
	case SDLK_F2:
		*ascii = K_F2;
		break;
	case SDLK_F3:
		*ascii = K_F3;
		break;
	case SDLK_F4:
		*ascii = K_F4;
		break;
	case SDLK_F5:
		*ascii = K_F5;
		break;
	case SDLK_F6:
		*ascii = K_F6;
		break;
	case SDLK_F7:
		*ascii = K_F7;
		break;
	case SDLK_F8:
		*ascii = K_F8;
		break;
	case SDLK_F9:
		*ascii = K_F9;
		break;
	case SDLK_F10:
		*ascii = K_F10;
		break;
	case SDLK_F11:
		*ascii = K_F11;
		break;
	case SDLK_F12:
		*ascii = K_F12;
		break;
	case SDLK_F13:
		*ascii = K_F13;
		break;
	case SDLK_F14:
		*ascii = K_F14;
		break;
	case SDLK_F15:
		*ascii = K_F15;
		break;
	case SDLK_BACKSPACE:
		*ascii = K_BACKSPACE;
		break;
	case SDLK_KP_PERIOD:
		*ascii = K_KP_DEL;
		break;
	case SDLK_DELETE:
		*ascii = K_DEL;
		break;
	case SDLK_PAUSE:
		*ascii = K_PAUSE;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		*ascii = K_SHIFT;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		*ascii = K_CTRL;
		break;
	case SDLK_LALT:
	case SDLK_RALT:
		*ascii = K_ALT;
		break;
	case SDLK_INSERT:
		*ascii = K_INS;
		break;
	case SDLK_KP_PLUS:
		*ascii = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		*ascii = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		*ascii = K_KP_SLASH;
		break;
	case SDLK_KP_MULTIPLY:
		*ascii = K_KP_MULTIPLY;
		break;
	case SDLK_MODE:
		*ascii = K_MODE;
		break;
	case SDLK_HELP:
		*ascii = K_HELP;
		break;
	case SDLK_SYSREQ:
		*ascii = K_SYSREQ;
		break;
	case SDLK_MENU:
		*ascii = K_MENU;
		break;
	case SDLK_POWER:
		*ascii = K_POWER;
		break;
	case SDLK_UNDO:
		*ascii = K_UNDO;
		break;
	case SDLK_CAPSLOCK:
		*ascii = K_CAPSLOCK;
		break;
	case SDLK_SPACE:
		*ascii = K_SPACE;
		break;
	default:
		if (UTF8_encoded_len(keycode) == 1 && isprint(keycode))
			*ascii = keycode;
		else
			*ascii = 0;
		break;
	}
}

void IN_EventEnqueue (unsigned int keyNum, unsigned short keyUnicode, bool keyDown)
{
	if (keyNum > 0 || keyUnicode > 0) {
		if (in_debug->integer)
			Com_Printf("Enqueue: %s (%i) (down: %i)\n", Key_KeynumToString(keyNum), keyNum, keyDown);
		keyq[keyq_head].down = keyDown;
		keyq[keyq_head].unicode = keyUnicode;
		keyq[keyq_head].key = keyNum;
		keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1);
	}
}

static bool IN_ToggleFullscreen (const bool full)
{
#if SDL_VERSION_ATLEAST(2,0,0)
	const int mask = full ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
	const bool isFullScreen = SDL_GetWindowFlags(cls.window) & mask;
	SDL_SetWindowFullscreen(cls.window, isFullScreen ? 0 : mask);
	return SDL_GetWindowFlags(cls.window) & mask;
#else
#ifdef _WIN32
	return false;
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	if (!SDL_WM_ToggleFullScreen(surface)) {
		const int flags = surface->flags ^= SDL_FULLSCREEN;
		SDL_SetVideoMode(surface->w, surface->h, 0, flags);
	}

	return surface->flags & SDL_FULLSCREEN;
#endif
#endif
}

/**
 * @brief Handle input events like key presses and joystick movement as well
 * as window events
 * @sa CL_Frame
 * @sa IN_Parse
 * @sa IN_JoystickMove
 */
void IN_Frame (void)
{
	int mouse_buttonstate;
	unsigned short unicode;
	unsigned int key;
	SDL_Event event;

	IN_Parse();

	IN_JoystickMove();

	CL_ProcessMouseDragging();

	if (vid_grabmouse->modified) {
		vid_grabmouse->modified = false;

		if (!vid_grabmouse->integer) {
			/* ungrab the pointer */
			Com_Printf("Switch grab input off\n");
#if SDL_VERSION_ATLEAST(2,0,0)
			SDL_SetWindowGrab(cls.window, SDL_FALSE);
#else
			SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
		} else  {
			/* grab the pointer */
			Com_Printf("Switch grab input on\n");
#if SDL_VERSION_ATLEAST(2,0,0)
			SDL_SetWindowGrab(cls.window, SDL_TRUE);
#else
			SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
		}
	}

	oldMousePosX = mousePosX;
	oldMousePosY = mousePosY;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
#if SDL_VERSION_ATLEAST(2,0,0)
		case SDL_MOUSEWHEEL:
			mouse_buttonstate = event.wheel.y < 0 ? K_MWHEELDOWN : K_MWHEELUP;
			IN_EventEnqueue(mouse_buttonstate, 0, true);
			break;
		case SDL_TEXTEDITING:
			/** @todo */
			break;
		case SDL_TEXTINPUT: {
			const char* text = event.text.text;
			const char** str = &text;
			for (;;) {
				const int characterUnicode = UTF8_next(str);
				if (characterUnicode == -1) {
					break;
				}
				unicode = characterUnicode;
				IN_TranslateKey(characterUnicode, &key);
				IN_EventEnqueue(key, unicode, true);
				IN_EventEnqueue(key, unicode, false);
			}
			break;
		}
#endif
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			switch (event.button.button) {
			case SDL_BUTTON_LEFT:
				mouse_buttonstate = K_MOUSE1;
				break;
			case SDL_BUTTON_MIDDLE:
				mouse_buttonstate = K_MOUSE3;
				break;
			case SDL_BUTTON_RIGHT:
				mouse_buttonstate = K_MOUSE2;
				break;
#if SDL_VERSION_ATLEAST(2,0,0)
			case SDL_BUTTON_X1:
				mouse_buttonstate = K_MOUSE4;
				break;
			case SDL_BUTTON_X2:
				mouse_buttonstate = K_MOUSE5;
				break;
#else
			case SDL_BUTTON_WHEELUP:
				mouse_buttonstate = K_MWHEELUP;
				break;
			case SDL_BUTTON_WHEELDOWN:
				mouse_buttonstate = K_MWHEELDOWN;
				break;
			case 6:
				mouse_buttonstate = K_MOUSE4;
				break;
			case 7:
				mouse_buttonstate = K_MOUSE5;
				break;
#endif
			default:
				mouse_buttonstate = K_AUX1 + (event.button.button - 8) % 16;
				break;
			}
			IN_EventEnqueue(mouse_buttonstate, 0, (event.type == SDL_MOUSEBUTTONDOWN));
			break;

		case SDL_MOUSEMOTION:
			SDL_GetMouseState(&mousePosX, &mousePosY);
			mousePosX /= viddef.rx;
			mousePosY /= viddef.ry;
			break;

		case SDL_KEYDOWN:
			IN_PrintKey(&event, 1);
			if ((event.key.keysym.mod & KMOD_ALT) && event.key.keysym.sym == SDLK_RETURN) {
				Com_Printf("try to toggle fullscreen\n");
				if (IN_ToggleFullscreen(false)) {
					Cvar_SetValue("vid_fullscreen", 1);
					/* make sure, that input grab is deactivated in fullscreen mode */
					Cvar_SetValue("vid_grabmouse", 0);
				} else {
					Cvar_SetValue("vid_fullscreen", 0);
				}
				vid_fullscreen->modified = false; /* we just changed it with SDL. */
				break; /* ignore this key */
			}

			if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_RETURN) {
				Com_Printf("try to toggle fullscreen\n");
				if (IN_ToggleFullscreen(true)) {
					Cvar_SetValue("vid_fullscreen", 2);
				} else {
					Cvar_SetValue("vid_fullscreen", 0);
				}
				vid_fullscreen->modified = false; /* we just changed it with SDL. */
				break; /* ignore this key */
			}

			if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_g) {
#if SDL_VERSION_ATLEAST(2,0,0)
				const bool grab = SDL_GetWindowGrab(cls.window);
				Cvar_SetValue("vid_grabmouse", grab ? 0 : 1);
#else
				SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
				Cvar_SetValue("vid_grabmouse", (gm == SDL_GRAB_ON) ? 0 : 1);
#endif
				Com_Printf("toggled mouse grab (%s)\n", vid_grabmouse->integer == 1 ? "true" : "false");
				break; /* ignore this key */
			}

			/* console key is hardcoded, so the user can never unbind it */
			if ((event.key.keysym.mod & KMOD_SHIFT) && event.key.keysym.sym == SDLK_ESCAPE) {
				Con_ToggleConsole_f();
				break;
			}

#if SDL_VERSION_ATLEAST(2,0,0)
			unicode = 0;
#else
			unicode = event.key.keysym.unicode;
#endif
			IN_TranslateKey(event.key.keysym.sym, &key);
			IN_EventEnqueue(key, unicode, true);
			break;

		case SDL_KEYUP:
			IN_PrintKey(&event, 0);
#if SDL_VERSION_ATLEAST(2,0,0)
			unicode = 0;
#else
			unicode = event.key.keysym.unicode;
#endif
			IN_TranslateKey(event.key.keysym.sym, &key);
			IN_EventEnqueue(key, unicode, false);
			break;

#if SDL_VERSION_ATLEAST(2,0,0)
		/** @todo implement missing events for sdl2 */
		case SDL_WINDOWEVENT:
			switch (event.window.type) {
			case SDL_WINDOWEVENT_FOCUS_LOST:
				UI_ReleaseInput();
				break;
			case SDL_WINDOWEVENT_RESIZED:
				/* make sure that SDL_SetVideoMode is called again after we changed the size
				 * otherwise the mouse will make problems */
				vid_mode->modified = true;
				break;
			default:
				break;
			}
			break;
#else
		case SDL_VIDEOEXPOSE:
			break;

		case SDL_ACTIVEEVENT:
			/* make sure the menu no more captures the input when the game window loses focus */
			if (event.active.state == SDL_APPINPUTFOCUS && event.active.gain == 0)
				UI_ReleaseInput();
			break;

		case SDL_VIDEORESIZE:
			/* make sure that SDL_SetVideoMode is called again after we changed the size
			 * otherwise the mouse will make problems */
			vid_mode->modified = true;
#ifdef ANDROID
			/* On Android the OpenGL context is destroyed after we've received a resize event,
			 * so wee need to re-init OpenGL state machine and re-upload all textures */
			R_ReinitOpenglContext();
#endif
			break;
#endif

		case SDL_QUIT:
			Cmd_ExecuteString("quit");
			break;
		}
	}
}

/**
 * Simulate press of a key with a command.
 */
static void CL_PressKey_f (void)
{
	unsigned int keyNum;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <key> : simulate press of a key\n", Cmd_Argv(0));
		return;
	}

	keyNum = Key_StringToKeynum(Cmd_Argv(1));
	/* @todo unicode value is wrong */
	IN_EventEnqueue(keyNum, '?', true);
	IN_EventEnqueue(keyNum, '?', false);
}

typedef struct cursorChange_s {
	mouseSpace_t prevSpace;
	int cursor;
} cursorChange_t;

static cursorChange_t cursorChange;

void IN_SetMouseSpace (mouseSpace_t mspace)
{
	if (mspace != MS_NULL) {
		if (mspace != cursorChange.prevSpace) {
			SCR_ChangeCursor(cursorChange.cursor);
		}
	}
	if (mouseSpace != MS_NULL && mouseSpace != cursorChange.prevSpace) {
		cursorChange.prevSpace = mouseSpace;
		cursorChange.cursor = Cvar_GetValue("cursor");
	}
	mouseSpace = mspace;
}

/**
 * @sa CL_InitLocal
 */
void IN_Init (void)
{
	Com_Printf("\n------- input initialization -------\n");

	/* cvars */
	in_debug = Cvar_Get("in_debug", "0", 0, "Show input key codes on game console");
	cl_isometric = Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");

	/* commands */
	Cmd_AddCommand("+turnleft", IN_TurnLeftDown_f, N_("Rotate battlescape camera anti-clockwise"));
	Cmd_AddCommand("-turnleft", IN_TurnLeftUp_f);
	Cmd_AddCommand("+turnright", IN_TurnRightDown_f, N_("Rotate battlescape camera clockwise"));
	Cmd_AddCommand("-turnright", IN_TurnRightUp_f);
	Cmd_AddCommand("+turnup", IN_TurnUpDown_f, N_("Tilt battlescape camera up"));
	Cmd_AddCommand("-turnup", IN_TurnUpUp_f);
	Cmd_AddCommand("+turndown", IN_TurnDownDown_f, N_("Tilt battlescape camera down"));
	Cmd_AddCommand("-turndown", IN_TurnDownUp_f);
	Cmd_AddCommand("+pantilt", IN_PanTiltDown_f, N_("Move battlescape camera"));
	Cmd_AddCommand("-pantilt", IN_PanTiltUp_f);
	Cmd_AddCommand("+shiftleft", IN_ShiftLeftDown_f, N_("Move battlescape camera left"));
	Cmd_AddCommand("-shiftleft", IN_ShiftLeftUp_f);
	Cmd_AddCommand("+shiftleftup", IN_ShiftLeftUpDown_f, N_("Move battlescape camera top left"));
	Cmd_AddCommand("-shiftleftup", IN_ShiftLeftUpUp_f);
	Cmd_AddCommand("+shiftleftdown", IN_ShiftLeftDownDown_f, N_("Move battlescape camera bottom left"));
	Cmd_AddCommand("-shiftleftdown", IN_ShiftLeftDownUp_f);
	Cmd_AddCommand("+shiftright", IN_ShiftRightDown_f, N_("Move battlescape camera right"));
	Cmd_AddCommand("-shiftright", IN_ShiftRightUp_f);
	Cmd_AddCommand("+shiftrightup", IN_ShiftRightUpDown_f, N_("Move battlescape camera top right"));
	Cmd_AddCommand("-shiftrightup", IN_ShiftRightUpUp_f);
	Cmd_AddCommand("+shiftrightdown", IN_ShiftRightDownDown_f, N_("Move battlescape camera bottom right"));
	Cmd_AddCommand("-shiftrightdown", IN_ShiftRightDownUp_f);
	Cmd_AddCommand("+shiftup", IN_ShiftUpDown_f, N_("Move battlescape camera forward"));
	Cmd_AddCommand("-shiftup", IN_ShiftUpUp_f);
	Cmd_AddCommand("+shiftdown", IN_ShiftDownDown_f, N_("Move battlescape camera backward"));
	Cmd_AddCommand("-shiftdown", IN_ShiftDownUp_f);
	Cmd_AddCommand("+zoomin", IN_ZoomInDown_f, N_("Zoom in"));
	Cmd_AddCommand("-zoomin", IN_ZoomInUp_f);
	Cmd_AddCommand("+zoomout", IN_ZoomOutDown_f, N_("Zoom out"));
	Cmd_AddCommand("-zoomout", IN_ZoomOutUp_f);

	Cmd_AddCommand("+leftmouse", CL_LeftClickDown_f, N_("Left mouse button click (menu)"));
	Cmd_AddCommand("-leftmouse", CL_LeftClickUp_f);
	Cmd_AddCommand("+middlemouse", CL_MiddleClickDown_f, N_("Middle mouse button click (menu)"));
	Cmd_AddCommand("-middlemouse", CL_MiddleClickUp_f);
	Cmd_AddCommand("+rightmouse", CL_RightClickDown_f, N_("Right mouse button click (menu)"));
	Cmd_AddCommand("-rightmouse", CL_RightClickUp_f);
	Cmd_AddCommand("wheelupmouse", CL_WheelUp_f, N_("Mouse wheel up"));
	Cmd_AddCommand("wheeldownmouse", CL_WheelDown_f, N_("Mouse wheel down"));
	Cmd_AddCommand("+select", CL_SelectDown_f, N_("Select objects/Walk to a square/In fire mode, fire etc"));
	Cmd_AddCommand("-select", CL_SelectUp_f);
	Cmd_AddCommand("+action", CL_ActionDown_f, N_("Rotate Battlescape/In fire mode, cancel action"));
	Cmd_AddCommand("-action", CL_ActionUp_f);
	Cmd_AddCommand("+turn", CL_TurnDown_f, N_("Turn soldier toward mouse pointer"));
	Cmd_AddCommand("-turn", CL_TurnUp_f);
	Cmd_AddCommand("+hudradar", CL_HudRadarDown_f, N_("Toggles the hud radar mode"));
	Cmd_AddCommand("-hudradar", CL_HudRadarUp_f);

	Cmd_AddCommand("levelup", CL_LevelUp_f, N_("Slice through terrain at a higher level"));
	Cmd_AddCommand("leveldown", CL_LevelDown_f, N_("Slice through terrain at a lower level"));
	Cmd_AddCommand("zoominquant", CL_ZoomInQuant_f, N_("Zoom in"));
	Cmd_AddCommand("zoomoutquant", CL_ZoomOutQuant_f, N_("Zoom out"));

	Cmd_AddCommand("press", CL_PressKey_f, "Press a key from a command");

	mousePosX = mousePosY = 0.0;

	IN_StartupJoystick();
}

/**
 * @sa CL_SendCommand
 */
void IN_SendKeyEvents (void)
{
	while (keyq_head != keyq_tail) {
		Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].unicode, keyq[keyq_tail].down, CL_Milliseconds());
		keyq_tail = (keyq_tail + 1) & (MAX_KEYQ - 1);
	}
}
