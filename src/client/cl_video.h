/**
 * @file cl_video.h
 * @brief Video driver defs.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/vid.h
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

#ifndef CLIENT_VID_H
#define CLIENT_VID_H

#include "../shared/ufotypes.h"	/* for qboolean */
#include "../common/mem.h"		/* for memPool_t */
#include "../common/cvar.h"		/* for cvar_t */

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

/**
 * @brief Contains the game screen context, everything that is needed to create
 * the graphic context. These value are stored to allow to restore
 * the old context.
 */
typedef struct {
	unsigned width;			/**< game screen/window width */
	unsigned height;		/**< game screen/window height */
	int mode;				/**< resolution mode - see vidmode_t */
	qboolean fullscreen;	/**< currently in fullscreen mode? */
	int multisample;		/**< number of multisample used by the context */
	int swapinterval;		/**< number of swapinterval used by the context */
} viddefContext_t;

/**
 * @brief Contains the game screen size and drawing scale
 *
 * This is used to rationalize the GUI system rendering box
 * with the actual screen.
 * The width and height are the dimensions of the actual screen,
 * not just the rendering box.
 * The rx, ry positions are the width and height divided by
 * VID_NORM_WIDTH and VID_NORM_HEIGHT respectively.
 * This allows the GUI system to use a "normalized" coordinate
 * system of 1024x768 texels.
 *
 * this struct is also defined in src/renderer/r_local.h
 */
typedef struct {
	viddefContext_t context;

	qboolean stretch;		/**< currently stretch mode? */

	float rx;		/**< horizontal screen scale factor */
	float ry;		/**< vertical screen scale factor */

	int virtualWidth, virtualHeight;		/**< size of the virtual screen */

	int x, y, viewWidth, viewHeight;	/**< The menu system may define a rendering view port
			* on the screen. The values defines the properties of this view port
			* i.e. the height and width of the port, and the (x, y) offset from the
			* upper left corner. */
} viddef_t;

typedef struct vidmode_s {
	int width, height;
	int mode;
} vidmode_t;

extern memPool_t *vid_genericPool;
extern memPool_t *vid_imagePool;
extern memPool_t *vid_lightPool;
extern memPool_t *vid_modelPool;

extern viddef_t viddef;			/* global video state */

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_stretch;
extern cvar_t *vid_mode;
extern cvar_t *vid_gamma;
extern cvar_t *vid_ignoregamma;
extern cvar_t *vid_grabmouse;

/* Video module initialisation etc */
void VID_Init(void);
int VID_GetModeNums(void);
void VID_Restart_f(void);
qboolean VID_GetModeInfo(int modeIndex, vidmode_t *modeInfo);

#endif /* CLIENT_VID_H */
