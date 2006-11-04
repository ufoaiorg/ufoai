// cl_actor.c -- actor related routines

#include "client.h"

// public
le_t	*selActor;
int		actorMoveLength;
invChain_t	invChain[MAX_INVCHAIN];

le_t	*teamList[MAX_TEAMLIST];
int		numTeamList;

// private
le_t	*mouseActor;
pos3_t	mousePos, mouseLastPos;

/*
==============================================================

ACTOR MENU UPDATING

==============================================================
*/

/*
======================
CL_CharacterCvars
======================
*/
char *skill_strings[10] =
{
	"Awful",
	"Bad",
	"Poor",
	"Fair",
	"Average",
	"Good",
	"Very Good",
	"Great",
	"Excellent",
	"Heroic"
};

#define SKILL_TO_STRING(x)	skill_strings[x * 10/MAX_SKILL]

void CL_CharacterCvars( character_t *chr )
{
	Cvar_ForceSet( "mn_name", chr->name );
	Cvar_ForceSet( "mn_body", chr->body );
	Cvar_ForceSet( "mn_head", chr->head );
	Cvar_ForceSet( "mn_skin", va( "%i", chr->skin ) );

	Cvar_ForceSet( "mn_vstr", va( "%i", chr->strength+1 ) );
	Cvar_ForceSet( "mn_vdex", va( "%i", chr->dexterity+1 ) );
	Cvar_ForceSet( "mn_vswf", va( "%i", chr->swiftness+1 ) );
	Cvar_ForceSet( "mn_vint", va( "%i", chr->intelligence+1 ) );
	Cvar_ForceSet( "mn_vcrg", va( "%i", chr->courage+1 ) );
	Cvar_ForceSet( "mn_tstr", SKILL_TO_STRING( chr->strength ) );
	Cvar_ForceSet( "mn_tdex", SKILL_TO_STRING( chr->dexterity ) );
	Cvar_ForceSet( "mn_tswf", SKILL_TO_STRING( chr->swiftness ) );
	Cvar_ForceSet( "mn_tint", SKILL_TO_STRING( chr->intelligence ) );
	Cvar_ForceSet( "mn_tcrg", SKILL_TO_STRING( chr->courage ) );
}


/*
=================
CL_ActorUpdateCVars
=================
*/
char infoText[256];

