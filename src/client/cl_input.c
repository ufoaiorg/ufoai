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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t	*cl_nodelta;

extern	unsigned	sys_frame_time;
unsigned	frame_msec;
unsigned	old_sys_frame_time;

int			mouseSpace;
int			mx, my;
int			dragFrom, dragFromX, dragFromY;
item_t		dragItem;
float		*rotateAngles;

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

kbutton_t	in_turnleft, in_turnright, in_shiftleft, in_shiftright;
kbutton_t	in_shiftup, in_shiftdown;
kbutton_t	in_zoomin, in_zoomout;
kbutton_t	in_select, in_action, in_turn;
kbutton_t	in_turnup, in_turndown;

int			in_impulse;


void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Com_Printf (_("Three keys down for a button!\n") );
		return;
	}

	if (b->state & 1)
		return;		// still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}


void IN_SelectDown(void) {KeyDown(&in_select);}
void IN_SelectUp(void) {KeyUp(&in_select);}
void IN_ActionDown(void) {KeyDown(&in_action);}
void IN_ActionUp(void) {KeyUp(&in_action);}
void IN_TurnDown(void) {KeyDown(&in_turn);}
void IN_TurnUp(void) {KeyUp(&in_turn);}

void IN_TurnLeftDown(void) {KeyDown(&in_turnleft);}
void IN_TurnLeftUp(void) {KeyUp(&in_turnleft);}
void IN_TurnRightDown(void) {KeyDown(&in_turnright);}
void IN_TurnRightUp(void) {KeyUp(&in_turnright);}
void IN_TurnUpDown(void) {KeyDown(&in_turnup);}
void IN_TurnUpUp(void) {KeyUp(&in_turnup);}
void IN_TurnDownDown(void) {KeyDown(&in_turndown);}
void IN_TurnDownUp(void) {KeyUp(&in_turndown);}
void IN_ShiftLeftDown(void) {KeyDown(&in_shiftleft);}
void IN_ShiftLeftUp(void) {KeyUp(&in_shiftleft);}
void IN_ShiftRightDown(void) {KeyDown(&in_shiftright);}
void IN_ShiftRightUp(void) {KeyUp(&in_shiftright);}
void IN_ShiftUpDown(void) {KeyDown(&in_shiftup);}
void IN_ShiftUpUp(void) {KeyUp(&in_shiftup);}
void IN_ShiftDownDown(void) {KeyDown(&in_shiftdown);}
void IN_ShiftDownUp(void) {KeyUp(&in_shiftdown);}
void IN_ZoomInDown(void) {KeyDown(&in_zoomin);}
void IN_ZoomInUp(void) {KeyUp(&in_zoomin);}
void IN_ZoomOutDown(void) {KeyDown(&in_zoomout);}
void IN_ZoomOutUp(void) {KeyUp(&in_zoomout);}

void IN_Impulse (void) {in_impulse=atoi(Cmd_Argv(1));}

/*
===============
CL_Camera_Mode

Switches camera mode between remote and firstperson
===============
*/

void CL_CameraMode (void)
{
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange (CAMERA_MODE_REMOTE);
	else
		CL_CameraModeChange (CAMERA_MODE_FIRSTPERSON);
}

void CL_CameraModeChange (camera_mode_t new_camera_mode)
{
	static vec3_t save_camorg, save_camangles;
	static float  save_camzoom;
	static int save_level;

	// save remote camera position, angles, zoom

	if (camera_mode == CAMERA_MODE_REMOTE)
	{
		VectorCopy(cl.cam.camorg, save_camorg);
		VectorCopy(cl.cam.angles, save_camangles);
		save_camzoom = cl.cam.zoom;
		save_level = cl_worldlevel->value;
	}

	if (new_camera_mode == CAMERA_MODE_REMOTE) /* toggle camera mode */
	{
		Com_Printf ( _("Changed camera mode to remote.\n") );
		camera_mode = CAMERA_MODE_REMOTE;
		VectorCopy(save_camorg, cl.cam.camorg);
		VectorCopy(save_camangles, cl.cam.angles);
		cl.cam.zoom = save_camzoom;
		Cvar_SetValue( "cl_worldlevel", save_level );
	}
	else
	{
		Com_Printf ( _("Changed camera mode to first-person.\n") );
		camera_mode = CAMERA_MODE_FIRSTPERSON;
		VectorCopy(selActor->origin, cl.cam.camorg);
		Cvar_SetValue( "cl_worldlevel", selActor->pos[2] );
		if (!(selActor->state & STATE_CROUCHED))
			cl.cam.camorg[2] += 10; /* raise from waist to head */
		VectorCopy(selActor->angles, cl.cam.angles);
		cl.cam.zoom = 1.0;
		Cvar_SetValue( "cl_worldlevel", 7 );
	}
}

