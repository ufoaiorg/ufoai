/**
 * @file
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

#include "r_local.h"
#include "r_main.h"
#include "r_sdl.h"
#include "../../ports/system.h"
#include "../client.h"

r_sdl_config_t r_sdl_config;

static void R_SetSDLIcon (void)
{
#ifndef _WIN32
#include "../../ports/linux/ufoicon.xbm"
	SDL_Surface* icon = SDL_CreateRGBSurface(SDL_SWSURFACE, ufoicon_width, ufoicon_height, 8, 0, 0, 0, 0);
	if (icon == nullptr)
		return;
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_SetColorKey(icon, SDL_TRUE, 0);
#else
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	SDL_Color color;
	color.r = color.g = color.b = 255;
	SDL_SetColors(icon, &color, 0, 1); /* just in case */

	color.r = color.b = 0;
	color.g = 16;
	SDL_SetColors(icon, &color, 1, 1);
#endif

	Uint8 *ptr = (Uint8 *)icon->pixels;
	for (unsigned int i = 0; i < sizeof(ufoicon_bits); i++) {
		for (unsigned int mask = 1; mask != 0x100; mask <<= 1) {
			*ptr = (ufoicon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_SetWindowIcon(cls.window, icon);
#else
	SDL_WM_SetIcon(icon, nullptr);
#endif
	SDL_FreeSurface(icon);
#endif
}

bool Rimp_Init (void)
{
	SDL_version version;
	int attrValue;

	Com_Printf("\n------- video initialization -------\n");

	OBJZERO(r_sdl_config);

	if (r_driver->string[0] != '\0') {
		Com_Printf("I: using driver: %s\n", r_driver->string);
		SDL_GL_LoadLibrary(r_driver->string);
	}

	Sys_Setenv("SDL_VIDEO_CENTERED", "1");
	Sys_Setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "0");

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
			Com_Error(ERR_FATAL, "Video SDL_Init failed: %s", SDL_GetError());
	}

	SDL_VERSION(&version)
	Com_Printf("I: SDL version: %i.%i.%i\n", version.major, version.minor, version.patch);

	r_sdl_config.desktopWidth = 1024;
	r_sdl_config.desktopHeight = 768;

#if SDL_VERSION_ATLEAST(2,0,0)
	int screen = 0;
	const int modes = SDL_GetNumDisplayModes(screen);
	if (modes > 0) {
		r_sdl_config.modes = Mem_AllocTypeN(rect_t, modes);
		for (int i = 0; i < modes; i++) {
			SDL_DisplayMode displayMode;
			SDL_GetDisplayMode(screen, i, &displayMode);
			r_sdl_config.modes[i][0] = displayMode.w;
			r_sdl_config.modes[i][1] = displayMode.h;
		}
	}

	const int displays = SDL_GetNumVideoDisplays();
	Com_Printf("I: found %i display(s)\n", displays);

	SDL_DisplayMode displayMode;
	if (SDL_GetDesktopDisplayMode(0, &displayMode) != -1) {
		const char* name = SDL_GetPixelFormatName(displayMode.format);
		Com_Printf("I: current desktop mode: %dx%d@%dHz (%s)\n",
				displayMode.w, displayMode.h, displayMode.refresh_rate, name);
		Com_Printf("I: video driver: %s\n", SDL_GetCurrentVideoDriver());
		r_sdl_config.desktopWidth = displayMode.w;
		r_sdl_config.desktopHeight = displayMode.h;
	} else {
		Com_Printf("E: failed to get the desktop mode\n");
	}

	const int videoDrivers = SDL_GetNumVideoDrivers();
	for (int i = 0; i < videoDrivers; ++i) {
		Com_Printf("I: available driver: %s\n", SDL_GetVideoDriver(i));
	}
	Com_Printf("I: driver: %s\n", SDL_GetCurrentVideoDriver());

	SDL_SetModState(KMOD_NONE);
	SDL_StopTextInput();
#else
	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	if (info != nullptr) {
		SDL_VideoInfo videoInfo;
		SDL_PixelFormat pixelFormat;
		SDL_Rect** modes;
		Com_Printf("I: desktop depth: %ibpp\n", info->vfmt->BitsPerPixel);
		r_config.videoMemory = info->video_mem;
		Com_Printf("I: video memory: %i\n", r_config.videoMemory);
		memcpy(&pixelFormat, info->vfmt, sizeof(pixelFormat));
		memcpy(&videoInfo, info, sizeof(videoInfo));
		videoInfo.vfmt = &pixelFormat;
		modes = SDL_ListModes(videoInfo.vfmt, SDL_OPENGL | SDL_FULLSCREEN);
		if (modes) {
			if (modes == (SDL_Rect**)-1) {
				Com_Printf("I: Available resolutions: any resolution is supported\n");
				r_sdl_config.modes = nullptr;
			} else {
				for (r_sdl_config.numModes = 0; modes[r_sdl_config.numModes]; r_sdl_config.numModes++) {}

				r_sdl_config.modes = Mem_AllocTypeN(rect_t, r_sdl_config.numModes);
				for (int i = 0; i < r_sdl_config.numModes; i++) {
					r_sdl_config.modes[i][0] = modes[i]->w;
					r_sdl_config.modes[i][1] = modes[i]->h;
				}
			}
		} else {
			Com_Printf("I: Could not get list of available resolutions\n");
		}
	}
	char videoDriverName[MAX_VAR] = "";
	SDL_VideoDriverName(videoDriverName, sizeof(videoDriverName));
	Com_Printf("I: video driver: %s\n", videoDriverName);
#endif
	if (r_sdl_config.numModes > 0) {
		char buf[4096] = "";
		Q_strcat(buf, sizeof(buf), "I: Available resolutions:");
		for (int i = 0; i < r_sdl_config.numModes; i++) {
			Q_strcat(buf, sizeof(buf), " %ix%i", r_sdl_config.modes[i][0], r_sdl_config.modes[i][1]);
		}
		Com_Printf("%s (%i)\n", buf, r_sdl_config.numModes);
	}

	if (!R_SetMode())
		Com_Error(ERR_FATAL, "Video subsystem failed to initialize");

#if !SDL_VERSION_ATLEAST(2,0,0)
	SDL_WM_SetCaption(GAME_TITLE, GAME_TITLE_LONG);

	/* we need this in the renderer because if we issue an vid_restart we have
	 * to set these values again, too */
	SDL_EnableUNICODE(SDL_ENABLE);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif

	R_SetSDLIcon();

	if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &attrValue))
		Com_Printf("I: got %d bits of stencil\n", attrValue);
	if (!SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &attrValue))
		Com_Printf("I: got %d bits of depth buffer\n", attrValue);
	if (!SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &attrValue))
		Com_Printf("I: got double buffer\n");
	if (!SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &attrValue))
		Com_Printf("I: got %d bits for red\n", attrValue);
	if (!SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &attrValue))
		Com_Printf("I: got %d bits for green\n", attrValue);
	if (!SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &attrValue))
		Com_Printf("I: got %d bits for blue\n", attrValue);
	if (!SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &attrValue))
		Com_Printf("I: got %d bits for alpha\n", attrValue);
	if (!SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &attrValue))
		Com_Printf("I: got multisample %s\n", attrValue != 0 ? "enabled" : "disabled");
	if (!SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &attrValue))
		Com_Printf("I: got %d multisample buffers\n", attrValue);

	return true;
}

