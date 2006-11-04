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

kbutton_t	in_turnleft, in_turnright, in_shiftleft, in_shiftright, in_shiftup, in_shiftdown;
kbutton_t	in_zoomin, in_zoomout;


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
		Com_Printf ("Three keys down for a button!\n");
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

kbutton_t	in_select, in_action, in_turn;
kbutton_t	in_turnleft, in_turnright, in_shiftleft, in_shiftright, in_shiftup, in_shiftdown;
kbutton_t	in_zoomin, in_zoomout;

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
#define MAX_ZOOM			3.0
#define MIN_CAMZOOM_QUANT	0.2
#define MAX_CAMZOOM_QUANT	1.0
#define LEVEL_SPEED			3.0
#define LEVEL_MIN			0.05

#define CAMERA_START_ROT	280
#define CAMERA_LEVEL_ROT	24
#define CAMERA_START_HEIGHT	420
#define CAMERA_LEVEL_HEIGHT	32

//==========================================================================

/*
============
CL_LevelUp
============
*/
void CL_LevelUp( void )
{
	Cvar_SetValue( "cl_worldlevel", (cl_worldlevel->value < 7) ? cl_worldlevel->value + 1 : 7 );
}

/*
============
CL_LevelDown
============
*/
void CL_LevelDown( void )
{
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

	// test boundaris
	if ( cl.cam.zoom < 1.0 ) cl.cam.zoom = 1.0;
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
CL_ActionClick
============
*/
void CL_ActionClick( void )
{
	if ( mouseSpace == MS_WORLD )
		CL_ActorActionMouse();
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
	Cmd_AddCommand( "action", CL_ActionClick );
	Cmd_AddCommand( "turn", CL_ActorTurnMouse );
	Cmd_AddCommand( "standcrouch", CL_ActorStandCrouch );
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

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
}


#define STATE_FORWARD	1
#define STATE_RIGHT		2
#define STATE_ZOOM		3
#define STATE_ROT		4
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

	else
		return 0.0;

	return value;
}


/*
=================
CL_MouseTrace
=================
*/
trace_t CL_MouseTrace ( void )
{
	float	d;
	vec3_t	angles;
	vec3_t	fw, forward, end;
//	vec3_t	trend;
//	trace_t	tr;

	d = (scr_vrect.width / 2) / tan( (FOV / cl.cam.zoom)*M_PI/360 );		
	angles[YAW]   = atan( (mx*viddef.rx - scr_vrect.width /2 - scr_vrect.x) / d ) * 180/M_PI;
	angles[PITCH] = atan( (my*viddef.ry - scr_vrect.height/2 - scr_vrect.y) / d ) * 180/M_PI;
	angles[ROLL]  = 0;

	// get trace vectors
	AngleVectors( angles, fw, NULL, NULL );
	VectorRotate( cl.cam.axis, fw, forward );
	VectorMA( cl.cam.camorg, 1024 /*cl.cam.camorg[2]/forward[2]+8*/, forward, end );

	// do the trace
	return CM_CompleteBoxTrace( cl.cam.camorg, end, vec3_origin, vec3_origin, (1<<((int)cl_worldlevel->value+1))-1, MASK_ACTORSOLID);

//	return CL_Trace( cl.cam.camorg, end, vec3_origin, vec3_origin, NULL, MASK_ACTORSOLID );

//	CM_TestLineDM( cl.cam.camorg, end, trend );
//	Com_Printf( "(%i %i %i) (%i %i %i)\n", 
//		(int)tr.endpos[0], (int)tr.endpos[1], (int)tr.endpos[2], (int)trend[0], (int)trend[1], (int)trend[2] );
//
//	return tr;
}

