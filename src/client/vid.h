/**
 * @file vid.h
 * @brief Video driver defs.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Added inclusion guard.

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

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

/*    vrect_s

The menu system may define a quake rendering view port on the screen. The vrect_s
struct defines the properties of this view port - i.e. the height and width of the port,
and the (x,y) offset from the bottom (?) left (?) corner.

*/


typedef struct vrect_s {
	int x, y, width, height;
} vrect_t;

/*    viddef_t

Another structure used to rationalize the menu system rendering box with the actual screen.
The width and height are the dimensions of the actual screen, not just the rendering box.
The rx, ry positions are the width and height divided by VID_NORM_WIDTH and VID_NORM_HEIGHT
respectively. This allows the menu system to use a "normalized" coordinate system of
1024x768 texels.

*/

typedef struct {
	unsigned width, height;		/* coordinates from main game */
	float rx, ry;
} viddef_t;

typedef struct vidmode_s {
	int width, height;
	int mode;
} vidmode_t;

extern const vidmode_t vid_modes[];
extern int maxVidModes;
extern viddef_t viddef;			/* global video state */

/* Video module initialisation etc */
void VID_Init(void);
void VID_Shutdown(void);
void VID_CheckChanges(void);

#define VID_NUM_MODES ( sizeof(vid_modes) / sizeof(vidmode_t) )

#endif /* CLIENT_VID_H */
