/**
 * @file gl_sdl.c
 * @brief This file contains SDL specific stuff having to do with the OpenGL refresh
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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <SDL.h>

#include <SDL_opengl.h>

#include "../ref_gl/gl_local.h"
#include "glw_linux.h"

#include "../client/keys.h"
#include "rw_linux.h"

/*****************************************************************************/

static qboolean                 X11_active = qfalse;

qboolean have_stencil = qfalse;

static cvar_t	*m_filter;
static cvar_t	*in_mouse;

/* state struct passed in Init */
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

/* Console variables that we need to access from this module */

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

#define MOUSE_MAX 3000
#define MOUSE_MIN 40
qboolean mouse_active;
int mx, my, mouse_buttonstate;

/* this is inside the renderer shared lib, so these are called from vid_so */

void RW_IN_PlatformInit( void )
{
}

void RW_IN_Activate(qboolean active)
{
	mouse_active = qtrue;
}

/*****************************************************************************/

int XLateKey(unsigned int keysym)
{
	int key = 0;

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
		if(keysym < 128)
			key = keysym;
		break;
	}

	return key;
}

static unsigned char KeyStates[SDLK_LAST];

void GetEvent(SDL_Event *event)
{
	unsigned int key;
	float win_x = ( 1.0 + vid.rx );
	float win_y = ( 1.0 + vid.ry );

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
		keyq[keyq_head].down = event->type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse;
		keyq[keyq_head].key = mouse_buttonstate;
		keyq_head = (keyq_head + 1) & 63;
		break;
	case SDL_MOUSEMOTION:
		if (mouse_active)
		{
			mx = event->motion.x * win_x; /* * sensitivity->value */
			my = event->motion.y * win_y; /* * sensitivity->value */
		}
		break;
	case SDL_KEYDOWN:
		if ( (KeyStates[SDLK_LALT] || KeyStates[SDLK_RALT]) &&
			(event->key.keysym.sym == SDLK_RETURN) ) {

			SDL_WM_ToggleFullScreen(surface);

			if (surface->flags & SDL_FULLSCREEN) {
				ri.Cvar_SetValue( "vid_fullscreen", 1 );
			} else {
				ri.Cvar_SetValue( "vid_fullscreen", 0 );
			}

			vid_fullscreen->modified = qfalse; /* we just changed it with SDL. */

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
			keyq[keyq_head].down = qtrue;
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
				keyq[keyq_head].down = qfalse;
				keyq_head = (keyq_head + 1) & 63;
			}
		}
		break;
	case SDL_QUIT:
		ri.Cmd_ExecuteText(EXEC_NOW, "quit");
		break;
	}

}

/*****************************************************************************/

void *GLimp_GetProcAddress(const char *func)
{
	return SDL_GL_GetProcAddress(func);
}

static void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	GLimp_Shutdown();
	exit(0);
}

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

qboolean GLimp_Init( void *hInstance, void *wndProc )
{
	if (SDL_WasInit(SDL_INIT_AUDIO|SDL_INIT_CDROM|SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			Sys_Error("SDL Init failed: %s\n", SDL_GetError());
			return qfalse;
		}
	} else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Sys_Error("SDL Init failed: %s\n", SDL_GetError());
			return qfalse;
		}
	}

	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	InitSig();

	return qtrue;
}

static void SetSDLIcon( void )
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
	cvar_t* use_stencil = NULL;

	/* Just toggle fullscreen if that's all that has been changed */
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) {
		int isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);

		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen == isfullscreen)
			return qtrue;
	}

	srandom(getpid());

	/* free resources in use */
	if (surface)
		SDL_FreeSurface(surface);

	/* let the sound and input subsystems know about the new window */
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
		return qfalse;
	}

	/* stencilbuffer shadows */
 	if (use_stencil) {
		int stencil_bits;

		have_stencil = qfalse;

		if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits))
		{
			ri.Con_Printf(PRINT_ALL, "I: got %d bits of stencil\n", stencil_bits);
			if (stencil_bits >= 1)
				have_stencil = qtrue;
		}
	}

	SDL_WM_SetCaption("UFO:AI", "UFO:Alien Invasion");

	SDL_ShowCursor( SDL_DISABLE );

	X11_active = qtrue;

	return qtrue;
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

	if ( !ri.Vid_GetModeInfo( (int*)pwidth, (int*)pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d %s\n", *pwidth, *pheight, (char *)qglGetString (GL_RENDERER) );

	if ( !GLimp_InitGraphics( fullscreen ) ) {
		/* failed to set a valid mode in windowed mode */
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

	SDL_ShowCursor(SDL_ENABLE);

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

	X11_active = qfalse;
}

void GLimp_AppActivate( qboolean active )
{
}

/*=============================================================================== */

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

	/* get events from x server */
	if (X11_active)
	{
		while (SDL_PollEvent(&event))
			GetEvent(&event);

		if (!mx && !my)
			SDL_GetRelativeMouseState(&mx, &my);

		if (vid_grabmouse->modified) {
			vid_grabmouse->modified = qfalse;

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

void RW_IN_Init(in_state_t *in_state_p)
{
	in_state = in_state_p;

	/* mouse variables */
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
	mouse_avail = qtrue;
}

void RW_IN_Shutdown(void)
{
	RW_IN_Activate (qfalse);

	if (mouse_avail)
	{
		mouse_avail = qfalse;
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
	if ( mx < 0 ) mx = 0;
	if ( my < 0 ) my = 0;
	if ( mx > 1024 ) mx = 1024;
	if ( my > 768 ) my = 768;
	*x = mx;
	*y = my;
}

void RW_IN_Frame (void)
{
}
