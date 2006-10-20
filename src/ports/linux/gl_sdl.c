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

#include "../../ref_gl/gl_local.h"

#include <signal.h>
#include <sys/mman.h>

#ifdef USE_SDL_FRAMEWORK
#	include <SDL/SDL.h>
#	include <SDL/SDL_syswm.h>
#else
#	include <SDL.h>
#	include <SDL_syswm.h>
#endif

/*#include <SDL_opengl.h>*/

#include "../../client/keys.h"
#include "rw_linux.h"
#include "glw_linux.h"

glwstate_t glw_state;

static qboolean SDL_active = qfalse;

qboolean have_stencil = qfalse;

static cvar_t	*m_filter;
static cvar_t	*in_mouse;
static cvar_t	*sdl_debug;

/* state struct passed in Init */
static in_state_t *in_state;

static cvar_t *sensitivity;
static qboolean mouse_avail;

static SDL_Surface *surface;

struct
{
	unsigned char key;
	int down;
} keyq[64];
static int keyq_head=0;
static int keyq_tail=0;

/* Console variables that we need to access from this module */

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

#define MOUSE_MAX 3000
#define MOUSE_MIN 40
qboolean mouse_active;
int mx, my, mouse_buttonstate;

/* this is inside the renderer shared lib, so these are called from vid_so */

/**
 * @brief
 */
void RW_IN_PlatformInit( void )
{
}

/**
 * @brief
 */
void RW_IN_Activate(qboolean active)
{
	mouse_active = qtrue;
}

/*****************************************************************************/

/**
  * @brief Translate the keys to ufo keys
  */
static int SDLateKey(SDL_keysym *keysym, int *key)
{
	int buf = 0;
	*key = 0;

	switch (keysym->sym) {
	case SDLK_KP9:
		*key = K_KP_PGUP;
		break;
	case SDLK_PAGEUP:
		*key = K_PGUP;
		break;
	case SDLK_KP3:
		*key = K_KP_PGDN;
		break;
	case SDLK_PAGEDOWN:
		*key = K_PGDN;
		break;
	case SDLK_KP7:
		*key = K_KP_HOME;
		break;
	case SDLK_HOME:
		*key = K_HOME;
		break;
	case SDLK_KP1:
		*key = K_KP_END;
		break;
	case SDLK_END:
		*key = K_END;
		break;
	case SDLK_KP4:
		*key = K_KP_LEFTARROW;
		break;
	case SDLK_LEFT:
		*key = K_LEFTARROW;
		break;
	case SDLK_KP6:
		*key = K_KP_RIGHTARROW;
		break;
	case SDLK_RIGHT:
		*key = K_RIGHTARROW;
		break;
	case SDLK_KP2:
		*key = K_KP_DOWNARROW;
		break;
	case SDLK_DOWN:
		*key = K_DOWNARROW;
		break;
	case SDLK_KP8:
		*key = K_KP_UPARROW;
		break;
	case SDLK_UP:
		*key = K_UPARROW;
		break;
	case SDLK_ESCAPE:
		*key = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		*key = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		*key = K_ENTER;
		break;
	case SDLK_TAB:
		*key = K_TAB;
		break;
	case SDLK_F1:
		*key = K_F1;
		break;
	case SDLK_F2:
		*key = K_F2;
		break;
	case SDLK_F3:
		*key = K_F3;
		break;
	case SDLK_F4:
		*key = K_F4;
		break;
	case SDLK_F5:
		*key = K_F5;
		break;
	case SDLK_F6:
		*key = K_F6;
		break;
	case SDLK_F7:
		*key = K_F7;
		break;
	case SDLK_F8:
		*key = K_F8;
		break;
	case SDLK_F9:
		*key = K_F9;
		break;
	case SDLK_F10:
		*key = K_F10;
		break;
	case SDLK_F11:
		*key = K_F11;
		break;
	case SDLK_F12:
		*key = K_F12;
		break;
	case SDLK_BACKSPACE:
		*key = K_BACKSPACE;
		break;
	case SDLK_KP_PERIOD:
		*key = K_KP_DEL;
		break;
	case SDLK_DELETE:
		*key = K_DEL;
		break;
	case SDLK_PAUSE:
		*key = K_PAUSE;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		*key = K_SHIFT;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		*key = K_CTRL;
		break;
	case SDLK_LMETA:
	case SDLK_RMETA:
	case SDLK_LALT:
	case SDLK_RALT:
		*key = K_ALT;
		break;
	case SDLK_KP5:
		*key = K_KP_5;
		break;
	case SDLK_INSERT:
		*key = K_INS;
		break;
	case SDLK_KP0:
		*key = K_KP_INS;
		break;
	case SDLK_KP_MULTIPLY:
		*key = '*';
		break;
	case SDLK_KP_PLUS:
		*key = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		*key = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		*key = K_KP_SLASH;
		break;
	/* suggestions on how to handle this better would be appreciated */
	case SDLK_WORLD_7:
		*key = '`';
		break;
	default:
		if (!keysym->unicode && (keysym->sym >= ' ') && (keysym->sym <= '~'))
			*key = (int) keysym->sym;
		break;
	}
	if ((keysym->unicode & 0xFF80) == 0) {  /* maps to ASCII? */
		buf = (int) keysym->unicode & 0x7F;
		if (buf == '~')
			*key = '~'; /* console HACK */
	}
	if (sdl_debug->value)
		printf("unicode: %hx keycode: %i key: %c\n", keysym->unicode, *key, *key);

	return buf;
}