/*
===============
CL_Time_f

print the time long integer value
===============
*/

void CL_Time_f (void)
{
	Com_Printf ("time: %d\n", cl.time);
}

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state)
	{	// still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

#if 0
	if (msec)
	{
		Com_Printf ("%i ", msec);
	}
#endif

	val = (float)msec / frame_msec;
	if (val < 0)
		val = 0;
	if (val > 1)
		val = 1;

	return val;
}


//==========================================================================

float MIN_ZOOM = 0.5;
float MAX_ZOOM = 3.0;

cvar_t	*cl_camrotspeed;
cvar_t	*cl_camrotaccel;
cvar_t	*cl_cammovespeed;
cvar_t	*cl_cammoveaccel;
cvar_t	*cl_camyawspeed;
cvar_t	*cl_campitchspeed;
cvar_t	*cl_camzoomquant;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;

#define MIN_CAMROT_SPEED	50
#define MIN_CAMROT_ACCEL	50
#define MAX_CAMROT_SPEED	1000
#define MAX_CAMROT_ACCEL	1000
#define MIN_CAMMOVE_SPEED	150
#define MIN_CAMMOVE_ACCEL	150
#define MAX_CAMMOVE_SPEED	3000
#define MAX_CAMMOVE_ACCEL	3000
#define ZOOM_SPEED			1.4
//#define MIN_ZOOM            0.5
//#define MAX_ZOOM			3.0
#define MIN_CAMZOOM_QUANT	0.2
#define MAX_CAMZOOM_QUANT	1.0
#define LEVEL_SPEED			3.0
#define LEVEL_MIN			0.05

#define CAMERA_START_DIST	600
#define CAMERA_LEVEL_DIST	0
#define CAMERA_START_HEIGHT	0
#define CAMERA_LEVEL_HEIGHT	64

//==========================================================================

/*
============
CL_LevelUp
============
*/
void CL_LevelUp( void )
{
	if (camera_mode != CAMERA_MODE_FIRSTPERSON)
		Cvar_SetValue( "cl_worldlevel", (cl_worldlevel->value < 7) ? cl_worldlevel->value + 1 : 7 );
}

/*
============
CL_LevelDown
============
*/
void CL_LevelDown( void )
{
	if (camera_mode != CAMERA_MODE_FIRSTPERSON)
		Cvar_SetValue( "cl_worldlevel", (cl_worldlevel->value > 0) ? cl_worldlevel->value - 1 : 0 );
}


/*
============
CL_ZoomInQuant
============
*/
void CL_ZoomInQuant( void )
{
	float	quant;

	// check zoom quant
	if ( cl_camzoomquant->value < MIN_CAMZOOM_QUANT ) quant = 1 + MIN_CAMZOOM_QUANT;
	else if ( cl_camzoomquant->value > MAX_CAMZOOM_QUANT ) quant = 1 + MAX_CAMZOOM_QUANT;
	else quant = 1 + cl_camzoomquant->value;

	// change zoom
	cl.cam.zoom *= quant;

	// test boundaris
	if ( cl.cam.zoom > MAX_ZOOM ) cl.cam.zoom = MAX_ZOOM;
}

/*
============
CL_ZoomOutQuant
============
*/
void CL_ZoomOutQuant( void )
{
	float	quant;

	// check zoom quant
	if ( cl_camzoomquant->value < MIN_CAMZOOM_QUANT ) quant = 1 + MIN_CAMZOOM_QUANT;
	else if ( cl_camzoomquant->value > MAX_CAMZOOM_QUANT ) quant = 1 + MAX_CAMZOOM_QUANT;
	else quant = 1 + cl_camzoomquant->value;

	// change zoom
	cl.cam.zoom /= quant;

	// test boundaries
	if ( cl.cam.zoom < MIN_ZOOM ) cl.cam.zoom = MIN_ZOOM;
}

