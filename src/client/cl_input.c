/**
 * @file cl_input.c
 * @brief Client input handling - bindable commands.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

Changes:
15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Updated copyright notice.

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

#include "client.h"

extern unsigned sys_frame_time;

int mouseSpace;
int mx, my;
int dragFrom, dragFromX, dragFromY;
item_t dragItem = {NONE,NONE,NONE}; /* to crash as soon as possible */
float *rotateAngles;
qboolean wasCrouched = qfalse, doCrouch = qfalse;

static qboolean cameraRoute = qfalse;
static vec3_t routeFrom, routeDelta;
static float routeDist;
static int routeLevelStart, routeLevelEnd;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, qboolean down, unsigned time);

  +mlook src time

===============================================================================
*/

static kbutton_t in_turnleft, in_turnright, in_shiftleft, in_shiftright;
static kbutton_t in_shiftup, in_shiftdown;
static kbutton_t in_zoomin, in_zoomout;
static kbutton_t in_turnup, in_turndown;

static int in_impulse;


/**
 * @brief
 */
static void KeyDown(kbutton_t * b)
{
	int k;
	char *c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		/* typed manually at the console for continuous down */
		k = -1;

	/* repeating key */
	if (k == b->down[0] || k == b->down[1])
		return;

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Printf("Three keys down for a button!\n");
		return;
	}

	/* still down */
	if (b->state & 1)
		return;

	/* save timestamp */
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	/* down + impulse down */
	b->state |= 1 + 2;
}

/**
 * @brief
 */
void KeyUp(kbutton_t * b)
{
	int k;
	char *c;
	unsigned uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	/* typed manually at the console, assume for unsticking, so clear all */
	else {
		b->down[0] = b->down[1] = 0;
		/* impulse up */
		b->state = 4;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	/* key up without coresponding down (menu pass through) */
	else
		return;

	/* some other key is still holding it down */
	if (b->down[0] || b->down[1])
		return;

	/* still up (this should not happen) */
	if (!(b->state & 1))
		return;

	/* save timestamp */
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	/* now up */
	b->state &= ~1;
	/* impulse up */
	b->state |= 4;
}

static void IN_TurnLeftDown(void)
{
	KeyDown(&in_turnleft);
}
static void IN_TurnLeftUp(void)
{
	KeyUp(&in_turnleft);
}
static void IN_TurnRightDown(void)
{
	KeyDown(&in_turnright);
}
static void IN_TurnRightUp(void)
{
	KeyUp(&in_turnright);
}
static void IN_TurnUpDown(void)
{
	KeyDown(&in_turnup);
}
static void IN_TurnUpUp(void)
{
	KeyUp(&in_turnup);
}
static void IN_TurnDownDown(void)
{
	KeyDown(&in_turndown);
}
static void IN_TurnDownUp(void)
{
	KeyUp(&in_turndown);
}
static void IN_ShiftLeftDown(void)
{
	KeyDown(&in_shiftleft);
}
static void IN_ShiftLeftUp(void)
{
	KeyUp(&in_shiftleft);
}
static void IN_ShiftRightDown(void)
{
	KeyDown(&in_shiftright);
}
static void IN_ShiftRightUp(void)
{
	KeyUp(&in_shiftright);
}
static void IN_ShiftUpDown(void)
{
	KeyDown(&in_shiftup);
}
static void IN_ShiftUpUp(void)
{
	KeyUp(&in_shiftup);
}
static void IN_ShiftDownDown(void)
{
	KeyDown(&in_shiftdown);
}
static void IN_ShiftDownUp(void)
{
	KeyUp(&in_shiftdown);
}
static void IN_ZoomInDown(void)
{
	KeyDown(&in_zoomin);
}
static void IN_ZoomInUp(void)
{
	KeyUp(&in_zoomin);
}
static void IN_ZoomOutDown(void)
{
	KeyDown(&in_zoomout);
}
static void IN_ZoomOutUp(void)
{
	KeyUp(&in_zoomout);
}

static void IN_Impulse(void)
{
	in_impulse = atoi(Cmd_Argv(1));
}

/**
 * @brief Switches camera mode between remote and firstperson
 */
void CL_CameraMode(void)
{
	if (!selActor)
		return;
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);
	else
		CL_CameraModeChange(CAMERA_MODE_FIRSTPERSON);
}

/**
 * @brief
 */