/**
 * @brief Init the SDL window
 */
bool R_InitGraphics (const viddefContext_t* context)
{
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (context->multisample > 0) {
		Com_Printf("I: set multisample buffers to %i\n", context->multisample);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, context->multisample);
	} else {
		Com_Printf("I: disable multisample buffers\n");
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	/* valid values are between -1 and 1 */
	const int i = std::min(1, std::max(-1, context->swapinterval));
	Com_Printf("I: set swap control to %i\n", i);
	SDL_GL_SetSwapInterval(i);
	uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

	if (context->fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	cls.window = SDL_CreateWindow(GAME_TITLE_LONG, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, context->width, context->height, flags);
	if (!cls.window) {
		const char* error = SDL_GetError();
		Com_Printf("E: SDL_CreateWindow failed: %s\n", error);
		SDL_ClearError();
		return -1;
	}

	cls.context = SDL_GL_CreateContext(cls.window);
#else
	/* valid values are between 0 and 2 */
	const int i = std::min(2, std::max(0, context->swapinterval));
	Com_Printf("I: set swap control to %i\n", i);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, i);
	uint32_t flags = SDL_OPENGL;
	if (context->fullscreen)
		flags |= SDL_FULLSCREEN;
	/*flags |= SDL_NOFRAME;*/

	SDL_Surface* screen = SDL_SetVideoMode(context->width, context->height, 0, flags);
	if (!screen) {
		const char* error = SDL_GetError();
		Com_Printf("SDL SetVideoMode failed: %s\n", error);
		SDL_ClearError();
		return false;
	}
#endif

	SDL_ShowCursor(SDL_DISABLE);

	return true;
}

void Rimp_Shutdown (void)
{
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_DestroyWindow(cls.window);
	SDL_GL_DeleteContext(cls.context);
#endif
	/* SDL on Android does not support multiple video init/deinit yet, however calling SDL_SetVideoMode() multiple times works */
#ifndef ANDROID
	SDL_ShowCursor(SDL_ENABLE);

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
}
