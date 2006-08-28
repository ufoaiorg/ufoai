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

	cv = Cvar_Get ("in_initmouse", "1", CVAR_NOSET);
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
	m_filter = Cvar_Get ("m_filter", "0", 0);
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);

	/* centering */
	v_centermove = Cvar_Get ("v_centermove", "0.15", 0);
	v_centerspeed = Cvar_Get ("v_centerspeed", "500", 0);

	IN_StartupMouse ();
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