/*
============
CL_FireRightPrimary
============
*/
void CL_FireRightPrimary( void )
{
	if ( !selActor ) return;
	if ( cl.cmode == M_FIRE_PR ) cl.cmode = M_MOVE;
	else cl.cmode = M_FIRE_PR;
}

/*
============
CL_FireRightSecondary
============
*/
void CL_FireRightSecondary( void )
{
	if ( !selActor ) return;
	if ( cl.cmode == M_FIRE_SR ) cl.cmode = M_MOVE;
	else cl.cmode = M_FIRE_SR;
}

/*
============
CL_FireLeftPrimary
============
*/
void CL_FireLeftPrimary( void )
{
	if ( !selActor ) return;
	if ( cl.cmode == M_FIRE_PL ) cl.cmode = M_MOVE;
	else cl.cmode = M_FIRE_PL;
}

/*
============
CL_FireLeftSecondary
============
*/
void CL_FireLeftSecondary( void )
{
	if ( !selActor ) return;
	if ( cl.cmode == M_FIRE_SL ) cl.cmode = M_MOVE;
	else cl.cmode = M_FIRE_SL;
}

/*
============
CL_ConfirmAction
============
*/
void CL_ConfirmAction( void )
{
	extern pos3_t	mousePos;

	if ( !selActor ) return;
	if ( cl.cmode == M_PEND_MOVE )
		CL_ActorStartMove( selActor, mousePos );
	else if ( cl.cmode > M_PEND_MOVE )
	{
		CL_ActorShoot( selActor, mousePos );
		cl.cmode = M_MOVE;
	}
}


/*
============
CL_ReloadLeft
============
*/
void CL_ReloadLeft( void )
{
	CL_ActorReload( csi.idLeft );
}

/*
============
CL_ReloadRight
============
*/
void CL_ReloadRight( void )
{
	CL_ActorReload( csi.idRight );
}


/*
============
CL_SelectDown
============
*/
void CL_SelectDown( void )
{
	if ( mouseSpace == MS_WORLD )
		CL_ActorSelectMouse();
	else if ( mouseSpace == MS_MENU )
		MN_Click( mx, my );
}

/*
============
CL_SelectUp
============
*/
void CL_SelectUp( void )
{
	if ( mouseSpace == MS_DRAG )
		MN_Click( mx, my );
	mouseSpace = MS_NULL;
}

/*
============
CL_ActionDown
============
*/
void CL_ActionDown( void )
{
	if ( mouseSpace == MS_WORLD )
		CL_ActorActionMouse();
	else if ( mouseSpace == MS_MENU )
		MN_RightClick( mx, my );
}

/*
============
CL_ActionUp
============
*/
void CL_ActionUp( void )
{
	mouseSpace = MS_NULL;
}


/*
============
CL_TurnDown
============
*/
void CL_TurnDown( void )
{
	if ( mouseSpace == MS_WORLD )
		CL_ActorTurnMouse();
	else if ( mouseSpace == MS_MENU )
		MN_MiddleClick( mx, my );
}

/*
============
CL_TurnUp
============
*/
void CL_TurnUp( void )
{
	mouseSpace = MS_NULL;
}


/*
=================
CL_NextAlien
=================
*/
int lastAlien = 0;

void CL_NextAlien( void )
{
	le_t *le;
	int i;

	if ( camera_mode == CAMERA_MODE_FIRSTPERSON )
		CL_CameraModeChange( CAMERA_MODE_REMOTE );
	if ( lastAlien >= numLEs ) lastAlien = 0;
	i = lastAlien;
	do {
		if ( ++i >= numLEs ) i = 0;
		le = &LEs[i];
		if ( le->inuse && le->type == ET_ACTOR && !(le->state & STATE_DEAD) &&
			le->team != cls.team && le->team != TEAM_CIVILIAN )
		{
			lastAlien = i;
			V_CenterView( le->pos );
			return;
		}
	}
	while ( i != lastAlien );
}


//==========================================================================