void CL_CameraModeChange(camera_mode_t new_camera_mode)
{
	static vec3_t save_camorg, save_camangles;
	static float save_camzoom;
	static int save_level;

	/* no camera change if this is not our round */
	if (cls.team != cl.actTeam)
		return;

	/* save remote camera position, angles, zoom */
	if (camera_mode == CAMERA_MODE_REMOTE) {
		VectorCopy(cl.cam.camorg, save_camorg);
		VectorCopy(cl.cam.angles, save_camangles);
		save_camzoom = cl.cam.zoom;
		save_level = cl_worldlevel->value;
	}

	/* toggle camera mode */
	if (new_camera_mode == CAMERA_MODE_REMOTE) {
		Com_Printf("Changed camera mode to remote.\n");
		camera_mode = CAMERA_MODE_REMOTE;
		VectorCopy(save_camorg, cl.cam.camorg);
		VectorCopy(save_camangles, cl.cam.angles);
		cl.cam.zoom = save_camzoom;
		Cvar_SetValue("cl_worldlevel", save_level);
	} else {
		Com_Printf("Changed camera mode to first-person.\n");
		camera_mode = CAMERA_MODE_FIRSTPERSON;
		VectorCopy(selActor->origin, cl.cam.camorg);
		Cvar_SetValue("cl_worldlevel", selActor->pos[2]);
		if (!(selActor->state & STATE_CROUCHED))
			cl.cam.camorg[2] += EYE_HT_OFFSET; /* raise from waist to head */
		VectorCopy(selActor->angles, cl.cam.angles);
		cl.cam.zoom = FOV/FOV_FPS;
		wasCrouched = selActor->state & STATE_CROUCHED;
	}
}

/**
 * @brief Print the time long integer value
 */
void CL_Time_f(void)
{
	Com_Printf("time: %d\n", cl.time);
}

/**
 * @brief Returns the fraction of the frame that the key was down
 */
float CL_KeyState(kbutton_t * key)
{
	float val;
	int msec;

	/* clear impulses */
	key->state &= 1;

	msec = key->msec;
	key->msec = 0;

	/* still down */
	if (key->state) {
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}
#if 0
	if (msec)
		Com_Printf("%i ", msec);
#endif

	val = (float) msec;
	if (val < 0)
		val = 0;
	if (val > 1)
		val = 1;

	return val;
}


/*========================================================================== */

const float MIN_ZOOM = 0.5;
const float MAX_ZOOM = 20.0;

cvar_t *cl_camrotspeed;
cvar_t *cl_camrotaccel;
cvar_t *cl_cammovespeed;
cvar_t *cl_cammoveaccel;
cvar_t *cl_camyawspeed;
cvar_t *cl_campitchmax;
cvar_t *cl_campitchmin;
cvar_t *cl_campitchspeed;
cvar_t *cl_camzoomquant;
cvar_t *cl_camzoommax;
cvar_t *cl_camzoommin;

cvar_t *cl_anglespeedkey;

#define MIN_CAMROT_SPEED	50
#define MIN_CAMROT_ACCEL	50
#define MAX_CAMROT_SPEED	1000
#define MAX_CAMROT_ACCEL	1000
#define MIN_CAMMOVE_SPEED	150
#define MIN_CAMMOVE_ACCEL	150
#define MAX_CAMMOVE_SPEED	3000
#define MAX_CAMMOVE_ACCEL	3000
#define ZOOM_SPEED			1.4
#define MIN_CAMZOOM_QUANT	0.2
#define MAX_CAMZOOM_QUANT	1.0
#define LEVEL_SPEED			3.0
#define LEVEL_MIN			0.05

#define CAMERA_START_DIST	600
#define CAMERA_LEVEL_DIST	0
#define CAMERA_START_HEIGHT	0
#define CAMERA_LEVEL_HEIGHT	64

/*========================================================================== */

/**
 * @brief Switch one worldlevel up
 */
void CL_LevelUp(void)
{
	if (!CL_OnBattlescape())
		return;
	if (camera_mode != CAMERA_MODE_FIRSTPERSON)
		Cvar_SetValue("cl_worldlevel", (cl_worldlevel->value < map_maxlevel-1) ? cl_worldlevel->value + 1.0f : map_maxlevel-1);
}

/**
 * @brief Switch on worldlevel down
 */