void CL_ActorUpdateCVars( void )
{
	qboolean	refresh;
	int	time;

	if ( cls.state != ca_active )
		return;

	refresh = (int)Cvar_VariableValue( "hud_refresh" );
	if ( refresh ) 
	{
		Cvar_Set( "hud_refresh", "0" );
		Cvar_Set( "cl_worldlevel", cl_worldlevel->string );
	}

	if ( selActor )
	{
		// set generic cvars
		Cvar_Set( "mn_tu", va( "%i", selActor->TU ) );
		Cvar_Set( "mn_tumax", va( "%i", selActor->maxTU ) );
		Cvar_Set( "mn_moral", va( "%i", selActor->moral ) );
		Cvar_Set( "mn_hp", va( "%i", selActor->HP ) );

		// write info
		time = 0;
		if ( cl.time < cl.msgTime )
		{
			// special message
			strcpy( infoText, cl.msgText );
		}
		else if ( selActor->state & STATE_PANIC )
		{
			// panic
			sprintf( infoText, "Currently panics!\n" );
		} else {
			// move or shoot
			if ( cl.cmode != M_MOVE )
			{
				fireDef_t *fd;
				int	weapon;

				weapon = (cl.cmode-1)/2 ? selActor->i.left.t : selActor->i.right.t;
				if ( weapon == NONE && selActor->i.right.t != NONE && csi.ods[selActor->i.right.t].twohanded ) 
					weapon = selActor->i.right.t;

				if ( weapon != NONE )
				{
					fd = &csi.ods[weapon].fd[(cl.cmode-1)%2];
					sprintf( infoText, "%s\n%s (%i)\t%i\n",  csi.ods[weapon].name, fd->name, fd->ammo, fd->time );
					time = fd->time;
				} 
				else cl.cmode = M_MOVE;
			}
			if ( cl.cmode == M_MOVE )
			{
				time = actorMoveLength;
				if ( actorMoveLength < 0x3F )
					sprintf( infoText, "Health\t%i\nmove\t%i\n", selActor->HP, actorMoveLength );
				else
					sprintf( infoText, "Health\t%i\n", selActor->HP );
			} 
		}

		// calc remaining TUs
		time = selActor->TU - time;
		if ( time < 0 ) time = 0;
		Cvar_Set( "mn_turemain", va( "%i", time ) );

		// print ammo
		if ( selActor->i.right.t != NONE ) Cvar_SetValue( "mn_ammoright", selActor->i.right.a );
		else Cvar_Set( "mn_ammoright", "" );
		if ( selActor->i.left.t != NONE ) Cvar_SetValue( "mn_ammoleft", selActor->i.left.a );
		else Cvar_Set( "mn_ammoleft", "" );
		if ( selActor->i.left.t == NONE && selActor->i.right.t != NONE && csi.ods[selActor->i.right.t].twohanded )
			Cvar_Set( "mn_ammoleft", Cvar_VariableString( "mn_ammoright" ) );

		// change stand-crouch
		if ( cl.oldstate != selActor->state || refresh )
		{
			cl.oldstate = selActor->state;
			if ( selActor->state & STATE_CROUCHED ) Cbuf_AddText( "tocrouch\n" );
			else Cbuf_AddText( "tostand\n" );
		}

		// set info text
		menuText = infoText;
	} 
	else
	{
		// no actor selected, reset cvars
		Cvar_Set( "mn_tu", "" );
		Cvar_Set( "mn_turemain", "" );
		Cvar_Set( "mn_tumax", "30" );
		Cvar_Set( "mn_moral", "" );
		Cvar_Set( "mn_hp", "" );
		Cvar_Set( "mn_ammoright", "" );
		Cvar_Set( "mn_ammoleft", "" );
		if ( refresh ) Cbuf_AddText( "tostand\n" );
		menuText = NULL;
	}

	// mode
	if ( cl.oldcmode != cl.cmode || refresh )
	{
		cl.oldcmode = cl.cmode;
		switch ( cl.cmode )
		{
		case M_FIRE_PL: Cbuf_AddText( "towpl\n" ); break;
		case M_FIRE_SL: Cbuf_AddText( "towsl\n" ); break;
		case M_FIRE_PR: Cbuf_AddText( "towpr\n" ); break;
		case M_FIRE_SR: Cbuf_AddText( "towsr\n" ); break;
		default: Cbuf_AddText( "tomov\n" ); break;
		}
	}

	// player bar
	if ( cl_selected->modified || refresh )
	{
		int i;
		for ( i = 0; i < 8; i++ )
		{
			if ( !teamList[i] || teamList[i]->state & STATE_DEAD )
				Cbuf_AddText( va( "huddisable%i\n", i ) );
			else if ( i == (int)cl_selected->value )
				Cbuf_AddText( va( "hudselect%i\n", i ) );
			else
				Cbuf_AddText( va( "huddeselect%i\n", i ) );
		}
		cl_selected->modified = false;
	}
}


/*
==============================================================

ACTOR SELECTION AND TEAM LIST

==============================================================
*/

/*
=================
CL_AddActorToTeamList
=================
*/
void CL_AddActorToTeamList( le_t *le )
{
	int		i;

	// test team
	if ( !le || le->team != cls.team || le->pnum != cl.pnum || (le->state & STATE_DEAD) )
		return;

	// check list length
	if ( numTeamList >= MAX_TEAMLIST )
		return;

	// check list for that actor
	for ( i = 0; i < numTeamList; i++ )
		if ( teamList[i] == le )
			break;

	// add it
	if ( i == numTeamList )
	{
		teamList[numTeamList++] = le;
		Cbuf_AddText( va( "huddeselect%i\n", i ) );
		if ( numTeamList == 1 ) CL_ActorSelectList( 0 );
	}
}