/**
 * @brief Debug function to print sdl key events
 */
static void printkey(const SDL_Event* event)
{
	if (sdl_debug->value) {
		printf("key name: %s", SDL_GetKeyName(event->key.keysym.sym));
		if(event->key.keysym.unicode) {
			printf(" unicode: %hx", event->key.keysym.unicode);
			if (event->key.keysym.unicode >= '0' && event->key.keysym.unicode <= '~')  /* printable? */
				printf(" (%c)", (unsigned char)(event->key.keysym.unicode));
		}
		puts("");
	}
}

/**
 * @brief
 */
void GetEvent(SDL_Event *event)
{
	int key;
	int p = 0;

	switch(event->type) {
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event->button.button) {
			case 1:
				mouse_buttonstate = K_MOUSE1;
				break;
			case 2:
				mouse_buttonstate = K_MOUSE3;
				break;
			case 3:
				mouse_buttonstate = K_MOUSE2;
				break;
			case 4:
				mouse_buttonstate = K_MWHEELUP;
				break;
			case 5:
				mouse_buttonstate = K_MWHEELDOWN;
				break;
			case 6:
				mouse_buttonstate = K_MOUSE4;
				break;
			case 7:
				mouse_buttonstate = K_MOUSE5;
				break;
			default:
				mouse_buttonstate = K_AUX1 + (event->button.button - 8) % 16;
				break;
		}
		keyq[keyq_head].down = event->type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse;
		keyq[keyq_head].key = mouse_buttonstate;
		keyq_head = (keyq_head + 1) & 63;
		break;
	case SDL_MOUSEMOTION:
		if (mouse_active) {
			mx = event->motion.x;
			my = event->motion.y;
		}
		break;
	case SDL_KEYDOWN:
		printkey(event);
		if (event->key.keysym.mod & KMOD_ALT && event->key.keysym.sym == SDLK_RETURN) {
			SDL_WM_ToggleFullScreen(surface);

			if (surface->flags & SDL_FULLSCREEN) {
				ri.Cvar_SetValue( "vid_fullscreen", 1 );
			} else {
				ri.Cvar_SetValue( "vid_fullscreen", 0 );
			}
			vid_fullscreen->modified = qfalse; /* we just changed it with SDL. */
			break; /* ignore this key */
		}

		if (event->key.keysym.mod & KMOD_CTRL && event->key.keysym.sym == SDLK_g) {
			SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
			ri.Cvar_SetValue( "vid_grabmouse", (gm == SDL_GRAB_ON) ? 0 : 1 );
			break; /* ignore this key */
		}

		p = SDLateKey(&event->key.keysym, &key);
		if (key) {
			keyq[keyq_head].key = key;
			keyq[keyq_head].down = qtrue;
			keyq_head = (keyq_head + 1) & 63;
		}
		if (p) {
			keyq[keyq_head].key = p;
			keyq[keyq_head].down = qtrue;
			keyq_head = (keyq_head + 1) & 63;
		}
		break;
	case SDL_VIDEOEXPOSE:
		break;
	case SDL_KEYUP:
		p = SDLateKey(&event->key.keysym, &key);
		if (key) {
			keyq[keyq_head].key = key;
			keyq[keyq_head].down = qfalse;
			keyq_head = (keyq_head + 1) & 63;
		}
		if (p) {
			keyq[keyq_head].key = p;
			keyq[keyq_head].down = qfalse;
			keyq_head = (keyq_head + 1) & 63;
		}
		break;
	case SDL_QUIT:
		ri.Cmd_ExecuteText(EXEC_NOW, "quit");
		break;
	}

}