void CL_LevelDown(void)
{
	if (!CL_OnBattlescape())
		return;
	if (camera_mode != CAMERA_MODE_FIRSTPERSON)
		Cvar_SetValue("cl_worldlevel", (cl_worldlevel->value > 0.0f) ? cl_worldlevel->value - 1.0f : 0.0f);
}


/**
 * @brief
 */
void CL_ZoomInQuant(void)
{
	float quant;

	/* no zooming in first person mode */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		return;

	/* check zoom quant */
	if (cl_camzoomquant->value < MIN_CAMZOOM_QUANT)
		quant = 1 + MIN_CAMZOOM_QUANT;
	else if (cl_camzoomquant->value > MAX_CAMZOOM_QUANT)
		quant = 1 + MAX_CAMZOOM_QUANT;
	else
		quant = 1 + cl_camzoomquant->value;

	/* change zoom */
	cl.cam.zoom *= quant;

	/* ensure zoom doesnt exceed either MAX_ZOOM or cl_camzoommax */
	cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
}

/**
 * @brief
 */
void CL_ZoomOutQuant(void)
{
	float quant;

	/* no zooming in first person mode */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		return;

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
}

/**
 * @brief
 */
void CL_FireRightPrimary(void)
{
	if (!selActor)
		return;
	if (cl.cmode == M_FIRE_PR)
		cl.cmode = M_MOVE;
	else
		cl.cmode = M_FIRE_PR;
}

/**
 * @brief
 */
void CL_FireRightSecondary(void)
{
	if (!selActor)
		return;
	if (cl.cmode == M_FIRE_SR)
		cl.cmode = M_MOVE;
	else
		cl.cmode = M_FIRE_SR;
}

/**
 * @brief
 */
void CL_FireLeftPrimary(void)
{
	if (!selActor)
		return;
	if (cl.cmode == M_FIRE_PL)
		cl.cmode = M_MOVE;
	else
		cl.cmode = M_FIRE_PL;
}

/**
 * @brief
 */
void CL_FireLeftSecondary(void)
{
	if (!selActor)
		return;
	if (cl.cmode == M_FIRE_SL)
		cl.cmode = M_MOVE;
	else
		cl.cmode = M_FIRE_SL;
}

/**
 * @brief
 */
void CL_ConfirmAction(void)
{
	extern pos3_t mousePos;

	if (!selActor)
		return;
	if (cl.cmode == M_PEND_MOVE)
		CL_ActorStartMove(selActor, mousePos);
	else if (cl.cmode > M_PEND_MOVE) {
		CL_ActorShoot(selActor, mousePos);
		cl.cmode = M_MOVE;
	}
}


/**
 * @brief
 */
void CL_ReloadLeft(void)
{
	CL_ActorReload(csi.idLeft);
}

/**
 * @brief
 */
void CL_ReloadRight(void)
{
	CL_ActorReload(csi.idRight);
}


/**
 * @brief
 */
void CL_SelectDown(void)
{
	if (mouseSpace == MS_WORLD)
		CL_ActorSelectMouse();
	else if (mouseSpace == MS_MENU)
		MN_Click(mx, my);
}

/**
 * @brief
 */
void CL_SelectUp(void)
{
	if (mouseSpace == MS_DRAG)
		MN_Click(mx, my);
	mouseSpace = MS_NULL;
}

/**
 * @brief Mouse click
 */
void CL_ActionDown(void)
{
	switch (mouseSpace) {
	case MS_WORLD:
		CL_ActorActionMouse();
		break;
	case MS_MENU:
		MN_RightClick(mx, my);
		break;
	}
}

/**
 * @brief
 */
void CL_ActionUp(void)
{
	mouseSpace = MS_NULL;
}


/**
 * @brief
 */
void CL_TurnDown(void)
{
	if (mouseSpace == MS_WORLD)
		CL_ActorTurnMouse();
	else if (mouseSpace == MS_MENU)
		MN_MiddleClick(mx, my);
}

/**
 * @brief
 */
void CL_TurnUp(void)
{
	mouseSpace = MS_NULL;
}


/**
 * @brief Cycles between visible aliens
 */
static int lastAlien = 0;

void CL_NextAlien(void)
{
	le_t *le;
	int i;

	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);
	if (lastAlien >= numLEs)
		lastAlien = 0;
	i = lastAlien;
	do {
		if (++i >= numLEs)
			i = 0;
		le = &LEs[i];
		if (le->inuse && (le->type == ET_ACTOR || le->type == ET_UGV) && !(le->state & STATE_DEAD) && le->team != cls.team && le->team != TEAM_CIVILIAN) {
			lastAlien = i;
			V_CenterView(le->pos);
			return;
		}
	}
	while (i != lastAlien);
}