/*
=================
CL_ActorSelect
=================
*/
qboolean CL_ActorSelect( le_t *le )
{
	int i;

	// test team
	if ( !le || le->team != cls.team || (le->state & STATE_DEAD) )
		return false;

	// select him
	if ( selActor ) selActor->selected = false;
	if ( le ) le->selected = true;
	selActor = le;
	menuInventory = &selActor->i;

	for ( i = 0; i < numTeamList; i++ )
		if ( teamList[i] == le )
		{
			// console commands, update cvars
			Cvar_ForceSet( "cl_selected", va( "%i", i ) );
			CL_CharacterCvars( &curTeam[i] );

			// calculate possible moves
			CL_BuildForbiddenList();
			Grid_MoveCalc( le->pos, MAX_ROUTE, fb_list, fb_length );
			return true;
		}

	return false;
}


/*
=================
CL_ActorSelectList
=================
*/
qboolean CL_ActorSelectList( int num )
{
	le_t	*le;

	// check if actor exists
	if ( num >= numTeamList )
		return false;

	// select actor
	le = teamList[num];
	if ( !CL_ActorSelect( le ) )
		return false;

	// center view
	VectorCopy( le->origin, cl.cam.reforg );
	Cvar_SetValue( "cl_worldlevel", le->pos[2] );
	return true;
}


/*
==============================================================

ACTOR MOVEMENT AND SHOOTING

==============================================================
*/
byte	*fb_list[MAX_FB_LIST];
int		fb_length;

/*
=================
CL_BuildForbiddenList
=================
*/
void CL_BuildForbiddenList( void )
{
	le_t	*le;
	int		i;

	fb_length = 0;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( le->inuse && !(le->state & STATE_DEAD) && le->type == ET_ACTOR )
			fb_list[fb_length++] = le->pos;

	if ( fb_length > MAX_FB_LIST )
		Com_Error( ERR_DROP, "CL_BuildForbiddenList: list too long" );
}


/*
=================
CL_CheckAction
=================
*/
int CL_CheckAction( void )
{
	if ( !selActor )
	{
		Com_Printf( "Nobody selected.\n" );
		return false;
	}

/*	if ( blockEvents )
	{
		Com_Printf( "Can't do that right now.\n" );
		return false;
	}
*/
	if ( cls.team != cl.actTeam )
	{
		Com_Printf( "This isn't your round.\n" );
		return false;
	}

	return true;
}


/*
=================
CL_ActorStartMove
=================
*/
void CL_ActorStartMove( le_t *le, pos3_t to )
{
	int		length;

	if ( !CL_CheckAction() )
		return;

	length = Grid_MoveLength( to, false );

	if ( !length || length >= 0x3F )
	{
		// move not valid, don't even care to send
		return;
	}

	// debugging
/*	Grid_PosToVec( to, oldVec );
	VectorCopy( to, pos );

	while ( (dv = Grid_MoveNext( pos )) < 0xFF )
	{
		PosAddDV( pos, dv );
		Grid_PosToVec( pos, vec );
		CL_RailTrail( oldVec, vec );
		VectorCopy( vec, oldVec );
	}*/
		
	// move seems to be possible
	// send request to server
	MSG_WriteFormat( &cls.netchan.message, "bbsg",
		clc_action, PA_MOVE, le->entnum, to );
}


/*
=================
CL_ActorShoot
=================
*/
void CL_ActorShoot( le_t *le, pos3_t at )
{
	int		mode;

	if ( !CL_CheckAction() )
		return;

	// send request to server
	mode = cl.cmode - 1;
	if ( mode >= ST_LEFT_PRIMARY && le->i.left.t == NONE ) mode -= 2;

	MSG_WriteFormat( &cls.netchan.message, "bbsgb",
		clc_action, PA_SHOOT, le->entnum, at, mode );
}