/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand( "+turnleft", IN_TurnLeftDown );
	Cmd_AddCommand( "-turnleft", IN_TurnLeftUp );
	Cmd_AddCommand( "+turnright", IN_TurnRightDown );
	Cmd_AddCommand( "-turnright", IN_TurnRightUp );
	Cmd_AddCommand( "+turnup", IN_TurnUpDown );
	Cmd_AddCommand( "-turnup", IN_TurnUpUp );
	Cmd_AddCommand( "+turndown", IN_TurnDownDown );
	Cmd_AddCommand( "-turndown", IN_TurnDownUp );
	Cmd_AddCommand( "+shiftleft", IN_ShiftLeftDown );
	Cmd_AddCommand( "-shiftleft", IN_ShiftLeftUp );
	Cmd_AddCommand( "+shiftright", IN_ShiftRightDown );
	Cmd_AddCommand( "-shiftright", IN_ShiftRightUp );
	Cmd_AddCommand( "+shiftup", IN_ShiftUpDown );
	Cmd_AddCommand( "-shiftup", IN_ShiftUpUp );
	Cmd_AddCommand( "+shiftdown", IN_ShiftDownDown );
	Cmd_AddCommand( "-shiftdown", IN_ShiftDownUp );
	Cmd_AddCommand( "+zoomin", IN_ZoomInDown );
	Cmd_AddCommand( "-zoomin", IN_ZoomInUp );
	Cmd_AddCommand( "+zoomout", IN_ZoomOutDown );
	Cmd_AddCommand( "-zoomout", IN_ZoomOutUp );

	Cmd_AddCommand( "impulse", IN_Impulse );

	Cmd_AddCommand( "+select", CL_SelectDown );
	Cmd_AddCommand( "-select", CL_SelectUp );
	Cmd_AddCommand( "+action", CL_ActionDown );
	Cmd_AddCommand( "-action", CL_ActionUp );
	Cmd_AddCommand( "+turn", CL_TurnDown );
	Cmd_AddCommand( "-turn", CL_TurnUp );
	Cmd_AddCommand( "standcrouch", CL_ActorStandCrouch );
	Cmd_AddCommand( "togglereaction", CL_ActorToggleReaction );
	Cmd_AddCommand( "nextalien", CL_NextAlien );
	Cmd_AddCommand( "firerp", CL_FireRightPrimary );
	Cmd_AddCommand( "firers", CL_FireRightSecondary );
	Cmd_AddCommand( "firelp", CL_FireLeftPrimary );
	Cmd_AddCommand( "firels", CL_FireLeftSecondary );
	Cmd_AddCommand( "reloadleft", CL_ReloadLeft );
	Cmd_AddCommand( "reloadright", CL_ReloadRight );
	Cmd_AddCommand( "nextround", CL_NextRound );
	Cmd_AddCommand( "levelup", CL_LevelUp );
	Cmd_AddCommand( "leveldown", CL_LevelDown );
	Cmd_AddCommand( "zoominquant", CL_ZoomInQuant );
	Cmd_AddCommand( "zoomoutquant", CL_ZoomOutQuant );
	Cmd_AddCommand( "confirmaction", CL_ConfirmAction );

	Cmd_AddCommand( "cameramode", CL_CameraMode );

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
}


#define STATE_FORWARD	1
#define STATE_RIGHT		2
#define STATE_ZOOM		3
#define STATE_ROT		4
#define STATE_TILT		5
/*
=================
CL_GetKeyMouseState
=================
*/
float CL_GetKeyMouseState ( int dir )
{
	float	value;

	if ( dir == STATE_FORWARD )
		value = (in_shiftup.state & 1) + (my <= 0) - (in_shiftdown.state & 1) - (my >= VID_NORM_HEIGHT-4);

	else if ( dir == STATE_RIGHT )
		value = (in_shiftright.state & 1) + (mx >= VID_NORM_WIDTH-4) - (in_shiftleft.state & 1) - (mx <= 0);

	else if ( dir == STATE_ZOOM )
		value = (in_zoomin.state & 1) - (in_zoomout.state & 1);

	else if ( dir == STATE_ROT )
		value = (in_turnleft.state & 1) - (in_turnright.state & 1);

	else if ( dir == STATE_TILT )
		value = (in_turnup.state & 1) - (in_turndown.state & 1);

	else
		return 0.0;

	return value;
}


//==========================================================================

qboolean	cameraRoute = false;
vec3_t		routeFrom, routeDelta;
float		routeDist;
int			routeLevelStart, routeLevelEnd;