/*========================================================================== */

#ifdef DEBUG
/**
 * @brief Prints the current camera angles
 * @note Only available in debug mode
 * Accessable via console command camangles
 */
void CL_CamPrintAngles(void)
{
	Com_Printf("camera angles %0.3f:%0.3f:%0.3f\n", cl.cam.angles[0], cl.cam.angles[1], cl.cam.angles[2]);
}
#endif /* DEBUG */

/**
 * @brief
 */
void CL_CamSetAngles(void)
{
	int c = Cmd_Argc();

	if (c < 3) {
		Com_Printf("Usage camsetangles <value> <value>\n");
		return;
	}

	cl.cam.angles[0] = atof(Cmd_Argv(1));
	cl.cam.angles[1] = atof(Cmd_Argv(2));
	cl.cam.angles[2] = 0.0f;
}

/**
 * @brief Makes a mapshop - called by basemapshot script command
 *
 * Execute basemapshot in console and load a basemap
 * after this all you have to do is hit the screenshot button (F12)
 * to make a new screenshot of the basetile
 */
void CL_MakeBaseMapShot(void)
{
	if (Cmd_Argc() > 1)
		Cbuf_ExecuteText(EXEC_NOW, va("map %s", Cmd_Argv(1)));
	cl.cam.angles[0] = 60.0f;
	cl.cam.angles[1] = 90.0f;
	Cvar_SetValue("r_isometric", 1);
	MN_PushMenu("nohud");
	Cbuf_ExecuteText(EXEC_NOW, "toggleconsole");
	Cbuf_ExecuteText(EXEC_NOW, "screenshot");
}

/**
 * @brief Init some bindable commands
 */
void CL_InitInput(void)
{
	Cmd_AddCommand("+turnleft", IN_TurnLeftDown, NULL);
	Cmd_AddCommand("-turnleft", IN_TurnLeftUp, NULL);
	Cmd_AddCommand("+turnright", IN_TurnRightDown, NULL);
	Cmd_AddCommand("-turnright", IN_TurnRightUp, NULL);
	Cmd_AddCommand("+turnup", IN_TurnUpDown, NULL);
	Cmd_AddCommand("-turnup", IN_TurnUpUp, NULL);
	Cmd_AddCommand("+turndown", IN_TurnDownDown, NULL);
	Cmd_AddCommand("-turndown", IN_TurnDownUp, NULL);
	Cmd_AddCommand("+shiftleft", IN_ShiftLeftDown, NULL);
	Cmd_AddCommand("-shiftleft", IN_ShiftLeftUp, NULL);
	Cmd_AddCommand("+shiftright", IN_ShiftRightDown, NULL);
	Cmd_AddCommand("-shiftright", IN_ShiftRightUp, NULL);
	Cmd_AddCommand("+shiftup", IN_ShiftUpDown, NULL);
	Cmd_AddCommand("-shiftup", IN_ShiftUpUp, NULL);
	Cmd_AddCommand("+shiftdown", IN_ShiftDownDown, NULL);
	Cmd_AddCommand("-shiftdown", IN_ShiftDownUp, NULL);
	Cmd_AddCommand("+zoomin", IN_ZoomInDown, NULL);
	Cmd_AddCommand("-zoomin", IN_ZoomInUp, NULL);
	Cmd_AddCommand("+zoomout", IN_ZoomOutDown, NULL);
	Cmd_AddCommand("-zoomout", IN_ZoomOutUp, NULL);

	Cmd_AddCommand("impulse", IN_Impulse, NULL);

	Cmd_AddCommand("+select", CL_SelectDown, NULL);
	Cmd_AddCommand("-select", CL_SelectUp, NULL);
	Cmd_AddCommand("+action", CL_ActionDown, NULL);
	Cmd_AddCommand("-action", CL_ActionUp, NULL);
	Cmd_AddCommand("+turn", CL_TurnDown, NULL);
	Cmd_AddCommand("-turn", CL_TurnUp, NULL);
	Cmd_AddCommand("standcrouch", CL_ActorStandCrouch, _("Toggle stand/crounch"));
	Cmd_AddCommand("togglereaction", CL_ActorToggleReaction, _("Toggle reaction fire"));
	Cmd_AddCommand("nextalien", CL_NextAlien, _("Toogle to next alien"));
	Cmd_AddCommand("firerp", CL_FireRightPrimary, NULL);
	Cmd_AddCommand("firers", CL_FireRightSecondary, NULL);
	Cmd_AddCommand("firelp", CL_FireLeftPrimary, NULL);
	Cmd_AddCommand("firels", CL_FireLeftSecondary, NULL);
	Cmd_AddCommand("reloadleft", CL_ReloadLeft, NULL);
	Cmd_AddCommand("reloadright", CL_ReloadRight, NULL);
	Cmd_AddCommand("nextround", CL_NextRound, _("Ends current round"));
	Cmd_AddCommand("levelup", CL_LevelUp, NULL);
	Cmd_AddCommand("leveldown", CL_LevelDown, NULL);
	Cmd_AddCommand("zoominquant", CL_ZoomInQuant, NULL);
	Cmd_AddCommand("zoomoutquant", CL_ZoomOutQuant, NULL);
	Cmd_AddCommand("confirmaction", CL_ConfirmAction, _("Confirm the current action"));

#ifdef DEBUG
	Cmd_AddCommand("camangles", CL_CamPrintAngles, NULL);
#endif /* DEBUG */
	Cmd_AddCommand("camsetangles", CL_CamSetAngles, NULL);
	Cmd_AddCommand("basemapshot", CL_MakeBaseMapShot, NULL);

	Cmd_AddCommand("cameramode", CL_CameraMode, NULL);
}