/*
=================
CL_ActorReload
=================
*/
void CL_ActorReload( int hand )
{
	inventory_t	*inv;
	invChain_t	*ic;
	objDef_t	*od;
	char	weapon[MAX_VAR];
	char	item[MAX_VAR];
	char	*underline;
	int		num, container, x, y, tu;

	if ( !CL_CheckAction() ) 
		return;

	// check weapon
	inv = &selActor->i;
	if ( hand == csi.idRight && inv->right.t != NONE ) strncpy( weapon, csi.ods[inv->right.t].kurz, MAX_VAR );
	else if ( hand == csi.idLeft && inv->left.t != NONE ) strncpy( weapon, csi.ods[inv->left.t].kurz, MAX_VAR );
	else if ( hand == csi.idLeft && inv->right.t != NONE && csi.ods[inv->right.t].twohanded ) { hand = csi.idRight; strncpy( weapon, csi.ods[inv->right.t].kurz, MAX_VAR ); }
	else return;

	// find clip definition number
	for ( num = 0, od = csi.ods; num < csi.numODs; num++, od++ )
	{
		strcpy( item, od->kurz );
		underline = strchr( item, '_' );
		if ( !underline ) 
			continue;
		*underline = 0;
		if ( !strcmp( item, weapon ) ) 
			break;
	}
	if ( num == csi.numODs )
		return;

	// search for clips
	container = -1;
	x = 0; y = 0;

	if ( inv->right.t == num ) { container = csi.idRight; x = 0; y = 0; }
	else if ( inv->left.t == num ) { container = csi.idLeft; x = 0; y = 0; }
	else
	{
		tu = 100;
		for ( ic = inv->inv; ic; ic = ic->next )
			if ( ic->item.t == num && csi.ids[ic->container].out < tu )
			{
				container = ic->container;
				x = ic->x; y = ic->y;
				tu = csi.ids[ic->container].out;
			}
		if ( inv->floor )
			for ( ic = inv->floor->inv; ic; ic = ic->next )
				if ( ic->item.t == num && csi.ids[ic->container].out < tu )
				{
					container = csi.idFloor;
					x = ic->x; y = ic->y;
					tu = csi.ids[ic->container].out;
				}
	}

	// send request
	if ( container != -1 )
		MSG_WriteFormat( &cls.netchan.message, "bbsbbbbbb",
			clc_action, PA_INVMOVE, selActor->entnum, container, x, y, hand, 0, 0 );
	else
		Com_Printf( "No clip left.\n" );
}


/*
=================
CL_ActorDoMove
=================
*/
void CL_ActorDoMove( sizebuf_t *sb )
{
	le_t	*le;
//	int		i;

	// get le
	le = LE_Get( MSG_ReadShort( sb ) );
	if ( !le )
	{
		Com_Printf( "Can't move, LE doesn't exist\n" );
		le = &LEs[numLEs];
		MSG_ReadFormat( sb, ev_format[EV_ACTOR_MOVE],
			&le->pathLength, le->path );
		return;
	}
	
	// get length
	MSG_ReadFormat( sb, ev_format[EV_ACTOR_MOVE],
		&le->pathLength, le->path );

	// activate PathMove function
	le->i.floor = NULL;
	le->think = LET_StartPathMove;
	le->pathPos = 0;
	le->startTime = cl.time;
	le->endTime = cl.time;
	if ( le->state & STATE_CROUCHED ) le->speed = 50;
	else le->speed = 100;
	blockEvents = true;
}


/*
=================
CL_ActorTurnMouse
=================
*/
void CL_ActorTurnMouse( void ) 
{
	vec3_t	div;
	byte	dv;

	if ( mouseSpace != MS_WORLD )
		return;

	if ( !CL_CheckAction() )
		return;

	// calculate dv
	VectorSubtract( mousePos, selActor->pos, div );
	dv = AngleToDV( (int)(atan2( div[1], div[0] ) * 180 / M_PI) );

	// send message to server
	MSG_WriteFormat( &cls.netchan.message, "bbsb",
		clc_action, PA_TURN, selActor->entnum, dv );
}


