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
/*
** GL_SDL.C
**
** This file contains ALL Linux specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
** GLimp_SetGamma
**
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <SDL.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include "../ref_gl/gl_local.h"
#include "glw_linux.h"

#include "../client/keys.h"
#include "rw_linux.h"

#ifdef Joystick
# if defined (__linux__)
#include <linux/joystick.h>
# elif defined (__FreeBSD__)
#include <sys/joystick.h>
# endif
#include <glob.h>
#endif

/*****************************************************************************/

static qboolean                 X11_active = false;

qboolean have_stencil = false;

static cvar_t	*m_filter;
static cvar_t	*in_mouse;

// state struct passed in Init
static in_state_t	*in_state;

static cvar_t *sensitivity;
static cvar_t *lookstrafe;
static cvar_t *m_side;
static cvar_t *m_yaw;
static cvar_t *m_pitch;
static cvar_t *m_forward;
static cvar_t *freelook;
static qboolean        mouse_avail;

static SDL_Surface *surface;

struct
{
	unsigned char key;
	int down;
} keyq[64];
int keyq_head=0;
int keyq_tail=0;

int config_notify=0;
int config_notify_width;
int config_notify_height;

glwstate_t glw_state;
extern cvar_t *use_stencil;

// Console variables that we need to access from this module

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

#define MOUSE_MAX 3000
#define MOUSE_MIN 40
qboolean mouse_active;
int mx, my, mouse_buttonstate;

// this is inside the renderer shared lib, so these are called from vid_so

static float old_grabmouse;

/************************
 * Joystick
 ************************/
#ifdef Joystick
static SDL_Joystick *joy;
static int joy_oldbuttonstate;
static int joy_numbuttons;
#endif

void RW_IN_PlatformInit( void ) {
	vid_grabmouse = ri.Cvar_Get ("vid_grabmouse", "0", CVAR_ARCHIVE);
}

#ifdef Joystick
qboolean CloseJoystick(void) {
	if (joy) {
		SDL_JoystickClose(joy);
		joy = NULL;
	}
	return true;
}

void PlatformJoyCommands(int *axis_vals, int *axis_map) {
	int i;
	int key_index;
	if (joy) {
		for (i=0 ; i < joy_numbuttons ; i++) {
			if ( SDL_JoystickGetButton(joy, i) && joy_oldbuttonstate!=i ) {
				key_index = (i < 4) ? K_JOY1 : K_AUX1;
				in_state->Key_Event_fp (key_index + i, true);
				joy_oldbuttonstate = i;
			}

			if ( !SDL_JoystickGetButton(joy, i) && joy_oldbuttonstate!=i ) {
				key_index = (i < 4) ? K_JOY1 : K_AUX1;
				in_state->Key_Event_fp (key_index + i, false);
				joy_oldbuttonstate = i;
			}
		}
		for (i=0;i<6;i++) {
			axis_vals[axis_map[i]] = (int)SDL_JoystickGetAxis(joy, i);
		}
	}
}
#endif

#if 0
static void IN_DeactivateMouse( void )
{
	if (!mouse_avail)
		return;

	if (mouse_active) {
		/* uninstall_grabs(); */
		mouse_active = false;
	}
}

static void IN_ActivateMouse( void )
{
	if (!mouse_avail)
		return;

	if (!mouse_active) {
		mx = my = 0; // don't spazz
		/* install_grabs(); */
		mouse_active = true;
	}
}
#endif

void RW_IN_Activate(qboolean active)
{
#if 0
	if (active)
		IN_ActivateMouse();
	else
		IN_DeactivateMouse();
#endif
	mouse_active = true;
}

/*****************************************************************************/

#if 0 /* SDL parachute should catch everything... */
// ========================================================================
// Tragic death handler
// ========================================================================
void TragicDeath(int signal_num)
{
	/* SDL_Quit(); */
	Sys_Error("This death brought to you by the number %d\n", signal_num);
}
#endif