/*
================
CL_CameraMoveFirstPerson
================
*/
void CL_CameraMoveFirstPerson (void)
{
	float frac;
	float rotation_speed;

	rotation_speed = (cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ( (cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED ) : MIN_CAMROT_SPEED;
	/* look left */
	if ( (in_turnleft.state & 1) && ((cl.cam.angles[YAW]-selActor->angles[YAW])<90) )
		cl.cam.angles[YAW]+=cls.frametime*rotation_speed;

	/* look right */
	if ( (in_turnright.state & 1) && ((selActor->angles[YAW]-cl.cam.angles[YAW])<90) )
		cl.cam.angles[YAW]-=cls.frametime*rotation_speed;

	/* look down */
	if ( (in_turndown.state & 1) && (cl.cam.angles[PITCH] < 45) )
		cl.cam.angles[PITCH]+=cls.frametime*rotation_speed;

	/* look up */
	if ( (in_turnup.state & 1) && (cl.cam.angles[PITCH] > -45) )
		cl.cam.angles[PITCH]-=cls.frametime*rotation_speed;

	/* zoom */
	frac = CL_GetKeyMouseState(STATE_ZOOM);
	if ( frac > 0.1 ) cl.cam.zoom *= 1.0 + cls.frametime * ZOOM_SPEED * frac;
	if ( frac <-0.1 ) cl.cam.zoom /= 1.0 - cls.frametime * ZOOM_SPEED * frac;

	if (cl.cam.zoom > MAX_ZOOM) cl.cam.zoom = MAX_ZOOM;
	if (cl.cam.zoom < MIN_ZOOM) cl.cam.zoom = MIN_ZOOM;
}


/*
================
CL_CameraMoveRemote
================
*/
void CL_CameraMoveRemote (void)
{
	float			angle, frac;
	static float	sy, cy;
	vec3_t			g_forward, g_right, g_up;
	vec3_t			delta;
	float			rotspeed, rotaccel;
	float			movespeed, moveaccel;
	int				i;

	// get relevant variables
	rotspeed = (cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ( (cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED ) : MIN_CAMROT_SPEED;
	rotaccel = (cl_camrotaccel->value > MIN_CAMROT_ACCEL) ? ( (cl_camrotaccel->value < MAX_CAMROT_ACCEL) ? cl_camrotaccel->value : MAX_CAMROT_ACCEL ) : MIN_CAMROT_ACCEL;
	movespeed = (cl_cammovespeed->value > MIN_CAMMOVE_SPEED) ? ( (cl_cammovespeed->value < MAX_CAMMOVE_SPEED) ? cl_cammovespeed->value : MAX_CAMMOVE_SPEED ) : MIN_CAMMOVE_SPEED;
	moveaccel = (cl_cammoveaccel->value > MIN_CAMMOVE_ACCEL) ? ( (cl_cammoveaccel->value < MAX_CAMMOVE_ACCEL) ? cl_cammoveaccel->value : MAX_CAMMOVE_ACCEL ) : MIN_CAMMOVE_ACCEL;

	// calculate camera omega
	// stop acceleration
	frac = cls.frametime * rotaccel;

	for ( i = 0; i < 2; i++ )
	{
		if ( fabs( cl.cam.omega[i] ) > frac )
		{
			if ( cl.cam.omega[i] > 0 ) cl.cam.omega[i] -= frac;
			else cl.cam.omega[i] += frac;
		}
		else cl.cam.omega[i] = 0;

		// rotational acceleration
		if ( i == YAW ) cl.cam.omega[i] += CL_GetKeyMouseState(STATE_ROT) * frac * 2;
		else cl.cam.omega[i] += CL_GetKeyMouseState(STATE_TILT) * frac * 2;

		if (  cl.cam.omega[i] > rotspeed ) cl.cam.omega[i] =  rotspeed;
		if ( -cl.cam.omega[i] > rotspeed ) cl.cam.omega[i] = -rotspeed;
	}

	cl.cam.omega[ROLL] = 0;
	VectorMA( cl.cam.angles, cls.frametime, cl.cam.omega, cl.cam.angles );
	if ( cl.cam.angles[PITCH] > 90.0 ) cl.cam.angles[PITCH] = 90.0;
	if ( cl.cam.angles[PITCH] < 50.0 ) cl.cam.angles[PITCH] = 50.0;

	AngleVectors( cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2] );

	// camera route overrides user input
	if ( cameraRoute )
	{
		// camera route
		frac = cls.frametime * moveaccel * 2;
		if ( VectorDist( cl.cam.reforg, routeFrom ) > routeDist - 200 )
		{
			VectorMA( cl.cam.speed, -frac, routeDelta, cl.cam.speed );
			VectorNormalize2( cl.cam.speed, delta );
			if ( DotProduct( delta, routeDelta ) < 0.05 )
			{
				blockEvents = false;
				cameraRoute = false;
			}
		}
		else VectorMA( cl.cam.speed, frac, routeDelta, cl.cam.speed );
	}
	else
	{
		// normal camera movement
		// calculate ground-based movement vectors
		angle = cl.cam.angles[YAW] * (M_PI*2 / 360);
		sy = sin(angle);
		cy = cos(angle);

		VectorSet( g_forward, cy, sy, 0.0 );
		VectorSet( g_right, sy, -cy, 0.0 );
		VectorSet( g_up, 0.0, 0.0, 1.0 );

		// calculate camera speed
		// stop acceleration
		frac = cls.frametime * moveaccel;
		if ( VectorLength( cl.cam.speed ) > frac )
		{
			VectorNormalize2( cl.cam.speed, delta );
			VectorMA( cl.cam.speed, -frac, delta, cl.cam.speed );
		}
		else VectorClear( cl.cam.speed );

		// acceleration
		frac = cls.frametime * moveaccel * 2;
		VectorClear( delta );
		VectorScale( g_forward, CL_GetKeyMouseState(STATE_FORWARD), delta );
		VectorMA( delta, CL_GetKeyMouseState(STATE_RIGHT), g_right, delta );
		VectorNormalize( delta );
		VectorMA( cl.cam.speed, frac, delta, cl.cam.speed );

		// lerp the level
		if ( cl.cam.lerplevel < cl_worldlevel->value )
		{
			cl.cam.lerplevel += LEVEL_SPEED * (cl_worldlevel->value - cl.cam.lerplevel + LEVEL_MIN) * cls.frametime;
			if ( cl.cam.lerplevel > cl_worldlevel->value ) cl.cam.lerplevel = cl_worldlevel->value;
		}
		else if ( cl.cam.lerplevel > cl_worldlevel->value )
		{
			cl.cam.lerplevel -= LEVEL_SPEED * (cl.cam.lerplevel - cl_worldlevel->value + LEVEL_MIN) * cls.frametime;
			if ( cl.cam.lerplevel < cl_worldlevel->value ) cl.cam.lerplevel = cl_worldlevel->value;
		}
	}

	// clamp speed
	frac = VectorLength( cl.cam.speed ) / movespeed;
	if ( frac > 1.0 )
		VectorScale( cl.cam.speed, 1.0 / frac, cl.cam.speed );

	// calc new camera reference origin
	VectorMA( cl.cam.reforg, cls.frametime, cl.cam.speed, cl.cam.reforg );
	if ( cl.cam.reforg[0] < map_min[0] ) cl.cam.reforg[0] = map_min[0];
	if ( cl.cam.reforg[1] < map_min[1] ) cl.cam.reforg[1] = map_min[1];
	if ( cl.cam.reforg[0] > map_max[0] ) cl.cam.reforg[0] = map_max[0];
	if ( cl.cam.reforg[1] > map_max[1] ) cl.cam.reforg[1] = map_max[1];
	cl.cam.reforg[2] = 0;

	// calc real camera origin
	VectorMA( cl.cam.reforg, -CAMERA_START_DIST + cl.cam.lerplevel * CAMERA_LEVEL_DIST, cl.cam.axis[0], cl.cam.camorg );
	cl.cam.camorg[2] += CAMERA_START_HEIGHT + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;

	// zoom change
	frac = CL_GetKeyMouseState(STATE_ZOOM);
	if ( frac > 0.1 ) cl.cam.zoom *= 1.0 + cls.frametime * ZOOM_SPEED * frac;
	if ( frac <-0.1 ) cl.cam.zoom /= 1.0 - cls.frametime * ZOOM_SPEED * frac;
	if ( cl.cam.zoom < MIN_ZOOM ) cl.cam.zoom = MIN_ZOOM;
	else if ( cl.cam.zoom > MAX_ZOOM ) cl.cam.zoom = MAX_ZOOM;
}

/*
=================
CL_CameraMove
=================
*/
void CL_CameraMove (void)
{
	if ( cls.state != ca_active )
		return;

	if ( !scr_vrect.width || !scr_vrect.height )
		return;

	if ( camera_mode == CAMERA_MODE_FIRSTPERSON )
		CL_CameraMoveFirstPerson();
	else
		CL_CameraMoveRemote();
}

/*
=================
CL_CameraRoute
=================
*/
void CL_CameraRoute( pos3_t from, pos3_t target )
{
	// initialize the camera route variables
	PosToVec( from, routeFrom );
	PosToVec( target, routeDelta );
	VectorSubtract( routeDelta, routeFrom, routeDelta );
	routeDelta[2] = 0;
	routeDist = VectorLength( routeDelta );
	VectorNormalize( routeDelta );

	routeLevelStart = from[2];
	routeLevelEnd = target[2];
	VectorCopy( routeFrom, cl.cam.reforg );
	Cvar_SetValue( "cl_worldlevel", target[2] );

	VectorClear( cl.cam.speed );
	cameraRoute = true;
	blockEvents = true;
}

/*
=================
CL_ParseInput
=================
*/
#define ROTATE_SPEED	0.5

void CL_ParseInput (void)
{
	int i, oldx, oldy;

	// get new position
	oldx = mx;
	oldy = my;
	IN_GetMousePos( &mx, &my );

	switch ( mouseSpace )
	{
	case MS_ROTATE:
		// rotate a model
		rotateAngles[1] -= ROTATE_SPEED * (mx - oldx);
		rotateAngles[2] += ROTATE_SPEED * (my - oldy);
		while ( rotateAngles[1] > 360.0 ) rotateAngles[1] -= 360.0;
		while ( rotateAngles[1] < 0.0 ) rotateAngles[1] += 360.0;
		if ( rotateAngles[2] < 0.0 ) rotateAngles[2] = 0.0;
		if ( rotateAngles[2] > 180.0 ) rotateAngles[2] = 180.0;
		return;

	case MS_SHIFTMAP:
		// shift the map
		ccs.center[0] -= (float)(mx - oldx) / (1024 * ccs.zoom);
		ccs.center[1] -= (float)(my - oldy) / (512 * ccs.zoom);
		for ( i = 0; i < 2; i++ )
		{
			while ( ccs.center[i] < 0.0 ) ccs.center[i] += 1.0;
			while ( ccs.center[i] > 1.0 ) ccs.center[i] -= 1.0;
		}
		if ( ccs.center[1] < 0.5/ccs.zoom ) ccs.center[1] = 0.5/ccs.zoom;
		if ( ccs.center[1] > 1.0 - 0.5/ccs.zoom ) ccs.center[1] = 1.0 - 0.5/ccs.zoom;
		return;

	case MS_ZOOMMAP:
		// zoom the map
		ccs.zoom *= pow( 0.995, my - oldy );
		if ( ccs.zoom < 1.0 ) ccs.zoom = 1.0;
		if ( ccs.zoom > 6.0 ) ccs.zoom = 6.0;

		if ( ccs.center[1] < 0.5/ccs.zoom ) ccs.center[1] = 0.5/ccs.zoom;
		if ( ccs.center[1] > 1.0 - 0.5/ccs.zoom ) ccs.center[1] = 1.0 - 0.5/ccs.zoom;
		return;

	case MS_ZOOMBASEMAP:
		// zoom the basemap
		ccs.basezoom *= pow( 0.995, my - oldy );
		if ( ccs.basezoom < 0.5 ) ccs.basezoom = 0.5;
		if ( ccs.basezoom > 3.0 ) ccs.basezoom = 3.0;
		return;


	case MS_SHIFTBASEMAP:
		// shift the basemap
		ccs.basecenter[0] -= (float)(mx - oldx) / (BASEMAP_SIZE_X * ccs.basezoom);
		ccs.basecenter[1] -= (float)(my - oldy) / (BASEMAP_SIZE_Y * ccs.basezoom);
		return;

	case MS_DRAG:
		// do nothing
		return;

	default:
		// standard menu and world mouse handling
		if ( MN_CursorOnMenu( mx, my ) )
		{
			mouseSpace = MS_MENU;
			return;
		}

		mouseSpace = MS_NULL;

		if ( cls.state != ca_active )
			return;

		if ( !scr_vrect.width || !scr_vrect.height )
			return;

		if ( cl.cmode < M_PEND_MOVE )
			CL_ActorMouseTrace();
		else
			mouseSpace = MS_WORLD;
		return;
	}
}