/*
=================
CL_ActorDoTurn
=================
*/
void CL_ActorDoTurn( sizebuf_t *sb ) 
{
	le_t	*le;

	// get le
	le = LE_Get( MSG_ReadShort( sb ) );
	if ( !le )
	{
		Com_Printf( "Can't turn, LE doesn't exist\n" );
		MSG_ReadByte( sb );
		return;
	}

	le->dir = MSG_ReadByte( sb );
	le->angles[YAW] = dangle[le->dir];

	cl.cmode = M_MOVE;

	// calculate possible moves
	CL_BuildForbiddenList();
	Grid_MoveCalc( le->pos, MAX_ROUTE, fb_list, fb_length );
}


/*
=================
CL_ActorStandCrouch
=================
*/
void CL_ActorStandCrouch( void ) 
{
	if ( !CL_CheckAction() )
		return;

	// send message to server
	MSG_WriteFormat( &cls.netchan.message, "bbsb",
		clc_action, PA_STATE, selActor->entnum, selActor->state ^ STATE_CROUCHED );
}


/*
=================
CL_ActorDoShoot
=================
*/
qboolean firstShot;

void CL_ActorDoShoot( sizebuf_t *sb ) 
{
	fireDef_t	*fd;
	le_t	*le;
	int		type;
	vec3_t	muzzle, impact;
	int	flags, normal;

	// get le
	le = LE_Get( MSG_ReadShort( sb ) );

	// read data
	MSG_ReadFormat( sb, ev_format[EV_ACTOR_SHOOT], 
		&type, &flags, &muzzle, &impact, &normal );	
	
	// get the fire def
	fd = &csi.ods[type & 0x7F].fd[!!(type & 0x80)];

	// add effect le
	LE_AddProjectile( fd, flags, muzzle, impact, normal );

	// start the sound
	if ( (!fd->soundOnce || firstShot) && fd->fireSound[0] ) 
		S_StartLocalSound( fd->fireSound );
	firstShot = false;

	// calculate possible moves
	CL_BuildForbiddenList();

	// do actor related stuff
	if ( !le ) return;
	le->think = LET_StartShoot;
	Grid_MoveCalc( le->pos, MAX_ROUTE, fb_list, fb_length );
}


/*
=====================
CL_ActorStartShoot
=====================
*/
void CL_ActorStartShoot( sizebuf_t *sb )
{
	fireDef_t *fd;
	le_t	*le;
	pos3_t	target;
	int		number, type;

	number = MSG_ReadShort( sb );
	type = MSG_ReadByte( sb );
	fd = GET_FIREDEF( type );
	le = LE_Get( number );
	MSG_ReadGPos( sb, target );

	// center view (if wanted)
	if ( (int)cl_centerview->value && cl.actTeam != cls.team )
	{
		vec3_t	tv;
		PosToVec( target, tv );
		VectorCopy( tv, cl.cam.reforg );
		Cvar_SetValue( "cl_worldlevel", target[2] );
	}

	// first shot
	firstShot = true;
	
	if ( !le ) return;
	re.AnimChange( &le->as, le->model1, LE_GetAnim( "move", &le->i, le->state ) );
	re.AnimAppend( &le->as, le->model1, LE_GetAnim( "shoot", &le->i, le->state ) );
}