int XLateKey(unsigned int keysym)
{
	int key;

	key = 0;
	switch(keysym) {
	case SDLK_KP9:			key = K_KP_PGUP; break;
	case SDLK_PAGEUP:		key = K_PGUP; break;

	case SDLK_KP3:			key = K_KP_PGDN; break;
	case SDLK_PAGEDOWN:		key = K_PGDN; break;

	case SDLK_KP7:			key = K_KP_HOME; break;
	case SDLK_HOME:			key = K_HOME; break;

	case SDLK_KP1:			key = K_KP_END; break;
	case SDLK_END:			key = K_END; break;

	case SDLK_KP4:			key = K_KP_LEFTARROW; break;
	case SDLK_LEFT:			key = K_LEFTARROW; break;

	case SDLK_KP6:			key = K_KP_RIGHTARROW; break;
	case SDLK_RIGHT:		key = K_RIGHTARROW; break;

	case SDLK_KP2:			key = K_KP_DOWNARROW; break;
	case SDLK_DOWN:			key = K_DOWNARROW; break;

	case SDLK_KP8:			key = K_KP_UPARROW; break;
	case SDLK_UP:			key = K_UPARROW; break;

	case SDLK_ESCAPE:		key = K_ESCAPE; break;

	case SDLK_KP_ENTER:		key = K_KP_ENTER; break;
	case SDLK_RETURN:		key = K_ENTER; break;

	case SDLK_TAB:			key = K_TAB; break;

	case SDLK_F1:			key = K_F1; break;
	case SDLK_F2:			key = K_F2; break;
	case SDLK_F3:			key = K_F3; break;
	case SDLK_F4:			key = K_F4; break;
	case SDLK_F5:			key = K_F5; break;
	case SDLK_F6:			key = K_F6; break;
	case SDLK_F7:			key = K_F7; break;
	case SDLK_F8:			key = K_F8; break;
	case SDLK_F9:			key = K_F9; break;
	case SDLK_F10:			key = K_F10; break;
	case SDLK_F11:			key = K_F11; break;
	case SDLK_F12:			key = K_F12; break;

	case SDLK_BACKSPACE:		key = K_BACKSPACE; break;

	case SDLK_KP_PERIOD:		key = K_KP_DEL; break;
	case SDLK_DELETE:		key = K_DEL; break;

	case SDLK_PAUSE:		key = K_PAUSE; break;

	case SDLK_LSHIFT:
	case SDLK_RSHIFT:		key = K_SHIFT; break;

	case SDLK_LCTRL:
	case SDLK_RCTRL:		key = K_CTRL; break;

	case SDLK_LMETA:
	case SDLK_RMETA:
	case SDLK_LALT:
	case SDLK_RALT:			key = K_ALT; break;

	case SDLK_KP5:			key = K_KP_5; break;

	case SDLK_INSERT:		key = K_INS; break;
	case SDLK_KP0:			key = K_KP_INS; break;

	case SDLK_KP_MULTIPLY:		key = '*'; break;
	case SDLK_KP_PLUS:		key = K_KP_PLUS; break;
	case SDLK_KP_MINUS:		key = K_KP_MINUS; break;
	case SDLK_KP_DIVIDE:		key = K_KP_SLASH; break;

	/* suggestions on how to handle this better would be appreciated */
	case SDLK_WORLD_7:		key = '`'; break;

	default: /* assuming that the other sdl keys are mapped to ascii */
		if (keysym < 128)
			key = keysym;
		break;
	}

	return key;
}

static unsigned char KeyStates[SDLK_LAST];