#if 0
/**
 * @brief
 */
void *GLimp_GetProcAddress(const char *func)
{
	return SDL_GL_GetProcAddress(func);
}
#endif

/**
 * @brief
 */
static void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	GLimp_Shutdown();
	exit(0);
}

/**
 * @brief
 */
void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/**
 * @brief
 */
qboolean GLimp_Init( void *hInstance, void *wndProc )
{
	if (SDL_WasInit(SDL_INIT_AUDIO|SDL_INIT_CDROM|SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			Sys_Error("Video SDL_Init failed: %s\n", SDL_GetError());
			return qfalse;
		}
	} else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Sys_Error("Video SDL_InitSubsystem failed: %s\n", SDL_GetError());
			return qfalse;
		}
	}

	SDL_EnableUNICODE(SDL_ENABLE);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	InitSig();

	return qtrue;
}

/**
 * @brief
 */
static void SetSDLIcon( void )
{
#include "ufoicon.xbm"
	SDL_Surface *icon;
	SDL_Color color;
	Uint8 *ptr;
	unsigned int i, mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE, ufoicon_width, ufoicon_height, 8, 0, 0, 0, 0);
	if (icon == NULL)
		return; /* oh well... */
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	color.r = color.g = color.b = 255;
	SDL_SetColors(icon, &color, 0, 1); /* just in case */
	color.r = color.b = 0;
	color.g = 16;
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

/**
 * @brief Init the SDL window
 * @param fullscreen Start in fullscreen or not (bool value)
 */
static qboolean GLimp_InitGraphics( qboolean fullscreen )
{
	int flags;
	int stencil_bits;
	int width = 0;
	int height = 0;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	Com_Printf("SDL version: %i.%i.%i\n", info.version.major, info.version.minor, info.version.patch);

	/* turn off DGA mouse support: leaving it makes cursor _slow_ in fullscreen under X11 at least */
	setenv("SDL_VIDEO_X11_DGAMOUSE", "0", 1);

	have_stencil = qfalse;
	if (SDL_GetWMInfo(&info) > 0 ) {
		if (info.subsystem == SDL_SYSWM_X11) {
			info.info.x11.lock_func();
			width = DisplayWidth(info.info.x11.display, DefaultScreen(info.info.x11.display));
			height = DisplayHeight(info.info.x11.display, DefaultScreen(info.info.x11.display));
			info.info.x11.unlock_func();
			Com_Printf("Desktop resolution: %i:%i\n", width, height);
		}
	}

	/* Just toggle fullscreen if that's all that has been changed */
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) {
		qboolean isfullscreen = (surface->flags & SDL_FULLSCREEN) ? qtrue : qfalse;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);

		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? qtrue : qfalse;
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
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	flags = SDL_OPENGL;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;

	SetSDLIcon(); /* currently uses ufoicon.xbm data */

	if ((surface = SDL_SetVideoMode(vid.width, vid.height, 0, flags)) == NULL) {
		Sys_Error("(SDLGL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return qfalse;
	}

	SDL_WM_SetCaption(GAME_TITLE, GAME_TITLE_LONG);

	SDL_ShowCursor( SDL_DISABLE );

	if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits)) {
		ri.Con_Printf(PRINT_ALL, "I: got %d bits of stencil\n", stencil_bits);
		if (stencil_bits >= 1) {
			have_stencil = qtrue;
		}
	}

	SDL_active = qtrue;

	return qtrue;
}