/*
=================
CL_ActorDie
=================
*/
void CL_ActorDie( sizebuf_t *sb ) 
{
	le_t	*le;
	int		i, j;

	// get le
	le = LE_Get( MSG_ReadShort( sb ) );

	if ( !le )
	{
		Com_Printf( "Invisible actor can't die... LE not found\n" );
		return;
	}

	// set relevant vars
	le->HP = 0;
	le->state = MSG_ReadByte( sb );

	// play animation
	le->think = NULL;
	re.AnimChange( &le->as, le->model1, va( "death%i", le->state & STATE_DEAD ) );
	re.AnimAppend( &le->as, le->model1, va( "dead%i",  le->state & STATE_DEAD ) );

	// check selection
	if ( selActor == le ) 
	{
		for ( i = 0; i < numTeamList; i++ )
			if ( CL_ActorSelect( teamList[i] ) )
				break;

		if ( i == numTeamList )
		{
			selActor->selected = false;
			selActor = NULL;
			Cvar_ForceSet( "mn_name", "" );
			Cvar_ForceSet( "mn_body", "" );
			Cvar_ForceSet( "mn_head", "" );
		}
	}

	for ( i = 0; i < numTeamList; i++ )
		if ( teamList[i] == le )
		{
			// disable hud button
			Cbuf_AddText( va( "huddisable%i\n", i ) );

			// campaign death
			if ( !curCampaign )
				return;

			for ( j = 0; j < numWholeTeam; j++ )
				if ( curTeam[i].ucn == wholeTeam[j].ucn )
				{
					deathMask |= 1 << j;
					break;
				}

			// remove from list
			teamList[i] = NULL;
			return;
		}
}


/*
==============================================================

MOUSE INPUT

==============================================================
*/

/*
=================
CL_ActorSelectMouse
=================
*/
void CL_ActorSelectMouse( void )
{
	if ( mouseSpace != MS_WORLD )
		return;

	if ( cl.cmode == M_MOVE )
		CL_ActorSelect( mouseActor );
	else
		CL_ActorShoot( selActor, mousePos );
}


/*
=================
CL_ActorActionMouse
=================
*/
void CL_ActorActionMouse( void )
{
	if ( !selActor || mouseSpace != MS_WORLD ) 
		return;

	if ( cl.cmode == M_MOVE )
		CL_ActorStartMove( selActor, mousePos );
	else
		cl.cmode = M_MOVE;
}


/*
==============================================================

ROUND MANAGEMENT

==============================================================
*/

/*
=================
CL_NextRound
=================
*/
void CL_NextRound( void )
{
	// can't end round if we're not active
	if ( cls.team != cl.actTeam )
		return;

	// send endround
	MSG_WriteByte( &cls.netchan.message, clc_endround );
}


/*
=================
CL_DoEndRound
=================
*/
void CL_DoEndRound( sizebuf_t *sb )
{
	// hud changes
	if ( cls.team == cl.actTeam ) Cbuf_AddText( "endround\n" );

	// change active player
	Com_Printf( "team %i ended round", cl.actTeam );
	cl.actTeam = MSG_ReadByte( sb );
	Com_Printf( ", team %i's round started!\n", cl.actTeam );

	// hud changes
	if ( cls.team == cl.actTeam ) 
	{
		Cbuf_AddText( "startround\n" );
		cl.msgTime = cl.time + 2000;
		strcpy( cl.msgText, "Your round started!\n" );
		if ( selActor ) Grid_MoveCalc( selActor->pos, MAX_ROUTE, fb_list, fb_length );
	}
}


/*
==============================================================

MOUSE SCANNING

==============================================================
*/

/*
=================
CL_ActorMouseTrace
=================
*/
void CL_ActorMouseTrace( void )
{
	trace_t	mouseTrace;
	le_t	*le;
	int		i;

	mouseTrace = CL_MouseTrace();

	if ( mouseTrace.endpos[2] < 0.0 )
		// the mouse is out of the world
		return;

	VectorMA( mouseTrace.endpos, 5, mouseTrace.plane.normal, mouseTrace.endpos );
	VecToPos( mouseTrace.endpos, mousePos );

	// don't get too high
	if ( mousePos[2] > cl_worldlevel->value ) 
		mousePos[2] = cl_worldlevel->value;

	// search for an actor on this field
	mouseActor = NULL;
	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( le->inuse && !(le->state & STATE_DEAD) && le->type == ET_ACTOR && VectorCompare( le->pos, mousePos ) )
		{
			mouseActor = le;
			break;
		}

	// calculate move length
	if ( selActor && !VectorCompare( mousePos, mouseLastPos ) )
	{
		VectorCopy( mousePos, mouseLastPos );
		actorMoveLength = Grid_MoveLength( mousePos, false );
		if ( selActor->state & STATE_CROUCHED ) actorMoveLength *= 1.5;
	}

	// mouse is in the world
	mouseSpace = MS_WORLD;
}