void GetEvent(SDL_Event *event)
{
	unsigned int key;

	switch(event->type) {
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event->button.button)
		{
			case  1:
				mouse_buttonstate = K_MOUSE1;
				break;
			case  2:
				mouse_buttonstate = K_MOUSE3;
				break;
			case  3:
				mouse_buttonstate = K_MOUSE2;
				break;
			case  4:
				mouse_buttonstate = K_MWHEELUP;
				break;
			case  5:
				mouse_buttonstate = K_MWHEELDOWN;
				break;
#if 0
			case  6:
				mouse_buttonstate = K_MOUSE4; break;
			case  7:
				mouse_buttonstate = K_MOUSE5; break;
#endif
			default: mouse_buttonstate = K_AUX1 + (event->button.button - 8)%16; break;
		}
		keyq[keyq_head].down = event->type == SDL_MOUSEBUTTONDOWN ? true : false;
		keyq[keyq_head].key = mouse_buttonstate;
		keyq_head = (keyq_head + 1) & 63;
		break;
	case SDL_MOUSEMOTION:
		if (mouse_active)
		{
			mx = event->button.x; // * sensitivity->value
			my = event->button.y; // * sensitivity->value
			if ( mx > vid.width ) mx = vid.width;
			if ( my > vid.height ) my = vid.height;
			if ( mx < 0 ) mx = 0;
			if ( my < 0 ) my = 0;
		}
		break;
#ifdef Joystick
	case SDL_JOYBUTTONDOWN:
		keyq[keyq_head].key =
		((((SDL_JoyButtonEvent*)event)->button < 4)?K_JOY1:K_AUX1)+
		((SDL_JoyButtonEvent*)event)->button;
		keyq[keyq_head].down = true;
		keyq_head = (keyq_head + 1) & 63;
		break;
	case SDL_JOYBUTTONUP:
		keyq[keyq_head].key =
		((((SDL_JoyButtonEvent*)event)->button < 4)?K_JOY1:K_AUX1)+
		((SDL_JoyButtonEvent*)event)->button;
		keyq[keyq_head].down = false;
		keyq_head = (keyq_head + 1) & 63;
		break;
#endif
	case SDL_KEYDOWN:
		if ( (KeyStates[SDLK_LALT] || KeyStates[SDLK_RALT]) &&
			(event->key.keysym.sym == SDLK_RETURN) ) {

			SDL_WM_ToggleFullScreen(surface);

			if (surface->flags & SDL_FULLSCREEN) {
				ri.Cvar_SetValue( "vid_fullscreen", 1 );
			} else {
				ri.Cvar_SetValue( "vid_fullscreen", 0 );
			}

			vid_fullscreen->modified = false; /* we just changed it with SDL. */

			break; /* ignore this key */
		}

		if ( (KeyStates[SDLK_LCTRL] || KeyStates[SDLK_RCTRL]) &&
			(event->key.keysym.sym == SDLK_g) ) {
			SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
			/*
			SDL_WM_GrabInput((gm == SDL_GRAB_ON) ? SDL_GRAB_OFF : SDL_GRAB_ON);
			gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
			*/
			ri.Cvar_SetValue( "vid_grabmouse", (gm == SDL_GRAB_ON) ? /*1*/ 0 : /*0*/ 1 );

			break; /* ignore this key */
		}

		KeyStates[event->key.keysym.sym] = 1;

		key = XLateKey(event->key.keysym.sym);
		if (key) {
			keyq[keyq_head].key = key;
			keyq[keyq_head].down = true;
			keyq_head = (keyq_head + 1) & 63;
		}
		break;
	case SDL_VIDEOEXPOSE:
		break;
	case SDL_KEYUP:
		if (KeyStates[event->key.keysym.sym]) {
			KeyStates[event->key.keysym.sym] = 0;

			key = XLateKey(event->key.keysym.sym);
			if (key) {
				keyq[keyq_head].key = key;
				keyq[keyq_head].down = false;
				keyq_head = (keyq_head + 1) & 63;
			}
		}
		break;
	case SDL_QUIT:
		ri.Cmd_ExecuteText(EXEC_NOW, "quit");
		break;
	}

}