/*
=================
CL_CameraMove
=================
*/
void CL_CameraMove (void)
{
	float			angle, frac;
	static float	sy, cy;
	vec3_t			g_forward, g_right, g_up;
	vec3_t			delta;
	float			rotspeed, rotaccel;
	float			movespeed, moveaccel;

	if ( cls.state != ca_active )
		return;

	if ( !scr_vrect.width || !scr_vrect.height )
		return;

	// calculate camera omega
	// stop acceleration
	rotspeed = (cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ( (cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED ) : MIN_CAMROT_SPEED;
	rotaccel = (cl_camrotaccel->value > MIN_CAMROT_ACCEL) ? ( (cl_camrotaccel->value < MAX_CAMROT_ACCEL) ? cl_camrotaccel->value : MAX_CAMROT_ACCEL ) : MIN_CAMROT_ACCEL;
	frac = cls.frametime * rotaccel;
	if ( fabs( cl.cam.omega ) > frac )
	{
		if ( cl.cam.omega > 0 ) cl.cam.omega -= frac;
		else cl.cam.omega += frac;
	} 
	else cl.cam.omega = 0;

	// rotational acceleration
	frac = cls.frametime * rotaccel * 2;
	cl.cam.omega += CL_GetKeyMouseState(STATE_ROT) * frac;

	if (  cl.cam.omega > rotspeed ) cl.cam.omega =  rotspeed;
	if ( -cl.cam.omega > rotspeed ) cl.cam.omega = -rotspeed;

	cl.cam.angles[YAW] += cl.cam.omega * cls.frametime;

	AngleVectors( cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2] );

	// calculate ground-based movement vectors
	angle = cl.cam.angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);


	VectorSet( g_forward, cy, sy, 0.0 );
	VectorSet( g_right, sy, -cy, 0.0 );
	VectorSet( g_up, 0.0, 0.0, 1.0 );

	// calculate camera speed
	// stop acceleration
	movespeed = (cl_cammovespeed->value > MIN_CAMMOVE_SPEED) ? ( (cl_cammovespeed->value < MAX_CAMMOVE_SPEED) ? cl_cammovespeed->value : MAX_CAMMOVE_SPEED ) : MIN_CAMMOVE_SPEED;
	moveaccel = (cl_cammoveaccel->value > MIN_CAMMOVE_ACCEL) ? ( (cl_cammoveaccel->value < MAX_CAMMOVE_ACCEL) ? cl_cammoveaccel->value : MAX_CAMMOVE_ACCEL ) : MIN_CAMMOVE_ACCEL;
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

	frac = VectorLength( cl.cam.speed ) / movespeed;
	if ( frac > 1.0 ) {
		VectorScale( cl.cam.speed, 1.0 / frac, cl.cam.speed );
	}

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

	// calc new camera reference origin
	VectorMA( cl.cam.reforg, cls.frametime, cl.cam.speed, cl.cam.reforg );
	if ( cl.cam.reforg[0] < map_min[0] ) cl.cam.reforg[0] = map_min[0];
	if ( cl.cam.reforg[1] < map_min[1] ) cl.cam.reforg[1] = map_min[1];
	if ( cl.cam.reforg[0] > map_max[0] ) cl.cam.reforg[0] = map_max[0];
	if ( cl.cam.reforg[1] > map_max[1] ) cl.cam.reforg[1] = map_max[1];

	// calc real camera origin
	VectorMA( cl.cam.reforg, -CAMERA_START_ROT + cl.cam.lerplevel * CAMERA_LEVEL_ROT, g_forward, cl.cam.camorg );
	cl.cam.camorg[2] = CAMERA_START_HEIGHT + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;

	// zoom change
	frac = CL_GetKeyMouseState(STATE_ZOOM);
	if ( frac > 0.1 ) cl.cam.zoom *= 1.0 + cls.frametime * ZOOM_SPEED * frac;
	if ( frac <-0.1 ) cl.cam.zoom /= 1.0 - cls.frametime * ZOOM_SPEED * frac;
	if ( cl.cam.zoom < 1.0 ) cl.cam.zoom = 1.0;
	else if ( cl.cam.zoom > MAX_ZOOM ) cl.cam.zoom = MAX_ZOOM;
}


/*
=================
CL_ParseInput
=================
*/
void CL_ParseInput (void)
{
	IN_GetMousePos( &mx, &my );

	if ( mouseSpace == MS_DRAG )
		return;

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

	CL_ActorMouseTrace();
}
