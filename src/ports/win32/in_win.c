/**
 * @file in_win.c
 * @brief windows 95 mouse code
 * 02/21/97 JCB Added extended DirectInput code to support external controllers.
 */

/*
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

#include "../../client/client.h"
#include "winquake.h"

extern	unsigned sys_msg_time;

cvar_t *in_mouse;

qboolean in_appactive;

/*
============================================================
KEYBOARD CONTROL
============================================================
*/

#ifdef NEWKBCODE
HKL		kbLayout;
#endif /* NEWKBCODE */

/**
 * @brief Map from windows to ufo keynums
 */
int IN_MapKey (int wParam, int lParam)
{
	static const byte scanToKey[128] = {
		0,			K_ESCAPE,	'1',		'2',		'3',		'4',		'5',		'6',
		'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE,9,		// 0
		'q',		'w',		'e',		'r',		't',		'y',		'u',		'i',
		'o',		'p',		'[',		']',		K_ENTER,	K_CTRL,		'a',		's',	// 1
		'd',		'f',		'g',		'h',		'j',		'k',		'l',		';',
		'\'',		'`',		K_SHIFT,	'\\',		'z',		'x',		'c',		'v',	// 2
		'b',		'n',		'm',		',',		'.',		'/',		K_SHIFT,	'*',
		K_ALT,		' ',		K_CAPSLOCK,	K_F1,		K_F2,		K_F3,		K_F4,		K_F5,	// 3
		K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_PAUSE,	0,			K_HOME,
		K_UPARROW,	K_PGUP,		K_KP_MINUS,	K_LEFTARROW,K_KP_5,		K_RIGHTARROW,K_KP_PLUS,	K_END,	// 4
		K_DOWNARROW,K_PGDN,		K_INS,		K_DEL,		0,			0,			0,			K_F11,
		K_F12,		0,			0,			0,			0,			0,			0,			0,		// 5
		0,			0,			0,			0,			0,			0,			0,			0,
		0,			0,			0,			0,			0,			0,			0,			0,		// 6
		0,			0,			0,			0,			0,			0,			0,			0,
		0,			0,			0,			0,			0,			0,			0,			0		// 7
	};
	int		modified;
#ifdef NEWKBCODE
	int		scanCode;
	byte	kbState[256];
	byte	result[4];

	/* new stuff */
	switch (wParam) {
	case VK_TAB:
		return K_TAB;
	case VK_RETURN:
		return K_ENTER;
	case VK_ESCAPE:
		return K_ESCAPE;
	case VK_SPACE:
		return K_SPACE;

	case VK_BACK:
		return K_BACKSPACE;
	case VK_UP:
		return K_UPARROW;
	case VK_DOWN:
		return K_DOWNARROW;
	case VK_LEFT:
		return K_LEFTARROW;
	case VK_RIGHT:
		return K_RIGHTARROW;

/*	case VK_LMENU:
	case VK_RMENU:*/
	case VK_MENU:
		return K_ALT;

/*	case VK_LCONTROL:
	case VK_RCONTROL:*/
	case VK_CONTROL:
		return K_CTRL;

	case VK_SHIFT:
		return K_SHIFT;
	case VK_LSHIFT:
		return K_LSHIFT;
	case VK_RSHIFT:
		return K_RSHIFT;

	case VK_CAPITAL:
		return K_CAPSLOCK;

	case VK_F1:
		return K_F1;
	case VK_F2:
		return K_F2;
	case VK_F3:
		return K_F3;
	case VK_F4:
		return K_F4;
	case VK_F5:
		return K_F5;
	case VK_F6:
		return K_F6;
	case VK_F7:
		return K_F7;
	case VK_F8:
		return K_F8;
	case VK_F9:
		return K_F9;
	case VK_F10:
		return K_F10;
	case VK_F11:
		return K_F11;
	case VK_F12:
		return K_F12;

	case VK_INSERT:
		return K_INS;
	case VK_DELETE:
		return K_DEL;
	case VK_NEXT:
		return K_PGDN;
	case VK_PRIOR:
		return K_PGUP;
	case VK_HOME:
		return K_HOME;
	case VK_END:
		return K_END;

	case VK_NUMPAD7:
		return K_KP_HOME;
	case VK_NUMPAD8:
		return K_KP_UPARROW;
	case VK_NUMPAD9:
		return K_KP_PGUP;
	case VK_NUMPAD4:
		return K_KP_LEFTARROW;
	case VK_NUMPAD5:
		return K_KP_FIVE;
	case VK_NUMPAD6:
		return K_KP_RIGHTARROW;
	case VK_NUMPAD1:
		return K_KP_END;
	case VK_NUMPAD2:
		return K_KP_DOWNARROW;
	case VK_NUMPAD3:
		return K_KP_PGDN;
	case VK_NUMPAD0:
		return K_KP_INS;
	case VK_DECIMAL:
		return K_KP_DEL;
	case VK_DIVIDE:
		return K_KP_SLASH;
	case VK_SUBTRACT:
		return K_KP_MINUS;
	case VK_ADD:
		return K_KP_PLUS;

	case VK_PAUSE:
		return K_PAUSE;

	default:
		break;
	}
#endif /* NEWKBCODE */

	/* old stuff */
	modified = (lParam >> 16) & 255;
	if (modified < 128) {
		modified = scanToKey[modified];
		if (lParam & (1 << 24)) {
			switch (modified) {
			case 0x0D:
				return K_KP_ENTER;
			case 0x2F:
				return K_KP_SLASH;
			case 0xAF:
				return K_KP_PLUS;
			}
		} else {
			switch (modified) {
			case K_HOME:
				return K_KP_HOME;
			case K_UPARROW:
				return K_KP_UPARROW;
			case K_PGUP:
				return K_KP_PGUP;
			case K_LEFTARROW:
				return K_KP_LEFTARROW;
			case K_RIGHTARROW:
				return K_KP_RIGHTARROW;
			case K_END:
				return K_KP_END;
			case K_DOWNARROW:
				return K_KP_DOWNARROW;
			case K_PGDN:
				return K_KP_PGDN;
			case K_INS:
				return K_KP_INS;
			case K_DEL:
				return K_KP_DEL;
			}
		}
	}

#ifdef NEWKBCODE
	/* get the VK_ keyboard state */
	if (!GetKeyboardState (kbState))
		return modified;

	/* convert ascii */
	scanCode = (lParam >> 16) & 255;
	if (ToAsciiEx (wParam, scanCode, kbState, (uint16 *)result, 0, kbLayout) < 1)
		return modified;

	return result[0];
#else
	return modified;
#endif /* NEWKBCODE */
}