#ifdef Joystick
qboolean OpenJoystick(cvar_t *joy_dev) {
	int num_joysticks, i;
	joy = NULL;

	if (!(SDL_INIT_JOYSTICK&SDL_WasInit(SDL_INIT_JOYSTICK))) {
		ri.Con_Printf(PRINT_ALL, "SDL Joystick not initialized, trying to init...\n");
		SDL_Init(SDL_INIT_JOYSTICK);
	}
	ri.Con_Printf(PRINT_ALL, "Trying to start-up joystick...\n");
	if ((num_joysticks=SDL_NumJoysticks())) {
		for(i=0;i<num_joysticks;i++) {
			ri.Con_Printf(PRINT_ALL, "Trying joystick [%s]\n",
					SDL_JoystickName(i));
			if (!SDL_JoystickOpened(i)) {
				joy = SDL_JoystickOpen(i);
				if (joy) {
					ri.Con_Printf(PRINT_ALL, "Joytick activated.\n");
					joy_numbuttons = SDL_JoystickNumButtons(joy);
					break;
				}
			}
		}
		if (!joy) {
			ri.Con_Printf(PRINT_ALL, "Failed to open any joysticks\n");
			return false;
		}
	}
	else {
		ri.Con_Printf(PRINT_ALL, "No joysticks available\n");
		return false;
	}
	return true;
}
#endif
//
/*****************************************************************************/

void *GLimp_GetProcAddress(const char *func)
{
	return SDL_GL_GetProcAddress(func);
}

qboolean GLimp_Init( void *hInstance, void *wndProc )
{
	if (SDL_WasInit(SDL_INIT_AUDIO|SDL_INIT_CDROM|SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			Sys_Error("SDL Init failed: %s\n", SDL_GetError());
			return false;
		}
	} else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Sys_Error("SDL Init failed: %s\n", SDL_GetError());
			return false;
		}
	}

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	// catch signals so i can turn on auto-repeat
#if 0
	{
		struct sigaction sa;
		sigaction(SIGINT, 0, &sa);
		sa.sa_handler = TragicDeath;
		sigaction(SIGINT, &sa, 0);
		sigaction(SIGTERM, &sa, 0);
	}
#endif
	return true;
}

static void SetSDLIcon()
{
#include "ufoicon.xbm"
	SDL_Surface *icon;
	SDL_Color color;
	Uint8 *ptr;
	int i, mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE, ufoicon_width, ufoicon_height, 8, 0, 0, 0, 0);
	if (icon == NULL)
		return; /* oh well... */
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	color.r = 255;
	color.g = 255;
	color.b = 255;
	SDL_SetColors(icon, &color, 0, 1); /* just in case */
	color.r = 0;
	color.g = 16;
	color.b = 0;
	SDL_SetColors(icon, &color, 1, 1);

	ptr = (Uint8 *)icon->pixels;
	for (i = 0; i < sizeof(ufoicon_bits); i++) {
		for (mask = 1; mask != 0x100; mask <<= 1) {
			*ptr = (ufoicon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}
	}

	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
}

static qboolean GLimp_InitGraphics( qboolean fullscreen )
{
	int flags;
	cvar_t* use_stencil;

	/* Just toggle fullscreen if that's all that has been changed */
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) {
		int isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);

		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen == isfullscreen)
			return true;
	}

	srandom(getpid());

	// free resources in use
	if (surface)
		SDL_FreeSurface(surface);

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (vid.width, vid.height);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (use_stencil)
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	flags = SDL_OPENGL;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;

	SetSDLIcon(); /* currently uses ufoicon.xbm data */

	if ((surface = SDL_SetVideoMode(vid.width, vid.height, 0, flags)) == NULL) {
		Sys_Error("(SDLGL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return false;
	}

	// stencilbuffer shadows
 	if (use_stencil) {
		int stencil_bits;

		have_stencil = false;

		if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits))
		{
			ri.Con_Printf(PRINT_ALL, "I: got %d bits of stencil\n", stencil_bits);
			if (stencil_bits >= 1)
				have_stencil = true;
		}
	}

	SDL_WM_SetCaption("UFO:AI", "UFO:Alien Invasion");

	SDL_ShowCursor( SDL_DISABLE );

	X11_active = true;

	return true;
}

void GLimp_BeginFrame( float camera_seperation )
{
}

