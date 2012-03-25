/**
 * @file cl_camera.c
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

#include "../client.h"
#include "cl_view.h"
#include "cl_hud.h"
#include "../input/cl_input.h"
#include "events/e_parse.h"

static qboolean cameraRoute = qfalse;
static vec3_t routeFrom, routeDelta;
static float routeDist;

const float MIN_ZOOM = 0.5;
const float MAX_ZOOM = 32.0;

#define MIN_CAMROT_SPEED	50
#define MIN_CAMROT_ACCEL	50
#define MAX_CAMROT_SPEED	1000
#define MAX_CAMROT_ACCEL	1000
#define MIN_CAMMOVE_SPEED	150
#define MIN_CAMMOVE_ACCEL	150
#define MAX_CAMMOVE_SPEED	3000
#define MAX_CAMMOVE_ACCEL	3000
#define LEVEL_MIN			0.05
#define LEVEL_SPEED			3.0
#define ZOOM_SPEED			2.0
#define MIN_CAMZOOM_QUANT	0.05
#define MAX_CAMZOOM_QUANT	1.0

static cvar_t *cl_camrotspeed;
static cvar_t *cl_cammovespeed;
static cvar_t *cl_cammoveaccel;
static cvar_t *cl_campitchmin;
static cvar_t *cl_campitchmax;
cvar_t *cl_camzoommax;
cvar_t *cl_camzoomquant;
cvar_t *cl_camzoommin;
cvar_t *cl_centerview;

/**
 * @brief forces the camera to stay within the horizontal bounds of the
 * map plus some border
 */
static inline void CL_ClampCamToMap (const float border)
{
	if (cl.cam.origin[0] < cl.mapData->mapMin[0] - border)
		cl.cam.origin[0] = cl.mapData->mapMin[0] - border;
	else if (cl.cam.origin[0] > cl.mapData->mapMax[0] + border)
		cl.cam.origin[0] = cl.mapData->mapMax[0] + border;

	if (cl.cam.origin[1] < cl.mapData->mapMin[1] - border)
		cl.cam.origin[1] = cl.mapData->mapMin[1] - border;
	else if (cl.cam.origin[1] > cl.mapData->mapMax[1] + border)
		cl.cam.origin[1] = cl.mapData->mapMax[1] + border;
}

/**
 * @brief Update the camera position. This can be done in two different reasons. The first is the user input, the second
 * is an active camera route. The camera route overrides the user input and is lerping the movement until the final position
 * is reached.
 */