#define STATE_FORWARD	1
#define STATE_RIGHT		2
#define STATE_ZOOM		3
#define STATE_ROT		4
#define STATE_TILT		5

#define SCROLL_BORDER	4

/**
 * @brief
 * @note see SCROLL_BORDER define
 */
float CL_GetKeyMouseState(int dir)
{
	float value;

	switch (dir) {
	case STATE_FORWARD:
		value = (in_shiftup.state & 1) + (my <= SCROLL_BORDER) - (in_shiftdown.state & 1) - (my >= VID_NORM_HEIGHT - SCROLL_BORDER);
		break;
	case STATE_RIGHT:
		value = (in_shiftright.state & 1) + (mx >= VID_NORM_WIDTH - SCROLL_BORDER) - (in_shiftleft.state & 1) - (mx <= SCROLL_BORDER);
		break;
	case STATE_ZOOM:
		value = (in_zoomin.state & 1) - (in_zoomout.state & 1);
		break;
	case STATE_ROT:
		value = (in_turnleft.state & 1) - (in_turnright.state & 1);
		break;
	case STATE_TILT:
		value = (in_turnup.state & 1) - (in_turndown.state & 1);
		break;
	default:
		value = 0.0;
		break;
	}

	return value;
}


/**
 * @brief
 */