void GLimp_EndFrame (void)
{
	SDL_GL_SwapBuffers();
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen )
{
	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d %s\n", *pwidth, *pheight, (char *)qglGetString (GL_RENDERER) );

	if ( !GLimp_InitGraphics( fullscreen ) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
** GLimp_SetGamma
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	float g;

	g = Cvar_Get("vid_gamma", "1.0", 0)->value;
	SDL_SetGamma(g, g, g);
}

void GLimp_Shutdown( void )
{
	if (surface)
		SDL_FreeSurface(surface);
	surface = NULL;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

	X11_active = false;
}

void GLimp_AppActivate( qboolean active )
{
}

//===============================================================================

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize-1)) - psize;

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

	r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
		Sys_Error("Protection change failed\n");
}

/*****************************************************************************/
/* KEYBOARD                                                                  */
/*****************************************************************************/

Key_Event_fp_t Key_Event_fp;

void getMouse(int *x, int *y, int *state)
{
	*x = mx;
	*y = my;
	*state = mouse_buttonstate;
}

void KBD_Init(Key_Event_fp_t fp)
{
	Key_Event_fp = fp;
	RW_IN_PlatformInit();
}

void KBD_Update(void)
{
	SDL_Event event;
	static int KBD_Update_Flag;

	if (KBD_Update_Flag == 1)
		return;

	KBD_Update_Flag = 1;

	// get events from x server
	if (X11_active)
	{
		while (SDL_PollEvent(&event))
			GetEvent(&event);

		if (!mx && !my)
			SDL_GetRelativeMouseState(&mx, &my);

		if (old_grabmouse != vid_grabmouse->value) {
			old_grabmouse = vid_grabmouse->value;

			if (!vid_grabmouse->value) {
				/* ungrab the pointer */
				SDL_WM_GrabInput(SDL_GRAB_OFF);
			} else {
				/* grab the pointer */
				SDL_WM_GrabInput(SDL_GRAB_ON);
			}
		}
		while (keyq_head != keyq_tail)
		{
			in_state->Key_Event_fp(keyq[keyq_tail].key, keyq[keyq_tail].down);
			keyq_tail = (keyq_tail + 1) & 63;
		}
	}
	else
		ri.Con_Printf( PRINT_ALL, "X11 not active right now\n" );
	KBD_Update_Flag = 0;
}

void KBD_Close(void)
{
	keyq_head = 0;
	keyq_tail = 0;

	memset(keyq, 0, sizeof(keyq));
}

#if 0
void Fake_glColorTableEXT( GLenum target, GLenum internalformat,
                             GLsizei width, GLenum format, GLenum type,
                             const GLvoid *table )
{
	byte temptable[256][4];
	byte *intbl;
	int i;

	for (intbl = (byte *)table, i = 0; i < 256; i++) {
		temptable[i][2] = *intbl++;
		temptable[i][1] = *intbl++;
		temptable[i][0] = *intbl++;
		temptable[i][3] = 255;
	}
	qgl3DfxSetPaletteEXT((GLuint *)temptable);
}
#endif

void RW_IN_Init(in_state_t *in_state_p)
{
	in_state = in_state_p;

	// mouse variables
	m_filter = ri.Cvar_Get ("m_filter", "0", 0);
	in_mouse = ri.Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);

	freelook = ri.Cvar_Get( "freelook", "0", 0 );
	lookstrafe = ri.Cvar_Get ("lookstrafe", "0", 0);
	sensitivity = ri.Cvar_Get ("sensitivity", "2", CVAR_ARCHIVE);
	m_pitch = ri.Cvar_Get ("m_pitch", "0.022", 0);

	m_yaw = ri.Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = ri.Cvar_Get ("m_forward", "1", 0);
	m_side = ri.Cvar_Get ("m_side", "0.8", 0);

	mx = my = 0.0;
	mouse_avail = true;
}

void RW_IN_Shutdown(void)
{
	RW_IN_Activate (false);

	if (mouse_avail)
	{
		mouse_avail = false;
	}
}

/*
===========
IN_Commands
===========
*/
void RW_IN_Commands (void)
{
}

/*
===========
IN_GetMousePos
===========
*/
void RW_IN_GetMousePos (int *x, int *y)
{
	*x = mx;
	*y = my;
}

void RW_IN_Frame (void)
{
}
