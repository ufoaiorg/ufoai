/**
 * @file cl_input.h
 * @brief External (non-keyboard) input devices.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/
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

#ifndef CLIENT_INPUT_H
#define CLIENT_INPUT_H

typedef enum {
	MS_NULL,
	MS_MENU,		/**< we are over some menu node */
	MS_DRAGITEM,	/**< we are dragging some stuff / equipment */
	MS_ROTATE,		/**< we are rotating models (UFOpaedia) */
	MS_WORLD,		/**< we are in tactical mode */
} mouseSpace_t;

#define FOV				75.0
#define FOV_FPS			90.0
#define CAMERA_START_DIST   600
#define CAMERA_START_HEIGHT UNIT_HEIGHT * 1.5
#define CAMERA_LEVEL_HEIGHT UNIT_HEIGHT

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

typedef enum { CAMERA_MODE_REMOTE, CAMERA_MODE_FIRSTPERSON } camera_mode_t;

extern camera_mode_t camera_mode;
extern int mouseSpace;
extern int mousePosX, mousePosY;
extern float *rotateAngles;
extern const float MIN_ZOOM, MAX_ZOOM;

void IN_Init(void);
void IN_Frame(void);
void IN_SendKeyEvents(void);

void IN_EventEnqueue(unsigned int key, unsigned short, qboolean down);

void CL_CameraMove(void);
void CL_CameraRoute(pos3_t from, pos3_t target);
void CL_CameraModeChange(camera_mode_t newcameramode);

#endif /* CLIENT_INPUT_H */