void CL_CameraMove (void)
{
	float frac;
	vec3_t delta;
	int i;

	/* get relevant variables */
	const float rotspeed =
		(cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ((cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED) : MIN_CAMROT_SPEED;
	const float movespeed =
		(cl_cammovespeed->value > MIN_CAMMOVE_SPEED) ?
		((cl_cammovespeed->value < MAX_CAMMOVE_SPEED) ? cl_cammovespeed->value / cl.cam.zoom : MAX_CAMMOVE_SPEED / cl.cam.zoom) : MIN_CAMMOVE_SPEED / cl.cam.zoom;
	const float moveaccel =
		(cl_cammoveaccel->value > MIN_CAMMOVE_ACCEL) ?
		((cl_cammoveaccel->value < MAX_CAMMOVE_ACCEL) ? cl_cammoveaccel->value / cl.cam.zoom : MAX_CAMMOVE_ACCEL / cl.cam.zoom) : MIN_CAMMOVE_ACCEL / cl.cam.zoom;

	if (cls.state != ca_active)
		return;

	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	/* calculate camera omega */
	/* stop acceleration */
	frac = cls.frametime * moveaccel * 2.5;

	for (i = 0; i < 2; i++) {
		if (fabs(cl.cam.omega[i]) > frac) {
			if (cl.cam.omega[i] > 0)
				cl.cam.omega[i] -= frac;
			else
				cl.cam.omega[i] += frac;
		} else
			cl.cam.omega[i] = 0;

		/* rotational acceleration */
		if (i == YAW)
			cl.cam.omega[i] += CL_GetKeyMouseState(STATE_ROT) * frac * 2;
		else
			cl.cam.omega[i] += CL_GetKeyMouseState(STATE_TILT) * frac * 2;

		if (cl.cam.omega[i] > rotspeed)
			cl.cam.omega[i] = rotspeed;
		if (-cl.cam.omega[i] > rotspeed)
			cl.cam.omega[i] = -rotspeed;
	}

	cl.cam.omega[ROLL] = 0;
	/* calculate new camera angles for this frame */
	VectorMA(cl.cam.angles, cls.frametime, cl.cam.omega, cl.cam.angles);

	if (cl.cam.angles[PITCH] > cl_campitchmax->value)
		cl.cam.angles[PITCH] = cl_campitchmax->value;
	if (cl.cam.angles[PITCH] < cl_campitchmin->value)
		cl.cam.angles[PITCH] = cl_campitchmin->value;

	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);

	/* camera route overrides user input */
	if (cameraRoute) {
		/* camera route */
		frac = cls.frametime * moveaccel * 2;
		if (VectorDist(cl.cam.origin, routeFrom) > routeDist - 200) {
			VectorMA(cl.cam.speed, -frac, routeDelta, cl.cam.speed);
			VectorNormalize2(cl.cam.speed, delta);
			if (DotProduct(delta, routeDelta) < 0.05) {
				cameraRoute = qfalse;

				CL_BlockBattlescapeEvents(qfalse);
			}
		} else
			VectorMA(cl.cam.speed, frac, routeDelta, cl.cam.speed);
	} else {
		/* normal camera movement */
		/* calculate ground-based movement vectors */
		const float angle = cl.cam.angles[YAW] * torad;
		const float sy = sin(angle);
		const float cy = cos(angle);
		vec3_t g_forward, g_right;

		VectorSet(g_forward, cy, sy, 0.0);
		VectorSet(g_right, sy, -cy, 0.0);

		/* calculate camera speed */
		/* stop acceleration */
		frac = cls.frametime * moveaccel;
		if (VectorLength(cl.cam.speed) > frac) {
			VectorNormalize2(cl.cam.speed, delta);
			VectorMA(cl.cam.speed, -frac, delta, cl.cam.speed);
		} else
			VectorClear(cl.cam.speed);

		/* acceleration */
		frac = cls.frametime * moveaccel * 3.5;
		VectorClear(delta);
		VectorScale(g_forward, CL_GetKeyMouseState(STATE_FORWARD), delta);
		VectorMA(delta, CL_GetKeyMouseState(STATE_RIGHT), g_right, delta);
		VectorNormalize(delta);
		VectorMA(cl.cam.speed, frac, delta, cl.cam.speed);

		/* lerp the level change */
		if (cl.cam.lerplevel < cl_worldlevel->value) {
			cl.cam.lerplevel += LEVEL_SPEED * (cl_worldlevel->value - cl.cam.lerplevel + LEVEL_MIN) * cls.frametime;
			if (cl.cam.lerplevel > cl_worldlevel->value)
				cl.cam.lerplevel = cl_worldlevel->value;
		} else if (cl.cam.lerplevel > cl_worldlevel->value) {
			cl.cam.lerplevel -= LEVEL_SPEED * (cl.cam.lerplevel - cl_worldlevel->value + LEVEL_MIN) * cls.frametime;
			if (cl.cam.lerplevel < cl_worldlevel->value)
				cl.cam.lerplevel = cl_worldlevel->value;
		}
	}

	/* clamp speed */
	frac = VectorLength(cl.cam.speed) / movespeed;
	if (frac > 1.0)
		VectorScale(cl.cam.speed, 1.0 / frac, cl.cam.speed);

	/* zoom change */
	frac = CL_GetKeyMouseState(STATE_ZOOM);
	if (frac > 0.1) {
		cl.cam.zoom *= 1.0 + cls.frametime * ZOOM_SPEED * frac;
		/* ensure zoom isn't greater than either MAX_ZOOM or cl_camzoommax */
		cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
	} else if (frac < -0.1) {
		cl.cam.zoom /= 1.0 - cls.frametime * ZOOM_SPEED * frac;
		/* ensure zoom isn't less than either MIN_ZOOM or cl_camzoommin */
		cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
	}
	CL_ViewCalcFieldOfViewX();

	/* calc new camera reference and new camera real origin */
	VectorMA(cl.cam.origin, cls.frametime, cl.cam.speed, cl.cam.origin);
	cl.cam.origin[2] = 0.;
	if (cl_isometric->integer) {
		CL_ClampCamToMap(72.);
		VectorMA(cl.cam.origin, -CAMERA_START_DIST + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT, cl.cam.axis[0], cl.cam.camorg);
		cl.cam.camorg[2] += CAMERA_START_HEIGHT + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;
	} else {
		CL_ClampCamToMap(min(144. * (cl.cam.zoom - cl_camzoommin->value - 0.4) / cl_camzoommax->value, 86));
		VectorMA(cl.cam.origin, -CAMERA_START_DIST / cl.cam.zoom , cl.cam.axis[0], cl.cam.camorg);
		cl.cam.camorg[2] += CAMERA_START_HEIGHT / cl.cam.zoom + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;
	}
}

/**
 * @brief Interpolates the camera movement from the given start point to the given end point
 * @sa CL_CameraMove
 * @sa CL_ViewCenterAtGridPosition
 * @param[in] from The grid position to start the camera movement from
 * @param[in] target The grid position to move the camera to
 */
void CL_CameraRoute (const pos3_t from, const pos3_t target)
{
	if (!cl_centerview->integer)
		return;

	/* initialize the camera route variables */
	PosToVec(from, routeFrom);
	PosToVec(target, routeDelta);
	VectorSubtract(routeDelta, routeFrom, routeDelta);
	routeDelta[2] = 0;
	routeDist = VectorLength(routeDelta);
	VectorNormalize(routeDelta);

	/* center the camera on the route starting position */
	VectorCopy(routeFrom, cl.cam.origin);
	/* set the world level to the z axis value of the camera target
	 * the camera lerp will do a smooth translate from the old level
	 * to the new one */
	Cvar_SetValue("cl_worldlevel", target[2]);

	VectorClear(cl.cam.speed);
	cameraRoute = qtrue;

	CL_BlockBattlescapeEvents(qtrue);
}

/**
 * @brief Zooms the scene of the battlefield in
 */
void CL_CameraZoomIn (void)
{
	float quant;

	/* check zoom quant */
	if (cl_camzoomquant->value < MIN_CAMZOOM_QUANT)
		quant = 1 + MIN_CAMZOOM_QUANT;
	else if (cl_camzoomquant->value > MAX_CAMZOOM_QUANT)
		quant = 1 + MAX_CAMZOOM_QUANT;
	else
		quant = 1 + cl_camzoomquant->value;

	/* change zoom */
	cl.cam.zoom *= quant;

	/* ensure zoom doesn't exceed either MAX_ZOOM or cl_camzoommax */
	cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
	CL_ViewCalcFieldOfViewX();
}

/**
 * @brief Zooms the scene of the battlefield out
 */
void CL_CameraZoomOut (void)
{
	float quant;

	/* check zoom quant */
	if (cl_camzoomquant->value < MIN_CAMZOOM_QUANT)
		quant = 1 + MIN_CAMZOOM_QUANT;
	else if (cl_camzoomquant->value > MAX_CAMZOOM_QUANT)
		quant = 1 + MAX_CAMZOOM_QUANT;
	else
		quant = 1 + cl_camzoomquant->value;

	/* change zoom */
	cl.cam.zoom /= quant;

	/* ensure zoom isnt less than either MIN_ZOOM or cl_camzoommin */
	cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
	CL_ViewCalcFieldOfViewX();
}

#ifdef DEBUG
/**
 * @brief Prints the current camera angles
 * @note Only available in debug mode
 * Accessable via console command camangles
 */
static void CL_CamPrintAngles_f (void)
{
	Com_Printf("camera angles %0.3f:%0.3f:%0.3f\n", cl.cam.angles[0], cl.cam.angles[1], cl.cam.angles[2]);
}
#endif /* DEBUG */

static void CL_CamSetAngles_f (void)
{
	int c = Cmd_Argc();

	if (c < 3) {
		Com_Printf("Usage %s <value> <value>\n", Cmd_Argv(0));
		return;
	}

	cl.cam.angles[PITCH] = atof(Cmd_Argv(1));
	cl.cam.angles[YAW] = atof(Cmd_Argv(2));
	cl.cam.angles[ROLL] = 0.0f;
}

static void CL_CamSetZoom_f (void)
{
	int c = Cmd_Argc();

	if (c < 2) {
		Com_Printf("Usage %s <value>\n", Cmd_Argv(0));
		return;
	}

	Com_Printf("old zoom value: %.2f\n", cl.cam.zoom);
	cl.cam.zoom = atof(Cmd_Argv(1));
	cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
	cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
}

static void CL_CenterCameraIntoMap_f (void)
{
	VectorCenterFromMinsMaxs(cl.mapData->mapMin, cl.mapData->mapMax, cl.cam.origin);
}

void CL_CameraInit (void)
{
	cl_camrotspeed = Cvar_Get("cl_camrotspeed", "250", CVAR_ARCHIVE, "Rotation speed of the battlescape camera.");
	cl_cammovespeed = Cvar_Get("cl_cammovespeed", "750", CVAR_ARCHIVE, "Movement speed of the battlescape camera.");
	cl_cammoveaccel = Cvar_Get("cl_cammoveaccel", "1250", CVAR_ARCHIVE, "Movement acceleration of the battlescape camera.");
	cl_campitchmax = Cvar_Get("cl_campitchmax", "89", 0, "Max. battlescape camera pitch - over 90 presents apparent mouse inversion.");
	cl_campitchmin = Cvar_Get("cl_campitchmin", "10", 0, "Min. battlescape camera pitch - under 35 presents difficulty positioning cursor.");
	cl_camzoomquant = Cvar_Get("cl_camzoomquant", "0.16", CVAR_ARCHIVE, "Battlescape camera zoom quantisation.");
	cl_camzoommin = Cvar_Get("cl_camzoommin", "0.3", 0, "Minimum zoom value for tactical missions.");
	cl_camzoommax = Cvar_Get("cl_camzoommax", "3.4", 0, "Maximum zoom value for tactical missions.");
	cl_centerview = Cvar_Get("cl_centerview", "1", CVAR_ARCHIVE, "Center the view when selecting a new soldier.");

#ifdef DEBUG
	Cmd_AddCommand("debug_camangles", CL_CamPrintAngles_f, "Print current camera angles.");
#endif /* DEBUG */
	Cmd_AddCommand("camsetangles", CL_CamSetAngles_f, "Set camera angles to the given values.");
	Cmd_AddCommand("camsetzoom", CL_CamSetZoom_f, "Set camera zoom level.");
	Cmd_AddCommand("centercamera", CL_CenterCameraIntoMap_f, "Center camera on the map.");
}
