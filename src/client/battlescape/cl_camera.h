/**
 * @file cl_camera.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CL_CAMERA_H
#define CL_CAMERA_H

typedef struct {
	vec3_t origin;		/**< the reference origin used for rotating around and to look at */
	vec3_t camorg;		/**< origin of the camera (look from) */
	vec3_t speed;		/**< speed of camera movement */
	vec3_t angles;		/**< current camera angle */
	vec3_t omega;		/**< speed of rotation */
	vec3_t axis[3];		/**< set when refdef.angles is set */

	float lerplevel;	/**< linear interpolation between frames while changing the world level */
	float zoom;			/**< the current zoom level (see MIN_ZOOM and MAX_ZOOM) */
} camera_t;

#define FOV				75.0
#define CAMERA_START_DIST   600
#define CAMERA_START_HEIGHT UNIT_HEIGHT * 1.5
#define CAMERA_LEVEL_HEIGHT UNIT_HEIGHT

extern cvar_t *cl_centerview;
extern cvar_t *cl_camzoommax;
extern cvar_t *cl_camzoommin;
extern cvar_t *cl_camzoomquant;

extern const float MIN_ZOOM, MAX_ZOOM;

void CL_CameraInit(void);
void CL_CameraMove(void);
void CL_CameraRoute(const pos3_t from, const pos3_t target);
void CL_CameraZoomIn(void);
void CL_CameraZoomOut(void);

#endif