/**
 * @brief
 */
void GLimp_BeginFrame( float camera_seperation )
{
}

/**
 * @brief
 */
void GLimp_EndFrame (void)
{
	SDL_GL_SwapBuffers();
}

/**
 * @brief
 */
rserr_t GLimp_SetMode( unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen )
{
	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( (int*)pwidth, (int*)pheight, mode ) ) {
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !GLimp_InitGraphics( fullscreen ) ) {
		/* failed to set a valid mode in windowed mode */
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

/**
 * @brief
 */
void GLimp_SetGamma(void)
{
	float g;

	g = Cvar_Get("vid_gamma", "1.0", 0, NULL)->value;
	SDL_SetGamma(g, g, g);
}

/**
 * @brief
 */
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

	SDL_active = qfalse;
}

/**
 * @brief
 */
void GLimp_AppActivate( qboolean active )
{
}

/*****************************************************************************/
/* KEYBOARD                                                                  */
/*****************************************************************************/

Key_Event_fp_t Key_Event_fp;

/**
 * @brief
 * @sa KBD_Close
 */
void KBD_Init(Key_Event_fp_t fp)
{
	Key_Event_fp = fp;
	RW_IN_PlatformInit();
}

/**
 * @brief
 */
void KBD_Update(void)
{
	SDL_Event event;
	static int KBD_Update_Flag = 0;

	if (KBD_Update_Flag == 1)
		return;

	KBD_Update_Flag = 1;

	/* get events from x server */
	if (SDL_active) {
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
		while (keyq_head != keyq_tail) {
			in_state->Key_Event_fp(keyq[keyq_tail].key, keyq[keyq_tail].down);
			keyq_tail = (keyq_tail + 1) & 63;
		}
	} else
		ri.Con_Printf( PRINT_ALL, "SDL not active right now\n" );
	KBD_Update_Flag = 0;
}

/**
 * @brief
 * @sa KBD_Close
 */
void KBD_Close(void)
{
	keyq_head = 0;
	keyq_tail = 0;

	memset(keyq, 0, sizeof(keyq));
}

/**
 * @brief
 * @sa RW_IN_Shutdown
 */
void RW_IN_Init(in_state_t *in_state_p)
{
	in_state = in_state_p;

	/* mouse variables */
	m_filter = ri.Cvar_Get ("m_filter", "0", 0, NULL);
	in_mouse = ri.Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE, NULL);
	sensitivity = ri.Cvar_Get ("sensitivity", "2", CVAR_ARCHIVE, NULL);

	/* other cvars */
	sdl_debug = ri.Cvar_Get ("sdl_debug", "0", 0, NULL);

	mx = my = 0.0;
	mouse_avail = qtrue;
}

/**
 * @brief
 * @sa RW_IN_Init
 */
void RW_IN_Shutdown(void)
{
	RW_IN_Activate (qfalse);

	if (mouse_avail)
		mouse_avail = qfalse;
}

/**
 * @brief
 */
void RW_IN_Commands (void)
{
}

/**
 * @brief
 */
void RW_IN_GetMousePos (int *x, int *y)
{
	*x = mx / vid.rx;
	*y = my / vid.ry;
}

/**
 * @brief
 */
void RW_IN_Frame (void)
{
}