void CL_CameraMoveFirstPerson(void)
{
	float rotation_speed;

	rotation_speed =
		(cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ((cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED) : MIN_CAMROT_SPEED;
	/* look left */
	if ((in_turnleft.state & 1) && ((cl.cam.angles[YAW] - selActor->angles[YAW]) < 90))
		cl.cam.angles[YAW] += cls.frametime * rotation_speed;

	/* look right */
	if ((in_turnright.state & 1) && ((selActor->angles[YAW] - cl.cam.angles[YAW]) < 90))
		cl.cam.angles[YAW] -= cls.frametime * rotation_speed;

	/* look down */
	if ((in_turndown.state & 1) && (cl.cam.angles[PITCH] < 45))
		cl.cam.angles[PITCH] += cls.frametime * rotation_speed;

	/* look up */
	if ((in_turnup.state & 1) && (cl.cam.angles[PITCH] > -45))
		cl.cam.angles[PITCH] -= cls.frametime * rotation_speed;

	/* ensure camera's horizontal position is over actor */
	Vector2Copy(selActor->origin, cl.cam.camorg);

	/* check if actor is starting a crouch/stand action */
	if (wasCrouched != (selActor->state & STATE_CROUCHED)) {
		doCrouch = qtrue;
		wasCrouched = selActor->state & STATE_CROUCHED;
	}

	/* if in process of crouching or standing, move camera vertically */
	if (doCrouch) {
		if (selActor->state & STATE_CROUCHED) {
			cl.cam.camorg[2] -= 11.*cls.frametime;
			if (cl.cam.camorg[2] < selActor->origin[2]) {
				cl.cam.camorg[2] = selActor->origin[2];
				doCrouch = qfalse;
			}
		} else {
			cl.cam.camorg[2] += 16.*cls.frametime;
			if (cl.cam.camorg[2] > selActor->origin[2] + EYE_HT_OFFSET) {
				cl.cam.camorg[2] = selActor->origin[2] + EYE_HT_OFFSET;
				doCrouch = qfalse;
			}
		}
	}

	/* move camera forward horizontally to where soldier eyes are */
	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);
	cl.cam.axis[0][2] = 0.;
	VectorMA(cl.cam.camorg, 4., cl.cam.axis[0], cl.cam.camorg);
}


/**
 * @brief
 */
void CL_CameraMoveRemote(void)
{
	float angle, frac;
	static float sy, cy;
	vec3_t g_forward, g_right, g_up;
	vec3_t delta;
	float rotspeed, rotaccel;
	float movespeed, moveaccel;
	int i;

	/* get relevant variables */
	rotspeed =
		(cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ((cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED) : MIN_CAMROT_SPEED;
	rotaccel =
		(cl_camrotaccel->value > MIN_CAMROT_ACCEL) ? ((cl_camrotaccel->value < MAX_CAMROT_ACCEL) ? cl_camrotaccel->value : MAX_CAMROT_ACCEL) : MIN_CAMROT_ACCEL;
	movespeed =
		(cl_cammovespeed->value >
		MIN_CAMMOVE_SPEED) ? ((cl_cammovespeed->value < MAX_CAMMOVE_SPEED) ? cl_cammovespeed->value : MAX_CAMMOVE_SPEED) : MIN_CAMMOVE_SPEED;
	moveaccel =
		(cl_cammoveaccel->value >
		MIN_CAMMOVE_ACCEL) ? ((cl_cammoveaccel->value < MAX_CAMMOVE_ACCEL) ? cl_cammoveaccel->value : MAX_CAMMOVE_ACCEL) : MIN_CAMMOVE_ACCEL;

	/* calculate camera omega */
	/* stop acceleration */
	frac = cls.frametime * rotaccel;

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
		if (VectorDist(cl.cam.reforg, routeFrom) > routeDist - 200) {
			VectorMA(cl.cam.speed, -frac, routeDelta, cl.cam.speed);
			VectorNormalize2(cl.cam.speed, delta);
			if (DotProduct(delta, routeDelta) < 0.05) {
				blockEvents = qfalse;
				cameraRoute = qfalse;
			}
		} else
			VectorMA(cl.cam.speed, frac, routeDelta, cl.cam.speed);
	} else {
		/* normal camera movement */
		/* calculate ground-based movement vectors */
		angle = cl.cam.angles[YAW] * (M_PI * 2 / 360);
		sy = sin(angle);
		cy = cos(angle);

		VectorSet(g_forward, cy, sy, 0.0);
		VectorSet(g_right, sy, -cy, 0.0);
		VectorSet(g_up, 0.0, 0.0, 1.0);

		/* calculate camera speed */
		/* stop acceleration */
		frac = cls.frametime * moveaccel;
		if (VectorLength(cl.cam.speed) > frac) {
			VectorNormalize2(cl.cam.speed, delta);
			VectorMA(cl.cam.speed, -frac, delta, cl.cam.speed);
		} else
			VectorClear(cl.cam.speed);

		/* acceleration */
		frac = cls.frametime * moveaccel * 2;
		VectorClear(delta);
		VectorScale(g_forward, CL_GetKeyMouseState(STATE_FORWARD), delta);
		VectorMA(delta, CL_GetKeyMouseState(STATE_RIGHT), g_right, delta);
		VectorNormalize(delta);
		VectorMA(cl.cam.speed, frac, delta, cl.cam.speed);

		/* lerp the level */
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

	/* calc new camera reference origin */
	VectorMA(cl.cam.reforg, cls.frametime, cl.cam.speed, cl.cam.reforg);
	if (cl.cam.reforg[0] < map_min[0])
		cl.cam.reforg[0] = map_min[0];
	if (cl.cam.reforg[1] < map_min[1])
		cl.cam.reforg[1] = map_min[1];
	if (cl.cam.reforg[0] > map_max[0])
		cl.cam.reforg[0] = map_max[0];
	if (cl.cam.reforg[1] > map_max[1])
		cl.cam.reforg[1] = map_max[1];
	cl.cam.reforg[2] = 0;

	/* calc real camera origin */
	VectorMA(cl.cam.reforg, -CAMERA_START_DIST + cl.cam.lerplevel * CAMERA_LEVEL_DIST, cl.cam.axis[0], cl.cam.camorg);
	cl.cam.camorg[2] += CAMERA_START_HEIGHT + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;

	/* zoom change */
	frac = CL_GetKeyMouseState(STATE_ZOOM);
	if (frac > 0.1) {
		cl.cam.zoom *= 1.0 + cls.frametime * ZOOM_SPEED * frac;
		/* ensure zoom not greater than either MAX_ZOOM or cl_camzoommax */
		cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
	} else if (frac < -0.1) {
		cl.cam.zoom /= 1.0 - cls.frametime * ZOOM_SPEED * frac;
		/* ensure zoom isnt less than either MIN_ZOOM or cl_camzoommin */
		cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
	}
}

/**
 * @brief
 */
void CL_CameraMove(void)
{
	if (cls.state != ca_active)
		return;

	if (!scr_vrect.width || !scr_vrect.height)
		return;

	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraMoveFirstPerson();
	else
		CL_CameraMoveRemote();
}

/**
 * @brief
 */
void CL_CameraRoute(pos3_t from, pos3_t target)
{
	/* initialize the camera route variables */
	PosToVec(from, routeFrom);
	PosToVec(target, routeDelta);
	VectorSubtract(routeDelta, routeFrom, routeDelta);
	routeDelta[2] = 0;
	routeDist = VectorLength(routeDelta);
	VectorNormalize(routeDelta);

	routeLevelStart = from[2];
	routeLevelEnd = target[2];
	VectorCopy(routeFrom, cl.cam.reforg);
	Cvar_SetValue("cl_worldlevel", target[2]);

	VectorClear(cl.cam.speed);
	cameraRoute = qtrue;
	blockEvents = qtrue;
}

#define ROTATE_SPEED	0.5
/**
 * @brief Called every frame to parse the input
 * @sa CL_Frame
 */
void CL_ParseInput(void)
{
	int i, oldx, oldy;

	/* get new position */
	oldx = mx;
	oldy = my;
	IN_GetMousePos(&mx, &my);

	switch (mouseSpace) {
	case MS_ROTATE:
		/* rotate a model */
		rotateAngles[1] -= ROTATE_SPEED * (mx - oldx);
		rotateAngles[2] += ROTATE_SPEED * (my - oldy);
		while (rotateAngles[1] > 360.0)
			rotateAngles[1] -= 360.0;
		while (rotateAngles[1] < 0.0)
			rotateAngles[1] += 360.0;

		if (rotateAngles[2] < 0.0)
			rotateAngles[2] = 0.0;
		else if (rotateAngles[2] > 180.0)
			rotateAngles[2] = 180.0;
		return;

	case MS_SHIFTMAP:
		/* shift the map */
		ccs.center[0] -= (float) (mx - oldx) / (1024 * ccs.zoom);
		ccs.center[1] -= (float) (my - oldy) / (512 * ccs.zoom);
		for (i = 0; i < 2; i++) {
			while (ccs.center[i] < 0.0)
				ccs.center[i] += 1.0;
			while (ccs.center[i] > 1.0)
				ccs.center[i] -= 1.0;
		}
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
		return;

	case MS_ZOOMMAP:
		/* zoom the map */
		ccs.zoom *= pow(0.995, my - oldy);
		if (ccs.zoom < 1.0)
			ccs.zoom = 1.0;
		else if (ccs.zoom > 6.0)
			ccs.zoom = 6.0;

		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
		return;

	case MS_DRAG:
		/* do nothing */
		return;

	default:
		/* standard menu and world mouse handling */
		if (MN_CursorOnMenu(mx, my)) {
			mouseSpace = MS_MENU;
			return;
		}

		mouseSpace = MS_NULL;

		if (cls.state != ca_active)
			return;

		if (!scr_vrect.width || !scr_vrect.height)
			return;

		if (cl.cmode <= M_PEND_MOVE)
			CL_ActorMouseTrace();
		else
			mouseSpace = MS_WORLD;
		return;
	}
}