/*
============================================================
MOUSE CONTROL
============================================================
*/

/* mouse variables */
cvar_t *m_filter;

int mouse_buttons;
int mouse_oldbuttonstate;
POINT current_pos;
int mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;
int old_x, old_y;

qboolean mouseactive;	/* false when not focus app */

qboolean restore_spi;
qboolean mouseinitialized;
int originalmouseparms[3], newmouseparms[3] = {0, 0, 1};
qboolean mouseparmsvalid;

int window_center_x, window_center_y;
RECT window_rect;


/**
 * @brief Called when the window gains focus or changes in some way
 */
void IN_ActivateMouse (void)
{
	int width, height;

	if (!mouseinitialized)
		return;
	if (!in_mouse->value) {
		mouseactive = qfalse;
		return;
	}
	if (mouseactive)
		return;

	mouseactive = qtrue;

	if (mouseparmsvalid)
		restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);

	width = GetSystemMetrics (SM_CXSCREEN);
	height = GetSystemMetrics (SM_CYSCREEN);

	GetWindowRect ( cl_hwnd, &window_rect);
	if (window_rect.left < 0)
		window_rect.left = 0;
	if (window_rect.top < 0)
		window_rect.top = 0;
	if (window_rect.right >= width)
		window_rect.right = width-1;
	if (window_rect.bottom >= height-1)
		window_rect.bottom = height-1;

	window_center_x = (window_rect.right + window_rect.left)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;

	SetCursorPos (window_center_x, window_center_y);

	old_x = window_center_x;
	old_y = window_center_y;