/*
==============================================================

ACTOR GRAPHICS

==============================================================
*/

/*
=================
CL_AddActor
=================
*/
qboolean CL_AddActor( le_t *le, entity_t *ent )
{
	entity_t	add;

	if ( !(le->state & STATE_DEAD) )
	{
		// add weapon
		if ( le->i.left.t != NONE )
		{
			memset( &add, 0, sizeof( entity_t ) );
			
			add.sunfrac = le->sunfrac;
			add.model = cl.model_weapons[le->i.left.t];

			add.tagent = V_GetEntity() + 2 + (le->i.right.t != NONE);
			add.tagname = "tag_lweapon";

			V_AddEntity( &add );
		}

		// add weapon
		if ( le->i.right.t != NONE )
		{
			memset( &add, 0, sizeof( entity_t ) );
			
			add.sunfrac = le->sunfrac;
			add.alpha = le->alpha;
			add.model = cl.model_weapons[le->i.right.t];

			add.tagent = V_GetEntity() + 2;
			add.tagname = "tag_rweapon";

			V_AddEntity( &add );
		}
	}

	// add head
	memset( &add, 0, sizeof( entity_t ) );
	
	add.sunfrac = le->sunfrac;
	add.alpha = le->alpha;
	add.model = le->model2;

	add.tagent = V_GetEntity() + 1;

	add.tagname = "tag_head";

	V_AddEntity( &add );

	// add actor special effects
	ent->flags |= RF_SHADOW;

	if ( !(le->state & STATE_DEAD) )
	{
		if ( le->selected ) ent->flags |= RF_SELECTED;
		if ( cl_markactors->value && le->team == cls.team )
		{
			if ( le->pnum == cl.pnum ) ent->flags |= RF_MEMBER;
			if ( le->pnum != cl.pnum ) ent->flags |= RF_ALLIED;
		}
	}

	return true;
}


/*
==============================================================

TARGETING GRAPHICS

==============================================================
*/

const vec3_t	box_delta = {11, 11, 27};

/*
=================
CL_AddTargeting
=================
*/
void CL_AddTargeting( void )
{
	if ( mouseSpace != MS_WORLD )
		return;

	if ( cl.cmode == M_MOVE )
	{
		entity_t	ent;

		memset( &ent, 0, sizeof(entity_t) );
		ent.flags = RF_BOX;

		Grid_PosToVec( mousePos, ent.origin );
//		V_AddLight( ent.origin, 256, 1.0, 0, 0 );
		VectorAdd( ent.origin, box_delta, ent.oldorigin );
		VectorSubtract( ent.origin, box_delta, ent.origin );

		// color
		if ( mouseActor )
			ent.alpha = 0.4 + 0.2*sin((float)cl.time/80);
		else 
			ent.alpha = 0.3;

		if ( selActor && actorMoveLength < 0x3F && actorMoveLength <= selActor->TU )
			VectorSet( ent.angles, 0, 1, 0 );
		else
			VectorSet( ent.angles, 0, 0, 1 );

		// add it
		V_AddEntity( &ent );
	} else {
		ptl_t	*ptl;
		vec3_t	start, end;

		// show cross
		Grid_PosToVec( mousePos, start );
		ptl = CL_ParticleSpawn( "cross", start, NULL, NULL );

		if ( selActor ) 
		{
			// show trace
			Grid_PosToVec( selActor->pos, end );
			ptl = CL_ParticleSpawn( "tracer", start, end, NULL );
		}
	}
}