#if 0
	if (vid_grabmouse->value)
#endif
		SetCapture ( cl_hwnd );
	ClipCursor (&window_rect);
	while (ShowCursor (FALSE) >= 0);
}


/**
 * @brief Called when the window loses focus
 */
void IN_DeactivateMouse (void)
{
	if (!mouseinitialized)
		return;
	if (!mouseactive)
		return;

	if (restore_spi)
		SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

	mouseactive = qfalse;

	ClipCursor (NULL);
#if 0
	if (vid_grabmouse->value)
#endif
		ReleaseCapture ();
	while (ShowCursor (TRUE) < 0);
}


/**
 * @brief
 */
void IN_StartupMouse (void)
{
	cvar_t		*cv;

	cv = Cvar_Get ("in_initmouse", "1", CVAR_NOSET, NULL);
	if ( !cv->value )
		return;

	mouseinitialized = qtrue;
	mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);
	mouse_buttons = 3;
}

/**
 * @brief
 */
void IN_MouseEvent (int mstate)
{
	int		i;

	if (!mouseinitialized)
		return;

	/* perform button actions */
	for (i=0 ; i<mouse_buttons ; i++) {
		if ( (mstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
			Key_Event (K_MOUSE1 + i, qtrue, sys_msg_time);

		if ( !(mstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
				Key_Event (K_MOUSE1 + i, qfalse, sys_msg_time);
	}

	mouse_oldbuttonstate = mstate;
}


/**
 * @brief
 */
void IN_GetMousePos (int *mx, int *my)
{
	if (!mouseactive || !GetCursorPos(&current_pos) ) {
		*mx = VID_NORM_WIDTH/2;
		*my = VID_NORM_HEIGHT/2;
	} else {
		*mx = VID_NORM_WIDTH * (current_pos.x - window_rect.left) / (window_rect.right - window_rect.left);
		*my = VID_NORM_HEIGHT * (current_pos.y - window_rect.top)  / (window_rect.bottom - window_rect.top);
	}
}


/*
=========================================================================
VIEW CENTERING
=========================================================================
*/

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;

/**
 * @brief
 */
void IN_Init (void)
{
	/* mouse variables */
	m_filter = Cvar_Get ("m_filter", "0", 0, NULL);
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE, NULL);

	/* centering */
	v_centermove = Cvar_Get ("v_centermove", "0.15", 0, NULL);
	v_centerspeed = Cvar_Get ("v_centerspeed", "500", 0, NULL);

	IN_StartupMouse ();
#ifdef NEWKBCODE
	kbLayout = GetKeyboardLayout(0);
#endif /* NEWKBCODE */
}

/**
 * @brief
 */
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
}


/**
 * @brief Called when the main window gains or loses focus.
 * @note The window may have been destroyed and recreated between a deactivate and an activate.
 */
void IN_Activate (qboolean active)
{
	in_appactive = active;
	mouseactive = !active;		/* force a new window check or turn off */
}


/**
 * @brief Called every frame, even if not generating commands
 */
void IN_Frame (void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse || !in_appactive) {
		IN_DeactivateMouse ();
		return;
	}

#if 0
	/* TODO: NEEDED? */
	if ( !cl.refresh_prepped || cls.key_dest == key_console) {
		/* temporarily deactivate if in fullscreen */
		if (!Cvar_VariableValue ("vid_fullscreen")) {
			IN_DeactivateMouse ();
			return;
		}
	}
#endif

	IN_ActivateMouse ();
}

/**
 * @brief
 */
void IN_ClearStates (void)
{
	mx_accum = 0;
	my_accum = 0;
	mouse_oldbuttonstate = 0;
}

/**
 * @brief
 */
void IN_Commands (void)
{
}
